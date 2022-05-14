#!/bin/bash

set -e -u

TMPDIR=/tmp/ci-build-$$

usage() {
	[ -n "$1" ] && echo "ERROR: $1"
	echo "**************************************"
	echo "*** Nobody but me will use this!!! ***"
	echo "**************************************"
	echo "usage: $0 <no args>"
	echo "does a clean build of committed HEAD and uploads to eehouse.org"
	echo "same as TravisCI does when working"
	exit 1
}

[ -z "${1+x}" ] || usage "takes no args"
[ -z "${XW4D_UPLOAD+x}" ] && usage "XW4D_UPLOAD not defined"

REPO=''
while :; do
	if [ -f .travis.yml ]; then
		REPO=$(pwd)
		break
	fi
	cd ../
done

mkdir -p $TMPDIR
cd $TMPDIR
git clone $REPO
for BUILD in $(find . -name build.gradle); do
	DIR=$(basename $(dirname $BUILD))
	if [ "$DIR" == "android" ]; then
		cd $(dirname $BUILD)
		break
	fi
done
pwd

./gradlew asXw4dDeb
scp $(find . -name '*.apk') ${XW4D_UPLOAD}
