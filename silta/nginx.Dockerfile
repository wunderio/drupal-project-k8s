# Dockerfile for building nginx.
FROM wunderio/silta-nginx:latest
# cache12
COPY . /app/web
