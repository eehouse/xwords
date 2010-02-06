#!/bin/sh

APP=xwords4

OLDDIR=$(pwd)

if [ -z "$NDK_ROOT" ]; then
    echo -n "NDK_ROOT not set... "
    NDK_ROOT="$HOME/android-ndk-1.6_r1"
    if [ -d $NDK_ROOT ]; then
        echo "using $NDK_ROOT"
    else
        echo "NDK not found; install and set NDK_ROOT to point to it"
        exit 1
    fi
fi

cd $(dirname $0)
cd ../

if [ -h $NDK_ROOT/apps/$APP -a $(readlink $NDK_ROOT/apps/$APP) != $(pwd) ]; then
    rm $NDK_ROOT/apps/$APP
fi
if [ ! -h $NDK_ROOT/apps/$APP ]; then
    echo "adding symlink to apps"
    ln -sf $(pwd) $NDK_ROOT/apps/$APP
fi

cd $NDK_ROOT
make -j3 APP=$APP $*

cd $OLDDIR
echo "$0 done"
