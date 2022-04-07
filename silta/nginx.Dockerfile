# Dockerfile for building nginx.
FROM eu.gcr.io/silta-images/nginx:latest

COPY . /app/web

RUN adduser nginx www-data
