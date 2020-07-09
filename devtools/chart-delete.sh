if [ -z "$1" ]; then
    echo "! Provide release namespace as the first argument"
    kubectl get ns
    exit 1;
fi

NAMESPACE=$1

if [ -z "$2" ]; then
  echo "! Provide release name as the second argument"
  helm list --all -n $NAMESPACE
  exit 1;
fi

RELEASE_NAME=$2

echo  "* Deleting jobs associated with $RELEASE_NAME"
kubectl delete job -l release=$RELEASE_NAME -n $NAMESPACE --ignore-not-found=true

echo "* Deleting release $RELEASE_NAME"
helm delete -n $NAMESPACE $RELEASE_NAME

echo "* Delete leftover pvcs from statefulsets"
kubectl delete pvc -l release=$RELEASE_NAME -n $NAMESPACE --ignore-not-found=true
kubectl delete pvc -l app="${RELEASE_NAME}-es" -n $NAMESPACE --ignore-not-found=true

echo "* Removing any additional resources tagged with this release"
kubectl delete all -l release=$RELEASE_NAME -n $NAMESPACE --ignore-not-found=true
