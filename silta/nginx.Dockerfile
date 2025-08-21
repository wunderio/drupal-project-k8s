# Dockerfile for building nginx.
FROM wunderio/silta-nginx:1.28-v1-test1

COPY . /app/web
