FROM wunderio/php-fpm-alpine:v0.0.1

USER root

ENV XDEBUG_ENABLE=0 \
    XDEBUG_REMOTE_PORT=9000 \
    XDEBUG_HOST=localhost

ENV NEWRELIC_ENABLED=0 \
    NEWRELIC_APP_NAME="PHP Application" \
    NEWRELIC_LICENSE="" \
    NEWRELIC_LOG_FILE="/dev/stdout" \
    NEWRELIC_LOG_LEVEL="info" \
    NEWRELIC_DAEMON_LOG_FILE="/dev/stdout" \
    NEWRELIC_DAEMON_LOG_LEVEL="info"

ENV PHP_MEMORY_LIMIT=256M \
    PHP_PRECISION=-1 \
    PHP_OUTPUT_BUFFERING=4096 \
    PHP_SERIALIZE_PRECISION=-1 \
    PHP_MAX_EXECUTION_TIME=60 \
    PHP_ERROR_REPORTING=E_ALL \
    PHP_DISPLAY_ERRORS=0 \
    PHP_DISPLAY_STARTUP_ERRORS=0 \
    PHP_TRACK_ERRORS=0 \
    PHP_LOG_ERRORS=1 \
    PHP_LOG_ERRORS_MAX_LEN=10240 \
    PHP_POST_MAX_SIZE=40M \
    PHP_MAX_UPLOAD_FILESIZE=10M \
    PHP_MAX_FILE_UPLOADS=20 \
    PHP_MAX_INPUT_TIME=60 \
    PHP_DATE_TIMEZONE=Europe/Helsinki \
    PHP_VARIABLES_ORDER=EGPCS \
    PHP_REQUEST_ORDER=GP \
    PHP_SESSION_SERIALIZE_HANDLER=php_binary \
    PHP_SESSION_SAVE_HANDLER=files \
    PHP_SESSION_SAVE_PATH=/tmp \
    PHP_SESSION_GC_PROBABILITY=1 \
    PHP_SESSION_GC_DIVISOR=10000 \
    PHP_OPCACHE_ENABLE=1 \
    PHP_OPCACHE_ENABLE_CLI=0 \
    PHP_OPCACHE_MEMORY_CONSUMPTION=128 \
    PHP_OPCACHE_INTERNED_STRINGS_BUFFER=32 \
    PHP_OPCACHE_MAX_ACCELERATED_FILES=10000 \
    PHP_OPCACHE_USE_CWD=1 \
    PHP_OPCACHE_VALIDATE_TIMESTAMPS=1 \
    PHP_OPCACHE_REVALIDATE_FREQ=2 \
    PHP_OPCACHE_ENABLE_FILE_OVERRIDE=0 \
    PHP_ZEND_ASSERTIONS=-1 \
    PHP_IGBINARY_COMPACT_STRINGS=1 \
    PHP_PM=ondemand \
    PHP_PM_MAX_CHILDREN=100 \
    PHP_PM_START_SERVERS=20 \
    PHP_PM_MIN_SPARE_SERVERS=20 \
    PHP_PM_MAX_SPARE_SERVERS=20 \
    PHP_PM_PROCESS_IDLE_TIMEOUT=60s \
    PHP_PM_MAX_REQUESTS=500

COPY ./conf/php.ini /etc/php7/php.ini
COPY ./conf/www.conf /etc/php7/php-fpm.d/www.conf
COPY ./conf/php-fpm.conf /etc/php7/php-fpm.conf
COPY ./conf/xdebug.ini /etc/php7/conf.d/xdebug.ini
COPY ./conf/newrelic.ini /etc/php7/conf.d/newrelic.ini

COPY --chown=www-data:www-data . /var/www/html

WORKDIR /var/www/html/web/

USER www-data

CMD ["/usr/sbin/php-fpm7"]