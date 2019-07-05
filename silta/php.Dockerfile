# Dockerfile for the Drupal container.
FROM wunderio/drupal-php-fpm:v0.1.4

COPY --chown=www-data:www-data . /app

