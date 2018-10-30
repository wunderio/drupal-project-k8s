# Dockerfile for the Drupal container.
FROM wunderio/drupal-php-fpm:v0.1

USER root
RUN mkdir -p /var/reference-data && chown www-data:www-data /var/reference-data

COPY --chown=www-data:www-data . /var/www/html
USER www-data
RUN mkdir -p -m +w /var/www/html/web/sites/default/files

