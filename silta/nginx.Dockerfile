# Dockerfile for building nginx.
FROM wunderio/silta-nginx:latest
# cache20
COPY . /app/web
