# Dockerfile for the Drupal container.
FROM wunderio/silta-php-shell:php8.4-v1.0.0-rc2

COPY --chown=www-data:www-data . /app
