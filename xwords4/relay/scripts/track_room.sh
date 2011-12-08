#!/bin/sh

set -e -u

CONNNAME=""
LASTLINE=""
INTERVAL=30

usage() {
    [ $# -gt 0 ] && echo "Error: $1"
    echo "Usage: $(basename $0) connname"
    exit 1
}

read_row() {
    echo "select * from games where connname='${CONNNAME}';" | psql xwgames | grep $CONNNAME
}

print_line() {
    echo "$(date): $LASTLINE"
}

[ $# -eq 1 ] || usage

CONNNAME=$1
LASTLINE=$(read_row $CONNNAME)
[ -z "$LASTLINE" ] && usage "$CONNNAME not found"

print_line

while :; do
    sleep $INTERVAL
    NEWLINE=$(read_row)
    if [ "$NEWLINE" != "$LASTLINE" ]; then
        LASTLINE="$NEWLINE"
        print_line
    fi
done
