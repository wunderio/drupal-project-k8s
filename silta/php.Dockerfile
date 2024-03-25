# Dockerfile for the Drupal container.
# Cache bust 2
FROM wunderio/silta-php-fpm:8.2-fpm-v1

COPY --chown=www-data:www-data . /app

USER www-data
