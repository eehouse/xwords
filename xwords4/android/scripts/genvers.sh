#!/bin/sh

set -e -u

DIR=$1
VARIANT=$2

cd $(dirname $0)
cd ../../

GITVERSION=$(scripts/gitversion.sh)

# TODO: deal with case where there's no hash available -- exported
# code maybe?  Better: gitversion.sh does that.

cat <<EOF > android/${DIR}/res/values/git_string.xml
<?xml version="1.0" encoding="utf-8"?>
<!-- auto-generated; do not edit -->

<resources>
    <string name="git_rev">$GITVERSION</string>
</resources>
EOF

# Eventually this should pick up a tag if we're at one.  That'll be
# the way to mark a release
SHORTVERS="$(git describe --always $GITVERSION 2>/dev/null || echo unknown)"

cat <<EOF > android/${DIR}/src/org/eehouse/android/${VARIANT}/GitVersion.java
// auto-generated; do not edit
package org.eehouse.android.${VARIANT};
class GitVersion {
    public static final String VERS = "$SHORTVERS";
}
EOF

# touch the files that depend on git_string.xml.  (I'm not sure that
# this list is complete or if ant and java always get dependencies
# right.  Clean builds are the safest.)
touch android/${DIR}/res/xml/xwprefs.xml 
touch android/${DIR}/gen/org/eehouse/android/${VARIANT}/R.java
touch android/${DIR}/src/org/eehouse/android/${VARIANT}/Utils.java
