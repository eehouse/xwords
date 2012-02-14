#!/bin/sh

set -e -u

if [ ! -e build.xml ]; then
    echo "no build.xml; please run from root of source tree"
    exit 1
fi

if [ -z "$NDK_ROOT" ]; then
    echo -n "NDK_ROOT not set... "
    echo "NDK not found; install and set NDK_ROOT to point to it"
    exit 1
fi

${NDK_ROOT}/ndk-build $*

echo "$0 done"
