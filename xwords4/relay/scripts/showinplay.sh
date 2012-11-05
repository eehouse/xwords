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

echo "Device (pid) count: $(pidof xwords | wc | awk '{print $2}')"
echo "Row count:" $(psql -t xwgames -c "select count(*) FROM games $QUERY;")

echo "SELECT dead,connname,cid,room,lang,clntVers,ntotal,nperdevice,seeds,devids,ack,nsent "\
     "FROM games $QUERY ORDER BY NOT dead, connname LIMIT $LIMIT;" \
    | psql xwgames

echo "SELECT connname, hid, devid, count(*), sum(msglen) "\
     "FROM msgs where connname in (SELECT connname from games where not games.dead group by connname)" \
     "GROUP BY connname, hid, devid ORDER BY connname;" \
    | psql xwgames

echo "SELECT * from devices;" \
    | psql xwgames

