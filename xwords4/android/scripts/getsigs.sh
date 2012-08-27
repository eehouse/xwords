#!/bin/sh

set -e -u

DIR=""
NODE=""

usage() {
    [ $# -gt 0 ] && echo "Error: $1"
    echo "usage: $0 [--dir <dir>] [--node <pkg-node>]         "
    echo "            # e.g. XWords4 or XWords4-dbg and xw4 or xw4_dbg"
    exit 1
}

while [ $# -gt 1 ]; do
    case $1 in
        --dir)
            shift
            DIR=$1
            ;;
        --node)
            shift
            NODE=$1
            ;;
        *)
            usage
            ;;
    esac
    shift
done

if [ -z "$DIR" ]; then
    if [ -f ./AndroidManifest.xml ]; then # we're in the right directory
        DIR=$(basename $(pwd))
    fi
fi
if [ -z "$NODE" ]; then
    if [ -f ./AndroidManifest.xml ]; then # we're in the right directory
        NODE=$(grep 'package="org.eehouse.android' AndroidManifest.xml | sed 's,^.*android.\(.*\)",\1,')
    fi
fi

[ -n "$DIR" -a -n "$NODE" ] || usage


BASE=$(dirname $0)
cd $BASE/../${DIR}/bin/classes

javah -o /tmp/javah$$.txt org.eehouse.android.${NODE}.jni.XwJNI
javap -s org.eehouse.android.${NODE}.jni.XwJNI
javap -s org.eehouse.android.${NODE}.jni.DrawCtx
javap -s org.eehouse.android.${NODE}.jni.UtilCtxt
javap -s org.eehouse.android.${NODE}.jni.CommsAddrRec
javap -s org.eehouse.android.${NODE}.jni.TransportProcs
javap -s org.eehouse.android.${NODE}.jni.JNIUtils
cat /tmp/javah$$.txt
rm /tmp/javah$$.txt
