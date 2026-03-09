#!/bin/bash

set -e -u -x

NO_RM=''
NO_UPLOAD=''

BRANCH=$(git branch --show-current)
DIR=/tmp/build_$$_dir

# REMOTE=https://github.com/eehouse/xwords.git
REMOTE=ssh://prod@eehouse/home/prod/repos/xwords

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

mkdir -p $DIR
pushd $DIR

git clone --branch $BRANCH --recurse-submodules ${REMOTE}
cd xwords/xwords4/android
case $BRANCH in
	"main")
		TARGET=asXw4dDeb
		;;
	"gameref")
		TARGET=asXw4grdDeb
		;;
	*) fail
	   ;;
esac
./gradlew $TARGET

APK="$(find . -name '*.apk')"
# pull something like xw4d out of the path
SERVER_DIR=$(basename $(dirname $(dirname $APK)))
echo "APK: $APK; SERVER_DIR: ${SERVER_DIR}"

if [ -n "${NO_UPLOAD}" ]; then
	: # do nothing
elif [ -n "${XW4D_UPLOAD}" ]; then
	IFS=","
	for UPPATH in ${XW4D_UPLOAD}; do
		WITH_DIR="${UPPATH}/${SERVER_DIR}/"
		scp "$APK" "$WITH_DIR"
		echo "uploaded $APK to ${WITH_DIR}"
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
