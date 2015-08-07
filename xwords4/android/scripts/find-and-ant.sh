#!/bin/sh

set -e -u

MANIFEST=AndroidManifest.xml
XWORDS=org.eehouse.android.xw4
INSTALL=''
UNINSTALL=''
CMDS=''

usage() {
    [ $# -ge 1 ] && echo "ERROR: $1"
    echo "usage: $0 <cmds to ant>"
    exit 1
}

uninstall() {
	adb devices | while read DEV TYPE; do
		case "$TYPE" in
			device)
				adb -s $DEV uninstall $XWORDS
				;;
			emulator)
				adb -s $DEV uninstall $XWORDS
				;;
		esac
	done
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
		reinstall)
			UNINSTALL=1
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
        echo "running in unexpected directory $DIRNAME; hope that's ok"
        ;;
esac

# if we're running for the first time in this directory/variant,
# generate local.properties
[ -e local.properties ] || ../scripts/setup_local_props.sh

ant $CMDS

if [ -n "$UNINSTALL" ]; then
	uninstall
fi
if [ -n "$INSTALL" ]; then
	adb-install.sh -e -d
fi

# if [ "$CMDS" != "${CMDS%%install}" ]; then
# 	adb shell am start -n org.eehouse.android.${PKG}/org.eehouse.android.${PKG}.GamesListActivity
# fi
