# Dockerfile for building nginx.
FROM wunderio/silta-nginx:latest
# cache15
COPY . /app/web
