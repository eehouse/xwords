#!/bin/sh

BASE=$(dirname $0)
cd $BASE/../XWords4/bin/classes

javah -o /tmp/javah$$.txt org.eehouse.android.xw4.jni.XwJNI
javap -s org.eehouse.android.xw4.jni.DrawCtx
javap -s org.eehouse.android.xw4.jni.XW_UtilCtxt
javap -s org.eehouse.android.xw4.jni.CommsAddrRec
javap -s org.eehouse.android.xw4.jni.TransportProcs
cat /tmp/javah$$.txt
rm /tmp/javah$$.txt
