#!/bin/bash

set -u -e

usage() {
    [ $# -ge 1 ] && echo "Error: $1"
    echo "usage: $(basename $0) [-e|-d] (-p /path/to/.apk)+"
    exit 1
}

if [ ! -e build.xml ]; then
    usage "No build.xml; please run me from the top android directory"
fi

APKS=''
DEVICES=''
DIRNAME=$(basename $(pwd))
ADB="$(which adb)"
MAIN=MainActivity

PKG=$(xmlstarlet sel -T -t -m "/manifest" -v @package -n AndroidManifest.xml)

while [ $# -ge 1 ]; do
    case $1 in
        -e)
            DEV="$($ADB devices | grep '^emulator' | awk '{print $1}')"
            DEVICES="$DEVICES $DEV"
            ;;
        -d)
            DEV="$($ADB devices | grep -v emulator | grep 'device$' | awk '{print $1}')"
            DEVICES="$DEVICES $DEV"
            ;;
        -p)
            [ $# -gt 1 ] || usage "-p requires an argument"
            shift
            APKS="$APKS $1"
            ;;
        *)
            usage
            ;;
    esac
    shift
done

if [ -z "$DEVICES" ]; then
	while read LINE; do
		if echo $LINE | grep -q "device$"; then
			DEVICE=$(echo $LINE | awk '{print $1}')
			DEVICES="$DEVICES $DEVICE"
		fi
	done <<< "$($ADB devices)"
fi

# If no apk specified, take the newest built
if [ -z "$APKS" ]; then
	APKS=$(ls -t bin/*.apk | head -n 1)
fi

COUNT=0
for DEVICE in $DEVICES; do
	for APK in $APKS; do
		echo "installing $APK; details:"
		ls -l $APK
		$ADB -s $DEVICE install -r $APK
		$ADB -s $DEVICE shell am start \
			 -n ${PKG}/${PKG}.${MAIN}
	done
	COUNT=$((COUNT+1))
done

echo "installed into $COUNT devices/emulators"
