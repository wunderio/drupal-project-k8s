# Dockerfile for building nginx.
FROM wunderio/silta-nginx:latest
# Trigger build 2
COPY . /app/web
