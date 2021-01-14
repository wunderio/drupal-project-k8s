# Dockerfile for the Drupal container.
FROM eu.gcr.io/silta-images/php:7.3-fpm-v0.1

COPY --chown=www-data:www-data . /app

