/*
 * otelsilta_observer.c — Observer API router + zend_execute_ex override.
 *
 * The Observer API (zend_observer_fcall_register) is used for targeted
 * hooks on specific internal functions (PDO, curl, Redis, Memcached,
 * templates).  These work well because the engine caches the returned
 * {begin, end} handlers per function for the process lifetime.
 *
 * Generic userland function tracing uses zend_execute_ex override
 * instead, because it needs to fire on every invocation — the observer
 * API's per-function caching means it won't trace every call.
 */

#include "observer.h"
#include "php_otelsilta.h"
#include "tracer.h"
#include "span.h"
#include "sanitizer.h"
#include "propagation.h"
#include "aggregator.h"

#include "zend_observer.h"
#include "zend_execute.h"
#include "zend_interfaces.h"

#include <string.h>
#include <stdio.h>

/* ================================================================
 * Forward declarations for begin/end handler pairs, grouped by feature
 * ================================================================ */

/* DB (PDO + MySQLi) */
static void ob_pdo_query_begin(zend_execute_data *ex);
static void ob_pdo_query_end(zend_execute_data *ex, zval *retval);
static void ob_pdo_exec_begin(zend_execute_data *ex);
static void ob_pdo_exec_end(zend_execute_data *ex, zval *retval);
static void ob_pdo_prepare_begin(zend_execute_data *ex);
static void ob_pdo_prepare_end(zend_execute_data *ex, zval *retval);
static void ob_stmt_execute_begin(zend_execute_data *ex);
static void ob_stmt_execute_end(zend_execute_data *ex, zval *retval);
static void ob_pdo_construct_begin(zend_execute_data *ex);
static void ob_pdo_construct_end(zend_execute_data *ex, zval *retval);
static void ob_mysqli_query_begin(zend_execute_data *ex);
static void ob_mysqli_query_end(zend_execute_data *ex, zval *retval);

/* HTTP (curl) */
static void ob_curl_exec_begin(zend_execute_data *ex);
static void ob_curl_exec_end(zend_execute_data *ex, zval *retval);
static void ob_curl_setopt_begin(zend_execute_data *ex);
static void ob_curl_setopt_end(zend_execute_data *ex, zval *retval);

/* Cache (Redis, Memcached) */
static void ob_cache_begin(zend_execute_data *ex);
static void ob_cache_end(zend_execute_data *ex, zval *retval);

/* Templates (Twig, Blade) */
static void ob_tpl_begin(zend_execute_data *ex);
static void ob_tpl_end(zend_execute_data *ex, zval *retval);

/* Generic function-level tracing is handled via zend_execute_ex override
 * (see otelsilta_execute_ex below), NOT via the observer API, because
 * the observer caches handler decisions per-function per-process and
 * does not invoke callbacks on every call. */

/* ================================================================
 * Helper: match class name (case-insensitive)
 * ================================================================ */

static inline int match_class(zend_execute_data *ex, const char *name) {
    if (!ex->func || !ex->func->common.scope) return 0;
    return strcasecmp(ZSTR_VAL(ex->func->common.scope->name), name) == 0;
}

static inline int match_function(zend_execute_data *ex, const char *name) {
    if (!ex->func || !ex->func->common.function_name) return 0;
    return strcasecmp(ZSTR_VAL(ex->func->common.function_name), name) == 0;
}

static inline const char *get_class_name(zend_execute_data *ex) {
    if (!ex->func || !ex->func->common.scope) return NULL;
    return ZSTR_VAL(ex->func->common.scope->name);
}

static inline const char *get_function_name(zend_execute_data *ex) {
    if (!ex->func || !ex->func->common.function_name) return NULL;
    return ZSTR_VAL(ex->func->common.function_name);
}

/* ================================================================
 * Per-request span handle stack for observer begin/end pairing.
 *
 * The Observer API's begin/end callbacks don't share per-invocation
 * state, so we use a small stack to associate begin → end.
 * ================================================================ */

/* Observer begin/end pairs don't share per-invocation state,
 * so we use a small stack in module globals to pair them. */

static inline void ob_push_span(otelsilta_span_t *span) {
    if (OTELSILTA_G(ob_span_stack_depth) < OTELSILTA_OB_SPAN_STACK_SIZE) {
        OTELSILTA_G(ob_span_stack)[OTELSILTA_G(ob_span_stack_depth)++] = span;
    }
}

static inline otelsilta_span_t *ob_pop_span(void) {
    if (OTELSILTA_G(ob_span_stack_depth) > 0) {
        return OTELSILTA_G(ob_span_stack)[--OTELSILTA_G(ob_span_stack_depth)];
    }
    return NULL;
}

/* ================================================================
 * Deferred-decision metadata stack for DB / cache operations.
 *
 * For aggregation + duration gating we cannot create the span in
 * the begin handler — we need to know the duration first.  Instead,
 * begin handlers push metadata onto this stack, and end handlers
 * pop it and decide: create a real span (slow) or aggregate (fast).
 * ================================================================ */

#define OB_META_STACK_SIZE  OTELSILTA_OB_SPAN_STACK_SIZE

typedef struct {
    uint64_t    start_ns;                           /* from otelsilta_time_ns() */
    char        db_system[32];
    char        db_operation[32];
    char        table_name[128];                    /* normalized table or cache system */
    char        sanitized_sql[OTELSILTA_MAX_STR_LEN];
    char        span_name[320];
    char        parent_span_id[17];
    int         is_cache;                           /* 1 = cache op, 0 = DB op */
    int         valid;                              /* 0 = skipped (not active) */
} ob_meta_t;

