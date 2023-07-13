# Dockerfile for building nginx.
FROM wunderio/silta-nginx:latest
# cache16
COPY . /app/web
