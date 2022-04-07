# Dockerfile for the Drupal container.
#FROM eu.gcr.io/silta-images/shell:php7.3-v0.1
#FROM eu.gcr.io/silta-images/shell:php7.4-v0.1
FROM eu.gcr.io/silta-images/shell:php8.0-v0.1-test

COPY --chown=www-data:www-data . /app
