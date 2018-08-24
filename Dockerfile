FROM php:7.1.21-fpm-alpine3.8

COPY --chown=www-data:www-data . /var/www/html
USER root

RUN set -ex; \
    # Install composer
    wget -qO- https://getcomposer.org/installer | php -- --install-dir=/usr/local/bin --filename=composer; \
    # Add apps
    apk add --update --no-cache -t .php-rundeps \
	    mariadb-client \
        vim \
        su-exec;\
    docker-php-ext-install \
        sockets \
	    mysqli \
	    pdo \ 
        pdo_mysql; \
    # Install drush.
    su-exec wodby composer global require drush/drush:^8.0; \
    mkdir -p -m +w /var/www/html/web/sites/default/files; \
    chown -R www-data:www-data /var/www/html/web/sites/default/files
