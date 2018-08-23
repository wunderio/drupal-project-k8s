FROM wunderio/drupal-php-fpm

COPY --chown=www-data:www-data . /var/www/html
WORKDIR /var/www/html/web/

USER www-data
