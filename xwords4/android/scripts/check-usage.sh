#!/bin/sh

set -e -u

# Grabs the list of string names from values.xml and greps for each.
# If it's not found in a .java or .xml file lists it.

cd $(dirname $0)

checkStrings() {
	TYP=$1
	IDS="$2"
	STR_COUNT=$(echo $IDS | wc -w)

	JAVA_FILES=$(find ../ -name '*.java')
	XML_FILES="$(find ../XWords4/res/ -name '*.xml') ../XWords4/AndroidManifest.xml"
	FILE_COUNT=$(( $(echo $JAVA_FILES | wc -w) + $(echo $XML_FILES | wc -w) ))

	echo "checking $STR_COUNT $TYP ids in $FILE_COUNT files..."

	for ID in $IDS; do
		RID="R.${TYP}.${ID}"
		if grep -qr $RID $JAVA_FILES; then
			continue;
		fi

		RID="@string/${ID}"
		if grep -qr $RID $XML_FILES; then
			continue;
		fi
		echo "$ID not found"
	done
}

STR_IDS=$(./string-names.sh ../XWords4/res/values/strings.xml)
checkStrings string "$STR_IDS"
PLRL_IDS=$(./plurals-names.sh ../XWords4/res/values/strings.xml)
checkStrings plurals "$PLRL_IDS"
