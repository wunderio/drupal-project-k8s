#include "tracer.h"
#include "span.h"
#include "propagation.h"
#include "routing/router.h"
#include "php_otelsilta.h"

#include "SAPI.h"
#include "zend_alloc.h"

#include <string.h>
#include <stdlib.h>
#include <time.h>

/* ---- $_SERVER access helper ----
 *
 * In PHP-FPM, $_SERVER is an auto-global whose backing zval
 * (PG(http_globals)[TRACK_VARS_SERVER]) is NOT populated until
 * something triggers its creation.  At RINIT time the zval may be
 * IS_UNDEF (or even uninitialised garbage in edge cases).
 *
 * Calling zend_is_auto_global_str() forces the engine to run the
 * auto-global callback, populating the zval.  We then check for
 * IS_ARRAY as usual.
 *
 * Returns the HashTable* of $_SERVER, or NULL if unavailable.
 */
static HashTable *otelsilta_get_server_ht(void) {
    zend_string *name = zend_string_init("_SERVER", sizeof("_SERVER") - 1, 0);
    zend_is_auto_global(name);
    zend_string_release(name);

    zval *server = &PG(http_globals)[TRACK_VARS_SERVER];
    if (Z_TYPE_P(server) == IS_ARRAY) {
        return Z_ARRVAL_P(server);
    }
    return NULL;
}

/* ---- helpers ---- */

static void append_span(otelsilta_span_t *span) {
    if (OTELSILTA_G(last_span)) {
        OTELSILTA_G(last_span)->next = span;
    } else {
        OTELSILTA_G(all_spans) = span;
    }
    OTELSILTA_G(last_span) = span;
    OTELSILTA_G(span_count)++;
}

/* ---- URI resolution helper ----
 *
 * In standard setups, REQUEST_URI contains the original client path
 * (e.g. "/ping?foo=1").  Some PHP-FPM / nginx configurations do not
 * pass REQUEST_URI at all; in that case we fall back to SCRIPT_NAME
 * which is almost always set and contains the path portion (without
 * query string).
 *
 * Returns a pointer into the $_SERVER zval storage (do NOT free).
 * Returns NULL if no suitable key is found.
 */
static const char *otelsilta_resolve_request_uri(HashTable *server_ht) {
    if (!server_ht) return NULL;

    /* Primary: REQUEST_URI (includes query string) */
    zval *v = zend_hash_str_find(server_ht,
        "REQUEST_URI", sizeof("REQUEST_URI") - 1);
    if (v && Z_TYPE_P(v) == IS_STRING && Z_STRLEN_P(v) > 0
        && Z_STRVAL_P(v)[0] == '/') {
        return Z_STRVAL_P(v);
    }

    /* Fallback: SCRIPT_NAME (path only, no query string) */
    v = zend_hash_str_find(server_ht,
        "SCRIPT_NAME", sizeof("SCRIPT_NAME") - 1);
    if (v && Z_TYPE_P(v) == IS_STRING && Z_STRLEN_P(v) > 0
        && Z_STRVAL_P(v)[0] == '/') {
        return Z_STRVAL_P(v);
    }

    return NULL;
}

/* ---- public API ---- */

void otelsilta_tracer_request_init(void) {
    OTELSILTA_G(request_active)    = 0;
    OTELSILTA_G(is_sampled)        = 0;
    OTELSILTA_G(has_error)         = 0;
    OTELSILTA_G(request_excluded)  = 0;
    OTELSILTA_G(root_span)         = NULL;
    OTELSILTA_G(span_stack_depth)  = 0;
    OTELSILTA_G(all_spans)         = NULL;
    OTELSILTA_G(last_span)         = NULL;
    OTELSILTA_G(span_count)        = 0;
    OTELSILTA_G(request_start_ns)  = otelsilta_time_ns();
    OTELSILTA_G(memory_start)      = (zend_long)zend_memory_usage(0);
    OTELSILTA_G(function_dynamic_min_span_duration_ms) =
        OTELSILTA_G(min_span_duration_ms);
    OTELSILTA_G(function_calls_seen)    = 0;
    OTELSILTA_G(function_spans_emitted) = 0;

    memset(OTELSILTA_G(trace_id), 0, sizeof(OTELSILTA_G(trace_id)));
    memset(OTELSILTA_G(span_stack), 0, sizeof(OTELSILTA_G(span_stack)));

    /* NULL dtor: entries are emalloc'd structs freed at request end. */
    zend_hash_init(&OTELSILTA_G(curl_handles), 8, NULL, NULL, 0);
}