static ob_meta_t  ob_meta_stack[OB_META_STACK_SIZE];
static int        ob_meta_stack_depth = 0;

static inline void ob_push_meta(ob_meta_t *meta) {
    if (ob_meta_stack_depth < OB_META_STACK_SIZE) {
        memcpy(&ob_meta_stack[ob_meta_stack_depth++], meta, sizeof(ob_meta_t));
    }
}

static inline ob_meta_t *ob_pop_meta(void) {
    if (ob_meta_stack_depth > 0) {
        return &ob_meta_stack[--ob_meta_stack_depth];
    }
    return NULL;
}

/* Push an invalid/empty meta entry (for when we skip) */
static inline void ob_push_meta_empty(void) {
    if (ob_meta_stack_depth < OB_META_STACK_SIZE) {
        memset(&ob_meta_stack[ob_meta_stack_depth], 0, sizeof(ob_meta_t));
        ob_meta_stack[ob_meta_stack_depth].valid = 0;
        ob_meta_stack_depth++;
    }
}

/* Reset at request start */
void otelsilta_observer_rinit(void) {
    OTELSILTA_G(ob_span_stack_depth) = 0;
    memset(OTELSILTA_G(ob_span_stack), 0, sizeof(OTELSILTA_G(ob_span_stack)));
    ob_meta_stack_depth = 0;
}

void otelsilta_observer_pdo_rinit(void) {
    OTELSILTA_G(pdo_dsn_map) = (HashTable *)emalloc(sizeof(HashTable));
    zend_hash_init(OTELSILTA_G(pdo_dsn_map), 4, NULL, ZVAL_PTR_DTOR, 0);
    OTELSILTA_G(stmt_sql_map) = (HashTable *)emalloc(sizeof(HashTable));
    zend_hash_init(OTELSILTA_G(stmt_sql_map), 8, NULL, ZVAL_PTR_DTOR, 0);
}

void otelsilta_observer_pdo_rshutdown(void) {
    if (OTELSILTA_G(pdo_dsn_map)) {
        zend_hash_destroy(OTELSILTA_G(pdo_dsn_map));
        efree(OTELSILTA_G(pdo_dsn_map));
        OTELSILTA_G(pdo_dsn_map) = NULL;
    }
    if (OTELSILTA_G(stmt_sql_map)) {
        zend_hash_destroy(OTELSILTA_G(stmt_sql_map));
        efree(OTELSILTA_G(stmt_sql_map));
        OTELSILTA_G(stmt_sql_map) = NULL;
    }
}

static const char *get_db_system_for_object(zend_object *obj) {
    if (!OTELSILTA_G(pdo_dsn_map)) return "other";
    zval *v = zend_hash_index_find(OTELSILTA_G(pdo_dsn_map), (zend_ulong)obj->handle);
    if (v && Z_TYPE_P(v) == IS_STRING) {
        return otelsilta_dsn_to_db_system(Z_STRVAL_P(v));
    }
    return "other";
}

/* ================================================================
 * Redis/Memcached method lists for cache detection
 * ================================================================ */

static const char *redis_methods[] = {
    "get", "set", "del", "mget", "mset", "hget", "hset", "hdel",
    "lpush", "rpush", "lpop", "rpop", "sadd", "srem", "sismember",
    "zadd", "zrange", "zrangebyscore", "expire", "ttl", "exists",
    "incr", "decr", "ping", NULL
};

static const char *memcached_methods[] = {
    "get", "getmulti", "set", "setmulti", "add", "replace", "delete",
    "increment", "decrement", "flush", "getstats", NULL
};

static inline int is_in_list(const char *name, const char **list) {
    for (int i = 0; list[i]; i++) {
        if (strcasecmp(name, list[i]) == 0) return 1;
    }
    return 0;
}

/* ================================================================
 * The central router — called once per function PER PROCESS.
 *
 * CRITICAL: The engine caches the returned {begin, end} handlers
 * for the lifetime of the PHP worker process.  Therefore this
 * function must NOT check ANY per-request state or PHP_INI_ALL
 * settings (enabled, feature flags, request_active, span depth, …).
 * Only function-name / class-name matching is safe here.
 * All runtime gating goes in the begin handlers.
 * ================================================================ */

