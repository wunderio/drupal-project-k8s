# Dockerfile for the Drupal container.
# im01
#FROM wunderio/silta-php-shell:php7.3-v0.1
FROM wunderio/silta-php-shell:php7.4-v0.1
#FROM wunderio/silta-php-shell:php8.0-v0.1

COPY --chown=www-data:www-data . /app
