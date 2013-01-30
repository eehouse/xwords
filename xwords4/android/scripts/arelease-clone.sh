#!/bin/sh

set -e -u

usage () {
    echo "usage: $(basename $0) [--tag tagname | --branch branchname] [--variant variant]"
    echo "   # (uses current branch as default)"
    exit 0
}

getSDK() {
    LINE=$(grep 'android:minSdkVersion' ./AndroidManifest.xml)
    SDK=$(echo $LINE | sed 's/^.*targetSdkVersion=\"\([0-9]*\)\".*$/\1/')
    echo $SDK
}

TAG=""
BRANCH=""
VARIANT="XWords4"

if [ -f ./AndroidManifest.xml ]; then # we're in the right directory
    VARIANT=$(basename $(pwd))
fi

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
        --variant)
            VARIANT=$2
            shift
            ;;
        *)
            usage
            ;;
    esac
    shift
done

echo "VARIANT=$VARIANT"

if [ -n "$TAG" ]; then
    if ! git tag | grep -w "$TAG"; then
        echo "tag $TAG not found"
        usage
    fi
elif [ -z $BRANCH ]; then
    BRANCH=$(git branch | grep '^*' | sed 's,^.* ,,')
fi

echo "building $VARIANT with ${TAG}${BRANCH}"

BUILDIR=/tmp/$(basename $0)_build_$$
SRCDIR=$(pwd)/$(dirname $0)/../../../

CURDIR=$(pwd)

mkdir -p $BUILDIR
cd $BUILDIR
git clone $SRCDIR BUILD
cd BUILD
git checkout ${TAG}${BRANCH}
cd ./xwords4/android/${VARIANT}
../scripts/setup_local_props.sh --target android-$(getSDK)
../scripts/arelease.sh --variant ${VARIANT}
mkdir -p /tmp/releases_${VARIANT}
cp *.apk /tmp/releases_${VARIANT}

cd $CURDIR
rm -rf $BUILDIR
