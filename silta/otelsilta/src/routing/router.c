#include "router.h"
#include <string.h>
#include <ctype.h>
#include <stdio.h>

/* ---- UUID detection ----
 * Simple heuristic: 8-4-4-4-12 hex chars separated by hyphens = 36 chars. */
static int is_uuid(const char *seg, size_t len) {
    if (len != 36) return 0;
    /* Pattern: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx */
    static const int hyph[] = {8, 13, 18, 23};
    for (int i = 0; i < 36; i++) {
        int is_hyph_pos = 0;
        for (int j = 0; j < 4; j++) {
            if (i == hyph[j]) { is_hyph_pos = 1; break; }
        }
        if (is_hyph_pos) {
            if (seg[i] != '-') return 0;
        } else {
            if (!isxdigit((unsigned char)seg[i])) return 0;
        }
    }
    return 1;
}

/* Is segment purely numeric? */
static int is_numeric(const char *seg, size_t len) {
    if (len == 0) return 0;
    for (size_t i = 0; i < len; i++) {
        if (!isdigit((unsigned char)seg[i])) return 0;
    }
    return 1;
}

/* Strip query string and fragment from path first */
static void strip_query(const char *path, char *buf, size_t buf_size) {
    const char *q = strchr(path, '?');
    const char *h = strchr(path, '#');
    size_t keep = strlen(path);
    if (q && (size_t)(q - path) < keep) keep = (size_t)(q - path);
    if (h && (size_t)(h - path) < keep) keep = (size_t)(h - path);
    if (keep >= buf_size) keep = buf_size - 1;
    memcpy(buf, path, keep);
    buf[keep] = '\0';
}

void otelsilta_normalize_route(const char *path, char *out, size_t out_size) {
    if (!path || !out || out_size == 0) {
        if (out && out_size > 0) out[0] = '\0';
        return;
    }

    char clean[2048];
    strip_query(path, clean, sizeof(clean));

    /* Drupal aggregated asset paths: collapse everything after the
     * /sites/default/files/ prefix into a single {file} placeholder. */
    #define DRUPAL_FILES_PREFIX "/sites/default/files/"
    if (strncmp(clean, DRUPAL_FILES_PREFIX, sizeof(DRUPAL_FILES_PREFIX) - 1) == 0) {
        snprintf(out, out_size, "/sites/default/files/{file}");
        return;
    }

    /* Walk segments */
    size_t j   = 0;
    const char *p = clean;

    /* Leading slash */
    if (*p == '/') {
        if (j + 1 < out_size) out[j++] = '/';
        p++;
    }

    while (*p && j + 2 < out_size) {
        /* Find end of segment */
        const char *end = p;
        while (*end && *end != '/') end++;
        size_t seg_len = (size_t)(end - p);

        const char *replacement = NULL;
        if (is_uuid(p, seg_len))         replacement = "{uuid}";
        else if (is_numeric(p, seg_len)) replacement = "{id}";

        if (replacement) {
            size_t rlen = strlen(replacement);
            if (j + rlen < out_size) {
                memcpy(out + j, replacement, rlen);
                j += rlen;
            }
        } else {
            if (j + seg_len < out_size) {
                memcpy(out + j, p, seg_len);
                j += seg_len;
            }
        }

        p = end;
        if (*p == '/') {
            if (j + 1 < out_size) out[j++] = '/';
            p++;
        }
    }

    if (j == 0 && out_size > 0) { out[j++] = '/'; }
    out[j] = '\0';
}
