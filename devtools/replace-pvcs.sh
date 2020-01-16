#!/usr/bin/env bash


release="test-recreate-service"
namespace="drupal-project"

set -e

pvc_names=$(kubectl get pvc -n $namespace -l release=$release | grep silta-shared | tr -s ' ' | cut -f 1 -d ' ')
pv_names=$(kubectl get pvc -n $namespace -l release=$release | grep silta-shared | tr -s ' ' | cut -f 3 -d ' ')

kubectl delete pvc -n $namespace $pvc_names &
kubectl delete pv $pv_names &
kubectl delete jobs -n $namespace --all &
kubectl delete pods -n $namespace -l app=drupal,release=$release &

until [[ -z $(kubectl get pvc -n $namespace -l release=$release | grep silta-shared) ]]
do
  echo -n "."
  sleep 1
done

~/Downloads/helm3 upgrade -n $namespace $release wunderio/drupal --no-hooks --reuse-values

kubectl get deployment -n "$namespace" -l "release=$release" -o name | xargs -n 1 kubectl rollout status -n "$namespace"