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

# touch the files that depend on git_string.xml.  (I'm not sure that
# this list is complete or if ant and java always get dependencies
# right.  Clean builds are the safest.)
touch android/XWords4/res/xml/xwprefs.xml 
touch android/XWords4/gen/org/eehouse/android/xw4/R.java
