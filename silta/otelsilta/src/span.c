#include "span.h"
#include "php_otelsilta.h"

#include <string.h>
#include <stdlib.h>

/* ---- helpers ---- */

static int attr_limit(otelsilta_span_t *span) {
    zend_long lim = OTELSILTA_G(max_attributes_per_span);
    if (lim <= 0) lim = OTELSILTA_MAX_ATTRIBUTES;
    return (int)lim;
}

/* ---- public API ---- */

otelsilta_span_t *otelsilta_span_create(
    const char *name,
    otelsilta_span_kind_t kind,
    const char *trace_id,
    const char *parent_span_id
) {
    otelsilta_span_t *span = (otelsilta_span_t *)emalloc(sizeof(otelsilta_span_t));
    if (!span) return NULL;

    memset(span, 0, sizeof(*span));

    strncpy(span->name, name ? name : "unknown", sizeof(span->name) - 1);
    span->kind = kind;

    if (trace_id) {
        strncpy(span->trace_id, trace_id, sizeof(span->trace_id) - 1);
    }
    if (parent_span_id && parent_span_id[0] != '\0') {
        strncpy(span->parent_span_id, parent_span_id, sizeof(span->parent_span_id) - 1);
    }

    otelsilta_generate_id(span->span_id, 8);
    span->start_time_ns = otelsilta_time_ns();
    span->status        = SPAN_STATUS_UNSET;
    span->is_finished   = 0;
    span->next          = NULL;

    return span;
}

void otelsilta_span_finish(otelsilta_span_t *span) {
    if (!span || span->is_finished) return;
    span->end_time_ns = otelsilta_time_ns();
    span->is_finished = 1;
}

void otelsilta_span_set_str(otelsilta_span_t *span, const char *key, const char *value) {
    if (!span || !key || span->attribute_count >= attr_limit(span)) return;
    otelsilta_attribute_t *a = &span->attributes[span->attribute_count++];
    strncpy(a->key, key, sizeof(a->key) - 1);
    a->type = ATTR_TYPE_STRING;
    strncpy(a->value.str_val, value ? value : "", sizeof(a->value.str_val) - 1);
}

void otelsilta_span_set_int(otelsilta_span_t *span, const char *key, zend_long value) {
    if (!span || !key || span->attribute_count >= attr_limit(span)) return;
    otelsilta_attribute_t *a = &span->attributes[span->attribute_count++];
    strncpy(a->key, key, sizeof(a->key) - 1);
    a->type = ATTR_TYPE_INT;
    a->value.int_val = value;
}

void otelsilta_span_set_dbl(otelsilta_span_t *span, const char *key, double value) {
    if (!span || !key || span->attribute_count >= attr_limit(span)) return;
    otelsilta_attribute_t *a = &span->attributes[span->attribute_count++];
    strncpy(a->key, key, sizeof(a->key) - 1);
    a->type = ATTR_TYPE_DOUBLE;
    a->value.dbl_val = value;
}

void otelsilta_span_set_bool(otelsilta_span_t *span, const char *key, zend_bool value) {
    if (!span || !key || span->attribute_count >= attr_limit(span)) return;
    otelsilta_attribute_t *a = &span->attributes[span->attribute_count++];
    strncpy(a->key, key, sizeof(a->key) - 1);
    a->type = ATTR_TYPE_BOOL;
    a->value.bool_val = value;
}

void otelsilta_span_set_status(otelsilta_span_t *span,
                                otelsilta_span_status_t status,
                                const char *message) {
    if (!span) return;
    span->status = status;
    if (message) {
        strncpy(span->status_message, message, sizeof(span->status_message) - 1);
    }
}

int otelsilta_span_add_event(otelsilta_span_t *span,
                             const char *type,
                             const char *message,
                             const char *stacktrace) {
    if (!span) return 0;
    if (span->event_count >= OTELSILTA_MAX_EVENTS_PER_SPAN) {
        span->event_dropped++;
        return 0;
    }
    if (!span->events) {
        span->events = (otelsilta_span_event_t *)ecalloc(
            OTELSILTA_MAX_EVENTS_PER_SPAN, sizeof(otelsilta_span_event_t));
    }
    otelsilta_span_event_t *ev = &span->events[span->event_count];
    ev->time_unix_nano = otelsilta_time_ns();
    strncpy(ev->type,    type    ? type    : "", sizeof(ev->type) - 1);
    strncpy(ev->message, message ? message : "", sizeof(ev->message) - 1);
    if (stacktrace && stacktrace[0]) {
        strncpy(ev->stacktrace, stacktrace, sizeof(ev->stacktrace) - 1);
    }
    span->event_count++;
    return 1;
}

void otelsilta_span_free_all(otelsilta_span_t *head) {
    while (head) {
        otelsilta_span_t *next = head->next;
        if (head->events) efree(head->events);
        efree(head);
        head = next;
    }
}
