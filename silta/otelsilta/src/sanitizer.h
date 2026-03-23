#ifndef OTELSILTA_SANITIZER_H
#define OTELSILTA_SANITIZER_H

#include <stddef.h>

/* Sanitize a SQL query: replace literal values with '?'.
 * Writes result into out (max out_size bytes). */
void otelsilta_sanitize_sql(const char *sql, char *out, size_t out_size);

/* Sanitize a URL: strip query string.
 * Writes result into out (max out_size bytes). */
void otelsilta_sanitize_url(const char *url, char *out, size_t out_size);

/* Infer db.system from a PDO DSN string (e.g. "mysql:host=..."). */
const char *otelsilta_dsn_to_db_system(const char *dsn);

/* Infer db.operation from a SQL query (SELECT, INSERT, UPDATE, DELETE ...). */
void otelsilta_sql_operation(const char *sql, char *out, size_t out_size);

#endif /* OTELSILTA_SANITIZER_H */
