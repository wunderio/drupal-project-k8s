#!/bin/sh
# Build the extension and run its .phpt suite inside php:<ver>-cli-alpine
# containers (the host has no phpize). The source is copied out of the
# read-only mount inside each container so per-version build artifacts
# never mix with each other or with the host tree.
#
# Usage:   ./run-tests.sh [tests/foo.phpt ...]      # full 8.3/8.4/8.5 matrix
#          PHP_VERSIONS="8.3" ./run-tests.sh ...    # single version (iteration)
set -eu
cd "$(dirname "$0")"

VERSIONS="${PHP_VERSIONS:-8.3 8.4 8.5}"
TESTS="${*:-tests}"

for v in $VERSIONS; do
    IMAGE="otelsilta-test:$v"
    if ! docker image inspect "$IMAGE" >/dev/null 2>&1; then
        docker build --build-arg PHP_VERSION="$v" -t "$IMAGE" tests/docker
    fi
    echo "==> PHP $v"
    docker run --rm -u "$(id -u):$(id -g)" -e HOME=/tmp -e NO_INTERACTION=1 \
        -e TEST_PHP_ARGS="--show-diff" \
        -v "$PWD":/ext:ro -w /tmp "$IMAGE" sh -c "
            mkdir -p /tmp/build &&
            cp -r /ext/. /tmp/build &&
            cd /tmp/build &&
            phpize && ./configure --enable-otelsilta &&
            make -j\$(nproc) &&
            make test TESTS=\"$TESTS\"
        "
done
echo "OK: all versions passed"
