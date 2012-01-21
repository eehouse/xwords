#!/bin/sh

set -u -e

APK=./bin/XWords4-debug.apk

usage() {
    [ $# -ge 1 ] && echo "Error: $1"
    echo "usage: $(basename $0) [-e] [-d]"
    exit 1
}

[ -e $APK ] || usage "$APK not found"

DEVICES=""

while [ $# -ge 1 ]; do
    case $1 in
        -e)
            DEVICES="$DEVICES $(adb devices | adb devices | grep '^emulator' | awk '{print $1}')"
            ;;
        -d)
            DEVICES="$DEVICES $(adb devices | grep -v emulator | grep 'device$' | awk '{print $1}')"
            ;;
        *)
            usage
            ;;
    esac
    shift
done

if [ -n "$DEVICES" ]; then
    echo "installing this binary.  Check the age..."
    ls -l $APK
    echo ""
fi

COUNT=0

for DEVICE in $DEVICES; do
    echo $DEVICE
    adb -s $DEVICE install -r $APK
    COUNT=$((COUNT+1))
done

echo "installed into $COUNT devices/emulators"
