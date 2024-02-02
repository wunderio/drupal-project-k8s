# Dockerfile for the Drupal container.
FROM wunderio/silta-php-shell:php8.2-v1

# Fixes ERROR 1045 (28000): Plugin caching_sha2_password could not be loaded: Error loading shared library /usr/lib/mariadb/plugin/caching_sha2_password.so: No such file or directory
RUN apk add mariadb-connector-c

COPY --chown=www-data:www-data . /app
