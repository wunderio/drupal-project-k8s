/*
 * otelsilta – Zero-code PHP APM extension.
 * OpenTelemetry-based automatic instrumentation with OTLP/HTTP export.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php_otelsilta.h"

#include "src/span.h"
#include "src/tracer.h"
#include "src/exporter.h"
#include "src/sanitizer.h"
#include "src/propagation.h"
#include "src/routing/router.h"
#include "src/observer.h"
#include "src/aggregator.h"
#include "src/hooks/errors.h"

#include "zend_smart_str.h"

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <time.h>

/* php_random_bytes_throw():
 *   PHP 8.2+ moved the random API to ext/random/php_random.h
 *   PHP 8.0–8.1 keeps it in ext/standard/php_random.h              */
#if PHP_VERSION_ID >= 80200
# include "ext/random/php_random.h"
#else
# include "ext/standard/php_random.h"
#endif

/* ===== Module globals ===== */

ZEND_DECLARE_MODULE_GLOBALS(otelsilta)

static void php_otelsilta_init_globals(zend_otelsilta_globals *g) {
    memset(g, 0, sizeof(*g));
    g->enabled                  = 1;
    g->sample_rate              = 0.1;
    g->slow_request_threshold_ms = 100;
    g->feature_errors           = 1;
    g->feature_db               = 1;
    g->feature_http             = 1;
    g->feature_cache            = 0;
    g->feature_templates        = 0;
    g->feature_functions        = 0;
    g->feature_profiling        = 0;
    g->max_spans_per_trace      = OTELSILTA_MAX_SPANS;
    g->max_attributes_per_span  = OTELSILTA_MAX_ATTRIBUTES;
    g->debug_mode               = 0;
    g->max_span_depth           = 5;
    g->min_span_duration_ms     = 1;
}

/* ===== INI entries ===== */

