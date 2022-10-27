# Dockerfile for the Drupal container.
# im02
#FROM wunderio/silta-php-fpm:7.3-fpm-v0.1
#FROM wunderio/silta-php-fpm:7.4-fpm-v0.1
# FROM wunderio/silta-php-fpm:8.0-fpm-v0.1
FROM eu.gcr.io/silta-images/php:8.0-fpm-gdpr-test

COPY --chown=www-data:www-data . /app

USER www-data
