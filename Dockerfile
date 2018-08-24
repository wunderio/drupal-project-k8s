# Dockerfile for the Drupal container.
FROM wodby/php:7.1-4.5.3

COPY --chown=www-data:www-data . /var/www/html
USER www-data

RUN set -ex; \
    # Install composer
    wget -qO- https://getcomposer.org/installer | php -- --install-dir=/usr/local/bin --filename=composer; \
    # Install drush
    su-exec wodby composer global require drush/drush:^8.0; \
    \
    mkdir -p -m +w /var/www/html/web/sites/default/files;