#!/bin/sh

cd $(dirname $0)
cd ../../

SVNVERSION=$(svnversion)
if [ -z "$SVNVERSION" -o "$SVNVERSION" = "exported" ]; then
    SVNVERSION=$(git svn find-rev $(git log -1 --pretty=format:%H 2>/dev/null) \
        2>/dev/null)
    if git status | grep -q modified; then
        SVNVERSION=${SVNVERSION}M
    fi
fi

cat <<EOF > android/XWords4/src/org/eehouse/android/xw4/SvnVersion.java
// auto-generated; do not edit
package org.eehouse.android.xw4;
class SvnVersion {
    public static final String VERS = "$SVNVERSION";
}
EOF

# touch the file that depends on VERS.java
touch android/XWords4/src/org/eehouse/android/xw4/Utils.java
