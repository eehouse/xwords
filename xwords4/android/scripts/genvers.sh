#!/bin/sh

set -e -u

STRINGS_HASH=""
OUT_PATH=""
VARIANT=""
CLIENT_VERS_RELAY=""
GCM_SENDER_ID=${GCM_SENDER_ID:-""}
CRITTERCISM_APP_ID=${CRITTERCISM_APP_ID:-""}

usage() {
    echo "usage: $0 --variant <variant> --client-vers <relay_vers> \\"
	echo "   [--vers-outfile path/to/versout.txt]"
    exit 1
}

while [ $# -gt 0 ]; do
	echo $1
	case $1 in
		--variant)
			VARIANT=$2
			shift
			;;
		--client-vers)
			CLIENT_VERS_RELAY=$2
			shift
			;;
		--vers-outfile)
			OUT_PATH=$2
			shift
			;;
		*)
			usage
			;;
	esac
	shift
done

[ -n "$VARIANT" -a -n "$CLIENT_VERS_RELAY" ] || usage

BUILD_DIR=$(basename $(pwd))
cd $(dirname $0)
cd ../

GITVERSION=$(../scripts/gitversion.sh)
if [ -n "$OUT_PATH" ]; then
	echo $GITVERSION > $BUILD_DIR/$OUT_PATH
	git describe >> $BUILD_DIR/$OUT_PATH
fi

case $VARIANT in
	xw4)
		APPNAME=Crosswords
		SMSPORT=3344
		INVITE_PREFIX=/and/
		DBG_TAG=XW4
		;;
	xw4dbg)
		APPNAME=CrossDbg
		SMSPORT=3345
		INVITE_PREFIX=/anddbg/
		DBG_TAG=X4BG
		;;
	*)
		usage
		;;
esac


# Need to verify that R.java is unmodified; otherwise we can't set
# this constant!!!  Shouldn't be a problem with release builds,
# though.
if ! git status | grep -q "modified:.*${BUILD_DIR}/archive/R.java"; then
	STRINGS_HASH=$(git log -- ${BUILD_DIR}/archive/R.java | grep '^commit ' | head -n 1 | awk '{print $2}')
fi
# TODO: deal with case where there's no hash available -- exported
# code maybe?  Better: gitversion.sh does that.

cat <<EOF > ${BUILD_DIR}/res/values/gen_strings.xml
<?xml version="1.0" encoding="utf-8"?>
<!-- auto-generated (by $(basename $0)); do not edit -->

<resources>
    <string name="app_name">$APPNAME</string>  
    <string name="git_rev">$GITVERSION</string>
    <string name="nbs_port">$SMSPORT</string>
    <string name="invite_prefix">$INVITE_PREFIX</string>
</resources>
EOF

# Eventually this should pick up a tag if we're at one.  That'll be
# the way to mark a release
SHORTVERS="$(git describe --always $GITVERSION 2>/dev/null || echo ${GITVERSION}+)"
GITHASH=$(git rev-parse --verify HEAD)

if [ -z "$GCM_SENDER_ID" ]; then
    echo "GCM_SENDER_ID empty; GCM use will be disabled" >&2
fi
if [ -z "$CRITTERCISM_APP_ID" ]; then
    echo "CRITTERCISM_APP_ID empty; Crittercism will not be enabled" >&2
fi

cat <<EOF > ${BUILD_DIR}/src/org/eehouse/android/${VARIANT}/BuildConstants.java
// auto-generated (by $(basename $0)); do not edit
package org.eehouse.android.${VARIANT};
public class BuildConstants {
    public static final String GIT_REV = "$SHORTVERS";
    public static final String STRINGS_HASH = "$STRINGS_HASH";
    public static final short CLIENT_VERS_RELAY = $CLIENT_VERS_RELAY;
    public static final long BUILD_STAMP = $(date +'%s');
    public static final String DBG_TAG = "$DBG_TAG";
    public static final String VARIANT = "$VARIANT";
    public static final String GCM_SENDER_ID = "${GCM_SENDER_ID}";
    public static final String CRITTERCISM_APP_ID  = "${CRITTERCISM_APP_ID}";
}
EOF

# touch the files that depend on git_string.xml.  (I'm not sure that
# this list is complete or if ant and java always get dependencies
# right.  Clean builds are the safest.)
touch ${BUILD_DIR}/res/xml/xwprefs.xml 
echo "touched ${BUILD_DIR}/res/xml/xwprefs.xml"
mkdir -p ${BUILD_DIR}/gen/org/eehouse/android/${VARIANT}
touch ${BUILD_DIR}/gen/org/eehouse/android/${VARIANT}/R.java
touch ${BUILD_DIR}/src/org/eehouse/android/${VARIANT}/Utils.java
