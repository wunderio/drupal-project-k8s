# Dockerfile for the Drupal container.
FROM wodby/php:7.1-4.5.3

COPY --chown=www-data:www-data . /var/www/html
USER www-data
RUN mkdir -p -m +w /var/www/html/web/sites/default/files
