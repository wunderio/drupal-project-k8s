# Dockerfile for the Drupal container.
FROM eu.gcr.io/silta-images/php:8.0-fpm-v0.1

# TODO: Move to php8 img
COPY silta/php/100-php-config-alter.sh /lagoon/entrypoints/100-php-config-alter.sh

COPY --chown=www-data:www-data . /app

