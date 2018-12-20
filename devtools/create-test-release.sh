#!/usr/bin/env bash

if [ $1 ]; then
  SUFFIX=$1
else
  echo "Please specify the project name suffix as a parameter."
  exit 1
fi

PROJECT=test-project-$SUFFIX

BRANCH=master
RELEASE_NAME="$PROJECT--$BRANCH"

helm upgrade \
    --install $RELEASE_NAME ./chart \
    --set environmentName=$BRANCH \
    --set php.image=wunderio/silta-drupal-test-images:php-minimal \
    --set nginx.image=wunderio/silta-drupal-test-images:nginx-minimal \
    --set mariadb.rootUser.password='abcdef1234' \
    --set mariadb.db.password='abcdef1234' \
    --set shell.enabled=false \
    --namespace=$PROJECT \
    --timeout 600

kubectl logs job/${RELEASE_NAME}-post-release -n $PROJECT -f

## Wait to be sure that readiness checks have been propagated.
kubectl rollout status -w deployment/${RELEASE_NAME}-drupal -n $PROJECT
sleep 3
