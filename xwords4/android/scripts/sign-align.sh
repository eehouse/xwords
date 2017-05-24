#!/bin/bash

set -u -e

APKS=''
XW_WWW_PATH=${XW_WWW_PATH:-''}
LIST_FILE=1

usage() {
	[ $# -gt 0 ] && echo "ERROR: $1"
	echo "usage: $0 [--apk path/to/unsigned.apk]*"
	}

while [ $# -gt 0 ]; do
	case $1 in
		--apk)
			[ -e $2 ] || usage "no such file $2"
			APKS="$APKS $2"
			shift
			;;
		*)
			usage "Unexpected flag $1"
			;;
	esac
	shift
done

for APK in $APKS; do
	if [ ${APK/-unsigned} == $APK ]; then
		echo "$APK not unsigned; skipping"
		continue
	fi
    APK_SIGNED=/tmp/$$_tmp.apk
	cp $APK $APK_SIGNED
    jarsigner -verbose -digestalg SHA1 -keystore ~/.keystore $APK_SIGNED mykey
	rm -f ${APK/-unsigned/-signed}
    zipalign -v 4 $APK_SIGNED ${APK/-unsigned/-signed}
	rm -f $APK_SIGNED
done
