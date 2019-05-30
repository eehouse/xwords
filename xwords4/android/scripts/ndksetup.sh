#!/bin/sh

set -u -e

USE_CLANG=''

APPMK=./jni/Application.mk
TMP_MK=/tmp/tmp_$$_Application.mk
XWORDS_DEBUG_ARMONLY=${XWORDS_DEBUG_ARMONLY:-""}
XWORDS_DEBUG_X86ONLY=${XWORDS_DEBUG_X86ONLYx:-""}

usage() {
	echo "usage $0 [--with-clang] [--arm-only|--x86-only]"
	exit 1
}

while [ $# -gt 0 ]; do
	case $1 in
		--with-clang)
			USE_CLANG=1
			;;
		--arm-only)
			XWORDS_DEBUG_ARMONLY=1
			;;
		--x86-only)
			XWORDS_DEBUG_X86ONLY=1
			;;
		*)
			usage "Unexpected param $1"
			;;
	esac
	shift
done

echo "# Generated by $0; do not edit!!!" > $TMP_MK

[ -n "$USE_CLANG" ] && echo "NDK_TOOLCHAIN_VERSION := clang" >> $TMP_MK

# TODO: reserach whether armeabi-v7a is better here
if [ -n "$XWORDS_DEBUG_ARMONLY" ]; then
    echo "APP_ABI := armeabi" >> $TMP_MK
elif [ -n "$XWORDS_DEBUG_X86ONLY" ]; then
    echo "APP_ABI := x86" >> $TMP_MK
else
    echo "APP_ABI := armeabi arm64-v8a x86" >> $TMP_MK
fi
# echo "APP_OPTIM := debug" >> $TMP_MK

# Now replace the existing file, but only if it's different.  Touching
# it causes the library to be completely rebuilt, so avoid that if
# possible!

if [ ! -f $APPMK ]; then
    cp $TMP_MK $APPMK
elif ! diff -q $APPMK $TMP_MK >/dev/null; then
    cp $TMP_MK $APPMK
fi
rm -f $TMP_MK