zend_observer_fcall_handlers otelsilta_observer_fcall_init(
    zend_execute_data *execute_data)
{
    zend_observer_fcall_handlers empty = {NULL, NULL};

    zend_function *func = execute_data->func;
    if (!func) return empty;

    const char *fn_name  = get_function_name(execute_data);
    const char *cls_name = get_class_name(execute_data);

    /* ---- DB: PDO + MySQLi ---- */
    /* PDO::__construct */
    if (match_class(execute_data, "PDO") &&
        match_function(execute_data, "__construct")) {
        return (zend_observer_fcall_handlers){ob_pdo_construct_begin,
                                               ob_pdo_construct_end};
    }
    /* PDO::query */
    if (match_class(execute_data, "PDO") &&
        match_function(execute_data, "query")) {
        return (zend_observer_fcall_handlers){ob_pdo_query_begin,
                                               ob_pdo_query_end};
    }
    /* PDO::exec */
    if (match_class(execute_data, "PDO") &&
        match_function(execute_data, "exec")) {
        return (zend_observer_fcall_handlers){ob_pdo_exec_begin,
                                               ob_pdo_exec_end};
    }
    /* PDO::prepare */
    if (match_class(execute_data, "PDO") &&
        match_function(execute_data, "prepare")) {
        return (zend_observer_fcall_handlers){ob_pdo_prepare_begin,
                                               ob_pdo_prepare_end};
    }
    /* PDOStatement::execute */
    if (match_class(execute_data, "PDOStatement") &&
        match_function(execute_data, "execute")) {
        return (zend_observer_fcall_handlers){ob_stmt_execute_begin,
                                               ob_stmt_execute_end};
    }
    /* mysqli::query / mysqli_query */
    if ((match_class(execute_data, "mysqli") &&
         match_function(execute_data, "query")) ||
        match_function(execute_data, "mysqli_query")) {
        return (zend_observer_fcall_handlers){ob_mysqli_query_begin,
                                               ob_mysqli_query_end};
    }

    /* ---- HTTP: curl ---- */
    if (fn_name && strcmp(fn_name, "curl_exec") == 0) {
        return (zend_observer_fcall_handlers){ob_curl_exec_begin,
                                               ob_curl_exec_end};
    }
    if (fn_name && strcmp(fn_name, "curl_setopt") == 0) {
        return (zend_observer_fcall_handlers){ob_curl_setopt_begin,
                                               ob_curl_setopt_end};
    }

    /* ---- Cache: Redis, Memcached ---- */
    if (fn_name) {
        if (match_class(execute_data, "Redis") &&
            is_in_list(fn_name, redis_methods)) {
            return (zend_observer_fcall_handlers){ob_cache_begin, ob_cache_end};
        }
        if (match_class(execute_data, "Memcached") &&
            is_in_list(fn_name, memcached_methods)) {
            return (zend_observer_fcall_handlers){ob_cache_begin, ob_cache_end};
        }
    }

    /* ---- Templates: Twig, Blade ---- */
    if (cls_name) {
        if ((strstr(cls_name, "Twig") &&
             (match_function(execute_data, "render") ||
              match_function(execute_data, "display"))) ||
            (strstr(cls_name, "View") &&
             match_function(execute_data, "render"))) {
            return (zend_observer_fcall_handlers){ob_tpl_begin, ob_tpl_end};
        }
    }

    /* Generic function-level tracing is handled by zend_execute_ex override,
     * not the observer API. */

    return empty;
}

/* ================================================================
 * Registration — called from MINIT
 * ================================================================ */

void otelsilta_observer_register(void) {
    zend_observer_fcall_register(otelsilta_observer_fcall_init);
}

/* ================================================================
 * DB handlers — PDO
 *
 * Uses deferred-decision pattern for duration-threshold gating +
 * aggregation.  The begin handler captures metadata (SQL, operation,
 * db.system) and a start timestamp into ob_meta_stack.  The end
 * handler computes duration and either:
 *   (a) creates a real span (duration >= threshold), or
 *   (b) aggregates into the aggregation buffer (fast operation).
 * ================================================================ */

/* Helper: extract SQL table name from sanitised SQL for aggregation key.
 * Simple heuristic: look for "FROM <table>" or "INTO <table>" or
 * "UPDATE <table>".  Returns pointer into a static buffer. */
static const char *extract_table_name(const char *sql) {
    static char table_buf[128];
    table_buf[0] = '\0';
    if (!sql || !sql[0]) return table_buf;

    /* Case-insensitive search for FROM, INTO, UPDATE, JOIN keywords */
    const char *keywords[] = {" FROM ", " from ", " INTO ", " into ",
                               " UPDATE ", " update ", " JOIN ", " join ", NULL};
    for (int i = 0; keywords[i]; i++) {
        const char *pos = strstr(sql, keywords[i]);
        if (pos) {
            pos += strlen(keywords[i]);
            /* Skip whitespace */
            while (*pos == ' ' || *pos == '`' || *pos == '"' || *pos == '{') pos++;
            /* Copy table name */
            int j = 0;
            while (*pos && *pos != ' ' && *pos != '(' && *pos != ';' &&
                   *pos != '`' && *pos != '"' && *pos != '}' && *pos != ',' &&
                   j < (int)(sizeof(table_buf) - 1)) {
                table_buf[j++] = *pos++;
            }
            table_buf[j] = '\0';
            return table_buf;
        }
    }
    return table_buf;
}

/* Helper: begin capturing a DB operation for deferred decision */
static void db_meta_begin(zend_execute_data *ex,
                           const char *method,
                           const char *sql) {
    if (!OTELSILTA_G(request_active) || !OTELSILTA_G(feature_db)) {
        ob_push_meta_empty();
        return;
    }

    ob_meta_t meta;
    memset(&meta, 0, sizeof(meta));
    meta.valid    = 1;
    meta.is_cache = 0;
    meta.start_ns = otelsilta_time_ns();

    /* Determine db.system from PDO DSN map */
    const char *db_sys = "other";
    if (ex->func && ex->func->common.scope) {
        zend_object *obj = Z_OBJ_P(&ex->This);
        if (obj) db_sys = get_db_system_for_object(obj);
    }
    strncpy(meta.db_system, db_sys, sizeof(meta.db_system) - 1);

    /* Determine db.operation */
    if (sql) {
        otelsilta_sql_operation(sql, meta.db_operation,
                                 sizeof(meta.db_operation));
    } else {
        strncpy(meta.db_operation, "QUERY", sizeof(meta.db_operation) - 1);
    }

    /* Sanitise SQL */
    if (sql) {
        otelsilta_sanitize_sql(sql, meta.sanitized_sql,
                                sizeof(meta.sanitized_sql));
    }

    /* Extract table name for aggregation key */
    if (sql) {
        const char *tbl = extract_table_name(sql);
        strncpy(meta.table_name, tbl, sizeof(meta.table_name) - 1);
    }

    /* Build span name */
    snprintf(meta.span_name, sizeof(meta.span_name), "%s %s",
             db_sys, meta.db_operation);

    /* Capture current parent span ID */
    int depth = OTELSILTA_G(span_stack_depth);
    if (depth > 0) {
        strncpy(meta.parent_span_id,
                OTELSILTA_G(span_stack)[depth - 1]->span_id,
                sizeof(meta.parent_span_id) - 1);
    }

    ob_push_meta(&meta);
}

