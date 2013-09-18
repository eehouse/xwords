#!/bin/sh

set -e -u

FILTER=""

LIMIT=10000

usage() {
    echo "usage: $0 [--limit <n>] [--filter]"
    exit 1
}

while [ $# -gt 0 ]; do
    case $1 in
        --limit)
            LIMIT=$2
            shift
            ;;
        --filter)
            FILTER=1
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

# Games
echo "SELECT dead as d,connname,cid,room,lang as lg,clntVers as cv ,ntotal as t,nperdevice as nPerDev,nsents as snts, seeds,devids,tokens,ack, mtimes "\
     "FROM games $QUERY ORDER BY NOT dead, connname LIMIT $LIMIT;" \
    | psql xwgames

# Messages
echo "SELECT connname, hid, devid, count(*), sum(msglen) "\
     "FROM msgs where connname in (SELECT connname from games where not games.dead group by connname) "\
     "OR devid IN (SELECT unnest(devids) from games where not games.dead) "\
     "GROUP BY connname, hid, devid ORDER BY connname LIMIT $LIMIT;" \
    | psql xwgames

# Devices
LINE="SELECT id, model, osvers, array_length(mtimes, 1) as mcnt, mtimes[1] as mtime, array_length(devTypes, 1) as dcnt, devTypes[1] as dTyp, devids[1] as devid FROM devices "
if [ -n "$FILTER" ]; then
     LINE="${LINE} WHERE id IN (select UNNEST(devids) FROM games $QUERY)"
fi
LINE="$LINE ORDER BY mtimes[1] DESC LIMIT $LIMIT;"
echo "$LINE" | psql xwgames

