#!/bin/bash

set -e -u

if [ ! -e build.xml ]; then
    echo "no build.xml; please run from root of source tree"
    exit 1
fi

CLEAN=""

if [ $# -gt 1 ]; then
    if [ $1 = '--clean' ]; then
        CLEAN=1
    fi
fi

# There needs to be target in the makefile for each of these (giving
# the output .png size)

TARGET_DIRS="drawable-hdpi drawable-mdpi drawable-xhdpi"


for SVG in img_src/*.svg; do
    for DIR in $TARGET_DIRS; do
        SVG=$(basename $SVG)
        OUT=res/$DIR/${SVG/.svg/__gen.png}
        if [ -z "$CLEAN" ]; then
            make -f $(dirname $0)/images.mk $OUT
        else
            rm -f $OUT
        fi
    done
done
