# Dockerfile for the Drupal container.
FROM eu.gcr.io/silta-images/shell:php7.3-v0.1

COPY --chown=www-data:www-data . /app
