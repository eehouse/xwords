#!/bin/bash

set -e -u

if [ ! -d img_src ]; then
    echo "no img_src; please run from root of source tree"
    exit 1
fi

CLEAN=""

usage() {
    echo "usage: $0 [--clean]"
    exit 1
}

while [ $# -ge 1 ]; do
    case $1 in
        --clean)
            CLEAN=1
            ;;
        *)
            usage
            ;;
    esac
    shift
done

# There needs to be target in the makefile for each of these (giving
# the output .png size)

TARGET_DIRS="drawable-hdpi drawable-mdpi drawable-xhdpi"

for SVG in img_src/*.svg; do
    for DIR in $TARGET_DIRS; do
        SVG=$(basename $SVG)
        OUT=app/src/main/res/$DIR/${SVG/.svg/__gen.png}
        mkdir -p $(dirname $OUT)
        if [ -z "$CLEAN" ]; then
            make -f $(dirname $0)/images.mk $OUT >/dev/null 2>&1
        else
            rm -f $OUT
        fi
    done
done

OTHER_IMAGES="app/src/main/res/drawable/green_chat__gen.png"
for IMAGE in $OTHER_IMAGES; do
	make -f $(dirname $0)/images.mk $IMAGE >/dev/null 2>&1
done
