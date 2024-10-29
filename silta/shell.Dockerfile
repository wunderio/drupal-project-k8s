# Dockerfile for the Shell container.
FROM wunderio/silta-php-shell:php8.3-v1-test20241029-1

COPY --chown=www-data:www-data . /app
