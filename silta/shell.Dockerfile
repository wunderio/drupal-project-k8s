# Dockerfile for the Drupal container.
# Cache bust.
FROM wunderio/silta-php-shell:php8.3-v1.2-test2

COPY --chown=www-data:www-data . /app
