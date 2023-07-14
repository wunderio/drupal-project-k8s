# Dockerfile for building nginx.
FROM wunderio/silta-nginx:latest
# cache18
COPY . /app/web
