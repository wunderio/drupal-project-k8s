# Dockerfile for building nginx.
FROM wunderio/silta-nginx:latest
# cache03
COPY . /app/web
