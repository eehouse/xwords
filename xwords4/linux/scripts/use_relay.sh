#!/bin/sh

COOKIE=$$
GUEST_COUNT=1

usage() {
    echo "usage: $0: \\"
    echo "           [-q]  # quit after done \\"
    echo "           [-g <1..3>] # num guest devices; default: 1 \\"
    exit 0
}

while [ -n "$1" ]; do
    case $1 in
        -q)
            QUIT="-q 2"
            ;;
        -g)
            GUEST_COUNT=$2
            if [ $GUEST_COUNT -lt 1 -o $GUEST_COUNT -gt 3 ]; then
                usage
            fi
            shift
            ;;
        *)
            usage
            ;;
    esac
    shift
done

NUM=0                           # not strictly needed....
for NAME in Kati Brynn Ariela; do
    ./obj_linux_memdbg/xwords -d dict.xwd -r $NAME -a localhost -p 10999 -C $COOKIE $QUIT &

    REMOTES="$REMOTES -N"
    NUM=$((NUM+1))
    [ $NUM -ge $GUEST_COUNT ] && break
done

./obj_linux_memdbg/xwords -d dict.xwd -r Eric -s $REMOTES -a localhost -p 10999 -C $COOKIE $QUIT &

wait

