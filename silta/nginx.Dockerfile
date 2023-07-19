# Dockerfile for building nginx.
# cache 01
FROM wunderio/silta-nginx:1.17-status-test

COPY . /app/web
