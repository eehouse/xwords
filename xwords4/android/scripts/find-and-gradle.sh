#!/bin/sh

set -e -u

APK_DIR="app/build/outputs/apk"
MANIFEST=AndroidManifest.xml
XWORDS=org.eehouse.android.xw4
INSTALLS=''
REINSTALL=''
TASKS=''
VARIANTS=''
BUILDS=''

usage() {
    [ $# -ge 1 ] && echo "ERROR: $1"
    echo "usage: $0 [--task <gradle-task>]*"
	echo "   [--variant <variantName>|all]*  # variant to install (defaults to all built, or all if not building)"
	echo "   [--[re]install debug|release]"
	echo "examples"
	echo "$0 --install --variant xw4 --build debug # install the debug build of xw4 (main) variant"
	echo "$0 --reinstall --variant xw4 --build debug # install the debug build of xw4 (main) variant"
    exit 1
}

checkInstallTarget() {
	case $1 in
		debug|release)
		;;
		*)
			usage '"debug" or "release" expected'
			;;
	esac
}

getDevices() {
	DEVICES="$(adb devices | grep '^.*\sdevice$' | awk '{print $1}')"
	echo "getDevices() => $DEVICES" >&2
	echo "$DEVICES"
}

setVariants() {
	if [ -z "$VARIANTS" ]; then
		VARIANTS="xw4"
	fi
}

getApks() {
	APKS=""
	for VARIANT in $VARIANTS; do
		for BUILD in $BUILDS; do
			APKS="$APKS $(ls --sort=time $APK_DIR/app-${VARIANT}-${BUILD}.apk | head -n 1)"
		done
	done
	echo $APKS >&2
	echo $APKS
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
		--install)
			shift
			checkInstallTarget $1
			INSTALL=$1
			BUILDS="$BUILDS $1"
			;;
		--reinstall)
			shift
			checkInstallTarget $1
			REINSTALL=$1
			INSTALL=$1
			;;
		--variant)
			shift
			VARIANTS="$VARIANTS $1"
			;;
		--task)
			shift
			TASKS="$TASKS $1"
			;;
		*)						# assumed to be the end of flags
			usage "unexpected parameter $1"
			;;
    esac
    shift
done

while [ ! -e $MANIFEST ]; do
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
# [ -e local.properties ] || ../scripts/setup_local_props.sh

# Mark "now" so can look for newer .apks later
TSFILE=/tmp/ts$$.stamp
touch $TSFILE


if [ -n "$TASKS" ]; then
	./gradlew $TASKS
fi

if [ -z "$TASKS" -o 0 = "$?" ]; then
	setVariants
	APKS=$(getApks)
	DEVS="$(getDevices)"
	for DEV in DEVS; do
		for APK in $APKS; do
			[ -n "$REINSTALL" ] && uninstall $APK $DEV
			[ -n "$INSTALL" ] && installAndLaunch $APK $DEV
		done
	done
fi

# if [ -n "$UNINSTALL" ]; then
# 	uninstall
# fi
# if [ -n "$INSTALL" ]; then
# 	adb-install.sh -e -d
# fi

# if [ "$CMDS" != "${CMDS%%install}" ]; then
# 	adb shell am start -n org.eehouse.android.${PKG}/org.eehouse.android.${PKG}.GamesListActivity
# fi

rm -f $TSFILE
