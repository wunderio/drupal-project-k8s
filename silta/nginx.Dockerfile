# Dockerfile for building nginx.
# New line to trigger image rebuild.
FROM wunderio/silta-nginx:1.17-status-test

COPY . /app/web