/* ---- URL exclusion check ---- */

/* Returns 1 if the URI path starts with any of the comma-separated
 * prefixes in otelsilta.excluded_urls.  Comparison is path-only
 * (query string stripped) and is a simple prefix match:
 * "/_ping" matches "/_ping", "/_ping.php", "/_ping/deep", etc. */
static int otelsilta_is_url_excluded(const char *uri) {
    const char *list = OTELSILTA_G(excluded_urls);
    if (!list || *list == '\0') return 0;

    /* Extract path portion (before '?') */
    const char *qmark = strchr(uri, '?');
    size_t uri_path_len = qmark ? (size_t)(qmark - uri) : strlen(uri);

    /* Walk the comma-separated list */
    const char *p = list;
    while (*p) {
        /* Skip leading whitespace and commas */
        while (*p == ',' || *p == ' ') p++;
        if (*p == '\0') break;

        /* Find end of this entry */
        const char *end = p;
        while (*end && *end != ',') end++;

        /* Trim trailing whitespace */
        const char *trim = end;
        while (trim > p && *(trim - 1) == ' ') trim--;

        size_t entry_len = (size_t)(trim - p);
        if (entry_len > 0 && uri_path_len >= entry_len &&
            strncmp(uri, p, entry_len) == 0) {
            return 1;
        }

        p = end;
    }
    return 0;
}

void otelsilta_tracer_make_sampling_decision(void) {
    if (!OTELSILTA_G(enabled)) return;

    /* Force $_SERVER auto-global population (safe for CLI too). */
    HashTable *server_ht = otelsilta_get_server_ht();

    /* Resolve the request URI once — used for exclusion check and span.
     * Uses fallback chain across multiple $_SERVER keys to handle
     * misconfigured nginx fastcgi_params. */
    const char *resolved_uri = otelsilta_resolve_request_uri(server_ht);

    /* Early exit: check if the request URI matches excluded_urls */
    if (OTELSILTA_G(excluded_urls) && OTELSILTA_G(excluded_urls)[0] != '\0') {
        if (OTELSILTA_G(debug_mode)) {
            fprintf(stderr, "[otelsilta] exclusion check: uri='%s' "
                    "excluded_urls='%s'\n",
                    resolved_uri ? resolved_uri : "(null)",
                    OTELSILTA_G(excluded_urls));
        }
        if (resolved_uri && otelsilta_is_url_excluded(resolved_uri)) {
            OTELSILTA_G(request_excluded) = 1;
            otelsilta_debug_log("otelsilta: excluding URL '%s'", resolved_uri);
            return;
        }
    }

    /* Check for incoming W3C traceparent header first */
    char parent_trace_id[33] = {0};
    char parent_span_id[17]  = {0};
    int  flags               = 0;

    const char *traceparent_hdr = NULL;
    if (sapi_module.name && strcmp(sapi_module.name, "cli") != 0) {
        /* HTTP request - look for traceparent header */
        if (server_ht) {
            zval *v = zend_hash_str_find(server_ht,
                "HTTP_TRACEPARENT", sizeof("HTTP_TRACEPARENT") - 1);
            if (v && Z_TYPE_P(v) == IS_STRING) {
                traceparent_hdr = Z_STRVAL_P(v);
            }
        }
    }

    int has_parent = 0;
    if (traceparent_hdr) {
        has_parent = otelsilta_parse_traceparent(
            traceparent_hdr,
            parent_trace_id, parent_span_id, &flags);
    }

    /* Sampling decision */
    double r = (double)rand() / (double)RAND_MAX;
    int sampled = (r < OTELSILTA_G(sample_rate));
    /* If parent said sampled, honour it */
    if (has_parent && (flags & 0x01)) sampled = 1;

    OTELSILTA_G(is_sampled) = sampled;

    if (!sampled) return;

    OTELSILTA_G(request_active) = 1;

    if (has_parent) {
        strncpy(OTELSILTA_G(trace_id), parent_trace_id,
                sizeof(OTELSILTA_G(trace_id)) - 1);
    } else {
        otelsilta_generate_id(OTELSILTA_G(trace_id), 16);
    }

    /* Build root span */
    const char *method    = "UNKNOWN";
    const char *uri       = resolved_uri ? resolved_uri : "/";
    const char *parent_id = has_parent ? parent_span_id : "";

    if (server_ht) {
        zval *v;
        v = zend_hash_str_find(server_ht,
                               "REQUEST_METHOD",
                               sizeof("REQUEST_METHOD") - 1);
        if (v && Z_TYPE_P(v) == IS_STRING) method = Z_STRVAL_P(v);
    }

    char span_name[320];
    char route[256];
    otelsilta_normalize_route(uri, route, sizeof(route));
    snprintf(span_name, sizeof(span_name), "%s %s", method, route);

    otelsilta_span_t *root = otelsilta_span_create(
        span_name, SPAN_KIND_SERVER,
        OTELSILTA_G(trace_id), parent_id);

    if (!root) {
        OTELSILTA_G(request_active) = 0;
        return;
    }

    otelsilta_span_set_str(root, "http.method", method);
    otelsilta_span_set_str(root, "http.route",  route);
    otelsilta_span_set_str(root, "http.url",    uri);

    /* Push root onto stack */
    OTELSILTA_G(root_span) = root;
    OTELSILTA_G(span_stack)[0] = root;
    OTELSILTA_G(span_stack_depth) = 1;
    append_span(root);
}

