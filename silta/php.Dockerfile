# Dockerfile for the Drupal container.
FROM wunderio/silta-php-fpm:8.5-fpm-v1.0.0-rc1

COPY --chown=www-data:www-data . /app

USER www-data
