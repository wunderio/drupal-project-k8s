# Dockerfile for the Drupal container.
FROM wunderio/silta-php-fpm:8.3-fpm-alpine320-test

COPY --chown=www-data:www-data . /app

USER www-data
