#!/usr/bin/env sh
# Run from repo root: sh silta/otelsilta/test/errors/run.sh
set -e
IMG=wunderio/silta-php-fpm:8.3-fpm-v1
docker run --rm -i -v "$PWD":/app -w /app "$IMG" sh -s <<'EOF'
set -e
apk add --no-cache --virtual .build make g++ autoconf curl-dev fcgi python3 >/dev/null
cd silta/otelsilta
phpize >/dev/null && ./configure --enable-otelsilta >/dev/null && make >/dev/null && make install >/dev/null
rm -rf /tmp/otlp && mkdir -p /tmp/otlp
python3 test/errors/sink.py & SINK=$!
php-fpm -F -c test/errors/php.ini -y test/errors/www.conf & FPM=$!
sleep 2
drive() { # $1=script $2=uri $3=method
  SCRIPT_FILENAME=/app/silta/otelsilta/test/errors/$1 \
  SCRIPT_NAME=$2 REQUEST_URI=$2 REQUEST_METHOD=${3:-GET} \
  cgi-fcgi -bind -connect 127.0.0.1:9000 >/dev/null 2>&1 || true
  sleep 1
}
drive caught.php /caught
drive uncaught.php /uncaught
drive fatal.php /fatal
drive loop_throw.php /loop
kill $FPM $SINK 2>/dev/null || true
echo "=== captured payloads ==="
for f in /tmp/otlp/req-*.json; do echo "--- $f ---"; cat "$f"; echo; done
EOF
