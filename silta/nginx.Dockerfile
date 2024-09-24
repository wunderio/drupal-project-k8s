# Dockerfile for building nginx.
#trigger1
FROM wunderio/silta-nginx:1.26-v1

COPY . /app/web
