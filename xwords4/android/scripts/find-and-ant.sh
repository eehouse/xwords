#!/bin/sh

set -e -u

MANIFEST=AndroidManifest.xml

usage() {
    [ $# -ge 1 ] && echo "ERROR: $1"
    echo "usage: $0 <cmds to ant>"
	echo "   default commands: $CMDS"
    exit 1
}

CMDS="clean debug install"
if [ $# -gt 0 ]; then
    case $1 in
	--help|-h|-help|-?)
	    usage
	    ;;
	*)
	    CMDS="$*"
	    break;
	    ;;
    esac
    shift
fi

while [ ! -e $MANIFEST -o $(basename $(pwd)) = 'bin' ]; do
    [ '/' = $(pwd) ] && usage "reached root without finding $MANIFEST"
    cd ..
done

DIRNAME=$(basename $(pwd))
case $DIRNAME in
    XWords4-bt)
        PKG=xw4bt
        ;;
    XWords4-dbg)
        PKG=xw4dbg
        ;;
    XWords4)
        PKG=xw4
        ;;
    *)
        usage "running in unexpected directory $DIRNAME"
        ;;
esac

ant $CMDS

if [ "$CMDS" != "${CMDS%%install}" ]; then
	adb shell am start -n org.eehouse.android.${PKG}/org.eehouse.android.${PKG}.GamesList
fi
