#ifndef OTELSILTA_PROPAGATION_H
#define OTELSILTA_PROPAGATION_H

#include <stddef.h>

/* Parse a W3C traceparent header value.
 * Returns 1 on success, 0 on parse failure.
 * trace_id_out must be at least 33 bytes.
 * span_id_out  must be at least 17 bytes.
 * flags_out receives the trace-flags byte. */
int otelsilta_parse_traceparent(
    const char *header,
    char *trace_id_out,
    char *span_id_out,
    int  *flags_out
);

/* Generate a W3C traceparent header value.
 * buf must be at least 56 bytes (00-{32}-{16}-{2} + NUL). */
void otelsilta_build_traceparent(
    const char *trace_id,
    const char *span_id,
    int         sampled,
    char       *buf,
    size_t      buf_size
);

#endif /* OTELSILTA_PROPAGATION_H */