PHP_INI_BEGIN()
    /* Core */
    STD_PHP_INI_BOOLEAN("otelsilta.enabled",       "1",       PHP_INI_ALL,
        OnUpdateBool,   enabled,       zend_otelsilta_globals, otelsilta_globals)
    STD_PHP_INI_ENTRY("otelsilta.otel_service_name",    "", PHP_INI_ALL,
        OnUpdateString, otel_service_name,  zend_otelsilta_globals, otelsilta_globals)
    STD_PHP_INI_ENTRY("otelsilta.otel_service_namespace",    "", PHP_INI_ALL,
        OnUpdateString, otel_service_namespace,  zend_otelsilta_globals, otelsilta_globals)
    STD_PHP_INI_ENTRY("otelsilta.otel_deployment_environment",    "", PHP_INI_ALL,
        OnUpdateString, otel_deployment_environment,  zend_otelsilta_globals, otelsilta_globals)
    STD_PHP_INI_ENTRY("otelsilta.otel_resource_attributes",     "", PHP_INI_ALL,
        OnUpdateString, otel_resource_attributes,   zend_otelsilta_globals, otelsilta_globals)
    STD_PHP_INI_ENTRY("otelsilta.otel_exporter_otlp_endpoint",
        "", PHP_INI_ALL,
        OnUpdateString, otel_exporter_otlp_endpoint, zend_otelsilta_globals, otelsilta_globals)
    STD_PHP_INI_ENTRY("otelsilta.sample_rate",     "0.1",     PHP_INI_ALL,
        OnUpdateReal,   sample_rate,   zend_otelsilta_globals, otelsilta_globals)
    STD_PHP_INI_ENTRY("otelsilta.slow_request_threshold_ms", "100", PHP_INI_ALL,
        OnUpdateLong,   slow_request_threshold_ms,
        zend_otelsilta_globals, otelsilta_globals)

    /* Feature toggles */
    STD_PHP_INI_BOOLEAN("otelsilta.features.errors",     "1", PHP_INI_ALL,
        OnUpdateBool, feature_errors,     zend_otelsilta_globals, otelsilta_globals)
    STD_PHP_INI_BOOLEAN("otelsilta.features.db",         "1", PHP_INI_ALL,
        OnUpdateBool, feature_db,         zend_otelsilta_globals, otelsilta_globals)
    STD_PHP_INI_BOOLEAN("otelsilta.features.http",       "1", PHP_INI_ALL,
        OnUpdateBool, feature_http,       zend_otelsilta_globals, otelsilta_globals)
    STD_PHP_INI_BOOLEAN("otelsilta.features.cache",      "0", PHP_INI_ALL,
        OnUpdateBool, feature_cache,      zend_otelsilta_globals, otelsilta_globals)
    STD_PHP_INI_BOOLEAN("otelsilta.features.templates",  "0", PHP_INI_ALL,
        OnUpdateBool, feature_templates,  zend_otelsilta_globals, otelsilta_globals)
    STD_PHP_INI_BOOLEAN("otelsilta.features.functions",  "0", PHP_INI_ALL,
        OnUpdateBool, feature_functions,  zend_otelsilta_globals, otelsilta_globals)
    STD_PHP_INI_BOOLEAN("otelsilta.features.profiling",  "0", PHP_INI_ALL,
        OnUpdateBool, feature_profiling,  zend_otelsilta_globals, otelsilta_globals)

    /* Limits */
    STD_PHP_INI_ENTRY("otelsilta.max_spans_per_trace",      "200", PHP_INI_ALL,
        OnUpdateLong, max_spans_per_trace,
        zend_otelsilta_globals, otelsilta_globals)
    STD_PHP_INI_ENTRY("otelsilta.max_attributes_per_span",  "32",  PHP_INI_ALL,
        OnUpdateLong, max_attributes_per_span,
        zend_otelsilta_globals, otelsilta_globals)

    /* Debug */
    STD_PHP_INI_BOOLEAN("otelsilta.debug", "0", PHP_INI_ALL,
        OnUpdateBool, debug_mode, zend_otelsilta_globals, otelsilta_globals)

    /* URL exclusion */
    STD_PHP_INI_ENTRY("otelsilta.excluded_urls", "", PHP_INI_ALL,
        OnUpdateString, excluded_urls, zend_otelsilta_globals, otelsilta_globals)

    /* Span aggregation & gating */
    STD_PHP_INI_ENTRY("otelsilta.max_span_depth", "5", PHP_INI_ALL,
        OnUpdateLong, max_span_depth,
        zend_otelsilta_globals, otelsilta_globals)
    STD_PHP_INI_ENTRY("otelsilta.min_span_duration_ms", "1", PHP_INI_ALL,
        OnUpdateLong, min_span_duration_ms,
        zend_otelsilta_globals, otelsilta_globals)
PHP_INI_END()

/* ===== Utility functions ===== */

/* Resolve effective service name:
 *   1. otelsilta.otel_service_name INI (if non-empty)
 *   2. OTEL_SERVICE_NAME env var     (if set and non-empty)
 *   3. "php" — safe default so tracing is never blocked by a missing name */
const char *otelsilta_effective_service_name(void) {
    const char *v = OTELSILTA_G(otel_service_name);
    if (v && v[0] != '\0') return v;

    v = getenv("OTEL_SERVICE_NAME");
    if (v && v[0] != '\0') return v;

    return "php";
}

/* Resolve effective service namespace:
 *   1. otelsilta.otel_service_namespace INI (if non-empty)
 *   2. PROJECT_NAME env var              (if set and non-empty)
 *   3. NULL — attribute will be omitted from the OTLP payload */
const char *otelsilta_effective_service_namespace(void) {
    const char *v = OTELSILTA_G(otel_service_namespace);
    if (v && v[0] != '\0') return v;

    v = getenv("PROJECT_NAME");
    if (v && v[0] != '\0') return v;

    return NULL;
}

/* Resolve effective deployment environment:
 *   1. otelsilta.otel_deployment_environment INI (if non-empty)
 *   2. RELEASE_NAME env var                    (if set and non-empty)
 *   3. NULL — attribute will be omitted from the OTLP payload */
