#!/bin/bash

# Xdebug: [Config] The setting 'xdebug.remote_enable' has been renamed, see the upgrading guide at https://xdebug.org/docs/upgrade_guide#changed-xdebug.remote_enable (See: https://xdebug.org/docs/errors#CFG-C-CHANGED)
sed -i '/xdebug.remote_enable/d' /usr/local/etc/php/conf.d/00-lagoon-php.ini
sed -i '/xdebug.remote_enable/d' /usr/local/etc/php/php.ini
mv /usr/local/etc/php/conf.d/docker-php-ext-xdebug.ini /usr/local/etc/php/conf.d/docker-php-ext-xdebug.disabled
