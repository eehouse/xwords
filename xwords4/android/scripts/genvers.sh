#!/bin/sh

cd $(dirname $0)
cd ../../

cat <<EOF > android/XWords4/src/org/eehouse/android/xw4/VERS.java
// auto-generated; do not edit
package org.eehouse.android.xw4;
class SvnVersion {
    public static final String VERS = "$(svnversion .)";
}
EOF

