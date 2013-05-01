#!/bin/sh

set -e -u

INDEX=0

usage() {
    echo "usage: $0 [--help] [-n <index>]"
    echo "uninstall crosswords from the <index>th device"
    exit 0
}

while [ $# -ge 1 ]; do
    case $1 in
        -n)
            shift
            INDEX=$1
            ;;
        *) usage
            ;;
    esac
    shift
done

SERIAL="$(adb devices | grep 'device$' | sed -n  "$((1+INDEX)) p" | awk '{print $1}')"

adb -s $SERIAL uninstall org.eehouse.android.xw4
