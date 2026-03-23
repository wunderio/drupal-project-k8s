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
        !OTELSILTA_G(cli_mode)) {
        /* Force sampling so errors are always captured */
        if (!OTELSILTA_G(request_active)) {
            OTELSILTA_G(has_error) = 1;
            otelsilta_tracer_force_sample();
        }

        if (OTELSILTA_G(request_active)) {
            zend_class_entry *ce    = exception->ce;
            const char       *cname = ce ? ZSTR_VAL(ce->name) : "Exception";

            /* Get exception message */
            zval *msg_zv = zend_read_property(ce, exception,
                                               "message", sizeof("message") - 1,
                                               1, NULL);
            const char *msg = "";
            if (msg_zv && Z_TYPE_P(msg_zv) == IS_STRING) {
                msg = Z_STRVAL_P(msg_zv);
            }

            /* Get stack trace string */
            zval trace_str;
            ZVAL_UNDEF(&trace_str);
            zend_call_method_with_0_params(
                exception, ce, NULL, "gettraceasstring", &trace_str);

            /* Create an error span */
            char span_name[512];
            snprintf(span_name, sizeof(span_name), "exception %s", cname);

            otelsilta_span_t *span =
                otelsilta_tracer_start_span(span_name, SPAN_KIND_INTERNAL);

            if (span) {
                otelsilta_span_set_str(span, "exception.type",    cname);
                otelsilta_span_set_str(span, "exception.message", msg);
                if (Z_TYPE(trace_str) == IS_STRING) {
                    otelsilta_span_set_str(span, "exception.stacktrace",
                                           Z_STRVAL(trace_str));
                }
                otelsilta_span_set_status(span, SPAN_STATUS_ERROR, msg);
                otelsilta_tracer_end_span(span);
            }

            /* Also annotate the currently active span */
            otelsilta_span_t *cur = otelsilta_tracer_current_span();
            if (cur && cur != span) {
                otelsilta_span_set_str(cur, "exception.type",    cname);
                otelsilta_span_set_str(cur, "exception.message", msg);
                otelsilta_span_set_status(cur, SPAN_STATUS_ERROR, msg);
            }

            OTELSILTA_G(has_error) = 1;

            if (Z_TYPE(trace_str) != IS_UNDEF) {
                zval_ptr_dtor(&trace_str);
            }
        }
    }

    if (orig_throw_exception_hook) {
        orig_throw_exception_hook(exception);
    }
}

/* ---- Error callback ---- */

static void otelsilta_error_cb(int type, zend_string *error_filename,
                                const uint32_t error_lineno,
                                zend_string *message) {
    /* Only intercept fatal and catchable-fatal errors */
    int is_fatal = (type == E_ERROR          ||
                    type == E_PARSE          ||
                    type == E_CORE_ERROR     ||
                    type == E_COMPILE_ERROR  ||
                    type == E_RECOVERABLE_ERROR);

    if (is_fatal && OTELSILTA_G(enabled) && OTELSILTA_G(feature_errors) &&
        !OTELSILTA_G(cli_mode)) {
        const char *fname = error_filename ? ZSTR_VAL(error_filename) : "unknown";
        const char *msg   = message        ? ZSTR_VAL(message)        : "";

        if (!OTELSILTA_G(request_active)) {
            OTELSILTA_G(has_error) = 1;
            otelsilta_tracer_force_sample();
        }

        if (OTELSILTA_G(request_active)) {
            char span_name[64];
            snprintf(span_name, sizeof(span_name), "php.error");

            otelsilta_span_t *span =
                otelsilta_tracer_start_span(span_name, SPAN_KIND_INTERNAL);

            if (span) {
                char loc[512];
                snprintf(loc, sizeof(loc), "%s:%u", fname, error_lineno);
                otelsilta_span_set_str(span, "error.type",     "PHP Error");
                otelsilta_span_set_str(span, "error.message",  msg);
                otelsilta_span_set_str(span, "error.location", loc);
                otelsilta_span_set_int(span, "error.php_type", (zend_long)type);
                otelsilta_span_set_status(span, SPAN_STATUS_ERROR, msg);
                otelsilta_tracer_end_span(span);
            }

            /* Annotate root span */
            otelsilta_span_t *root = OTELSILTA_G(root_span);
            if (root) {
                otelsilta_span_set_status(root, SPAN_STATUS_ERROR, msg);
            }

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
