# Dockerfile for building nginx.
FROM wunderio/silta-nginx:latest
# cache14
COPY . /app/web