/* Helper: end a DB operation — decide keep vs aggregate */
static void db_meta_end(zend_bool is_error) {
    ob_meta_t *meta = ob_pop_meta();
    if (!meta || !meta->valid) return;

    uint64_t end_ns     = otelsilta_time_ns();
    uint64_t duration_ns = end_ns - meta->start_ns;
    double   duration_ms = (double)duration_ns / 1e6;

    /* Duration threshold: if fast, aggregate instead of creating a span */
    zend_long threshold = OTELSILTA_G(min_span_duration_ms);
    if (!is_error && threshold > 0 && duration_ms < (double)threshold) {
        /* Aggregate: build grouping key = operation + table */
        char agg_key[OTELSILTA_AGG_KEY_LEN];
        if (meta->table_name[0]) {
            snprintf(agg_key, sizeof(agg_key), "%s %s",
                     meta->db_operation, meta->table_name);
        } else {
            snprintf(agg_key, sizeof(agg_key), "%s", meta->db_operation);
        }

        otelsilta_aggregator_add(
            agg_key,
            meta->db_system,
            meta->db_operation,
            meta->table_name,
            meta->sanitized_sql,
            duration_ns,
            meta->parent_span_id);
        return;
    }

    /* Slow operation or error: create a real span */
    otelsilta_span_t *span =
        otelsilta_tracer_start_span(meta->span_name, SPAN_KIND_CLIENT);
    if (!span) return;

    /* Back-date start time to the actual begin time */
    span->start_time_ns = meta->start_ns;

    otelsilta_span_set_str(span, "db.system", meta->db_system);
    otelsilta_span_set_str(span, "db.operation", meta->db_operation);

    if (meta->sanitized_sql[0]) {
        otelsilta_span_set_str(span, "db.statement", meta->sanitized_sql);
    }

    if (is_error) {
        otelsilta_span_set_status(span, SPAN_STATUS_ERROR, "SQL error");
    }

    otelsilta_tracer_end_span(span);
}

/* PDO::__construct — capture DSN for db.system detection */
static void ob_pdo_construct_begin(zend_execute_data *ex) {
    (void)ex; /* nothing to do before construct */
}

static void ob_pdo_construct_end(zend_execute_data *ex, zval *retval) {
    (void)retval;
    if (!OTELSILTA_G(request_active) || !OTELSILTA_G(pdo_dsn_map)) return;

    if (ZEND_CALL_NUM_ARGS(ex) >= 1) {
        zval *dsn_arg = ZEND_CALL_ARG(ex, 1);
        if (dsn_arg && Z_TYPE_P(dsn_arg) == IS_STRING) {
            zval z_dsn;
            ZVAL_STRINGL(&z_dsn, Z_STRVAL_P(dsn_arg), Z_STRLEN_P(dsn_arg));
            zend_hash_index_update(OTELSILTA_G(pdo_dsn_map),
                                    (zend_ulong)Z_OBJ_P(&ex->This)->handle,
                                    &z_dsn);
        }
    }
}

/* PDO::query */
static void ob_pdo_query_begin(zend_execute_data *ex) {
    const char *sql = NULL;
    if (ZEND_CALL_NUM_ARGS(ex) >= 1) {
        zval *arg = ZEND_CALL_ARG(ex, 1);
        if (arg && Z_TYPE_P(arg) == IS_STRING) sql = Z_STRVAL_P(arg);
    }
    db_meta_begin(ex, "PDO::query", sql);
}

static void ob_pdo_query_end(zend_execute_data *ex, zval *retval) {
    (void)ex;
    db_meta_end(retval && Z_TYPE_P(retval) == IS_FALSE);
}

/* PDO::exec */
static void ob_pdo_exec_begin(zend_execute_data *ex) {
    const char *sql = NULL;
    if (ZEND_CALL_NUM_ARGS(ex) >= 1) {
        zval *arg = ZEND_CALL_ARG(ex, 1);
        if (arg && Z_TYPE_P(arg) == IS_STRING) sql = Z_STRVAL_P(arg);
    }
    db_meta_begin(ex, "PDO::exec", sql);
}

static void ob_pdo_exec_end(zend_execute_data *ex, zval *retval) {
    (void)ex;
    db_meta_end(retval && Z_TYPE_P(retval) == IS_FALSE);
}

/* PDO::prepare — capture SQL for later association with stmt */
static void ob_pdo_prepare_begin(zend_execute_data *ex) {
    (void)ex; /* nothing before */
    ob_push_span(NULL);  /* placeholder so end pops correctly */
}

