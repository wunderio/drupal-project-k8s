# Dockerfile for building nginx.
FROM wunderio/silta-nginx:1.26-v1.0.0-test20240612

COPY . /app/web
