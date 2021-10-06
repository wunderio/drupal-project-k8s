# Dockerfile for the Drupal container.
#FROM eu.gcr.io/silta-images/php:7.3-fpm-v0.1
#FROM eu.gcr.io/silta-images/php:7.4-fpm-v0.1
FROM eu.gcr.io/silta-images/php:8.0-fpm-v0.1

COPY --chown=www-data:www-data . /app

RUN echo "rebuild 2"
