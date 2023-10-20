# Dockerfile for building nginx.
# Cache 17.8.2023.
FROM wunderio/silta-nginx:1.17-v1

COPY . /app/web
