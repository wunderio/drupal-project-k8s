#ifndef OTELSILTA_ERRORS_H
#define OTELSILTA_ERRORS_H

/* Install error and exception hooks. Called from MINIT. */
void otelsilta_errors_minit(void);

/* Uninstall hooks. Called from MSHUTDOWN. */
void otelsilta_errors_mshutdown(void);

#endif /* OTELSILTA_ERRORS_H */
