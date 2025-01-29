# Dockerfile for the Drupal container.
FROM wunderio/silta-php-fpm:8.3-fpm-v1

COPY --chown=www-data:www-data . /app

RUN apk add --virtual buildDeps git autoconf make g++ zlib-dev \
    && git clone https://github.com/NoiseByNorthwest/php-spx.git /tmp/php-spx-alpine \
    && cd /tmp/php-spx-alpine \
    && git checkout release/latest \
    && phpize \
    && ./configure \
    && make \
    && make install \
    # && echo "extension=spx.so" > /usr/local/etc/php/conf.d/spx.ini \
    && rm -rf /tmp/php-spx-alpine \
    && apk del buildDeps
    
COPY silta/spx.ini /usr/local/etc/php/conf.d/spx.ini

USER www-data
