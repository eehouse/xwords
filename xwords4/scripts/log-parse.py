#!/usr/bin/python

import sys, re

LINE = re.compile('(<0x.*>)(\d\d:\d\d:\d\d): (.*)$')
DATE = re.compile('It\'s a new day: \d\d/\d\d/\d\d\d\d')
nMatches = 0
nDates = 0

def handleLine(thread, timestamp, rest):
    global nMatches
    nMatches += 1
    print "handleLine got", thread, "and", timestamp, "and", rest

def main():
    global nMatches, nDates

    for line in sys.stdin:
        line.strip()
        mtch = LINE.match(line)
        if mtch:
            handleLine( mtch.group(1), mtch.group(2), mtch.group(3) )
            break
        mtch = DATE.match(line)
        if mtch:
            nDates += 1
            continue
        print "unmatched: ", line
        break

    print "got", nMatches, "normal lines and", nDates, "dates."

main()
