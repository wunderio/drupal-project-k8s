# Dockerfile for building nginx.
FROM eu.gcr.io/silta-images/nginx:1.17-stopsignal

COPY . /app/web
