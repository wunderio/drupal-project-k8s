# Dockerfile for building nginx.
FROM wunderio/silta-nginx:latest
# cache09
COPY . /app/web
