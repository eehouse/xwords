#!/bin/sh

OLDDIR=$(pwd)

if [ -z "$NDK_ROOT" ]; then
    echo -n "NDK_ROOT not set... "
    echo "NDK not found; install and set NDK_ROOT to point to it"
    exit 1
fi

cd $(dirname $0)/../XWords4

${NDK_ROOT}/ndk-build $*

cd $OLDDIR
echo "$0 done"