const char *otelsilta_effective_deployment_environment(void) {
    const char *v = OTELSILTA_G(otel_deployment_environment);
    if (v && v[0] != '\0') return v;

    v = getenv("RELEASE_NAME");
    if (v && v[0] != '\0') return v;

    return NULL;
}

/* Resolve effective OTLP endpoint:
 *   1. otelsilta.otel_exporter_otlp_endpoint INI (if non-empty)
 *   2. OTEL_EXPORTER_OTLP_ENDPOINT env var       (if set and non-empty)
 *   3. NULL — caller must handle the unset case */
const char *otelsilta_effective_endpoint(void) {
    const char *v = OTELSILTA_G(otel_exporter_otlp_endpoint);
    if (v && v[0] != '\0') return v;

    v = getenv("OTEL_EXPORTER_OTLP_ENDPOINT");
    if (v && v[0] != '\0') return v;

    return NULL;
}

/* Resolve effective resource attributes:
 *   1. otelsilta.otel_resource_attributes INI (if non-empty)
 *   2. OTEL_RESOURCE_ATTRIBUTES env var       (if set and non-empty)
 *   3. NULL — no extra resource attributes */
const char *otelsilta_effective_resource_attributes(void) {
    const char *v = OTELSILTA_G(otel_resource_attributes);
    if (v && v[0] != '\0') return v;

    v = getenv("OTEL_RESOURCE_ATTRIBUTES");
    if (v && v[0] != '\0') return v;

    return NULL;
}

uint64_t otelsilta_time_ns(void) {
    struct timespec ts;
#ifdef CLOCK_REALTIME
    clock_gettime(CLOCK_REALTIME, &ts);
#else
    /* fallback: gettimeofday */
    struct timeval tv;
    gettimeofday(&tv, NULL);
    ts.tv_sec  = tv.tv_sec;
    ts.tv_nsec = tv.tv_usec * 1000;
#endif
    return (uint64_t)ts.tv_sec * UINT64_C(1000000000) +
           (uint64_t)ts.tv_nsec;
}

void otelsilta_generate_id(char *buf, int bytes) {
    /* Use PHP's CSPRNG (php_random_bytes) */
    unsigned char raw[16];
    if (bytes > 16) bytes = 16;
    php_random_bytes_throw(raw, bytes);
    for (int i = 0; i < bytes; i++) {
        sprintf(buf + (i * 2), "%02x", raw[i]);
    }
    buf[bytes * 2] = '\0';
}

