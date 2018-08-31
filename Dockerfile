FROM wunderio/php-fpm-alpine:v0.0.1

COPY --chown=www-data:www-data . /var/www/html 
WORKDIR /var/www/html/web/

USER www-data