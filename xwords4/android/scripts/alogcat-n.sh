#!/bin/sh

set -u -e

INDX=1

usage() {
    [ $# -ge 1 ] && echo "ERROR: $1"
    echo "usage: $(basename $0) [-n <1-based-indx>]"
    exit 1
}

while [ $# -ge 1 ]; do
    case $1 in
        -n) shift
            [ $# -ge 1 ] || usage "-n requires a parameter"
            INDX=$1
            ;;
        *)
            usage
            ;;
    esac
    shift
done

DEVICE=$(adb devices | grep 'device$' | awk '{print $1}' | sed -n "${INDX}p")
adb -s $DEVICE logcat
