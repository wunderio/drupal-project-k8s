# Dockerfile for the Drupal container.
FROM wunderio/drupal-php-fpm:v0.1

COPY --chown=www-data:www-data . /var/www/html

# Switch back to root
USER root
RUN apk add --no-cache openssh bash python py-requests py-yaml
EXPOSE 22

# Copy scripts
COPY silta/shell/gitauth_keys.py /etc/ssh/
COPY silta/shell/entrypoint.sh /

CMD ["/entrypoint.sh"]
