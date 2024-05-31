# Dockerfile for building nginx.
FROM wunderio/silta-nginx:1.26-v1.0.0-test20240531

COPY . /app/web
