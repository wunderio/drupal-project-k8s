# Dockerfile for the Drupal container.
FROM wunderio/silta-php-fpm:8.3-fpm-v1-test1

COPY --chown=www-data:www-data . /app

USER www-data
