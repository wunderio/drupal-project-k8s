# Dockerfile for the Drupal container.
FROM wunderio/drupal-php-fpm:latest

COPY --chown=www-data:www-data . /app
