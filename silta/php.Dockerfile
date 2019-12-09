# Dockerfile for the Drupal container.
FROM wunderio/drupal-php-fpm:latest

# Override lagoon's entrypoint
COPY silta/71-php-newrelic.sh /lagoon/entrypoints/

COPY --chown=www-data:www-data . /app
