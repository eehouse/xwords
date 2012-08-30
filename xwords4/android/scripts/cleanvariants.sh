#!/bin/sh

set -u -e

DIRS=""
VARIANT=""

usage() {
    [ $# -ge 1 ] && echo "ERROR: $1"
    echo "usage: $0 [--dest-dir <dir>]*"
    exit 1
}

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

for DIR in $DIRS; do
    for FILE in $(find $DIR -type f); do
        if git ls-files $FILE --error-unmatch 2>/dev/null; then
            echo "skipping $FILE; it's under version control within this variant"
        else
            rm $FILE
        fi
    done
done
