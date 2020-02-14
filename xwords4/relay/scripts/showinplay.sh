#!/bin/sh

set -e -u

FILTER_DEVS="1"
ROOMS=""
CONNNAMES=''

LIMIT=''

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
echo -n "Row count:" $(psql -t xwgames -c "select count(*) FROM games $QUERY;")
echo "; Relay sockets: $(for PID in $(pidof xwrelay); do ls /proc/$PID/fd; done | sort -un | tr '\n' ' ')"

ORDER="ORDER BY NOT dead, ctime DESC"
if [ -n "$LIMIT" ]; then
	LIMIT="LIMIT $LIMIT"
fi

# Games
echo "SELECT dead as d,connname,cid,room,ack,lang as lg,clntVers as cv ,ntotal as t,nperdevice as npd,nsents as snts, seeds,devids,tokens, mtimes "\
     "FROM games $QUERY $ORDER $LIMIT;" \
    | psql xwgames

# Messages
echo "Unack'd msgs count:" $(psql -t xwgames -c "select count(*) FROM msgs where stime = 'epoch' AND connname IN (SELECT connname from games $QUERY $ORDER);")
echo "SELECT id,connName,hid as h,token,ctime,stime,devid as dest,msg64 "\
     "FROM msgs WHERE stime = 'epoch' AND connname IN (SELECT connname from games $QUERY $ORDER $LIMIT) "\
     "ORDER BY ctime DESC, connname;" \
    | psql xwgames

# Devices
LINE="SELECT id, model, variantCode as var, osvers, array_length(mtimes, 1) as mcnt, mtimes[1] as mtime, array_length(devTypes, 1) as dcnt, devTypes as dTyps, devids[1] as devid_1 FROM devices "
if [ -n "$FILTER_DEVS" ]; then
     LINE="${LINE} WHERE id IN (select UNNEST(devids) FROM (select devids from games $QUERY $ORDER $LIMIT) as devids)"
fi
LINE="$LINE ORDER BY mtimes[1] DESC;"
echo "$LINE" | psql xwgames

