#!/bin/sh

for INFO in $(ls */info.txt); do
	DIR=$(dirname $INFO)
	echo "*** processing $(basename $DIRNAME) ***"
	(cd $DIR && make byodbins)
done
