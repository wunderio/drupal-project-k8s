# Dockerfile for building nginx.
FROM wunderio/silta-nginx:latest
# cache17
COPY . /app/web
