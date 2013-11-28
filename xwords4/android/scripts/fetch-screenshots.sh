#!/bin/bash

set -e -u

LIST=""
ALL=""
FILE=""
DIR="/sdcard/Pictures/Screenshots"
WD=$(pwd)

usage() {
    [ $# -ge 1 ] && echo "Error: $1"
    echo "usage: $0 --help | --list | <name.png>"
    exit 1
}

pullFile() {
    echo "pulling $1"
    adb pull ${DIR}/$1 ${WD}/
}

[ $# -eq 1 ] || usage "Requires a single parameter"

while [ $# -ge 1 ]; do
    echo $1
    case $1 in
        --list)
            LIST=1
            ;;
        --help)
            usage
            ;;
        *)
            FILE=$1
            ;;
    esac
    shift
done

SHOTS=$(adb shell "ls $DIR")

if [ -n "$FILE" ]; then
    pullFile ${FILE}
elif [ -n "$LIST" ]; then
    for SHOT in $SHOTS; do
        echo $SHOT
    done
fi

