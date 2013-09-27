#!/bin/sh

set -e -u

CONNNAME=""
ROOMS=""
DEVID=""
SHOW_SENT=""

LIMIT=10000

usage() {
    echo "usage: $0 [--connname <n>] [--room <room>]* [--devid <id>] [--show-sent]"
    exit 1
}

while [ $# -gt 0 ]; do
    case $1 in
	--devid)
	    DEVID=$2
	    shift
	    ;;
        --connname)
            CONNNAME=$2
            shift
            ;;
        --room)
	    if [ -n "$ROOMS" ]; then
		ROOMS="${ROOMS}, '$2'"
	    else 
		ROOMS="'$2'"
	    fi
            shift
            ;;
	--show-sent)
	    SHOW_SENT=1
	    ;;
        *) usage
            ;;
    esac
    shift
done

QUERY="WHERE NOT -NTOTAL = sum_array(nperdevice)"
if [ -n "$CONNNAME" ]; then
    QUERY="${QUERY} AND connname = '$CONNNAME' "
fi
if [ -n "$ROOMS" ]; then
    QUERY="${QUERY} AND room in ($ROOMS) "
fi
if [ -n "$DEVID" ]; then
    QUERY="${QUERY} AND $DEVID = ANY(devids) "
fi

echo "relay pids: $(pidof xwrelay)"

# Games
echo "SELECT dead as d,connname,cid,room,lang as lg,clntVers as cv ,ntotal as t,nperdevice as nPerDev,nsents as snts, seeds,devids,tokens,ack, mtimes "\
     "FROM games $QUERY ORDER BY NOT dead, connname LIMIT $LIMIT;" \
    | psql xwgames

# Messages
CMD="SELECT id, connname, hid, devid, ctime, msg64 FROM msgs WHERE "
if [ -z "$SHOW_SENT" ]; then
    CMD="$CMD stime = 'epoch' AND "
fi
CMD="$CMD (connname IN (SELECT connname from games $QUERY group by connname) "
CMD="$CMD OR (connname IS NULL AND devid IN (SELECT unnest(devids) as devid from games $QUERY GROUP BY devid)) ) "
CMD="$CMD ORDER BY ctime, connname LIMIT $LIMIT;"
echo "$CMD" | psql xwgames

# Devices
LINE="SELECT id, model, osvers, array_length(mtimes, 1) as mcnt, mtimes[1] as mtime, array_length(devTypes, 1) as dcnt, devTypes FROM devices "
LINE="$LINE WHERE id IN (SELECT unnest(devids) from games $QUERY);"
echo "$LINE" | psql xwgames

