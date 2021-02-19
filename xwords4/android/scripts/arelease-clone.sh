#!/bin/sh

set -e -u

VARIANT=Xw4Foss

usage () {
    echo "usage: $(basename $0) [--tag tagname | --branch branchname] \\"
    echo "    [--variant VARIANT] # default value: $VARIANT \\"
    echo "   # (uses current branch as default)"
    echo "   # e.g. $0 --tag android_beta_141 --variant Xw4d"
    exit 1
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
OUT_FILE=$BUILDIR/apks.txt
SRCDIR=$(pwd)/$(dirname $0)/../../../

CURDIR=$(pwd)

mkdir -p $BUILDIR
cd $BUILDIR
git clone $SRCDIR BUILD
cd BUILD
git checkout ${TAG}${BRANCH}
cd ./xwords4/android/
./scripts/arelease.sh --apk-list $OUT_FILE --variant $VARIANT
mkdir -p /tmp/releases
cp app/build/outputs/apk/*/release/*.apk /tmp/releases

if [ -n "$XW_RELEASE_SCP_DEST" ]; then
	cat $OUT_FILE | while read APK; do
		echo "running: scp /tmp/releases/$APK $XW_RELEASE_SCP_DEST ..."
		scp /tmp/releases/$APK $XW_RELEASE_SCP_DEST
	done
fi

cd $CURDIR
echo "remove build dir $BUILDIR? (y or n):"
echo -n "(y or n) ==> "
read ANSWER
if [ "$ANSWER" = 'y' ]; then
	rm -rf $BUILDIR
	echo "removed $BUILDIR"
else
	echo "$BUILDIR not removed"
fi
