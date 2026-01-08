# Dockerfile for the Drupal container.
FROM wunderio/silta-php-shell:php8.5-v1.0.0-rc1

COPY --chown=www-data:www-data . /app
