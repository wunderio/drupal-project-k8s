# Dockerfile for the Drupal container.
#FROM wunderio/silta-php-shell:php7.3-v1
#FROM wunderio/silta-php-shell:php7.4-v1
FROM wunderio/silta-php-shell:php8.0-v1

COPY --chown=www-data:www-data . /app
