# Dockerfile for the Drupal container.
FROM wunderio/silta-php-fpm:8.2-fpm-v1

COPY --chown=www-data:www-data . /app

#####
# Install Elasticdump
RUN apk add npm nodejs && npm install -g elasticdump
#####

USER www-data
