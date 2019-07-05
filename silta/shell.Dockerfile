# Dockerfile for the Drupal container.
FROM wunderio/drupal-shell:v0.1.4

COPY --chown=www-data:www-data . /app
