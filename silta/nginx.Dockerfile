# Dockerfile for building nginx.
FROM wunderio/silta-nginx:1.23-sigsci-test

COPY . /app/web
