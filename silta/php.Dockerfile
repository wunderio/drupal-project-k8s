# Dockerfile for the Drupal container.
FROM wunderio/silta-php-fpm:8.2-fpm-v1

# Fixes ERROR 1045 (28000): Plugin caching_sha2_password could not be loaded: Error loading shared library /usr/lib/mariadb/plugin/caching_sha2_password.so: No such file or directory
# Fixes ERROR 2059 (HY000): Authentication method dummy_fallback_auth is not supported
RUN apk add mariadb-connector-c

COPY --chown=www-data:www-data . /app

USER www-data
