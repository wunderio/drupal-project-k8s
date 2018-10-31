if [ -z "$1" ]; then
    echo "! Provide branch name as argument"
    helm list --all
    exit 1;
fi

if [ $2 ]; then
    NAMESPACE=$2
else
  # Get the namespace from the helm chart
  NAMESPACE=`helm ls | grep $1 | cut -f 7`
fi

if [ $NAMESPACE ]; then
    echo Deleting release $1 in the namespace $NAMESPACE

    echo "* Deleting deployment"
    helm delete --purge $1

    echo "* Removing all resources tagged with this release"
    kubectl delete all -l release=$1 -n $NAMESPACE

    echo "* Removing all jobs tagged with this release"
    kubectl delete job -l release=$1 -n $NAMESPACE

    echo "* Removing all persistent volume claims tagged with this release"
    kubectl delete pvc -l release=$1 -n $NAMESPACE
else
  echo "! No namespace could be found, provide it a as a parameter."
fi