# Dockerfile for the Drupal container.
# im01
#FROM wunderio/silta-php-shell:php7.3-v0.1
#FROM wunderio/silta-php-shell:php7.4-v0.1
#FROM wunderio/silta-php-shell:php8.0-v0.1
FROM wunderio/silta-php-shell:php8.1-v0.1

# remove drush-launcher
# https://github.com/drush-ops/drush-launcher/issues/105
RUN rm /usr/local/bin/drush;

COPY --chown=www-data:www-data . /app
