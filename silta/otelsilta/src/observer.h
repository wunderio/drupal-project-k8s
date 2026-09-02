#ifndef OTELSILTA_OBSERVER_H
#define OTELSILTA_OBSERVER_H

#include "php_otelsilta.h"
#include "zend_observer.h"

/*
 * Central observer — registered once at MINIT via
 * zend_observer_fcall_register(otelsilta_observer_fcall_init).
 *
 * Called once per function per request on first invocation.
 * Returns {begin, end} handlers based on feature flags and function
 * identity.  Returning {NULL, NULL} means zero overhead for that
 * function on subsequent calls (the engine caches the decision).
 *
 * Handles targeted hooks: PDO, curl, Redis, Memcached, templates.
 */

/* Register the central observer (targeted hooks).  Call from MINIT. */
void otelsilta_observer_register(void);

/*
 * Per-curl-handle metadata captured across curl_setopt()/curl_setopt_array()
 * calls, keyed by resource handle ID in OTELSILTA_G(curl_handles).
 *
 * `headers` holds a snapshot (zend_array_dup, not a reference) of the last
 * CURLOPT_HTTPHEADER array the app set, so traceparent injection can merge
 * with it instead of clobbering it.  IS_UNDEF until CURLOPT_HTTPHEADER is
 * seen; zval_ptr_dtor() on an IS_UNDEF zval is a safe no-op, so cleanup via
 * otelsilta_observer_curl_info_free() is unconditional.
 */
typedef struct {
    char url[OTELSILTA_MAX_STR_LEN];
    char method[16];
    zval headers;   /* IS_UNDEF until CURLOPT_HTTPHEADER seen; owned array zval */
} otelsilta_curl_info_t;

/*
 * Build a merged CURLOPT_HTTPHEADER array into `dst` (initialized as a new
 * PHP array): copies every string entry from `stored_or_null` except any
 * existing "traceparent:" header (case-insensitive), then appends
 * `traceparent_line`.  `stored_or_null` may be NULL (no prior headers).
 */
void otelsilta_curl_merge_headers(zval *dst, HashTable *stored_or_null,
                                   const char *traceparent_line);

/* Frees one otelsilta_curl_handles entry (dtors the headers zval, then
 * efrees the struct).  Suitable as the RSHUTDOWN cleanup for
 * OTELSILTA_G(curl_handles) entries. */
void otelsilta_observer_curl_info_free(void *ptr);

/*
 * Bailout safety for generic user-function tracing: frees any deferred
 * function-span frames whose end handler never ran (e.g. a fatal error
 * mid-call).  Call from RSHUTDOWN, after the profiling block and before
 * the PDO/aggregator/tracer shutdown calls.
 */
void otelsilta_observer_functions_rshutdown(void);

#endif /* OTELSILTA_OBSERVER_H */
