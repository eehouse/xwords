#!/bin/sh

set -e -u

INDEX=0
DRYRUN=''

usage() {
	[ $# -gt 0 ] && echo "ERROR: $1"
    echo "usage: $0 [--dry-run] [--help] [-n <index>]"
    echo "uninstall crosswords from the <index>th device"
    exit 0
}

while :; do
	WD=$(pwd)
	if [ -e ${WD}/AndroidManifest.xml ]; then
		break
	elif [ ${WD} = '/' ]; then
		usage "reached / without finding AndroidManifest.xml"
	else
		cd ..
	fi
done

PACK=$(grep 'package=\".*\..*\.*\"' ${WD}/AndroidManifest.xml | sed 's,^.*package="\(.*\)".*$,\1,')

if [ -z "${PACK}" ]; then
	usage "unable to find package in ${WD}/AndroidManifest.xml"
fi

while [ $# -ge 1 ]; do
    case $1 in
		--dry-run)
			DRYRUN=1
			;;
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

echo "adb -s $SERIAL uninstall $PACK"
[ -z "$DRYRUN" ] && adb -s $SERIAL uninstall $PACK