void otelsilta_tracer_request_shutdown(void) {
    if (!OTELSILTA_G(request_active)) {
        /* If we have errors but weren't sampled, force-sample now */
        if (OTELSILTA_G(has_error) && OTELSILTA_G(enabled)) {
            otelsilta_tracer_force_sample();
        }
        if (!OTELSILTA_G(request_active)) {
            /* curl_handles will be destroyed in the caller (RSHUTDOWN). */
            otelsilta_span_free_all(OTELSILTA_G(all_spans));
            OTELSILTA_G(all_spans) = NULL;
            return;
        }
    }

    otelsilta_span_t *root = OTELSILTA_G(root_span);
    if (root && !root->is_finished) {
        /* HTTP status code */
        long status_code = SG(sapi_headers).http_response_code;
        if (status_code <= 0) status_code = 200;
        otelsilta_span_set_int(root, "http.status_code", (zend_long)status_code);

        /* Duration and memory */
        uint64_t now   = otelsilta_time_ns();
        double   dur   = (double)(now - OTELSILTA_G(request_start_ns)) / 1e6;
        zend_long peak = (zend_long)zend_memory_peak_usage(0);
        otelsilta_span_set_dbl(root, "duration_ms",    dur);
        otelsilta_span_set_int(root, "memory_peak",    peak);

        if (status_code >= 500 || OTELSILTA_G(has_error)) {
            otelsilta_span_set_status(root, SPAN_STATUS_ERROR, NULL);
        } else {
            otelsilta_span_set_status(root, SPAN_STATUS_OK, NULL);
        }

        otelsilta_span_finish(root);

        /* Check slow-request threshold */
        if (!OTELSILTA_G(has_error) &&
            OTELSILTA_G(slow_request_threshold_ms) > 0 &&
            dur >= (double)OTELSILTA_G(slow_request_threshold_ms)) {
            otelsilta_span_set_bool(root, "slow_request", 1);
        }
    }

    /* Export */
    extern void otelsilta_export_trace(void);
    otelsilta_export_trace();

    /* Cleanup spans. curl_handles is destroyed by RSHUTDOWN after this. */
    otelsilta_span_free_all(OTELSILTA_G(all_spans));
    OTELSILTA_G(all_spans)        = NULL;
    OTELSILTA_G(last_span)        = NULL;
    OTELSILTA_G(root_span)        = NULL;
    OTELSILTA_G(span_count)       = 0;
    OTELSILTA_G(span_stack_depth) = 0;
    OTELSILTA_G(request_active)   = 0;
}

