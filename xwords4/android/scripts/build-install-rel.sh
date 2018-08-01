#!/bin/sh

set -e -u

usage() {
	[ $# -gt 0 ] && echo "ERROR: $1"
	echo "usage: $0 [--clean|--help]"

	echo "meant for rapid work where a release build's required, this"
	echo "builds, signs, and installs a Rel build of the xw4 variant"
	exit 1
}

CLEAN=''

while [ $# -gt 0 ]; do
	case $1 in
		--clean)
			CLEAN=clean
			;;
		--help)
			usage
			;;
		*)
			usage "unexpected command $1"
			;;
	esac
	shift
done

cd $(dirname $0)/..
pwd

./gradlew $CLEAN asXw4Rel
./scripts/sign-align.sh --apk $(find app/build -name '*xw4-release-unsigned-*.apk')
adb install -r $(find app/build -name '*xw4-release-signed-*.apk')
