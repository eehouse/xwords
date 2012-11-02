#!/bin/sh

set -e -u

# Grabs the list of string names from values.xml and greps for each.
# If it's not found in a .java or .xml file lists it.

cd $(dirname $0)

IDS=$(./string-names.sh ../XWords4/res/values/strings.xml)
STR_COUNT=$(echo $IDS | wc -w)

JAVA_FILES=$(find ../ -name '*.java')
XML_FILES="$(find ../XWords4/res/ -name '*.xml') ../XWords4/AndroidManifest.xml"
FILE_COUNT=$(( $(echo $JAVA_FILES | wc -w) + $(echo $XML_FILES | wc -w) ))

echo "checking $STR_COUNT string ids in $FILE_COUNT files..."

for ID in $IDS; do
    RID="R.string.${ID}"
    if grep -qr $RID $JAVA_FILES; then
        continue;
    fi

    RID="@string/${ID}"
    if grep -qr $RID $XML_FILES; then
        continue;
    fi
    echo "$ID not found"
done
