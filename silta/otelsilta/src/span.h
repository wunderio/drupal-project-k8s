#ifndef OTELSILTA_SPAN_H
#define OTELSILTA_SPAN_H

#include "php_otelsilta.h"

/* Allocate a new span (emalloc, per-request lifetime). */
otelsilta_span_t *otelsilta_span_create(
    const char *name,
    otelsilta_span_kind_t kind,
    const char *trace_id,
    const char *parent_span_id
);

/* Finish a span (set end_time_ns, mark is_finished). */
void otelsilta_span_finish(otelsilta_span_t *span);

/* Add a string attribute; silently drops beyond max. */
void otelsilta_span_set_str(otelsilta_span_t *span, const char *key, const char *value);
void otelsilta_span_set_int(otelsilta_span_t *span, const char *key, zend_long value);
void otelsilta_span_set_dbl(otelsilta_span_t *span, const char *key, double value);
void otelsilta_span_set_bool(otelsilta_span_t *span, const char *key, zend_bool value);

/* Mark span status. */
void otelsilta_span_set_status(otelsilta_span_t *span,
                                otelsilta_span_status_t status,
                                const char *message);

/* Free the span and its linked successors. */
void otelsilta_span_free_all(otelsilta_span_t *head);

#endif /* OTELSILTA_SPAN_H */
