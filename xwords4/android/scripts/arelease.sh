#!/bin/bash

usage() {
    echo "Error: $*"
    echo "usage: $0 [--tag <name>] [<package-unsigned.apk>]" >&2
    exit 1
}

do_build() {
    WD=$(pwd)
    cd $(dirname $0)/../XWords4/
    pwd
    touch jni/Android.mk
    ../scripts/ndkbuild.sh
    rm -rf bin/ gen/
    ant release
    cd $WD
}

while [ -n "$1" ]; do
    case $1 in
        --tag) TAGNAME=$2
            git describe $TAGNAME || usage "$TAGNAME not a valid git tag"
            shift
            ;;
        *)
            FILES="$1"
            ;;
    esac
    shift
done

if [ -n "$TAGNAME" ]; then
    git branch 2>/dev/null | grep '\* android_branch' \
        || usage "not currently at android_branch"
    git checkout $TAGNAME 2>/dev/null || usage "unable to checkout $TAGNAME"
    HASH=$(git log -1 --pretty=format:%H)
    CHECK_BRANCH=$(git describe $HASH 2>/dev/null)
    if [ "$CHECK_BRANCH" != $TAGNAME ]; then
        usage "tagname not found in repo or not as expected"
    fi
    git stash
fi

if [ -z "$FILES" ]; then
    do_build
    FILES=$(ls $(dirname $0)/../*/bin/*-unsigned.apk)
    if [ -z "$FILES" ]; then
        echo "unable to find any unsigned packages" >&2
        usage
    fi
fi

for PACK_UNSIGNED in $FILES; do
    echo $FILE

    PACK_SIGNED=$(basename $PACK_UNSIGNED)
    echo "base: $PACK_SIGNED"
    PACK_SIGNED=${PACK_SIGNED/-unsigned}
    echo "signed: $PACK_SIGNED"
    jarsigner -verbose -keystore ~/.keystore $PACK_UNSIGNED mykey
    rm -f $PACK_SIGNED
    zipalign -v 4 $PACK_UNSIGNED $PACK_SIGNED
    [ -n "$XW_WWW_PATH" ] && cp $PACK_SIGNED $XW_WWW_PATH
    cp $PACK_SIGNED ${PACK_SIGNED%.apk}_$(git describe).apk
done

if [ -n "$TAGNAME" ]; then
    git stash pop
    git checkout android_branch 2>/dev/null
fi
