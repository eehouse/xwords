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

echo -n "Device (pid) count: $(pidof xwords | wc | awk '{print $2}')"
echo ";   relay pid[s]: $(pidof xwrelay)"
echo "Row count:" $(psql -t xwgames -c "select count(*) FROM games $QUERY;")

echo "SELECT dead as d,connname,cid,room,lang as lg,clntVers as cv ,ntotal as t,nperdevice as nPerDev,nsents as snts, seeds,devids,tokens,ack, mtimes "\
     "FROM games $QUERY ORDER BY NOT dead, connname LIMIT $LIMIT;" \
    | psql xwgames

echo "SELECT connname, hid, devid, count(*), sum(msglen) "\
     "FROM msgs where connname in (SELECT connname from games where not games.dead group by connname) "\
     "OR devid IN (SELECT unnest(devids) from games where not games.dead) "\
     "GROUP BY connname, hid, devid ORDER BY connname LIMIT $LIMIT;" \
    | psql xwgames

echo "SELECT id, model, osvers, mtime, array_length(devTypes, 1) as cnt, devTypes[1] as dTyp, devids[1] as devid FROM devices WHERE id IN (select UNNEST(devids) FROM games $QUERY) ORDER BY id LIMIT $LIMIT;" \
    | psql xwgames

