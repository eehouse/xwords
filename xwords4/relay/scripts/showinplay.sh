#!/bin/sh

set -e -u

FILTER_DEVS="1"
ROOMS=""
CONNNAMES=''

LIMIT=10000

usage() {
    echo "usage: $0 [--limit <n>] \\"
    echo "   [--no-filter-devices]    # default is to show only devices involved in games shown \\"
    echo "   [--room <room>]*         # filter on specified room[s] \\"
    echo "   [--connname <connname>]* # filter on specified connname[s]"
    exit 1
}

while [ $# -gt 0 ]; do
    case $1 in
        --limit)
            LIMIT=$2
            shift
            ;;
        --no-filter-devices)
            FILTER_DEVS=''
            ;;
	--room)
	    [ -n "$ROOMS" ] && ROOMS="${ROOMS},"
	    ROOMS="$ROOMS '$2'"
	    shift
	    ;;
	--connname)
	    [ -n "$CONNNAMES" ] && CONNNAMES="${CONNNAMES},"
	    CONNNAMES="$CONNNAMES '$2'"
	    shift
	    ;;
        *) usage
            ;;
    esac
    shift
done

QUERY="WHERE NOT -NTOTAL = sum_array(nperdevice) AND NOT DEAD"
if [ -n "$ROOMS" ]; then
    QUERY="$QUERY AND room IN ($ROOMS) "
fi
if [ -n "$CONNNAMES" ]; then
    QUERY="$QUERY AND connname IN ($CONNNAMES) "
fi

echo -n "Device (pid) count: $(pidof xwords | wc | awk '{print $2}')"
echo ";   relay pid[s]: $(pidof xwrelay)"
echo "Row count:" $(psql -t xwgames -c "select count(*) FROM games $QUERY;")

# Games
echo "SELECT dead as d,connname,room,lang as lg,clntVers as cv ,ntotal as t,nperdevice as npd,nsents as snts, seeds,devids,tokens,ack, mtimes "\
     "FROM games $QUERY ORDER BY NOT dead, ctime DESC LIMIT $LIMIT;" \
    | psql xwgames

# Messages
echo "Unack'd msgs count:" $(psql -t xwgames -c "select count(*) FROM msgs where stime = 'epoch' AND connname IN (SELECT connname from games $QUERY);")
echo "SELECT id,connName,hid as h,token,ctime,stime,devid,msg64 "\
     "FROM msgs WHERE stime = 'epoch' AND connname IN (SELECT connname from games $QUERY) "\
     "ORDER BY ctime DESC, connname LIMIT $LIMIT;" \
    | psql xwgames

# Devices
LINE="SELECT id, model, osvers, array_length(mtimes, 1) as mcnt, mtimes[1] as mtime, array_length(devTypes, 1) as dcnt, devTypes[1] as dTyp, devids[1] as devid FROM devices "
if [ -n "$FILTER_DEVS" ]; then
     LINE="${LINE} WHERE id IN (select UNNEST(devids) FROM games $QUERY)"
fi
LINE="$LINE ORDER BY mtimes[1] DESC LIMIT $LIMIT;"
echo "$LINE" | psql xwgames

