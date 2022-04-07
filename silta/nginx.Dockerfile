# Dockerfile for building nginx.
FROM eu.gcr.io/silta-images/nginx:1.21-test
#11
COPY . /app/web

