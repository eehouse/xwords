#!/bin/sh

set -u -e

MAKEFILE=./Variant.mk
DIRS=""
VARIANT=""

usage() {
    [ $# -ge 1 ] && echo "ERROR: $1"
    echo "usage: $0 [--dest-dir <dir>]* --variant-name <dir>"
    exit 1
}

do_dir() {
    local SRC_PATH=$1
    local DEST_PATH=$2
    local DIR=$3

    SRC_PATH=$SRC_PATH/$DIR

    [ -d $SRC_PATH ] || usage "$SRC_PATH not found"
    DEST_PATH=$DEST_PATH/$DIR
    mkdir -p $DEST_PATH

    for FILE in $SRC_PATH; do
        if [ -d $FILE ]; then
            do_dir $SRC_PATH $DEST_PATH $FILE
        else
            make -f $MAKEFILE SRC_PATH=$SRC_PATH $DEST_PATH=$DEST_PATH make_file
        fi
    done
}

pwd
echo "$0 $*"

while [ $# -ge 1 ]; do
    echo "\"$1\""
    case $1 in
        --variant-name)
            shift
            VARIANT=$1
            ;;
        --dest-dir)
            shift
            DIRS="$DIRS $1"
            ;;
        *)
            usage "unexpected param $1"
            ;;
    esac
    shift
done

echo "$0 DIRS: $DIRS"

[ -n "$VARIANT" ] || usage "--variant-name not supplied"
