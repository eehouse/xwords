#!/bin/bash

# I use this thing this way: playnum.sh 10 2>&1 | ./wordlens.pl

NUM=$1
echo "NUM=$NUM"

while :; do

    ../linux/xwords -u -s -r Eric -d ../linux/dicts/OSPD2to15.xwd -q -i 

    NUM=$(( NUM - 1 ));

    if (( $NUM <= 0 )); then exit 0; fi
done