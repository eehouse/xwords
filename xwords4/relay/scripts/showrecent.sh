#!/bin/sh

set -e -u

LIMIT=10000

usage() {
    echo "usage: $0 [--limit <n>]"
    exit 1
}

while [ $# -gt 0 ]; do
    case $1 in
        --limit)
            LIMIT=$2
            shift
            ;;
        *) usage
            ;;
    esac
    shift
done

QUERY="WHERE NOT -NTOTAL = sum_array(nperdevice)"
COLUMNS="   dead,     connname,cid,room,pub,     lang,     ntotal,     nperdevice,        ack,nsents,ctime,mtimes"
COLUMNS_AS="dead as D,connname,cid,room,pub as P,lang as L,ntotal as n,nperdevice as nper,ack,nsents,ctime,mtimes"

echo "SELECT $COLUMNS_AS FROM ( SELECT $COLUMNS, unnest(mtimes) FROM games $QUERY) AS set GROUP BY $COLUMNS ORDER BY ctime DESC LIMIT $LIMIT;" | psql xwgames
