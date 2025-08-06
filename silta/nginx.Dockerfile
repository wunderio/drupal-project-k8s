# Dockerfile for building nginx.
FROM wunderio/silta-nginx:1.26-v1.1-test1

COPY . /app/web
