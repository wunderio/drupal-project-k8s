# Dockerfile for building nginx.
FROM wunderio/silta-nginx:1.26-v1.0-test1

COPY . /app/web
