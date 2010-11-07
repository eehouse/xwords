#!/bin/sh

set -e -u

usage () {
    echo "usage: $(basename $0) [--tag tagname | --branch branchname]"
    echo "   # (uses current branch as default)"
    exit 0
}

TAG=""
BRANCH=""

while [ 0 -lt $# ] ; do
    case $1 in
        --tag)
            TAG=$2
            shift
            ;;
        --branch)
            BRANCH=$2
            shift
            ;;
        *)
            usage
            ;;
    esac
    shift
done

if [ -n "$TAG" ]; then
    if ! git tag | grep -w "$TAG"; then
        echo "tag $TAG not found"
        usage
    fi
elif [ -z $BRANCH ]; then
    BRANCH=$(git branch | grep '^*' | sed 's,^.* ,,')
fi

echo "building with ${TAG}${BRANCH}"

BUILDIR=/tmp/$(basename $0)_build_$$
SRCDIR=$(pwd)/$(dirname $0)/../../../

CURDIR=$(pwd)

mkdir -p $BUILDIR
cd $BUILDIR
git clone $SRCDIR BUILD
cd BUILD
git checkout ${TAG}${BRANCH}
./xwords4/android/scripts/arelease.sh
cp *.apk /tmp

cd $CURDIR
rm -rf $BUILDIR
