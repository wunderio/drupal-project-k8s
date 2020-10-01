#!/usr/bin/env bash

# Set this to an empty string to actually run release upgrades.
dryRun="--dry-run"

function upgrade_helm {
  namespace=$1
  releaseName=$2
  echo "Upgrading $releaseName in $namespace"

  helmHistoryJson=$(helm history -n $namespace $releaseName -o json --max 1)
  chartVersion=$(echo "$helmHistoryJson" | jq -r '.[].chart')
  chartName=$(printf "$chartVersion" | sed -E "s/\-[0-9\-\.]*$//")

  if [[ "$chartName" = "drupal" ]];
  then
      # Some resources need to be deleted first.
      # kubectl delete statefulset -n $namespace $releaseName-mariadb --cascade=false --ignore-not-found=true

      # If any parameters need to be added for the upgrade to be successful, please use the --set parameter.
      helm upgrade -n $namespace $releaseName wunderio/drupal \
        --no-hooks --reuse-values \
        --cleanup-on-fail $dryRun
  fi

  if [[ "$chartName" = "frontend" ]];
  then
      helm upgrade -n $namespace $releaseName wunderio/frontend \
        --no-hooks --reuse-values \
        --cleanup-on-fail $dryRun
  fi

  if [[ "$chartName" = "simple" ]];
  then
      helm upgrade -n $namespace $releaseName wunderio/simple \
        --no-hooks --reuse-values \
        --cleanup-on-fail $dryRun
  fi
}

for namespace in $(kubectl get ns -o custom-columns=name:metadata.name); do
  for releaseName in $(helm list -n "$namespace" --short); do
    upgrade_helm $namespace $releaseName
  done
done