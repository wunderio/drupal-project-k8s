#!/usr/bin/env bash

namespace="example"
target_release_name="master"
source_release_name="example-123--master"
override_domain=0

if [[ $override_domain = 1 ]] ; then
  domain="--set clusterDomain=helm3.wdr.io"
else
  domain=""
fi

set -ex

shopt -s expand_aliases
alias helm3=~/Downloads/helm3

# Make sure we have the wunderio repository for helm3
helm3 repo add wunderio https://storage.googleapis.com/charts.wdr.io

# Delete any shell service that might be in the way.
kubectl delete service ${target_release_name}-shell -n $namespace || true

# Delete the PVC for reference data
kubectl delete job -l release=$source_release_name -n $namespace
kubectl delete pvc ${target_release_name}-reference-data -n $namespace || true

helm get values $source_release_name > /tmp/$source_release_name.yaml

helm3 upgrade --install $target_release_name wunderio/drupal \
  --values /tmp/$source_release_name.yaml \
  $domain \
  --set referenceData.enabled=false \
  --set elasticsearch.volumeClaimTemplate.resources.requests.storage=1Gi \
  --namespace=$namespace \
  --no-hooks

kubectl get statefulset -n "$namespace" -l "release=${target_release_name}" -o name | xargs -n 1 kubectl rollout status -n "$namespace"
kubectl get deployment -n "$namespace" -l "release=${target_release_name}" -o name | xargs -n 1 kubectl rollout status -n "$namespace"


podName=$(kubectl -n $namespace get pods -l release=$target_release_name,deployment=drupal -o name)
podName=$(echo "${podName/pod\//}" | head -n 1 )

#sourcePodName=$(kubectl -n $namespace get pods -l release=$source_release_name,deployment=drupal -o name)
#sourcePodName="${podName/pod\//}"

# Migrate the database
echo "Migrating database"
kubectl exec -it -c php -n $namespace $podName -- bash -c "
drush sql-drop -y

for TABLE in \$(echo 'show tables;' | mysql -N -u \$DB_USER --password=\$DB_PASS --host=$source_release_name-mariadb \$DB_NAME) ;
do
  echo \$TABLE
  if echo \$TABLE | grep -E '(cache|cache_.*)'
  then
    mysqldump -u \$DB_USER --password=\$DB_PASS --host=$source_release_name-mariadb --no-data \$DB_NAME \$TABLE | mysql -u \$DB_USER --password=\$DB_PASS --host=\$DB_HOST \$DB_NAME
  else
    mysqldump -u \$DB_USER --password=\$DB_PASS --host=$source_release_name-mariadb \$DB_NAME \$TABLE | mysql -u \$DB_USER --password=\$DB_PASS --host=\$DB_HOST \$DB_NAME
  fi
done
"
echo "Finished migrating database"

# TODO: use gsutil cp instead
#echo "Copying files locally"
#rm -rf tmp-files
#mkdir tmp-files
#kubectl cp -c php $sourcePodName:web/sites/default/files tmp-files
#echo "Copying files to new environment"
#kubectl cp tmp-files $podName:web/sites/default/files -c php
#echo "Finished copying files to new environment"

# Reindex elasticsearch indices.
kubectl exec -it -c php -n $namespace $podName -- bash -c '
if drush eshl
then
    drush queue:delete elasticsearch_helper_indexing
    for index in $(drush eshl | grep -o -E "| .* |" | cut -f 1 -d "|" | grep -v " id "); do
      echo $index;
      drush eshs $index;
      drush eshr $index;
    done
    drush queue-run elasticsearch_helper_indexing
fi
'