static void ob_pdo_prepare_end(zend_execute_data *ex, zval *retval) {
    ob_pop_span(); /* discard placeholder */
    if (!OTELSILTA_G(request_active) || !OTELSILTA_G(stmt_sql_map)) return;

    if (ZEND_CALL_NUM_ARGS(ex) >= 1 &&
        retval && Z_TYPE_P(retval) == IS_OBJECT) {
        zval *sql_arg = ZEND_CALL_ARG(ex, 1);
        if (sql_arg && Z_TYPE_P(sql_arg) == IS_STRING) {
            zval z_sql;
            ZVAL_STRINGL(&z_sql, Z_STRVAL_P(sql_arg), Z_STRLEN_P(sql_arg));
            zend_hash_index_update(OTELSILTA_G(stmt_sql_map),
                                    (zend_ulong)Z_OBJ_P(retval)->handle,
                                    &z_sql);
        }
    }
}

/* PDOStatement::execute */
static void ob_stmt_execute_begin(zend_execute_data *ex) {
    if (OTELSILTA_G(stmt_sql_map) && OTELSILTA_G(request_active)) {
        zend_ulong handle = (zend_ulong)Z_OBJ_P(&ex->This)->handle;
        zval *sql_zv = zend_hash_index_find(OTELSILTA_G(stmt_sql_map), handle);
        if (sql_zv && Z_TYPE_P(sql_zv) == IS_STRING) {
            db_meta_begin(ex, "PDOStatement::execute", Z_STRVAL_P(sql_zv));
            return;
        }
    }
    ob_push_meta_empty();
}

static void ob_stmt_execute_end(zend_execute_data *ex, zval *retval) {
    (void)ex;
    db_meta_end(retval && Z_TYPE_P(retval) == IS_FALSE);
}

/* mysqli::query / mysqli_query */
static void ob_mysqli_query_begin(zend_execute_data *ex) {
    const char *sql = NULL;
    /* For OOP: arg1 is query; for procedural: arg1 is link, arg2 is query */
    int sql_arg_idx = match_class(ex, "mysqli") ? 1 : 2;
    if (ZEND_CALL_NUM_ARGS(ex) >= (uint32_t)sql_arg_idx) {
        zval *arg = ZEND_CALL_ARG(ex, sql_arg_idx);
        if (arg && Z_TYPE_P(arg) == IS_STRING) sql = Z_STRVAL_P(arg);
    }
    /* For mysqli, override db_system to "mysql" in the meta */
    db_meta_begin(ex, "mysqli::query", sql);
    /* Patch db_system: db_meta_begin uses PDO DSN map which won't find
     * mysqli objects.  Directly set it on the top meta entry. */
    if (ob_meta_stack_depth > 0) {
        ob_meta_t *top = &ob_meta_stack[ob_meta_stack_depth - 1];
        if (top->valid) {
            strncpy(top->db_system, "mysql", sizeof(top->db_system) - 1);
        }
    }
}

static void ob_mysqli_query_end(zend_execute_data *ex, zval *retval) {
    (void)ex;
    db_meta_end(retval && Z_TYPE_P(retval) == IS_FALSE);
}

/* ================================================================
 * HTTP handlers — curl
 * ================================================================ */

/* curlopt constants (in case headers not available) */
#ifndef CURLOPT_URL
# define CURLOPT_URL            10002
#endif
#ifndef CURLOPT_CUSTOMREQUEST
# define CURLOPT_CUSTOMREQUEST  10036
#endif
#ifndef CURLOPT_POST
# define CURLOPT_POST           47
#endif
#ifndef CURLOPT_HTTPHEADER
# define CURLOPT_HTTPHEADER     10023
#endif
#ifndef CURLINFO_HTTP_CODE
# define CURLINFO_HTTP_CODE     2097154
#endif

/* curl_setopt: capture URL and method metadata per curl handle */
static void ob_curl_setopt_begin(zend_execute_data *ex) {
    if (!OTELSILTA_G(request_active) || !OTELSILTA_G(feature_http)) return;

    if (ZEND_CALL_NUM_ARGS(ex) < 3) return;

    zval *zid    = ZEND_CALL_ARG(ex, 1);
    zval *zopt   = ZEND_CALL_ARG(ex, 2);
    zval *zvalue = ZEND_CALL_ARG(ex, 3);

    if (!zid || !zopt || !zvalue) return;
    if (Z_TYPE_P(zopt) != IS_LONG) return;

    /* Get resource handle ID for the curl handle */
    zend_ulong rid = 0;
    if (Z_TYPE_P(zid) == IS_OBJECT) {
        rid = (zend_ulong)Z_OBJ_P(zid)->handle;
    }
#if PHP_VERSION_ID < 80000
    else if (Z_TYPE_P(zid) == IS_RESOURCE) {
        rid = (zend_ulong)Z_RES_P(zid)->handle;
    }
#endif
    else {
        return;
    }

    typedef struct {
        char url[OTELSILTA_MAX_STR_LEN];
        char method[16];
    } curl_info_t;

    curl_info_t *info = (curl_info_t *)
        zend_hash_index_find_ptr(&OTELSILTA_G(curl_handles), rid);

    if (!info) {
        info = (curl_info_t *)emalloc(sizeof(*info));
        memset(info, 0, sizeof(*info));
        strncpy(info->method, "GET", sizeof(info->method) - 1);
        zend_hash_index_update_ptr(&OTELSILTA_G(curl_handles), rid, info);
    }

    zend_long option = Z_LVAL_P(zopt);
    if (option == CURLOPT_URL && Z_TYPE_P(zvalue) == IS_STRING) {
        char clean[OTELSILTA_MAX_STR_LEN];
        otelsilta_sanitize_url(Z_STRVAL_P(zvalue), clean, sizeof(clean));
        strncpy(info->url, clean, sizeof(info->url) - 1);
    } else if (option == CURLOPT_CUSTOMREQUEST &&
               Z_TYPE_P(zvalue) == IS_STRING) {
        strncpy(info->method, Z_STRVAL_P(zvalue), sizeof(info->method) - 1);
    } else if (option == CURLOPT_POST && zval_is_true(zvalue)) {
        strncpy(info->method, "POST", sizeof(info->method) - 1);
    }
}

