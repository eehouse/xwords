#!/bin/sh

set -e -u

CONNNAME=""

usage() {
    echo "usage: $0 [--connname <s>]"
    exit 1
}

while [ $# -gt 0 ]; do
    case $1 in
        --connname)
            CONNNAME=$2
            shift
            ;;
        *) usage
            ;;
    esac
    shift
done


QUERY="SELECT * from msgs"
if [ -n "$CONNNAME" ]; then
    QUERY="$QUERY WHERE CONNNAME='$CONNNAME'"
fi
QUERY="$QUERY ORDER BY ctime DESC"

echo "${QUERY};" | psql xwgames

