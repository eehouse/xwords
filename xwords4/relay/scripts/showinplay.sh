#!/bin/sh

set -e -u

LIMIT=10000

usage() {
    echo "usage: $0 [--limit <n>]"
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

echo "Device count:  $(pidof xwords | wc | awk '{print $2}')"
echo "Row count:" $(psql -t xwgames -c "select count(*) FROM games $QUERY;")

echo "SELECT dead,connname,cid,room,lang,ntotal,nperdevice,seeds,ack,nsent "\
     "FROM games $QUERY ORDER BY NOT dead, connname LIMIT $LIMIT;" \
    | psql xwgames

