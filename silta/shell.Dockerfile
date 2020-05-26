# Dockerfile for the Drupal container.
FROM wunderio/drupal-shell:v0.2

COPY --chown=www-data:www-data . /app
