#!/bin/sh

HOSTNAME=localhost
ROOM_ADD=""
NGAMES=10
PORT=10999
DBUG=0
HOW_LONG=1000
DICT=dict.xwd

usage() {
    [ -n "$1" ] && echo "$1" >&2
    echo "usage: $0 \\" >&2
    echo "   [--relay <hostname>] # default: $HOSTNAME \\" >&2
    echo "   [--port <port>]      # default: $PORT \\" >&2
    echo "   [--room <anything>]  # appended to room; default: $ROOM_ADD \\" >&2
    echo "   [--games <0..N>]     # run how many games; default: $NGAMES \\" >&2
    echo "   [--dict <dict.xwd>]  # use what dict; default: $DICT \\" >&2
    echo "   [--delay <nSeconds>] # restart after nSeconds; default: $HOW_LONG \\" >&2
    echo "   [--debug]            # use debug version; default: $DBUG \\" >&2
    exit 0
}



do_one() {
    EXE=$1
    INDEX=$2

    while :; do
        ROOM="playme ${INDEX}$ROOM_ADD"
        #COMMAND="$EXE -u -0 -C \"$ROOM\" -a $HOSTNAME -r Relay -d $DICT -p $PORT"
        # it's a server if INDEX is odd
        if [ 0 -eq $((INDEX%2)) ]; then
            SERVER=" -s -N "
        fi
        #echo $COMMAND

        $EXE -u -0 -C "$ROOM" -a $HOSTNAME -r Relay -d $DICT -p $PORT $SERVER >/dev/null 2>&1 &
        PID=$!
        echo "launched $ROOM ($PID)"

        END_TIME=$(($(date +%s) + $HOW_LONG))
        while [ -d /proc/$PID ]; do
            sleep 10

            if [ $(date +%s) -ge $END_TIME ]; then
                echo "timing out $ROOM ($PID)"
                break
            fi
        done

        kill $PID               # in case we timed out
        sleep 2
    done
}



while [ -n "$1" ]; do
    case $1 in
        --delay)
            [ -n "$2" ] || usage
            HOW_LONG=$2
            shift
            ;;
        --relay)
            [ -n "$2" ] || usage
            HOSTNAME=$2
            shift
            ;;
        --port)
            [ -n "$2" ] || usage
            PORT=$2
            shift
            ;;
        --room)
            [ -n "$2" ] || usage
            ROOM_ADD=$2
            shift
            ;;
        --games)
            [ -n "$2" ] || usage
            NGAMES=$2
            shift
            ;;
        --debug)
            DBUG=1
            ;;
        *)
            usage
            ;;
    esac
    shift
done

if [ $DBUG ]; then
    EXE="./obj_linux_memdbg/xwords"
else
    EXE="./obj_linux_rel/xwords"
fi

[ -x $EXE ] || usage "file $EXE not found"

for II in $(seq $NGAMES); do
    do_one $EXE $II &
    sleep $(($HOW_LONG/$NGAMES))
done

wait
