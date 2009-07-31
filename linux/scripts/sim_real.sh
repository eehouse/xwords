#!/bin/bash

NRUNS=${NRUNS:-4}
XWORDS=${XWORDS:-"./obj_linux_memdbg/xwords"}
DICT=${DICT:-dict.xwd}
HOST=${HOST:-localhost}
PORT=${PORT:-10999}
QUIT=${QUIT:-"-q 2"}
USE_CURSES=${USE_CURSES:="yes"}
WAIT_MAX=${WAIT_MAX:-10}
KILL_INTERVAL_SECS=${KILL_INTERVAL_SECS:-0}

RUN_NAME=$(basename $0)_$$

usage() {
    echo "usage: $0 <no params>"
    cat <<EOF
The goal of this script is to simulate real-world loads on the relay,
with games starting, stopping, moves happening, etc. over a long time.
It uses ENV variables rather than commandline parms for configuration.
EOF
    echo "    env: NRUNS: number of simultaneous games; default 4"
    echo "    env: DICT: dictionary; default: dict.xwd"
    echo "    env: HOST: remote host; default: localhost"
    echo "    env: PORT: remote port; default: 10999"
    echo "    env: WAIT_MAX: most seconds to wait between moves; default: 10"
    echo "    env: KILL_INTERVAL_SECS: kill a random xwords every "
    echo "         this many seconds; 0 to disable; cur: $KILL_INTERVAL_SECS"

    exit 0
}


random() {
    # RANDOM is a bashism
#     RAND=$(dd if=/dev/urandom count=1 2>/dev/null | cksum | cut -f1 -d" ")
#     echo $RAND
    echo $RANDOM
}

game_curses() {
    NAME=$1
    COOKIE=$2
    WAIT=$3
    INDEX=$4
    SERVER_PARAMS=$5
    $XWORDS -u -d $DICT -r $NAME -a $HOST -p $PORT \
        -C $COOKIE $QUIT -z 0:$WAIT >/dev/null -0 $SERVER_PARAMS \
        2>/tmp/$RUN_NAME/log_${COOKIE}_${INDEX}.txt < /dev/null &
}

game_gtk() {
    NAME=$1
    COOKIE=$2
    WAIT=$3
    INDEX=$4
    SERVER_PARAMS=$5
    $XWORDS -d $DICT -r $NAME -a $HOST -p $PORT \
        -C $COOKIE $QUIT -z 0:$WAIT $SERVER_PARAMS \
        2>/tmp/$RUN_NAME/log_${COOKIE}_${INDEX}.txt &
}

check_logs() {
    COOKIE=$1

    if [ -d /tmp/$RUN_NAME ]; then
        OK=1
        for LOG in /tmp/$RUN_NAME/log_${COOKIE}_*.txt; do
            if ! grep -q XWPROTO_END_GAME $LOG; then
                echo "$LOG didn't end correctly; check it out."
                OK=0
            else
                rm $LOG         # save some space
            fi
        done

        [ 1 = $OK ] && echo "$(date +%T) game $COOKIE ended successfully"
    else
        echo "log directory gone..."
    fi
}

kill_at_random() {
    if [ -n "$KILL_INTERVAL_SECS" ]; then
        if [ $KILL_INTERVAL_SECS -gt 0 ]; then
            while [ -d /tmp/$RUN_NAME ]; do
                sleep $KILL_INTERVAL_SECS
                set $(pidof xwords)
                shift $(($#/2))
                PID=$1
                if [ -n "$PID" ]; then
                    echo "killing pid $PID"
                    kill -INT $PID
                fi
            done
        fi
    fi
}

do_one() {
    CROOT=${1:-$(exec sh -c 'echo $PPID')}
    INDX=1

    while [ -d /tmp/$RUN_NAME ]; do                 # loop forever
        COOKIE="$CROOT:$INDX"
        INDX=$((INDX+1))
        unset ZERO_DONE ONE_DONE TWO_DONE THREE_DONE

        TODO=$(($CROOT % 3))
        TODO=$((TODO+2))
        COUNT=0
        for NAME in Bbbbb Aaaaa Kkkkk Eeeee; do
            [ $COUNT = $TODO ] && break
            while :; do
                RAND=$(random)
                INDEX=$(( $RAND % $TODO ))
                WAIT=$(( $RAND % $WAIT_MAX ))
                case $INDEX in
                    0)
                        if [ -z "$ZERO_DONE" ]; then
                            REMOTES=""
                            for JJ in $(seq $(($TODO-1))); do
                                REMOTES="$REMOTES -N"; 
                            done
                            ZERO_DONE=1
                            if [ "$USE_CURSES" = "yes" ]; then
                                game_curses $NAME $COOKIE $WAIT $INDEX \
                                    "-s $REMOTES"
                            else
                                game_gtk  $NAME $COOKIE $WAIT $INDEX \
                                    "-s $REMOTES"
                            fi
                            break
                        fi
                        ;;
                    1)
                        if [ -z "$ONE_DONE" ]; then
                            ONE_DONE=1
                            if [ "$USE_CURSES" = "yes" ]; then
                                game_curses $NAME $COOKIE $WAIT $INDEX
                            else
                                game_gtk $NAME $COOKIE $WAIT $INDEX
                            fi
                            break
                        fi
                        ;;
                    2)
                        if [ -z "$TWO_DONE" ]; then
                            TWO_DONE=1
                            if [ "$USE_CURSES" = "yes" ]; then
                                game_curses $NAME $COOKIE $WAIT $INDEX
                            else
                                game_gtk $NAME $COOKIE $WAIT $INDEX
                            fi
                            break
                        fi
                        ;;
                    3)
                        if [ -z "$THREE_DONE" ]; then
                            THREE_DONE=1
                            if [ "$USE_CURSES" = "yes" ]; then
                                game_curses $NAME $COOKIE $WAIT $INDEX
                            else
                                game_gtk $NAME $COOKIE $WAIT $INDEX
                            fi
                            break
                        fi
                        ;;
                esac
            done
            COUNT=$((COUNT+1))
        done

        wait

        check_logs $COOKIE
        sleep $(( $(random) % 60 ))
    done
}

############################################################################
# main

[ -n "$1" ] && usage

mkdir -p /tmp/$RUN_NAME
echo "************************************************************"
echo "* created /tmp/$RUN_NAME; delete it to stop runaway script *"
echo "************************************************************"

for II in $(seq $NRUNS); do
    do_one $II &
done

kill_at_random &

wait
