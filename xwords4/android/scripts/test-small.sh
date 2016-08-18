#!/bin/sh

set -e -u

usage() {
	[ $# -ge 1 ] && echo "ERROR: $1"
	echo "usage: $0" >&2
	echo "Move small-device resources around so they build for normal ones" >&2
	exit 1
}

checkFile() {
	FILE=$1
	[ -z "$(git ls-files -m $FILE)" ] || usage "$FILE is modified"
}

swapFiles() {
	SRC_FILE=$1
	DEST_DIR=$2

	NAME=$(basename $SRC_FILE)
	mv $DEST_DIR/$NAME /tmp/
	mv $SRC_FILE $DEST_DIR
	mv /tmp/$NAME $SRC_FILE
}

for DIR in layout menu ; do
	for FILE in res/${DIR}-small/*.xml; do
		checkFile $FILE
		checkFile res/$DIR/$(basename $FILE)
	done
done

for DIR in layout menu ; do
	for FILE in res/${DIR}-small/*.xml; do
		swapFiles $FILE res/$DIR
	done
done
