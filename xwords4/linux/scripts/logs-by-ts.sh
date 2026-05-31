#!/bin/bash

# Script to print test logs interleaved and sorted by timestamp so, I
# hope, I can see network activity in the order it occurs.

cd $(dirname $0)

awk '{
	gsub(/.*\//, "", FILENAME);
    print FILENAME, $0
}' ../netGamesTest_state/*_log.txt | sort -k 3,3 | uniq
