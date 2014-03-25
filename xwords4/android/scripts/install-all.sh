#!/bin/sh

set -u -e

usage() {
    [ $# -ge 1 ] && echo "Error: $1"
    echo "usage: $(basename $0) [-e|-d] [-p /path/to/.apk]"
    exit 1
}

if [ ! -e build.xml ]; then
    usage "No build.xml; please run me from the top android directory"
fi

APK=./bin/XWords4-debug.apk
DIRNAME=$(basename $(pwd))
case $DIRNAME in
    XWords4-dbg)
        PKG=xw4dbg
        ;;
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

DEVICES=''

while [ $# -ge 1 ]; do
    case $1 in
        -e)
            DEV="$(adb devices | grep '^emulator' | awk '{print $1}')"
            DEVICES="$DEVICES $DEV"
            ;;
        -d)
            DEV="$(adb devices | grep -v emulator | grep 'device$' | awk '{print $1}')"
            DEVICES="$DEVICES $DEV"
            ;;
        -p)
            [ $# -gt 1 ] || usage "-p requires an argument"
            shift
            APK=$1
            ;;
        *)
            usage
            ;;
    esac
    shift
done

[ -e $APK ] || usage "$APK not found"

if [ -n "$DEVICES" ]; then
    echo "installing this binary.  Check the age..."
    ls -l $APK
    echo ""
fi

COUNT=0

for DEVICE in $DEVICES; do
    echo $DEVICE
    adb -s $DEVICE install -r $APK
    adb -s $DEVICE shell am start \
        -n org.eehouse.android.${PKG}/org.eehouse.android.${PKG}.GamesListActivity
    COUNT=$((COUNT+1))
done

echo "installed into $COUNT devices/emulators"
