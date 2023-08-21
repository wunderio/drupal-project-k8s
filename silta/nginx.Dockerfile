# Dockerfile for building nginx.
# Cache 17.8.2023.
FROM wunderio/silta-nginx:latest

COPY . /app/web
