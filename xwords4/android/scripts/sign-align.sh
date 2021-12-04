#!/bin/bash

set -u -e

APKS=''
XW_WWW_PATH=${XW_WWW_PATH:-''}
LIST_FILE=1

usage() {
	[ $# -gt 0 ] && echo "ERROR: $1"
	echo "usage: $0 [--apk path/to/unsigned.apk]+"
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

[ -z "$APKS" ] && usage "no apks provided"

for APK in $APKS; do
    APK_SIGNED=/tmp/$$_tmp.apk
	cp $APK $APK_SIGNED

    jarsigner -verbose -digestalg SHA1 -keystore ~/.keystore $APK_SIGNED mykey
	if [ ${APK/-unsigned} == $APK ]; then
		OUTNAME=${APK/.apk/.signed.apk}
	else
		OUTNAME=${APK/-unsigned/-signed}
	fi
	rm -f ${OUTNAME}
    zipalign -v 4 $APK_SIGNED ${OUTNAME}

	apksigner sign --ks ~/.keystore ${OUTNAME}

	rm -f $APK_SIGNED
	echo "saved as $OUTNAME"
done
