#!/bin/sh

usage() {
    [ $# -gt 0 ] && echo "Error: $1"
    echo "usage: $0 <dir> <pkg-node>         "
    echo "            # e.g. XWords4 or XWords4-dbg and xw4 or xw4_dbg"
    exit 1
}

[ $# -gt 1 ] || usage "dir and node parameters required"

DIR=$1
NODE=$2

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
