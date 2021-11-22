#!/usr/bin/python3

# Invoked with path to bad words list as single parameter, and with a
# stream of words via stdin, loads the bad words into a map and for
# every word in stdin echos it to stdout IFF it's not in the map.

import sys

dirtyMap = {}
dirtyList = sys.argv[1]
for f in open(dirtyList):
    dirtyMap[f] = True

for word in sys.stdin:
    if word in dirtyMap:
        sys.stderr.write( sys.argv[0] + ": dropping: " + word )
    else:
        sys.stdout.write( word )
