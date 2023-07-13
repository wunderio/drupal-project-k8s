# Dockerfile for building nginx.
FROM wunderio/silta-nginx:latest
# cache11
COPY . /app/web
