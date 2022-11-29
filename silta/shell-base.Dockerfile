# Dockerfile for the Shell container.
# ARG CODE_FROM
# FROM ${CODE_FROM} as code-image
FROM europe-north1-docker.pkg.dev/silta-dev/images/drupal-project-k8s-php:php-test-1 as code-image
FROM wunderio/silta-php-shell:php8.0-v0.1
COPY --from=code-image --chown=www-data:www-data /app /app
