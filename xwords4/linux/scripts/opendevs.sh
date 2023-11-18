#!/bin/bash
set -u -e

FILES=""
ARGS=""
APP=./obj_linux_memdbg/xwords

usage() {
	echo "usage: $0 [args...] file1.db [file2..n.db]"
	echo "opens them with CrossWords, assuming they're dbs."
	exit 0
	}

while [ $# -gt 0 ]; do
	if [ '--help' == $1 ]; then
	   usage
	elif [ -f $1 ]; then
		if file -L $1 | grep -q 'SQLite 3.x database'; then
			FILES="${FILES} $1"
		else
			ARGS="${ARGS} $1"
		fi
	else
		ARGS="${ARGS} $1"
	fi
	shift
done

for FILE in $FILES; do
	LOGFILE="${FILE/.db/_log.txt}"
	echo >> $LOGFILE
	echo "******************** launch by $0 ********************" >> $LOGFILE
	exec ${APP} ${ARGS} --db $FILE 2>>$LOGFILE &
done