void otelsilta_tracer_force_sample(void) {
    if (OTELSILTA_G(request_active))  return;
    if (!OTELSILTA_G(enabled))        return;
    if (OTELSILTA_G(request_excluded)) return;  /* excluded URLs stay excluded */
    if (OTELSILTA_G(cli_mode))        return;   /* no tracing in CLI mode */

    OTELSILTA_G(is_sampled)    = 1;
    OTELSILTA_G(request_active) = 1;

    if (OTELSILTA_G(trace_id)[0] == '\0') {
        otelsilta_generate_id(OTELSILTA_G(trace_id), 16);
    }

    const char *method = "UNKNOWN";
    const char *uri    = "/";

    HashTable *server_ht = otelsilta_get_server_ht();
    if (server_ht) {
        zval *v;
        v = zend_hash_str_find(server_ht,
                               "REQUEST_METHOD",
                               sizeof("REQUEST_METHOD") - 1);
        if (v && Z_TYPE_P(v) == IS_STRING) method = Z_STRVAL_P(v);

        const char *resolved = otelsilta_resolve_request_uri(server_ht);
        if (resolved) uri = resolved;
    }

    char span_name[320];
    char route[256];
    otelsilta_normalize_route(uri, route, sizeof(route));
    snprintf(span_name, sizeof(span_name), "%s %s", method, route);

    otelsilta_span_t *root = otelsilta_span_create(
        span_name, SPAN_KIND_SERVER, OTELSILTA_G(trace_id), "");

    if (!root) {
        OTELSILTA_G(request_active) = 0;
        return;
    }

    /* Back-date start to real request start */
    root->start_time_ns = OTELSILTA_G(request_start_ns);

    otelsilta_span_set_str(root, "http.method", method);
    otelsilta_span_set_str(root, "http.route",  route);
    otelsilta_span_set_str(root, "http.url",    uri);

    OTELSILTA_G(root_span)         = root;
    OTELSILTA_G(span_stack)[0]     = root;
    OTELSILTA_G(span_stack_depth)  = 1;
    append_span(root);
}

otelsilta_span_t *otelsilta_tracer_start_span(const char *name,
                                               otelsilta_span_kind_t kind) {
    if (!OTELSILTA_G(request_active)) return NULL;

    /* 0 = unlimited */
    if (OTELSILTA_G(max_spans_per_trace) > 0 &&
        OTELSILTA_G(span_count) >= OTELSILTA_G(max_spans_per_trace)) {
        otelsilta_debug_log("otelsilta: span limit %ld reached, dropping span '%s'",
                             (long)OTELSILTA_G(max_spans_per_trace), name);
        return NULL;
    }

    const char *parent_id = "";
    int depth = OTELSILTA_G(span_stack_depth);
    if (depth > 0) {
        parent_id = OTELSILTA_G(span_stack)[depth - 1]->span_id;
    }

    otelsilta_span_t *span = otelsilta_span_create(
        name, kind, OTELSILTA_G(trace_id), parent_id);
    if (!span) return NULL;

    if (depth < OTELSILTA_SPAN_STACK_SIZE) {
        OTELSILTA_G(span_stack)[depth] = span;
        OTELSILTA_G(span_stack_depth)  = depth + 1;
    }

    append_span(span);
    return span;
}

void otelsilta_tracer_end_span(otelsilta_span_t *span) {
    if (!span) return;
    otelsilta_span_finish(span);

    /* Pop from stack */
    int depth = OTELSILTA_G(span_stack_depth);
    for (int i = depth - 1; i >= 0; i--) {
        if (OTELSILTA_G(span_stack)[i] == span) {
            /* Shift remaining entries down */
            for (int j = i; j < depth - 1; j++) {
                OTELSILTA_G(span_stack)[j] = OTELSILTA_G(span_stack)[j + 1];
            }
            OTELSILTA_G(span_stack_depth)--;
            break;
        }
    }
}

otelsilta_span_t *otelsilta_tracer_current_span(void) {
    int depth = OTELSILTA_G(span_stack_depth);
    if (depth == 0) return NULL;
    return OTELSILTA_G(span_stack)[depth - 1];
}
