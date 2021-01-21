# Dockerfile for the Drupal container.
FROM eu.gcr.io/silta-images/shell:php8.0-v0.1

# Xdebug: [Config] The setting 'xdebug.remote_enable' has been renamed, see the upgrading guide at https://xdebug.org/docs/upgrade_guide#changed-xdebug.remote_enable (See: https://xdebug.org/docs/errors#CFG-C-CHANGED)
RUN mv /usr/local/etc/php/conf.d/docker-php-ext-xdebug.ini /usr/local/etc/php/conf.d/docker-php-ext-xdebug.disable \
  && sed -i '/xdebug.remote_enable/d' /usr/local/etc/php/php.ini

COPY --chown=www-data:www-data . /app