static void ob_curl_setopt_end(zend_execute_data *ex, zval *retval) {
    (void)ex; (void)retval; /* metadata already captured in begin */
}

/* curl_exec: create HTTP client span, inject traceparent, capture result */
static void ob_curl_exec_begin(zend_execute_data *ex) {
    if (!OTELSILTA_G(request_active) || !OTELSILTA_G(feature_http)) {
        ob_push_span(NULL);
        return;
    }

    zval *zid = NULL;
    if (ZEND_CALL_NUM_ARGS(ex) >= 1) {
        zid = ZEND_CALL_ARG(ex, 1);
    }

    const char *url    = "unknown";
    const char *method = "GET";

    if (zid) {
        zend_ulong rid = 0;
        if (Z_TYPE_P(zid) == IS_OBJECT) {
            rid = (zend_ulong)Z_OBJ_P(zid)->handle;
        }
#if PHP_VERSION_ID < 80000
        else if (Z_TYPE_P(zid) == IS_RESOURCE) {
            rid = (zend_ulong)Z_RES_P(zid)->handle;
        }
#endif

        typedef struct {
            char url[OTELSILTA_MAX_STR_LEN];
            char method[16];
        } curl_info_t;

        curl_info_t *info = (curl_info_t *)
            zend_hash_index_find_ptr(&OTELSILTA_G(curl_handles), rid);
        if (info) {
            url    = info->url;
            method = info->method;
        }
    }

    char span_name[320];
    snprintf(span_name, sizeof(span_name), "HTTP %s %s", method, url);

    otelsilta_span_t *span =
        otelsilta_tracer_start_span(span_name, SPAN_KIND_CLIENT);

    if (span) {
        otelsilta_span_set_str(span, "http.method", method);
        otelsilta_span_set_str(span, "http.url",    url);

        /* Inject traceparent header */
        if (zid && OTELSILTA_G(trace_id)[0] != '\0') {
            char traceparent[64];
            otelsilta_build_traceparent(
                OTELSILTA_G(trace_id), span->span_id,
                1, traceparent, sizeof(traceparent));

            zval z_headers, z_header_str, z_curlopt;
            array_init(&z_headers);
            char hdr[128];
            snprintf(hdr, sizeof(hdr), "traceparent: %s", traceparent);
            ZVAL_STRING(&z_header_str, hdr);
            add_next_index_zval(&z_headers, &z_header_str);
            ZVAL_LONG(&z_curlopt, CURLOPT_HTTPHEADER);

            zend_function *setopt_fn = (zend_function *)
                zend_hash_str_find_ptr(CG(function_table),
                                       "curl_setopt",
                                       sizeof("curl_setopt") - 1);
            if (setopt_fn) {
                zval retval_tmp;
                ZVAL_UNDEF(&retval_tmp);
                zval params[3];
                ZVAL_COPY_VALUE(&params[0], zid);
                ZVAL_COPY_VALUE(&params[1], &z_curlopt);
                ZVAL_COPY_VALUE(&params[2], &z_headers);
                zend_call_known_function(
                    setopt_fn, NULL, NULL, &retval_tmp, 3, params, NULL);
                zval_ptr_dtor(&retval_tmp);
            }
            zval_ptr_dtor(&z_headers);
        }
    }

    ob_push_span(span);
}

static void ob_curl_exec_end(zend_execute_data *ex, zval *retval) {
    otelsilta_span_t *span = ob_pop_span();
    if (!span) return;

    /* Try to capture HTTP status code via curl_getinfo */
    if (ZEND_CALL_NUM_ARGS(ex) >= 1) {
        zval *zid = ZEND_CALL_ARG(ex, 1);
        zend_function *getinfo_fn = (zend_function *)
            zend_hash_str_find_ptr(CG(function_table),
                                   "curl_getinfo",
                                   sizeof("curl_getinfo") - 1);
        if (getinfo_fn && zid) {
            zval rv, params[2], z_opt;
            ZVAL_UNDEF(&rv);
            ZVAL_COPY_VALUE(&params[0], zid);
            ZVAL_LONG(&z_opt, CURLINFO_HTTP_CODE);
            ZVAL_COPY_VALUE(&params[1], &z_opt);
            zend_call_known_function(
                getinfo_fn, NULL, NULL, &rv, 2, params, NULL);
            if (Z_TYPE(rv) == IS_LONG) {
                otelsilta_span_set_int(span, "http.status_code", Z_LVAL(rv));
                if (Z_LVAL(rv) >= 400) {
                    otelsilta_span_set_status(span, SPAN_STATUS_ERROR, NULL);
                }
            }
            zval_ptr_dtor(&rv);
        }
    }

    if (retval && Z_TYPE_P(retval) == IS_FALSE) {
        otelsilta_span_set_status(span, SPAN_STATUS_ERROR, "curl error");
    }
    otelsilta_tracer_end_span(span);
}

/* ================================================================
 * Cache handlers — Redis, Memcached
 * ================================================================ */

