# Dockerfile for building nginx.
FROM eu.gcr.io/silta-images/nginx:1.21-test
#FROM eu.gcr.io/silta-images/nginx:latest
# test1ddd
COPY . /app/web
