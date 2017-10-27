#!/bin/sh

set -e -u

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
	# for N in $(seq 2); do
	for N in $(seq 1); do
		DB=$DB_TMPLATE${GAME}_${N}.sqldb
		LOG=$LOG_TMPLATE${GAME}_${N}.log
		exec ./obj_linux_memdbg/xwords --server --curses --remote-player --name Player \
			 --room $ROOM --game-dict dict.xwd \
			 --skip-confirm --db $DB --close-stdin --server \
			 >/dev/null 2>>$LOG &
		PIDS="$PIDS $!"
	done
done
echo "launched $PIDS"

#			 --use-http \

sleep 10

for PID in $PIDS; do
	kill $PID
done
