# Dockerfile for the Drupal container.
FROM wunderio/silta-php-shell:php8.3-mariadb11-test

COPY --chown=www-data:www-data . /app
