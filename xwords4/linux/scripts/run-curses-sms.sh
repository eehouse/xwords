#!/bin/sh

usage() {
	[ $# -ge 1 ] && echo "ERROR: $1"
	echo "usage: %0 <self-key> <other-key>"
	echo "create one of two apps differentiated with <key> instance"
	echo "typically called .e.g $0 Eric, then use the invitE menuitem to create second game"
	exit 1
}

SELF=''

while [ $# -gt 0 ]; do
	case $1 in
		--help|-h)
			usage
			;;
		*)
			[ $# -eq 1 ] || usage "requires one parameter"
			SELF=$1
			;;
	esac
	shift
done

[ -n "$SELF" ] || usage "param is not optional"

./obj_linux_memdbg/xwords --curses --db ${SELF}.db --name "$SELF" --remote-player \
						  --sms-number "$SELF" --invitee-sms-number "$SELF" \
						  --server --game-dict dict.xwd \
						  2>"$SELF"_log.txt \

