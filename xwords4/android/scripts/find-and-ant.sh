#!/bin/sh

set -e -u

MANIFEST=AndroidManifest.xml
INSTALL=''
CMDS=''

usage() {
    [ $# -ge 1 ] && echo "ERROR: $1"
    echo "usage: $0 <cmds to ant>"
    exit 1
}

while [ $# -gt 0 ]; do
    case $1 in
		--help|-h|-help|-?)
			usage
			;;
		clean|debug|release)
			CMDS="$CMDS $1"
			;;
		install)
			INSTALL=1
			;;
		*)
			usage "Unexpected param $1"
			;;
    esac
    shift
done

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

if [ -n "$INSTALL" ]; then
	adb-install.sh -e -d
fi

# if [ "$CMDS" != "${CMDS%%install}" ]; then
# 	adb shell am start -n org.eehouse.android.${PKG}/org.eehouse.android.${PKG}.GamesListActivity
# fi
