#!/bin/bash

set -e -u

usage() {
	[ $# -gt 0 ] && echo "ERROR: $1"
	echo "usage: $0 [--with-fdroid]"
	echo "builds all variants except fdroid since that build will fail outside their system"
	exit 1
}

VARIANTS="xw4NoSMS xw4d xw4dNoSMS"
WD=$(cd $(dirname $0)/../ && pwd)

while [ $# -gt 0 ]; do
	case $1 in
		--with-fdroid)
			VARIANTS="$VARIANTS xw4fdroid"
			;;
		*)
			usage "unexpected param $1"
			;;
	esac
	shift
done

for VARIANT in $VARIANTS; do
	echo "***** building $VARIANT *****"
	VARIANT=$(echo ${VARIANT:0:1} | tr '[:lower:]' '[:upper:]')${VARIANT:1}
	TARGET=as${VARIANT}Deb
	(cd $WD && ./gradlew clean $TARGET)
done
