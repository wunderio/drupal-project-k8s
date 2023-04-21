# Dockerfile for building nginx.
FROM wunderio/silta-nginx:latest
# Trigger build
COPY . /app/web
