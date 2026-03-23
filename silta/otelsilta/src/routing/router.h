#ifndef OTELSILTA_ROUTER_H
#define OTELSILTA_ROUTER_H

#include <stddef.h>

/* Normalize a URL path: replace numeric segments with {id},
 * UUID segments with {uuid}, and Drupal file paths with {file}.
 * Writes result into out (max out_size bytes). */
void otelsilta_normalize_route(const char *path, char *out, size_t out_size);

/* Attempt to detect the HTTP route from framework routing tables.
 * Currently supports Laravel ($_SERVER['ROUTE_NAME']) and Symfony
 * (_route attribute).  Falls back to normalize_route.
 * Writes result into out (max out_size bytes). */
void otelsilta_detect_route(const char *uri, char *out, size_t out_size);

#endif /* OTELSILTA_ROUTER_H */
