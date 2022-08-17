# Dockerfile for the Drupal container.
#FROM eu.gcr.io/silta-images/php:7.3-fpm-v0.1
#FROM eu.gcr.io/silta-images/php:7.4-fpm-v0.1
# FROM eu.gcr.io/silta-images/php:8.0-fpm-v0.1
# ci01
FROM wunderio/silta-php-fpm:test-8.0

COPY --chown=www-data:www-data . /app

USER www-data
