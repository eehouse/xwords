#!/bin/sh

usage() {
	[ $# -eq 0 ] || echo "ERROR: $1"
	echo "usage: $0"
	echo "  git-copies all strings.xml translations from android_translate"
	echo "  branch to the current one. DOES NOT COMMIT nor consider history etc."
	exit 1
}

[ $# -eq 0 ] || usage "unexpected paramater $1"

for LANGDIR in res_src/values-??; do
	echo "copying from $LANGDIR"
	git show android_translate:xwords4/android/XWords4/${LANGDIR}/strings.xml > ${LANGDIR}/strings.xml
done
