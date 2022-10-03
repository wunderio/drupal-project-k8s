# Dockerfile for the Drupal container.
# im01
#FROM wunderio/silta-php-shell:php7.3-v0.1
#FROM wunderio/silta-php-shell:php7.4-v0.1
FROM wunderio/silta-php-shell:php8.0-v0.1

# Old
# FROM wunderio/silta-php-shell:php8.0-v0.1.8
# New, faulty
# FROM wunderio/silta-php-shell:php8.0-v0.1.10

# RUN mkdir /silta/tmp && chown www-data:www-data /silta/tmp && chmod 777 /silta/tmp

COPY --chown=www-data:www-data . /app