void otelsilta_debug_log(const char *fmt, ...) {
    if (!OTELSILTA_G(debug_mode)) return;

    char msg[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    /* Write to stderr instead of php_error_docref.  php_error_docref
     * fires userland error handlers which is unsafe during RSHUTDOWN
     * (Drupal's handler crashes on partially-torn-down state). */
    fprintf(stderr, "[otelsilta] %s\n", msg);
}

/* ===== PHP functions exposed to userland ===== */

/* otelsilta_span_start(string $name): resource|false */
PHP_FUNCTION(otelsilta_span_start) {
    char  *name     = NULL;
    size_t name_len = 0;

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STRING(name, name_len)
    ZEND_PARSE_PARAMETERS_END();

    otelsilta_span_t *span =
        otelsilta_tracer_start_span(name, SPAN_KIND_INTERNAL);
    if (!span) {
        RETURN_FALSE;
    }
    /* Return span pointer as a long (not a real resource, but usable) */
    RETURN_LONG((zend_long)(uintptr_t)span);
}

/* otelsilta_span_finish(int $handle): void */
PHP_FUNCTION(otelsilta_span_finish) {
    zend_long handle = 0;
    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_LONG(handle)
    ZEND_PARSE_PARAMETERS_END();

    otelsilta_span_t *span = (otelsilta_span_t *)(uintptr_t)handle;
    otelsilta_tracer_end_span(span);
}

/* otelsilta_span_set_attribute(int $handle, string $key, mixed $value): void */
PHP_FUNCTION(otelsilta_span_set_attribute) {
    zend_long handle = 0;
    char     *key    = NULL;
    size_t    key_len = 0;
    zval     *value   = NULL;

    ZEND_PARSE_PARAMETERS_START(3, 3)
        Z_PARAM_LONG(handle)
        Z_PARAM_STRING(key, key_len)
        Z_PARAM_ZVAL(value)
    ZEND_PARSE_PARAMETERS_END();

    otelsilta_span_t *span = (otelsilta_span_t *)(uintptr_t)handle;
    if (!span) return;

    switch (Z_TYPE_P(value)) {
    case IS_STRING:
        otelsilta_span_set_str(span, key, Z_STRVAL_P(value));  break;
    case IS_LONG:
        otelsilta_span_set_int(span, key, Z_LVAL_P(value));    break;
    case IS_DOUBLE:
        otelsilta_span_set_dbl(span, key, Z_DVAL_P(value));    break;
    case IS_TRUE:
        otelsilta_span_set_bool(span, key, 1);                 break;
    case IS_FALSE:
        otelsilta_span_set_bool(span, key, 0);                 break;
    default:
        /* Convert to string */
        convert_to_string_ex(value);
        otelsilta_span_set_str(span, key, Z_STRVAL_P(value));  break;
    }
}

/* otelsilta_current_trace_id(): string|false */
PHP_FUNCTION(otelsilta_current_trace_id) {
    ZEND_PARSE_PARAMETERS_NONE();
    if (!OTELSILTA_G(request_active) || OTELSILTA_G(trace_id)[0] == '\0') {
        RETURN_FALSE;
    }
    RETURN_STRING(OTELSILTA_G(trace_id));
}

/* otelsilta_force_sample(): void – force this request to be sampled */
PHP_FUNCTION(otelsilta_force_sample_request) {
    ZEND_PARSE_PARAMETERS_NONE();
    otelsilta_tracer_force_sample();
}

/* ===== Function table ===== */

/* Arginfo for userland functions (PHP 8.0+) */
ZEND_BEGIN_ARG_INFO_EX(arginfo_otelsilta_span_start, 0, 0, 1)
    ZEND_ARG_TYPE_INFO(0, name, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_otelsilta_span_finish, 0, 0, 1)
    ZEND_ARG_TYPE_INFO(0, handle, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_otelsilta_span_set_attribute, 0, 0, 3)
    ZEND_ARG_TYPE_INFO(0, handle, IS_LONG, 0)
    ZEND_ARG_TYPE_INFO(0, key, IS_STRING, 0)
    ZEND_ARG_TYPE_INFO(0, value, IS_MIXED, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_otelsilta_current_trace_id, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_otelsilta_force_sample_request, 0, 0, 0)
ZEND_END_ARG_INFO()

static const zend_function_entry otelsilta_functions[] = {
    PHP_FE(otelsilta_span_start,          arginfo_otelsilta_span_start)
    PHP_FE(otelsilta_span_finish,         arginfo_otelsilta_span_finish)
    PHP_FE(otelsilta_span_set_attribute,  arginfo_otelsilta_span_set_attribute)
    PHP_FE(otelsilta_current_trace_id,    arginfo_otelsilta_current_trace_id)
    PHP_FE(otelsilta_force_sample_request, arginfo_otelsilta_force_sample_request)
    PHP_FE_END
};

/* ===== Module lifecycle ===== */

PHP_MINIT_FUNCTION(otelsilta) {
    ZEND_INIT_MODULE_GLOBALS(otelsilta, php_otelsilta_init_globals, NULL);
    REGISTER_INI_ENTRIES();

    /* Seed PRNG for sampling decisions */
    srand((unsigned)time(NULL) ^ (unsigned)getpid());

    if (OTELSILTA_G(enabled)) {
        /* Register the central Observer API callback for targeted hooks
         * (PDO, curl, Redis, Memcached, templates).  These work well
         * with the observer API since they target specific functions. */
        otelsilta_observer_register();

        /* Hook zend_execute_ex for generic userland function tracing.
         * The observer API caches handler decisions per-function per-process
         * and does not invoke callbacks on every call, so we use the
         * traditional zend_execute_ex override to trace each invocation. */
        OTELSILTA_G(original_execute_ex) = zend_execute_ex;
        zend_execute_ex = otelsilta_execute_ex;

        /* Error/exception hooks still use global engine callbacks
         * (zend_throw_exception_hook / zend_error_cb) because they
         * are not function-call observers. */
        if (OTELSILTA_G(feature_errors)) {
            otelsilta_errors_minit();
        }
    }

    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(otelsilta) {
    otelsilta_errors_mshutdown();

    /* Restore original zend_execute_ex */
    if (OTELSILTA_G(original_execute_ex)) {
        zend_execute_ex = OTELSILTA_G(original_execute_ex);
        OTELSILTA_G(original_execute_ex) = NULL;
    }

    /* Observer API hooks are automatically cleaned up by the engine
     * when the module is unloaded — no explicit unregistration needed. */
    UNREGISTER_INI_ENTRIES();
    return SUCCESS;
}

PHP_RINIT_FUNCTION(otelsilta) {
#if defined(COMPILE_DL_OTELSILTA) && defined(ZTS)
    ZEND_TSRMLS_CACHE_UPDATE();
#endif

    if (!OTELSILTA_G(enabled)) return SUCCESS;

    /* Detect CLI mode early so MINIT hooks (error handler, etc.) can
     * bail out quickly without touching any engine state. */
    OTELSILTA_G(cli_mode) = (sapi_module.name &&
                              strcmp(sapi_module.name, "cli") == 0) ? 1 : 0;

    /* Initialise per-request state (always, so RSHUTDOWN can safely
     * destroy curl_handles even when we bail out early). */
    otelsilta_tracer_request_init();

    /* Skip full tracing for CLI SAPI — the extension is designed for
     * HTTP request instrumentation.  CLI tools (drush, composer,
     * artisan, etc.) don't need APM tracing and the PHP streams
     * exporter can crash during CLI RSHUTDOWN. */
    if (OTELSILTA_G(cli_mode)) {
        return SUCCESS;
    }

    /* ---- Sanity check: required configuration ----
     * Do not trace if essential settings are missing.  This prevents
     * silent failures (e.g. exporting to a non-existent endpoint) and
     * ensures the operator has consciously configured the extension.
     * Hooks installed at MINIT (error/exception handlers) remain active
     * but will see request_active=0 and short-circuit. */
    {
        const char *svc = otelsilta_effective_service_name();
        const char *ep  = otelsilta_effective_endpoint();

        if (!svc || svc[0] == '\0' ||
            !ep  || ep[0]  == '\0') {
            otelsilta_debug_log(
                "otelsilta: tracing disabled — required config missing"
                " (service_name=%s, endpoint=%s)",
                svc ? svc : "(unset)",
                ep  ? ep  : "(unset)");
            return SUCCESS;
        }
    }

    /* Make sampling decision (reads request headers) */
    otelsilta_tracer_make_sampling_decision();

    /* Reset per-request observer span stack */
    extern void otelsilta_observer_rinit(void);
    otelsilta_observer_rinit();

    /* Per-request init for DB DSN/statement tracking */
    if (OTELSILTA_G(feature_db)) {
        extern void otelsilta_observer_pdo_rinit(void);
        otelsilta_observer_pdo_rinit();
    }

    /* Initialise span aggregation buffer (DB + cache) */
    if (OTELSILTA_G(feature_db) || OTELSILTA_G(feature_cache)) {
        otelsilta_aggregator_init();
    }

    if (OTELSILTA_G(feature_profiling) && OTELSILTA_G(request_active)) {
        OTELSILTA_G(memory_start) = (zend_long)zend_memory_usage(0);
    }

    return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(otelsilta) {
    if (!OTELSILTA_G(enabled)) return SUCCESS;

    if (OTELSILTA_G(feature_profiling) && OTELSILTA_G(request_active)) {
        otelsilta_span_t *root = OTELSILTA_G(root_span);
        if (root) {
            otelsilta_span_set_int(root, "process.memory_start",
                                    OTELSILTA_G(memory_start));
            otelsilta_span_set_int(root, "process.memory_peak",
                                    (zend_long)zend_memory_peak_usage(0));
        }
    }

    /* Per-feature cleanup before trace export */
    if (OTELSILTA_G(feature_db)) {
        extern void otelsilta_observer_pdo_rshutdown(void);
        otelsilta_observer_pdo_rshutdown();
    }

    /* Flush aggregated spans (DB + cache) into the trace before export */
    if (OTELSILTA_G(agg_buckets)) {
        otelsilta_aggregator_flush();
        otelsilta_aggregator_destroy();
    }

    /* Finalise root span, export, and free all spans.
     * Must run after feature cleanups; destroys curl_handles HashTable. */
    otelsilta_tracer_request_shutdown();

    /* Free emalloc'd curl_info entries, then destroy the HashTable itself. */
    {
        zend_ulong idx;
        void *ptr;
        ZEND_HASH_FOREACH_NUM_KEY_PTR(&OTELSILTA_G(curl_handles), idx, ptr) {
            (void)idx;
            if (ptr) efree(ptr);
        } ZEND_HASH_FOREACH_END();
        zend_hash_destroy(&OTELSILTA_G(curl_handles));
    }

    return SUCCESS;
}

PHP_MINFO_FUNCTION(otelsilta) {
    const char *svc  = otelsilta_effective_service_name();
    const char *ns   = otelsilta_effective_service_namespace();
    const char *dep  = otelsilta_effective_deployment_environment();
    const char *ep   = otelsilta_effective_endpoint();
    const char *rattr = otelsilta_effective_resource_attributes();

    php_info_print_table_start();
    php_info_print_table_header(2, "otelsilta support", "enabled");
    php_info_print_table_row(2,    "Version",           PHP_OTELSILTA_VERSION);
    php_info_print_table_row(2,    "Service name",
        (svc && svc[0]) ? svc : "(not configured)");
    php_info_print_table_row(2,    "Service namespace",
        (ns && ns[0]) ? ns : "(not set)");
    php_info_print_table_row(2,    "Deployment environment",
        (dep && dep[0]) ? dep : "(not set)");
    php_info_print_table_row(2,    "Resource attributes",
        (rattr && rattr[0]) ? rattr : "(empty)");
    php_info_print_table_row(2,    "Exporter endpoint",
        (ep && ep[0])   ? ep  : "(not configured)");

    /* Show whether the sanity check would pass */
    int config_ok = (svc && svc[0] && ep && ep[0]);
    php_info_print_table_row(2,    "Tracing ready",
        OTELSILTA_G(enabled) && config_ok ? "yes" : "no — required config missing");

    php_info_print_table_row(2,    "Features",
        OTELSILTA_G(feature_errors)    ? "errors "    : "");
    php_info_print_table_end();
    DISPLAY_INI_ENTRIES();
}

/* ===== Module entry ===== */

zend_module_entry otelsilta_module_entry = {
    STANDARD_MODULE_HEADER,
    PHP_OTELSILTA_EXTNAME,
    otelsilta_functions,
    PHP_MINIT(otelsilta),
    PHP_MSHUTDOWN(otelsilta),
    PHP_RINIT(otelsilta),
    PHP_RSHUTDOWN(otelsilta),
    PHP_MINFO(otelsilta),
    PHP_OTELSILTA_VERSION,
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_OTELSILTA
# ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
# endif
ZEND_GET_MODULE(otelsilta)
#endif
