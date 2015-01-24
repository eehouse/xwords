#!/bin/sh

set -e -u

INDEX=0

usage() {
    echo "usage: $0 [--help] [-n <index>]"
    echo "uninstall crosswords from the <index>th device"
    exit 0
}

if [ ! -e build.xml ]; then
    usage "No build.xml; please run me from the top android directory"
fi

DIRNAME=$(basename $(pwd))
case $DIRNAME in
    XWords4-bt)
        PKG=xw4bt
        ;;
    XWords4)
        PKG=xw4
        ;;
    *)
        usage "running in unexpected directory $DIRNAME"
        ;;
esac

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

adb -s $SERIAL uninstall org.eehouse.android.${PKG}
