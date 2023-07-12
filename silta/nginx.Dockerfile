# Dockerfile for building nginx.
FROM wunderio/silta-nginx:latest
# cache04
COPY . /app/web
