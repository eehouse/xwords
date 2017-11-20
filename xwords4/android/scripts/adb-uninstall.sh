#!/bin/bash

set -e -u

# use this: adb -s 04bd25af2523aae6 shell "pm  list packages" | grep org.eehouse

declare -A PACKAGES=()
declare -A SERIALS=()
INDEX=''
DRYRUN=''
ALL_DEVS=''
AAPT=''

usage() {
	[ $# -gt 0 ] && echo "ERROR: $1"
    echo "usage: $0 [--dry-run] [--help] [--all-devs]"
	echo "    [--apk <path/to/apk>]  # default is to use package names of all known apks"
	echo "    [--pkg-name tld.whatever.more] # explicit package name"
	echo "    [--serial <serial>]    # default is to use all attached devices"
    echo "uninstall apps based on known .apk or package name"
    exit 1
}

getPackage() {
	APK=$1
	PACK=$($AAPT dumb badging $APK | grep '^package: ' | sed "s,^.*name='\([^']*\)'.*\$,\1,")
	echo $PACK
}

# FIXME: not all options require a working directory, e.g. --apk
WD=$(pwd)
while :; do
	if [ -e ${WD}/AndroidManifest.xml -a -e ${WD}/build.xml ]; then
		break
	elif [ -e ${WD}/app/build.gradle ]; then
		break
	elif [ ${WD} = '/' ]; then
		usage "reached / without finding AndroidManifest.xml or build.gradle"
	else
		WD=$(cd $WD/.. && pwd)
	fi
done
WD=$(cd $WD && pwd)

# find aapt
DIR=$(dirname $(which android))
DIR=$DIR/../build-tools
for F in $(ls -c $DIR/*/aapt); do
	[ -e $F ] && AAPT=$F && break
done
[ -n "$AAPT" -a -e "$AAPT" ] || usage "aapt not found; is android on your PATH?"

while [ $# -ge 1 ]; do
    case $1 in
		--apk)
			[ -e $2 ] || usage "apk $2 not found"
			PACKAGES[$(getPackage $2)]=1
			shift
			;;
		--pkg-name)
			PACKAGES[$2]=1
			shift
			;;
		--serial)
			SERIALS[$2]=1
			shift
			;;
		--dry-run)
			DRYRUN=1
			;;
		--all-devs)
			ALL_DEVS=1
			;;
		--help)
			usage
			;;
        *) usage "Unexpected parameter $1"
            ;;
    esac
    shift
done

# No packages specified? Use all we know about!
if [ 0 = "${#PACKAGES[*]}" ]; then
	echo "no apks specified; assuming all" >&2
	for f in $(find $WD -name '*.apk'); do
		PACK=$(getPackage $f)
		PACKAGES[$PACK]=1
	done
fi

if [ 0 = "${#SERIALS[*]}" ]; then
	echo "no serials specified; assuming all connected devices" >&2
	for DEV in $(adb devices | grep '\sdevice$' | awk '{print $1}'); do
		SERIALS[$DEV]=1
	done

	if [ 0 = "${#SERIALS[*]}" ]; then
		usage "no devices found"
	elif [ 1 -lt "${#SERIALS[*]}" -a -z "$ALL_DEVS" ]; then
		 usage "More than one device found. Be specific, or use --all-devs"
	fi
fi

for PACKAGE in "${!PACKAGES[@]}"; do
	for SERIAL in "${!SERIALS[@]}"; do
		CMD="adb -s $SERIAL shell pm uninstall $PACKAGE"
		echo "$CMD" >&2
		[ -z "$DRYRUN" ] && $CMD
	done
done
