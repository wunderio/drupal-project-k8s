# Dockerfile for building nginx.
FROM wunderio/silta-nginx:latest
# cache07
COPY . /app/web
