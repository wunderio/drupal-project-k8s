# Dockerfile for the Drupal container.
FROM wunderio/silta-php-shell:php8.2-v1

COPY --chown=www-data:www-data . /app

COPY silta/ssh-audit.sh /usr/local/bin/ssh-audit.sh
RUN apk add util-linux \
    chmod +x /usr/local/bin/ssh-audit.sh \
    echo 'ForceCommand /usr/local/bin/ssh-audit.sh' >> /etc/ssh/sshd_config
