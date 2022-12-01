# Dockerfile for building nginx.
# FROM wunderio/silta-nginx:latest
FROM wunderio/silta-nginx:1.23-test

COPY . /app/web
