# Dockerfile for the Drupal container.
FROM wunderio/silta-php-fpm:8.3-fpm-v1

COPY --chown=www-data:www-data . /app


# TODO: nginx trace_id

# RUN install-php-extensions opentelemetry-php/ext-opentelemetry@main
# RUN apk add php83-pecl-opentelemetry

# RUN pecl install opentelemetry-${PHP_OPENTELEMETRY_VERSION} && \
#     strip $(php-config --extension-dir)/opentelemetry.so
# PHP_OPENTELEMETRY_VERSION="1.2.1" \

# # TEST OPENTELEMETRY PHP
# RUN apk add --virtual .phpize-deps make g++ autoconf \
#     && pecl install opentelemetry \
#     && apk del .phpize-deps
   
# TEST OTEL SILTA
RUN apk add --virtual .phpize-deps make g++ autoconf curl-dev \
    && cd silta/otelsilta \
    && phpize \
    && ./configure --enable-otelsilta \
    && make && make install \
    && apk del .phpize-deps \
    && apk add strace

USER www-data
