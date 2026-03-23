#ifndef OTELSILTA_TRACER_H
#define OTELSILTA_TRACER_H

#include "php_otelsilta.h"
#include "span.h"

/* Start request tracing (called in RINIT). */
void otelsilta_tracer_request_init(void);

/* Finish request tracing (called in RSHUTDOWN). */
void otelsilta_tracer_request_shutdown(void);

/* Create a child span, push it onto the span stack, return it.
 * Returns NULL when not sampled or span limit reached. */
otelsilta_span_t *otelsilta_tracer_start_span(const char *name,
                                               otelsilta_span_kind_t kind);

/* Finish the top-of-stack span and pop it. */
void otelsilta_tracer_end_span(otelsilta_span_t *span);

/* Return the currently active span (top of stack), or NULL. */
otelsilta_span_t *otelsilta_tracer_current_span(void);

/* Determine whether the current request is sampled.
 * Call after request headers are available. */
void otelsilta_tracer_make_sampling_decision(void);

/* Force sampling (e.g. because an error was encountered). */
void otelsilta_tracer_force_sample(void);

#endif /* OTELSILTA_TRACER_H */
