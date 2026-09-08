# Dockerfile for building nginx.
FROM wunderio/silta-nginx:1.31-v1-rc1

COPY . /app/web
