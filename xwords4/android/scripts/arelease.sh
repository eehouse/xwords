#!/bin/bash

usage() {
    echo "usage: $0 [<package-unsigned.apk>]" >&2
    exit 1
}


FILES="$1"

if [ -z "$FILES" ]; then
    FILES=$(ls $(dirname $0)/../*/bin/*-unsigned.apk)
    if [ -z "$FILES" ]; then
        echo "unable to find any unsigned packages" >&2
        usage
    fi
fi

for PACK_UNSIGNED in $FILES; do
    echo $FILE

    PACK_SIGNED=$(basename $PACK_UNSIGNED)
    echo "base: $PACK_SIGNED"
    PACK_SIGNED=${PACK_SIGNED/-unsigned}
    echo "signed: $PACK_SIGNED"
    jarsigner -verbose -keystore ~/.keystore $PACK_UNSIGNED mykey
    rm -f $PACK_SIGNED
    zipalign -v 4 $PACK_UNSIGNED $PACK_SIGNED
    [ -n "$XW_WWW_PATH" ] && cp $PACK_SIGNED $XW_WWW_PATH
done
