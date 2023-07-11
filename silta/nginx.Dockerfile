# Dockerfile for building nginx.
# cache01
FROM wunderio/silta-nginx:latest

COPY . /app/web
