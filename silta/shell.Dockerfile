# Dockerfile for the Drupal container.
FROM wunderio/drupal-shell:latest

COPY --chown=www-data:www-data . /app
