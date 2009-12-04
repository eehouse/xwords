#!/bin/bash

NRUNS=${NRUNS:-4}
XWORDS=${XWORDS:-"./obj_linux_memdbg/xwords"}
DICT=${DICT:-dict.xwd}
HOST=${HOST:-localhost}
PORT=${PORT:-10999}
QUIT=${QUIT:-"-q 2"}
USE_CURSES=${USE_CURSES:="yes"}
WAIT_MAX=${WAIT_MAX:-10}
CONN_WAIT_MAX=${CONN_WAIT_MAX:-15}
KILL_INTERVAL_SECS=${KILL_INTERVAL_SECS:-0}
VERBOSE=""
ROLES="${ROLES:-0 1 2 3}"

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
    echo "    env: CONN_WAIT_MAX: most seconds to wait (per device) before"
    echo "         sending connect; cur: $CONN_WAIT_MAX"
    echo "    env: KILL_INTERVAL_SECS: kill a random xwords every "
    echo "         this many seconds; 0 to disable; cur: $KILL_INTERVAL_SECS"
    echo "    env: ROLES: what hosts to handle here; cur: $ROLES"
    echo "    env: USE_CURSES; cur: $USE_CURSES"
    echo "    env: DUPES; -L to duplicate all packets; cur: $DUPES"
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
    $XWORDS -u -d $DICT $DUPES -r $NAME -a $HOST -p $PORT \
        -C $COOKIE $QUIT -z 0:$WAIT >/dev/null -0 $SERVER_PARAMS \
        2>/tmp/$RUN_NAME/log_${COOKIE}_${INDEX}.txt < /dev/null &
    echo $!
}

game_gtk() {
    NAME=$1
    COOKIE=$2
    WAIT=$3
    INDEX=$4
    SERVER_PARAMS=$5
    $XWORDS -d $DICT $DUPES -r $NAME -a $HOST -p $PORT \
        -C $COOKIE $QUIT -z 0:$WAIT $SERVER_PARAMS \
        2>/tmp/$RUN_NAME/log_${COOKIE}_${INDEX}.txt &
}

check_logs() {
    COOKIE=$1

    if [ -d /tmp/$RUN_NAME ]; then
        mkdir -p /tmp/$RUN_NAME/bad
        OK=1
        for LOG in /tmp/$RUN_NAME/log_${COOKIE}_*.txt; do
            if ! grep -q XWPROTO_END_GAME $LOG; then
                echo "$LOG didn't end correctly; check it out."
                tail -n 10 $LOG
                mv $LOG /tmp/$RUN_NAME/bad
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

rearrange() {
    test -n $1 || return ""

    ARGS="$*"
    NARGS=$#

    # hack so 0 always comes out first
    RESULT="0"
    NARGS=$((NARGS-1))
    ARGS=${ARGS/0/}

    if [ $NARGS -gt 0 ]; then
        NFOUND=0
        while [ $NFOUND -lt $NARGS ]; do
            while :; do
                set $ARGS
                INDEX=$(($RANDOM % $NARGS))
                shift $INDEX
                VAL=$1
            # test that VAL's not yet in result
                [ x = "${RESULT/*$VAL*/x}" ] || break
            done

            RESULT="$RESULT $VAL"
            NFOUND=$((NFOUND+1))
        done
    fi

    echo $RESULT
}

do_one() {
    CROOT=${1:-$(exec sh -c 'echo $PPID')}
    INDX=1

    while [ -d /tmp/$RUN_NAME ]; do                 # loop forever
        COOKIE="Test_$CROOT:$INDX"
        INDX=$((INDX+1))

        TODO=$(($CROOT % 3))
        TODO=$((TODO+2))
        COUNT=0

        PIDS=""
        LOC_ROLES=$(rearrange $ROLES)

        for INDEX in $LOC_ROLES; do
            [ $COUNT -ge $TODO ] && break
            [ $INDEX -ge $TODO ] && continue

            RAND=$(random)
            WAIT=$(( $RAND % $WAIT_MAX ))
            CONN_WAIT=$(( $RAND % $CONN_WAIT_MAX ))
            case $INDEX in
                    0)
                    sleep $CONN_WAIT
                    NAME=Bbbbb
                    REMOTES=""
                    for JJ in $(seq $(($TODO-1))); do
                        REMOTES="$REMOTES -N"; 
                    done
                    if [ "$USE_CURSES" = "yes" ]; then
                        TMP=$(game_curses $NAME $COOKIE $WAIT $INDEX "-s $REMOTES")
                        PIDS="$PIDS $TMP"
                    else
                        game_gtk  $NAME $COOKIE $WAIT $INDEX \
                            "-s $REMOTES"
                    fi
                    sleep 2     # ensure host connects first
                    ;;
                1)
                    sleep $CONN_WAIT
                    NAME=Aaaaa
                    if [ "$USE_CURSES" = "yes" ]; then
                        PIDS="$PIDS $(game_curses $NAME $COOKIE $WAIT $INDEX)"
                    else
                        game_gtk $NAME $COOKIE $WAIT $INDEX
                    fi
                    ;;
                2)
                    sleep $CONN_WAIT
                    NAME=Kkkkk
                    if [ "$USE_CURSES" = "yes" ]; then
                        PIDS="$PIDS $(game_curses $NAME $COOKIE $WAIT $INDEX)"
                    else
                        game_gtk $NAME $COOKIE $WAIT $INDEX
                    fi
                    ;;
                3)
                    sleep $CONN_WAIT
                    NAME=Eeeee
                    if [ "$USE_CURSES" = "yes" ]; then
                        PIDS="$PIDS $(game_curses $NAME $COOKIE $WAIT $INDEX)"
                    else
                        game_gtk $NAME $COOKIE $WAIT $INDEX
                    fi
                    ;;
            esac
            COUNT=$((COUNT+1))
        done

        test -n "$VERBOSE" && echo "watching:$PIDS"
        # allow time for 25 moves, then timeout
        END_TIME=$(($(date +%s) + $((WAIT_MAX*20))))
        while [ -d /tmp/$RUN_NAME -a -n "$PIDS" ]; do
            sleep 10
            for PID in $PIDS; do
                if [ ! -d /proc/$PID ]; then
                    kill $PIDS 2>/dev/null
                    PIDS=""
                fi
            done
            if [ $(date +%s) -ge $END_TIME ]; then
                echo "killing $PIDS because out of time"
                kill $PIDS 2>/dev/null
                break
            fi
        done

        check_logs $COOKIE
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
