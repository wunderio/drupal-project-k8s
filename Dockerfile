# Dockerfile for the Drupal container.
FROM wunderio/drupal-php-fpm:v0.1

USER root
RUN mkdir -p /var/backups/db
RUN chown www-data:www-data /var/backups/db

COPY --chown=www-data:www-data . /var/www/html
USER www-data
RUN mkdir -p -m +w /var/www/html/web/sites/default/files

