# Dockerfile for building nginx.
FROM wunderio/silta-nginx:latest
# cache13
COPY . /app/web
