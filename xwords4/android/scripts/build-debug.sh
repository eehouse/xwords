#!/bin/bash

set -e -u

NO_RM=''
NO_UPLOAD=''

DIR=/tmp/build_$$_dir
mkdir -p $DIR
pushd $DIR

usage() {
    [ $# -ge 1 ] && echo "Error: $1"
	echo "usage: $0 [--no-rm --no-upload]    # do not remove the build directory"
    echo "builds debug variant from the current tip of github"
	echo "(last modified Jan 2024)"
    exit 1
}

while [ $# -ge 1 ]; do
    case $1 in
		--help)
			usage
			;;
		--no-rm)
			NO_RM=1
			;;
		--no-upload)
			NO_UPLOAD=1
			;;
		*)
			usage "unexpected command $1"
			;;
	esac
	shift
done

git clone --branch main --recurse-submodules https://github.com/eehouse/xwords.git
cd xwords/xwords4/android
./gradlew asXw4dDeb

APK="$(find . -name '*.apk')"
if [ -n "${NO_UPLOAD}" ]; then
	: # do nothing
elif [ -n "${XW4D_UPLOAD}" ]; then
	IFS=","
	for UPPATH in ${XW4D_UPLOAD}; do
		scp "$APK" ${UPPATH}
		echo "uploaded $APK to ${UPPATH}"
	done
else
	echo "not uploading $APK: XW4D_UPLOAD not set" >&2
fi

popd
if [ -z "${NO_RM}" ]; then
	rm -rf $DIR
else
	echo "not removing: $DIR"
fi