/* ================================================================
 * Cache handlers — Redis, Memcached
 *
 * Uses deferred-decision pattern like DB handlers:
 * begin captures metadata + timestamp, end decides keep vs aggregate.
 * ================================================================ */

static void ob_cache_begin(zend_execute_data *ex) {
    if (!OTELSILTA_G(request_active) || !OTELSILTA_G(feature_cache)) {
        ob_push_meta_empty();
        return;
    }

    const char *cls = get_class_name(ex);
    const char *fn  = get_function_name(ex);

    ob_meta_t meta;
    memset(&meta, 0, sizeof(meta));
    meta.valid    = 1;
    meta.is_cache = 1;
    meta.start_ns = otelsilta_time_ns();

    const char *system = (cls && strcasecmp(cls, "Redis") == 0)
                         ? "redis" : "memcached";
    strncpy(meta.db_system, system, sizeof(meta.db_system) - 1);
    strncpy(meta.db_operation, fn ? fn : "unknown",
            sizeof(meta.db_operation) - 1);
    strncpy(meta.table_name, system, sizeof(meta.table_name) - 1);

    snprintf(meta.span_name, sizeof(meta.span_name), "%s %s",
             cls ? cls : "cache", fn ? fn : "op");

    /* Capture key from first string arg as sample statement */
    if (ZEND_CALL_NUM_ARGS(ex) >= 1) {
        zval *arg = ZEND_CALL_ARG(ex, 1);
        if (arg && Z_TYPE_P(arg) == IS_STRING) {
            strncpy(meta.sanitized_sql, Z_STRVAL_P(arg),
                    sizeof(meta.sanitized_sql) - 1);
        }
    }

    /* Capture parent span ID */
    int depth = OTELSILTA_G(span_stack_depth);
    if (depth > 0) {
        strncpy(meta.parent_span_id,
                OTELSILTA_G(span_stack)[depth - 1]->span_id,
                sizeof(meta.parent_span_id) - 1);
    }

    ob_push_meta(&meta);
}

static void ob_cache_end(zend_execute_data *ex, zval *retval) {
    ob_meta_t *meta = ob_pop_meta();
    if (!meta || !meta->valid) return;

    uint64_t end_ns     = otelsilta_time_ns();
    uint64_t duration_ns = end_ns - meta->start_ns;
    double   duration_ms = (double)duration_ns / 1e6;

    /* Duration threshold: fast ops get aggregated */
    zend_long threshold = OTELSILTA_G(min_span_duration_ms);
    if (threshold > 0 && duration_ms < (double)threshold) {
        /* Aggregate: key = operation + system */
        char agg_key[OTELSILTA_AGG_KEY_LEN];
        snprintf(agg_key, sizeof(agg_key), "cache:%s %s",
                 meta->db_system, meta->db_operation);

        otelsilta_aggregator_add(
            agg_key,
            meta->db_system,
            meta->db_operation,
            meta->table_name,
            meta->sanitized_sql,
            duration_ns,
            meta->parent_span_id);
        return;
    }

    /* Slow operation: create a real span */
    otelsilta_span_t *span =
        otelsilta_tracer_start_span(meta->span_name, SPAN_KIND_CLIENT);
    if (!span) return;

    /* Back-date start time */
    span->start_time_ns = meta->start_ns;

    otelsilta_span_set_str(span, "db.system", meta->db_system);
    otelsilta_span_set_str(span, "db.operation", meta->db_operation);

    if (meta->sanitized_sql[0]) {
        otelsilta_span_set_str(span, "db.redis.key", meta->sanitized_sql);
    }

    /* Detect cache miss for GET-like operations */
    const char *fn = get_function_name(ex);
    if (fn && (strcasecmp(fn, "get") == 0 ||
               strcasecmp(fn, "hget") == 0)) {
        zend_bool miss = (retval &&
            (Z_TYPE_P(retval) == IS_FALSE || Z_TYPE_P(retval) == IS_NULL));
        otelsilta_span_set_bool(span, "cache.hit", miss ? 0 : 1);
    }

    otelsilta_tracer_end_span(span);
}

/* ================================================================
 * Template handlers — Twig, Blade
 * ================================================================ */

static void ob_tpl_begin(zend_execute_data *ex) {
    if (!OTELSILTA_G(request_active) || !OTELSILTA_G(feature_templates) ||
        (OTELSILTA_G(max_span_depth) > 0 &&
         OTELSILTA_G(span_stack_depth) >= OTELSILTA_G(max_span_depth))) {
        ob_push_span(NULL);
        return;
    }

    const char *cls = get_class_name(ex);

    otelsilta_span_t *span =
        otelsilta_tracer_start_span("template.render", SPAN_KIND_INTERNAL);
    if (!span) {
        ob_push_span(NULL);
        return;
    }

    if (cls) {
        otelsilta_span_set_str(span, "template.engine",
            strstr(cls, "Twig") ? "twig" : "blade");
    }

    ob_push_span(span);
}

static void ob_tpl_end(zend_execute_data *ex, zval *retval) {
    (void)ex; (void)retval;
    otelsilta_span_t *span = ob_pop_span();
    if (span) {
        otelsilta_tracer_end_span(span);
    }
}

/* ================================================================
 * Generic function-level tracing via zend_execute_ex override.
 *
 * Unlike the Observer API (which caches {begin,end} per function per
 * process), this hook is invoked on EVERY userland function call,
 * ensuring complete tracing coverage.
 * ================================================================ */

#define MAX_FUNC_DEPTH 32

