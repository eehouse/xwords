#!/bin/sh

set -e -u -x

MANIFEST=AndroidManifest.xml
# XWORDS=org.eehouse.android.xw4
INSTALL=''
UNINSTALL=''

usage() {
    [ $# -ge 1 ] && echo "ERROR: $1"
    echo "usage: $0 [--install] [--reinstall] <cmds to gradle>"
    exit 1
}

uninstall() {
	adb devices | while read DEV TYPE; do
		case "$TYPE" in
			device)
				adb -s $DEV uninstall $XWORDS
				;;
			emulator)
				adb -s $DEV uninstall $XWORDS
				;;
		esac
	done
}

while [ $# -gt 0 ]; do
    case $1 in
		--help|-h|-help|-?)
			usage
			;;
		--install)
			INSTALL=1
			;;
		--reinstall)
			UNINSTALL=1
			INSTALL=1
			;;
		*)
			break
			;;
    esac
    shift
done

while [ ! -e ./gradlew ]; do
    [ '/' = $(pwd) ] && usage "reached root without finding gradlew"
    cd ..
done

NOW_FILE=/tmp/NOW_$$
touch $NOW_FILE

# If this fails, the "set -e" above means we won't try to install anything
./gradlew $*

# Find the newest apk in the build output directory that's newer than our timestamp
APK=''
APKS=$(find app/build/outputs/apk/ -name '*.apk' -a -newer $NOW_FILE)
if [ -n "$APKS" ]; then
	APK=$(ls -t $APKS | head -n 1)
fi
[ -n "$APK" ] || usage "no new .apk files found; did the build succeed?"

if [ -n "$UNINSTALL" ]; then
	LINE=$(aapt dump badging $APK | grep '^package')
	PACKAGE=$(echo $LINE | sed "s,.*name='\([^' ]*\)'.*,\1,")
	adb shell pm uninstall $PACKAGE
fi

if [ -n "$INSTALL" ]; then
	adb-install.sh -p $APK
fi
