# Dockerfile for building nginx.
FROM wunderio/silta-nginx:sigsci-test

COPY . /app/web