/* Adaptive threshold for generic function spans.
 *
 * Goal: keep as many function spans as possible without slamming into the
 * hard max_spans_per_trace cap (which causes blind dropping).
 *
 * Strategy:
 * - min_span_duration_ms remains the base threshold.
 * - A request-scoped dynamic threshold rises as span budget pressure grows.
 * - We reserve a small tail budget for non-function spans (errors, late DB,
 *   request-end bookkeeping), then tighten function gating near the cap. */
static zend_long otelsilta_effective_function_threshold_ms(void) {
    zend_long base = OTELSILTA_G(min_span_duration_ms);
    if (base < 0) base = 0;

    zend_long dyn = OTELSILTA_G(function_dynamic_min_span_duration_ms);
    if (dyn < base) dyn = base;

    zend_long max_spans = OTELSILTA_G(max_spans_per_trace);
    if (max_spans <= 0) {
        OTELSILTA_G(function_dynamic_min_span_duration_ms) = dyn;
        return dyn;
    }

    zend_long used = OTELSILTA_G(span_count);

    /* Keep a small reserve for non-function spans near request end. */
    zend_long reserve = max_spans / 8;   /* 12.5% */
    if (reserve < 8) reserve = 8;
    if (reserve > 64) reserve = 64;

    zend_long function_budget = max_spans - reserve;
    if (function_budget < 1) function_budget = max_spans;
    if (function_budget < 1) function_budget = 1;

    int pressure = (int)((used * 100) / function_budget);
    zend_long target = base;

    if (pressure >= 95)      target = 20;
    else if (pressure >= 90) target = 10;
    else if (pressure >= 85) target = 5;
    else if (pressure >= 75) target = 2;

    if (target > dyn) {
        dyn = target;
        OTELSILTA_G(function_dynamic_min_span_duration_ms) = dyn;
        otelsilta_debug_log(
            "otelsilta: adaptive function threshold raised to %ldms (used=%ld max=%ld)",
            (long)dyn, (long)used, (long)max_spans);
    } else {
        OTELSILTA_G(function_dynamic_min_span_duration_ms) = dyn;
    }

    return dyn;
}

void otelsilta_execute_ex(zend_execute_data *execute_data) {
    /* Fast path: call original if tracing is inactive or feature disabled */
    if (!OTELSILTA_G(request_active) || !OTELSILTA_G(feature_functions)) {
        if (OTELSILTA_G(original_execute_ex)) {
            OTELSILTA_G(original_execute_ex)(execute_data);
        } else {
            execute_ex(execute_data);
        }
        return;
    }

    zend_function *func = execute_data->func;
    const char *fn_name = NULL;
    const char *cls_name = NULL;

    if (func && func->common.function_name) {
        fn_name = ZSTR_VAL(func->common.function_name);
    }
    if (func && func->common.scope) {
        cls_name = ZSTR_VAL(func->common.scope->name);
    }

    /* Skip: no name, internal functions, our own extension, or closures.
     * Closures show up as "{closure}" and produce noisy, low-value spans
     * (Composer autoloader, framework middleware internals, etc.). */
    int should_trace = 0;
    if (func && func->type == ZEND_USER_FUNCTION && fn_name) {
        if (strncmp(fn_name, "otelsilta", 9) != 0 &&
            strstr(fn_name, "{closure}") == NULL &&
            OTELSILTA_G(span_stack_depth) < MAX_FUNC_DEPTH &&
            (OTELSILTA_G(max_span_depth) == 0 ||
             OTELSILTA_G(span_stack_depth) < OTELSILTA_G(max_span_depth))) {
            should_trace = 1;
        }
    }

    if (!should_trace) {
        if (OTELSILTA_G(original_execute_ex)) {
            OTELSILTA_G(original_execute_ex)(execute_data);
        } else {
            execute_ex(execute_data);
        }
        return;
    }

    /* Build span name */
    char span_name[512];
    snprintf(span_name, sizeof(span_name), "%s%s%s",
             cls_name ? cls_name : "", cls_name ? "::" : "",
             fn_name ? fn_name : "{closure}");

    /* Capture start time before execution */
    uint64_t start_ns = otelsilta_time_ns();

    /* Call the original execute_ex (runs the actual function) */
    if (OTELSILTA_G(original_execute_ex)) {
        OTELSILTA_G(original_execute_ex)(execute_data);
    } else {
        execute_ex(execute_data);
    }

    /* Duration gating: skip spans shorter than min_span_duration_ms.
     * 0 = no threshold (keep everything). */
    uint64_t end_ns      = otelsilta_time_ns();
    uint64_t duration_ns = end_ns - start_ns;
    double   duration_ms = (double)duration_ns / 1e6;

    OTELSILTA_G(function_calls_seen)++;

    zend_long threshold = otelsilta_effective_function_threshold_ms();
    if (threshold > 0 && duration_ms < (double)threshold) {
        return;
    }

    /* Slow enough — create the span and back-date its start time */
    otelsilta_span_t *span =
        otelsilta_tracer_start_span(span_name, SPAN_KIND_INTERNAL);

    if (span) {
        span->start_time_ns = start_ns;

        if (func->op_array.filename) {
            otelsilta_span_set_str(span, "code.filepath",
                                    ZSTR_VAL(func->op_array.filename));
            otelsilta_span_set_int(span, "code.lineno",
                                    (zend_long)func->op_array.line_start);
        }

        OTELSILTA_G(function_spans_emitted)++;
        otelsilta_tracer_end_span(span);
    }
}
