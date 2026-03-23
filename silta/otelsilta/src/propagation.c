#include "propagation.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* Format: 00-{traceId32hex}-{parentId16hex}-{flags2hex}
 * Example: 00-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-01 */

int otelsilta_parse_traceparent(
    const char *header,
    char *trace_id_out,
    char *span_id_out,
    int  *flags_out
) {
    if (!header) return 0;

    /* Must be at least: 00-32hex-16hex-2hex = 55 chars */
    size_t len = strlen(header);
    if (len < 55) return 0;

    if (header[0] != '0' || header[1] != '0' || header[2] != '-') return 0;

    /* trace-id: chars 3..34 */
    for (int i = 3; i < 35; i++) {
        if (!isxdigit((unsigned char)header[i])) return 0;
        trace_id_out[i - 3] = (char)tolower((unsigned char)header[i]);
    }
    trace_id_out[32] = '\0';

    if (header[35] != '-') return 0;

    /* parent-id: chars 36..51 */
    for (int i = 36; i < 52; i++) {
        if (!isxdigit((unsigned char)header[i])) return 0;
        span_id_out[i - 36] = (char)tolower((unsigned char)header[i]);
    }
    span_id_out[16] = '\0';

    if (header[52] != '-') return 0;

    /* flags: chars 53..54 */
    if (!isxdigit((unsigned char)header[53]) ||
        !isxdigit((unsigned char)header[54])) return 0;

    char flags_str[3] = {header[53], header[54], '\0'};
    *flags_out = (int)strtol(flags_str, NULL, 16);

    /* Reject all-zeros IDs */
    int all_zero = 1;
    for (int i = 0; i < 32; i++) {
        if (trace_id_out[i] != '0') { all_zero = 0; break; }
    }
    if (all_zero) return 0;

    return 1;
}

void otelsilta_build_traceparent(
    const char *trace_id,
    const char *span_id,
    int         sampled,
    char       *buf,
    size_t      buf_size
) {
    snprintf(buf, buf_size, "00-%s-%s-%02x",
             trace_id, span_id, sampled ? 1 : 0);
}
