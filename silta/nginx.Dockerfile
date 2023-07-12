# Dockerfile for building nginx.
FROM wunderio/silta-nginx:latest
# cache05
COPY . /app/web
