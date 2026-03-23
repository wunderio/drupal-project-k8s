#ifndef PHP_OTELSILTA_H
#define PHP_OTELSILTA_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "SAPI.h"
#include "zend_exceptions.h"
#include "zend_hash.h"
#include "zend_smart_str.h"

#include <stdint.h>
#include <time.h>

#define PHP_OTELSILTA_VERSION  "0.4.0"
#define PHP_OTELSILTA_EXTNAME  "otelsilta"

/* Tunables */
#define OTELSILTA_MAX_SPANS             200
#define OTELSILTA_MAX_ATTRIBUTES        32
#define OTELSILTA_MAX_STR_LEN           1024
#define OTELSILTA_SPAN_STACK_SIZE       64
#define OTELSILTA_OB_SPAN_STACK_SIZE    128

/* ---- Attribute ---- */

typedef enum {
    ATTR_TYPE_STRING = 0,
    ATTR_TYPE_INT,
    ATTR_TYPE_DOUBLE,
    ATTR_TYPE_BOOL
} otelsilta_attr_type_t;

typedef struct {
    char                  key[128];
    otelsilta_attr_type_t type;
    union {
        char       str_val[OTELSILTA_MAX_STR_LEN];
        zend_long  int_val;
        double     dbl_val;
        zend_bool  bool_val;
    } value;
} otelsilta_attribute_t;

/* ---- Span ---- */

typedef enum {
    SPAN_STATUS_UNSET = 0,
    SPAN_STATUS_OK    = 1,
    SPAN_STATUS_ERROR = 2
} otelsilta_span_status_t;

typedef enum {
    SPAN_KIND_INTERNAL = 1,
    SPAN_KIND_SERVER   = 2,
    SPAN_KIND_CLIENT   = 3,
    SPAN_KIND_PRODUCER = 4,
    SPAN_KIND_CONSUMER = 5
} otelsilta_span_kind_t;

typedef struct _otelsilta_span_t {
    char                    trace_id[33];       /* 32 hex chars + NUL */
    char                    span_id[17];        /* 16 hex chars + NUL */
    char                    parent_span_id[17];
    char                    name[256];
    otelsilta_span_kind_t   kind;
    uint64_t                start_time_ns;
    uint64_t                end_time_ns;
    otelsilta_span_status_t status;
    char                    status_message[512];
    otelsilta_attribute_t   attributes[OTELSILTA_MAX_ATTRIBUTES];
    int                     attribute_count;
    int                     is_finished;
    struct _otelsilta_span_t *next;             /* intrusive linked list */
} otelsilta_span_t;

/* ---- Module globals (one set per thread in ZTS) ---- */

ZEND_BEGIN_MODULE_GLOBALS(otelsilta)
    /* --- static config (from php.ini) --- */
    zend_bool  enabled;
    char      *otel_service_name;
    char      *otel_service_namespace;
    char      *otel_deployment_environment;
    char      *otel_resource_attributes;
    char      *otel_exporter_otlp_endpoint;
    double     sample_rate;
    zend_long  slow_request_threshold_ms;

    zend_bool  feature_errors;
    zend_bool  feature_db;
    zend_bool  feature_http;
    zend_bool  feature_cache;
    zend_bool  feature_templates;
    zend_bool  feature_functions;
    zend_bool  feature_profiling;

    zend_long  max_spans_per_trace;
    zend_long  max_attributes_per_span;
    zend_bool  debug_mode;
    char      *excluded_urls;          /* comma-separated URL prefixes */

    /* Span aggregation & gating */
    zend_long  max_span_depth;         /* depth gating: max child span depth (default 5) */
    zend_long  min_span_duration_ms;   /* threshold gating: min duration to keep individual span (default 1) */

    /* --- per-request state (reset in RINIT) --- */
    zend_bool           request_active;
    zend_bool           is_sampled;
    zend_bool           has_error;
    zend_bool           request_excluded;  /* URL matched excluded_urls */
    zend_bool           cli_mode;          /* 1 when running under CLI SAPI */
    char                trace_id[33];
    otelsilta_span_t   *root_span;
    otelsilta_span_t   *span_stack[OTELSILTA_SPAN_STACK_SIZE];
    int                 span_stack_depth;
    otelsilta_span_t   *all_spans;  /* head of linked list */
    otelsilta_span_t   *last_span;  /* tail of linked list */
    int                 span_count;
    uint64_t            request_start_ns;
    zend_long           memory_start;

    /* Adaptive function-span gating (request-scoped).
     * Base threshold comes from min_span_duration_ms; this dynamic value
     * can rise during a request when approaching max_spans_per_trace. */
    zend_long           function_dynamic_min_span_duration_ms;
    zend_long           function_calls_seen;
    zend_long           function_spans_emitted;

    /* curl handle → URL mapping (key=resource_id as zend_long) */
    HashTable           curl_handles;

    /* --- Observer API per-request state --- */
    otelsilta_span_t   *ob_span_stack[OTELSILTA_OB_SPAN_STACK_SIZE];
    int                 ob_span_stack_depth;
    HashTable          *pdo_dsn_map;   /* PDO object handle → DSN string */
    HashTable          *stmt_sql_map;  /* PDOStatement handle → SQL string */

    /* Span aggregation buffer (DB + cache operations) */
    HashTable          *agg_buckets;   /* key → otelsilta_agg_bucket_t* */

    /* Saved original zend_execute_ex / zend_execute_internal pointers
     * (set in MINIT, restored in MSHUTDOWN). */
    void (*original_execute_ex)(zend_execute_data *execute_data);
    void (*original_execute_internal)(zend_execute_data *execute_data, zval *return_value);
ZEND_END_MODULE_GLOBALS(otelsilta)

#ifdef ZTS
# define OTELSILTA_G(v) TSRMG(otelsilta_globals_id, zend_otelsilta_globals *, v)
#else
# define OTELSILTA_G(v) (otelsilta_globals.v)
#endif

extern ZEND_DECLARE_MODULE_GLOBALS(otelsilta)

extern zend_module_entry otelsilta_module_entry;
#define phpext_otelsilta_ptr &otelsilta_module_entry

/* ---- Public helpers ---- */
uint64_t    otelsilta_time_ns(void);
void        otelsilta_generate_id(char *buf, int bytes);
void        otelsilta_debug_log(const char *fmt, ...);
const char *otelsilta_effective_service_name(void);
const char *otelsilta_effective_service_namespace(void);
const char *otelsilta_effective_deployment_environment(void);
const char *otelsilta_effective_endpoint(void);
const char *otelsilta_effective_resource_attributes(void);

PHP_MINIT_FUNCTION(otelsilta);
PHP_MSHUTDOWN_FUNCTION(otelsilta);
PHP_RINIT_FUNCTION(otelsilta);
PHP_RSHUTDOWN_FUNCTION(otelsilta);
PHP_MINFO_FUNCTION(otelsilta);

#endif /* PHP_OTELSILTA_H */
