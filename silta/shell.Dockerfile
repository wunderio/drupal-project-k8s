# Dockerfile for the Drupal container.
FROM wunderio/silta-php-shell:8.3-secfix-test

COPY --chown=www-data:www-data . /app
