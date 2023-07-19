# Dockerfile for building nginx.
# New line to trigger image rebuild.
FROM wunderio/silta-nginx:latest

COPY . /app/web
