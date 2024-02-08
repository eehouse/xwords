#!/bin/bash

DIR=/tmp/build_$$_dir
mkdir -p $DIR
pushd $DIR

usage() {
    [ $# -ge 1 ] && echo "Error: $1"
    echo "builds debug variant from the current tip of github"
	echo "(last modified Jan 2024)"
    exit 1
}

while [ $# -ge 1 ]; do
    case $1 in
		--help)
			usage
			;;
		*)
			usage "unexpected command $1"
			;;
	esac
	shift
done

git clone https://github.com/eehouse/xwords.git
cd xwords/xwords4/android
./gradlew asXw4dDeb

APK="$(find . -name '*.apk')"
if [ -n "${XW4D_UPLOAD}" ]; then
	IFS=","
	for UPPATH in ${XW4D_UPLOAD}; do
		# scp "$APK" ${UPPATH}
		echo "uploaded $APK to ${UPPATH}"
	done
else
	echo "not uploading $APK: XW4D_UPLOAD not set" >&2
fi

popd
rm -rf $DIR
