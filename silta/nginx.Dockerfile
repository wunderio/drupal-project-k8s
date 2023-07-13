# Dockerfile for building nginx.
FROM wunderio/silta-nginx:latest
# cache08
COPY . /app/web
