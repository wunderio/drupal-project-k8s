dnl config.m4 for extension otelsilta

PHP_ARG_ENABLE([otelsilta],
  [whether to enable otelsilta support],
  [AS_HELP_STRING([--enable-otelsilta],
    [Enable otelsilta APM instrumentation support])],
  [no])

if test "$PHP_OTELSILTA" != "no"; then
  AC_DEFINE(HAVE_OTELSILTA, 1, [Whether you have otelsilta])

  PHP_NEW_EXTENSION([otelsilta],
    [otelsilta.c \
     src/span.c \
     src/tracer.c \
     src/exporter.c \
     src/propagation.c \
     src/sanitizer.c \
     src/observer.c \
     src/aggregator.c \
     src/hooks/errors.c \
     src/routing/router.c],
    [$ext_shared],
    [],
    [-DZEND_ENABLE_STATIC_TSRMLS_CACHE=1 -std=c99])

  PHP_ADD_INCLUDE([$ext_srcdir])
  PHP_ADD_INCLUDE([$ext_srcdir/src])
  PHP_SUBST(OTELSILTA_SHARED_LIBADD)
fi
