# Dockerfile for building nginx.
FROM wunderio/silta-nginx:latest
# cache19
COPY . /app/web
