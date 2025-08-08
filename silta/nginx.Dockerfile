# Dockerfile for building nginx.
FROM wunderio/silta-nginx:1.28-v1.0-test2

COPY . /app/web
