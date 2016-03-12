#!/bin/sh

set -u -e

DIR=""

usage() {
    [ $# -ge 1 ] && echo "ERROR: $1"
    echo "usage: $0 [--dir <dir>]"
    echo "   uses variant parent of CWD as default if not provided"
    exit 1
}

while [ $# -ge 1 ]; do
    echo "\"$1\""
    case $1 in
        --dir)
            shift
            DIR="$DIR $1"
            ;;
        *)
            usage "unexpected param $1"
            ;;
    esac
    shift
done

if [ -z "$DIR" ]; then
    while :; do
        WD=$(pwd)
        if [ "/" = "$WD" ]; then
            echo "reached / without finding AndroidManifest.xml"
            exit 1
        elif [ -e ${WD}/AndroidManifest.xml ]; then
            DIR=$WD
            break
        else
            cd ..
        fi
    done
fi

for FILE in $(find $DIR -type f); do
    if git ls-files $FILE --error-unmatch 2>/dev/null; then
        echo "skipping $FILE; it's under version control within this variant"
    else
        echo "removing $FILE"
        rm $FILE
    fi
done
