#!/bin/sh

set -u -e

APPMK=./jni/Application.mk
XWORDS_DEBUG_ARMONLY=${XWORDS_DEBUG_ARMONLY:-""}

echo "# Generated by $0; do not edit!!!" > $APPMK

if [ "$1" = "release" ]; then
    echo "APP_ABI := armeabi x86" >> $APPMK
elif [ -n "$XWORDS_DEBUG_ARMONLY" ]; then
    echo "APP_ABI := armeabi" >> $APPMK
else
    echo "APP_ABI := armeabi x86" >> $APPMK
fi
