#include "sanitizer.h"

#include <string.h>
#include <ctype.h>
#include <stdio.h>

/* ---- SQL sanitization ----
 * Strategy: copy SQL char by char; when we enter a quoted literal or
 * numeric literal, replace content with '?'. */

void otelsilta_sanitize_sql(const char *sql, char *out, size_t out_size) {
    if (!sql || !out || out_size == 0) return;

    size_t j   = 0;
    size_t len = strlen(sql);

    for (size_t i = 0; i < len && j + 2 < out_size; i++) {
        char c = sql[i];

        /* Single-quoted string */
        if (c == '\'') {
            if (j + 3 < out_size) {
                out[j++] = '\'';
                out[j++] = '?';
                out[j++] = '\'';
            }
            i++;
            while (i < len) {
                if (sql[i] == '\'' &&
                    (i == 0 || sql[i - 1] != '\\')) { break; }
                i++;
            }
            continue;
        }

        /* Double-quoted string */
        if (c == '"') {
            if (j + 3 < out_size) {
                out[j++] = '"';
                out[j++] = '?';
                out[j++] = '"';
            }
            i++;
            while (i < len) {
                if (sql[i] == '"' && (i == 0 || sql[i - 1] != '\\')) break;
                i++;
            }
            continue;
        }

        /* Numeric literal (only after whitespace/operator) */
        if (isdigit((unsigned char)c) && j > 0 &&
            (isspace((unsigned char)out[j - 1]) ||
             out[j - 1] == '='  || out[j - 1] == ',' ||
             out[j - 1] == '('  || out[j - 1] == '>' ||
             out[j - 1] == '<')) {
            out[j++] = '?';
            while (i + 1 < len &&
                   (isdigit((unsigned char)sql[i + 1]) ||
                    sql[i + 1] == '.' || sql[i + 1] == 'e' ||
                    sql[i + 1] == 'E' || sql[i + 1] == '+' ||
                    sql[i + 1] == '-')) {
                i++;
            }
            continue;
        }

        /* NULL / TRUE / FALSE literal */
        if (i + 4 <= len) {
            char upper4[5];
            for (int k = 0; k < 4; k++)
                upper4[k] = (char)toupper((unsigned char)sql[i + k]);
            upper4[4] = '\0';
            if (strcmp(upper4, "NULL") == 0 ||
                strcmp(upper4, "TRUE") == 0) {
                out[j++] = '?';
                i += 3;
                continue;
            }
        }
        if (i + 5 <= len) {
            char upper5[6];
            for (int k = 0; k < 5; k++)
                upper5[k] = (char)toupper((unsigned char)sql[i + k]);
            upper5[5] = '\0';
            if (strcmp(upper5, "FALSE") == 0) {
                out[j++] = '?';
                i += 4;
                continue;
            }
        }

        out[j++] = c;
    }
    out[j] = '\0';
}

/* ---- URL sanitization: strip query and fragment ---- */

void otelsilta_sanitize_url(const char *url, char *out, size_t out_size) {
    if (!url || !out || out_size == 0) return;

    const char *q = strchr(url, '?');
    const char *h = strchr(url, '#');

    size_t keep = strlen(url);
    if (q && (size_t)(q - url) < keep) keep = (size_t)(q - url);
    if (h && (size_t)(h - url) < keep) keep = (size_t)(h - url);

    if (keep >= out_size) keep = out_size - 1;
    memcpy(out, url, keep);
    out[keep] = '\0';
}

/* ---- DSN → db.system ---- */

const char *otelsilta_dsn_to_db_system(const char *dsn) {
    if (!dsn) return "other";
    if (strncasecmp(dsn, "mysql",    5) == 0) return "mysql";
    if (strncasecmp(dsn, "pgsql",    5) == 0) return "postgresql";
    if (strncasecmp(dsn, "sqlite",   6) == 0) return "sqlite";
    if (strncasecmp(dsn, "sqlsrv",   6) == 0) return "mssql";
    if (strncasecmp(dsn, "oci",      3) == 0) return "oracle";
    if (strncasecmp(dsn, "ibm",      3) == 0) return "db2";
    return "other";
}

/* ---- SQL → operation ---- */

void otelsilta_sql_operation(const char *sql, char *out, size_t out_size) {
    if (!sql || !out || out_size == 0) return;

    /* Skip leading whitespace */
    while (*sql && isspace((unsigned char)*sql)) sql++;

    struct { const char *kw; } ops[] = {
        {"SELECT"}, {"INSERT"}, {"UPDATE"}, {"DELETE"},
        {"CREATE"}, {"DROP"},   {"ALTER"},  {"REPLACE"},
        {"CALL"},   {"EXEC"},   {"BEGIN"},  {"COMMIT"},
        {"ROLLBACK"}, {NULL}
    };

    for (int i = 0; ops[i].kw; i++) {
        size_t kwlen = strlen(ops[i].kw);
        if (strncasecmp(sql, ops[i].kw, kwlen) == 0 &&
            (sql[kwlen] == '\0' || isspace((unsigned char)sql[kwlen]))) {
            snprintf(out, out_size, "%s", ops[i].kw);
            return;
        }
    }
    snprintf(out, out_size, "QUERY");
}
