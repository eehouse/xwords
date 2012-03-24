#!/bin/bash

set -u -e

MAKEFILE=$(dirname $0)/Variant.mk
DIRS=""
VARIANT=""

usage() {
    [ $# -ge 1 ] && echo "ERROR: $1"
    echo "usage: $0 [--dest-dir <dir>]* --variant-name <dir>"
    exit 1
}

add_to_gitignored() {
    DIR=$1
    FILE=$2
    if [ -n "${FILE/*~/}" ]; then
        touch ${DIR}/.gitignore
        grep -q "^${FILE}\$" ${DIR}/.gitignore || echo $FILE >> ${DIR}/.gitignore
    fi
}

do_dir() {
    local SRC_PATH=$1
    local DEST_PATH=$2
    local SRC_DIR=$3
    local DEST_DIR=$SRC_DIR
    if [ $SRC_DIR = "xw4" ]; then
        DEST_DIR=$VARIANT
    fi

    SRC_PATH=$SRC_PATH/$SRC_DIR
    [ -d $SRC_PATH ] || usage "$SRC_PATH not found"

    DEST_PATH=$DEST_PATH/$DEST_DIR
    mkdir -p $DEST_PATH

    for FILE in $SRC_PATH/*; do
        if [ -d $FILE ]; then
            do_dir $SRC_PATH $DEST_PATH $(basename $FILE)
        else
            FILE=${FILE/$SRC_PATH/$DEST_PATH}
            if git ls-files $FILE --error-unmatch 2>/dev/null; then
                echo "skipping $FILE; it's under version control within this variant"
            else
                make -f $MAKEFILE SRC_PATH=$SRC_PATH DEST_PATH=$DEST_PATH \
                    VARIANT=${VARIANT} $FILE
                add_to_gitignored $DEST_PATH $(basename $FILE)
            fi
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

for DIR in $DIRS; do
    do_dir ../XWords4 . $DIR
done
