#!/bin/sh

BUILDIR=/tmp/$(basename $0)_build_$$
CURDIR=$(pwd)
SRCDIR=${CURDIR}/../../../

echo $BUILDIR
echo $SRCDIR
ls $SRCDIR

mkdir -p $BUILDIR
cd $BUILDIR
git clone $SRCDIR BUILD
cd BUILD
git checkout android_branch 
./xwords4/android/scripts/arelease.sh $*
cp *.apk /tmp

cd $CURDIR
rm -rf $BUILDIR
