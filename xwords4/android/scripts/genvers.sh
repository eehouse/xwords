#!/bin/sh

set -e -u

STRINGS_HASH=""

usage() {
    echo "usage: $0 <variant> <relay_vers> <chatSupported> <thumbSupported>"
    exit 1
}

[ $# -eq 4 ] || usage

VARIANT=$1
CLIENT_VERS_RELAY=$2
CHAT_SUPPORTED=$3
THUMBNAIL_SUPPORTED=$4

BUILD_DIR=$(basename $(pwd))
cd $(dirname $0)
cd ../

GITVERSION=$(../scripts/gitversion.sh)

# Need to verify that R.java is unmodified; otherwise we can't set
# this constant!!!  Shouldn't be a problem with release builds,
# though.
if ! git status | grep -q "modified:.*${BUILD_DIR}/archive/R.java"; then
	STRINGS_HASH=$(git log -- ${BUILD_DIR}/archive/R.java | grep '^commit ' | head -n 1 | awk '{print $2}')
fi
# TODO: deal with case where there's no hash available -- exported
# code maybe?  Better: gitversion.sh does that.

cat <<EOF > ${BUILD_DIR}/res/values/git_string.xml
<?xml version="1.0" encoding="utf-8"?>
<!-- auto-generated; do not edit -->

<resources>
    <string name="git_rev">$GITVERSION</string>
</resources>
EOF

# Eventually this should pick up a tag if we're at one.  That'll be
# the way to mark a release
SHORTVERS="$(git describe --always $GITVERSION 2>/dev/null || echo unknown)"

cat <<EOF > ${BUILD_DIR}/src/org/eehouse/android/${VARIANT}/BuildConstants.java
// auto-generated (by $(basename $0)); do not edit
package org.eehouse.android.${VARIANT};
class BuildConstants {
    public static final String GIT_REV = "$SHORTVERS";
    public static final String STRINGS_HASH = "$STRINGS_HASH";
    public static final short CLIENT_VERS_RELAY = $CLIENT_VERS_RELAY;
    public static final boolean CHAT_SUPPORTED = $CHAT_SUPPORTED;
    public static final boolean THUMBNAIL_SUPPORTED = $THUMBNAIL_SUPPORTED;
    public static final String BUILD_STAMP = "$(date +'%F at %R %Z')";
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
