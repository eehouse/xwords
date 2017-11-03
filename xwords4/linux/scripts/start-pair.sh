#!/bin/sh

set -e -u

IN_SEQ=''
HTTP='--use-http'
CURSES='--curses'
SLEEP_SEC=10000

usage() {
	[ $# -gt 0 ] && echo "ERROR: $1"
	echo "usage: $0 --in-sequence|--at-once [--no-use-http] [--gtk]"
	cat <<EOF

Starts a pair of devices meant to get into the same game. Verification
is by looking at the relay, usually with
./relay/scripts/showinplay.sh. Both should have an 'A' in the ACK
column.
EOF
	exit 1
}

while [ $# -gt 0 ]; do
	case $1 in
		--in-sequence)
			IN_SEQ=1
		;;
		--at-once)
			IN_SEQ=0
		;;
		--no-use-http)
			HTTP=''
			;;
		--gtk)
			CURSES=''
			;;
		*)
			usage "unexpected param $1"
			;;
	esac
	shift
done

[ -n "$IN_SEQ" ] || usage "missing required param"

DB_TMPLATE=_cursesdb_
LOG_TMPLATE=_curseslog_
ROOM_TMPLATE=cursesRoom

echo "delete from msgs;" | psql xwgames
echo "delete from games where room like '$ROOM_TMPLATE%';" | psql xwgames

rm -f ${DB_TMPLATE}*.sqldb
rm -f ${LOG_TMPLATE}*

PIDS=''
for GAME in $(seq 1); do
	ROOM=${ROOM_TMPLATE}${GAME}
	for N in $(seq 2); do
	# for N in $(seq 1); do
		DB=$DB_TMPLATE${GAME}_${N}.sqldb
		LOG=$LOG_TMPLATE${GAME}_${N}.log
		exec ./obj_linux_memdbg/xwords --server $CURSES --remote-player --robot Player \
			 --room $ROOM --game-dict dict.xwd $HTTP\
			 --skip-confirm --db $DB --close-stdin --server \
			 >/dev/null 2>>$LOG &
		PID=$!
		echo "launched $PID"
		if [ $IN_SEQ -eq 1 ]; then
			sleep 9
			kill $PID
			sleep 1
		elif [ $IN_SEQ -eq 0 ]; then
			PIDS="$PIDS $PID"
		fi
	done
done

[ -n "${PIDS}" ] && sleep $SLEEP_SEC
for PID in $PIDS; do
	kill $PID
done
