#!/usr/bin/env bash

# Delete all helm charts whose name contains "test-project".
helm ls | grep test-project | cut -f 1 | xargs -n 1 ./chart-delete.sh

# Also delete the associated namespaces.
kubectl get ns -o name | grep test-project | xargs -n 1 kubectl delete
