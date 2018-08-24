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
    # Install drush
    su-exec www-data composer global require drush/drush:^8.0; \
    ln -s /home/www-data/.composer/vendor/bin/drush /usr/local/bin/drush; \
    # Create directory for shared files
    mkdir -p -m +w /var/www/html/web/sites/default/files; \
    chown -R www-data:www-data /var/www/html/web/sites/default/files
