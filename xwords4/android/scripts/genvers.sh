#!/bin/sh

cd $(dirname $0)
cd ../../

GITVERSION=$(scripts/gitversion.sh)

# TODO: deal with case where there's no hash available -- exported
# code maybe?  Better: gitversion.sh does that.

cat <<EOF > android/XWords4/res/values/git_string.xml
<?xml version="1.0" encoding="utf-8"?>
<!-- auto-generated; do not edit -->

<resources>
    <string name="git_rev_gen">$GITVERSION</string>
</resources>
EOF

# Eventually this should pick up a tag if we're at one.  That'll be
# the way to mark a release
SHORTVERS="$(git describe --always $GITVERSION 2>/dev/null || echo unknown)"

cat <<EOF > android/XWords4/src/org/eehouse/android/xw4/GitVersion.java
// auto-generated; do not edit
package org.eehouse.android.xw4;
class GitVersion {
    public static final String VERS = "$SHORTVERS";
}
EOF

# touch the files that depend on git_string.xml.  (I'm not sure that
# this list is complete or if ant and java always get dependencies
# right.  Clean builds are the safest.)
touch android/XWords4/res/xml/xwprefs.xml 
touch android/XWords4/gen/org/eehouse/android/xw4/R.java
touch android/XWords4/src/org/eehouse/android/xw4/Utils.java
