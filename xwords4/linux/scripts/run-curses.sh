#!/bin/sh

# This script just runs the curses app with a set of params known to
# work. At least when it was last committed. :-)

usage() {
	echo "usage: $0 [--help] [param-for-xwords]*"
	exit 1
	}

PARAMS=''
while [ $# -gt 0 ]; do
	case $1 in
		--help)
			usage;
			;;
		*)
			PARAMS="$PARAMS $1"
			;;
	esac
	shift
done

WD=$(cd $(dirname $0)/.. && pwd)
cd $WD
./obj_linux_memdbg/xwords --curses --name Eric --name Kati \
						  --dict-dir ./ --game-dict ../dict.xwd \
						  $PARAMS \
