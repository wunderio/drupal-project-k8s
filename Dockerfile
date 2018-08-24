# Dockerfile for the Drupal container.
FROM wodby/base-php:7.1

COPY --chown=www-data:www-data . /var/www/html
USER root

RUN set -ex; \
    # Install composer
    wget -qO- https://getcomposer.org/installer | php -- --install-dir=/usr/local/bin --filename=composer; \
    # Install drush.
    su-exec wodby composer global require drush/drush:^8.0; \
    mkdir -p -m +w /var/www/html/web/sites/default/files; \
    chown -R www-data:www-data /var/www/html/web/sites/default/files
