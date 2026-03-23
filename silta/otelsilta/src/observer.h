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
 * zend_execute_ex override for generic userland function tracing.
 *
 * Unlike the Observer API (which caches per-function per-process),
 * this is invoked on every userland function call.  Install in MINIT:
 *     OTELSILTA_G(original_execute_ex) = zend_execute_ex;
 *     zend_execute_ex = otelsilta_execute_ex;
 * Restore in MSHUTDOWN:
 *     zend_execute_ex = OTELSILTA_G(original_execute_ex);
 */
void otelsilta_execute_ex(zend_execute_data *execute_data);

#endif /* OTELSILTA_OBSERVER_H */
