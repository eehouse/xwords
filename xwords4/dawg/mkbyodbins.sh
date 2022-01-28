#!/bin/sh

for INFO in $(ls */info.txt); do
	DIR=$(dirname $INFO)
	echo "*** processing $(basename $DIR) ***"
	(cd $DIR && make clean byodbins)
done

make dict2dawg
