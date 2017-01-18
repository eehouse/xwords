#!/bin/sh

set -e -u

ANDROID_NDK=${ANDROID_NDK:-''}

if [ ! -d jni ]; then
    echo "no jni directory; please run from root of source tree"
    exit 1
fi

if [ -z "$ANDROID_NDK" ]; then
	if which ndk-build >/dev/null; then
		ANDROID_NDK=$(dirname $(which ndk-build))
	else
		echo -n "ANDROID_NDK not set... "
		echo "NDK not found; install and set ANDROID_NDK to point to it"
		exit 1
	fi
fi

${ANDROID_NDK}/ndk-build $*

echo "$0 done"
