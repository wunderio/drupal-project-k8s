# Dockerfile for the Drupal container.
FROM wunderio/silta-php-shell:php8.2-v1

#####
# Install Elasticdump (skip when php base image contains it)
RUN apk add npm nodejs && npm install -g elasticdump
#####

COPY --chown=www-data:www-data . /app
