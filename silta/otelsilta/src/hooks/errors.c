#include "errors.h"
#include "php_otelsilta.h"
#include "../tracer.h"
#include "../span.h"

#include "zend_exceptions.h"
#include "zend_errors.h"
#include "zend_interfaces.h"

#include <string.h>
#include <stdio.h>

/* ---- Saved original hooks ---- */

static void (*orig_throw_exception_hook)(zend_object *) = NULL;
static void (*orig_error_cb)(int, zend_string *, const uint32_t,
                              zend_string *) = NULL;

/* ---- Exception hook ---- */

static void otelsilta_throw_exception_hook(zend_object *exception) {
    if (OTELSILTA_G(enabled) && OTELSILTA_G(feature_errors) &&
        !OTELSILTA_G(cli_mode) && OTELSILTA_G(request_active)) {

        otelsilta_span_t *span = otelsilta_tracer_current_span();
        if (!span) span = OTELSILTA_G(root_span);

        if (span) {
            if (span->event_count >= OTELSILTA_MAX_EVENTS_PER_SPAN) {
                /* cheap path: don't build a stacktrace we would discard */
                span->event_dropped++;
            } else {
                zend_class_entry *ce = exception->ce;
                const char *cname = ce ? ZSTR_VAL(ce->name) : "Exception";

                zval *msg_zv = zend_read_property(ce, exception,
                    "message", sizeof("message") - 1, 1, NULL);
                const char *msg = (msg_zv && Z_TYPE_P(msg_zv) == IS_STRING)
                                  ? Z_STRVAL_P(msg_zv) : "";

                zval trace_str;
                ZVAL_UNDEF(&trace_str);
                zend_call_method_with_0_params(
                    exception, ce, NULL, "gettraceasstring", &trace_str);

                char st[OTELSILTA_EVENT_STACKTRACE_LEN];
                st[0] = '\0';
                if (Z_TYPE(trace_str) == IS_STRING) {
                    size_t n = Z_STRLEN(trace_str);
                    if (n >= sizeof(st)) {
                        memcpy(st, Z_STRVAL(trace_str), sizeof(st) - 16);
                        strcpy(st + sizeof(st) - 16, "...(truncated)");
                    } else {
                        memcpy(st, Z_STRVAL(trace_str), n);
                        st[n] = '\0';
                    }
                }

                otelsilta_span_add_event(span, cname, msg, st);

                if (Z_TYPE(trace_str) != IS_UNDEF) zval_ptr_dtor(&trace_str);
            }
        }
        /* Deliberately NO has_error / status change: a throw is not an error. */
    }

    if (orig_throw_exception_hook) orig_throw_exception_hook(exception);
}

/* ---- Error callback ---- */

static void otelsilta_error_cb(int type, zend_string *error_filename,
                                const uint32_t error_lineno,
                                zend_string *message) {
    /* Only intercept fatal and catchable-fatal errors */
    /* PHP 8 ORs E_DONT_BAIL (0x8000) into `type` for uncaught errors;
     * mask it (and any high flags) off before comparing severities. */
    int base = type & E_ALL;
    int is_fatal = (base == E_ERROR          ||
                    base == E_PARSE          ||
                    base == E_CORE_ERROR     ||
                    base == E_COMPILE_ERROR  ||
                    base == E_RECOVERABLE_ERROR);

    if (is_fatal && OTELSILTA_G(enabled) && OTELSILTA_G(feature_errors) &&
        !OTELSILTA_G(cli_mode)) {
        const char *fname = error_filename ? ZSTR_VAL(error_filename) : "unknown";
        const char *msg   = message        ? ZSTR_VAL(message)        : "";

        if (!OTELSILTA_G(request_active)) {
            OTELSILTA_G(has_error) = 1;
            otelsilta_tracer_force_sample();
        }

        if (OTELSILTA_G(request_active)) {
            otelsilta_span_t *span = otelsilta_tracer_current_span();
            if (!span) span = OTELSILTA_G(root_span);

            char loc[OTELSILTA_EVENT_STACKTRACE_LEN];
            snprintf(loc, sizeof(loc), "%s:%u", fname, error_lineno);

            if (span) otelsilta_span_add_event(span, "PHP Error", msg, loc);

            if (OTELSILTA_G(root_span))
                otelsilta_span_set_status(OTELSILTA_G(root_span),
                                          SPAN_STATUS_ERROR, msg);

            OTELSILTA_G(has_error) = 1;
        }
    }

    if (orig_error_cb) {
        orig_error_cb(type, error_filename, error_lineno, message);
    }
}

/* ---- install / uninstall ---- */

void otelsilta_errors_minit(void) {
    orig_throw_exception_hook  = zend_throw_exception_hook;
    zend_throw_exception_hook  = otelsilta_throw_exception_hook;

    orig_error_cb = zend_error_cb;
    zend_error_cb = otelsilta_error_cb;
}

void otelsilta_errors_mshutdown(void) {
    if (zend_throw_exception_hook == otelsilta_throw_exception_hook) {
        zend_throw_exception_hook = orig_throw_exception_hook;
    }
    if (zend_error_cb == otelsilta_error_cb) {
        zend_error_cb = orig_error_cb;
    }
}
