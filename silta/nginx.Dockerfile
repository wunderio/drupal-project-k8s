# Dockerfile for building nginx.
FROM wunderio/silta-nginx:latest
# cache02
COPY . /app/web
