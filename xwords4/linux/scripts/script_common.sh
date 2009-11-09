#!/bin/sh

r_safe_mod() {
    NUM=$1
    [ $NUM -eq 0 ] || NUM=$(( RANDOM % $NUM ))
    echo $NUM
}

make_game_str() {
    NTHISGAME=$1

    NPLAYERS_HOST=$(( 1+ $(r_safe_mod $((NTHISGAME - 1))) ))
    GAME_STR="$NPLAYERS_HOST"
    NPLAYERS_GUESTS=$(( NTHISGAME - NPLAYERS_HOST ))

    while [ $NPLAYERS_GUESTS -gt 0 ]; do
        NTHISGUEST=$(( 1 + $( r_safe_mod $((NPLAYERS_GUESTS - 1))) ))
        GAME_STR="${GAME_STR}$NTHISGUEST"
        NPLAYERS_GUESTS=$((NPLAYERS_GUESTS - NTHISGUEST))
    done

    echo "$GAME_STR"
}

cmd_for() {
    GAME_STR="$1"
    INDX="$2"
    ROOM="$3"
    GAME="$4"

    LOCALS=""
    HOSTSTR=""

    LOCAL_COUNT=${GAME_STR:$INDX:1}
    for II in $(seq 0 $(($LOCAL_COUNT-1))); do LOCALS="$LOCALS -r Eric"; done

    if [ 0 -eq $INDX ]; then    # host?
        HOSTSTR="-s"
        for II in $(seq 1 $((${#GAME_STR}-1))); do
            REMOTE_COUNT=${GAME_STR:$II:1}
            for JJ in $(seq $REMOTE_COUNT); do HOSTSTR="$HOSTSTR -N"; done
        done
    fi

    RESULT="$XWORDS -d $DICT -a $HOST -p $PORT $CURSES_ARGS -C $ROOM -q 2 -z 0:$WAIT"
    RESULT="${RESULT} $LOCALS $HOSTSTR"
    [ -n "$GAME" ] && RESULT="$RESULT -f $GAME"
    echo "$RESULT"
}

log_name() {
    RUN_NAME=${RUN_NAME:-$(basename $0)/_$$}
    INDX=${1:-0}
    echo "/tmp/$RUN_NAME/log_${INDX}.txt"
}

game_name() {
    RUN_NAME=${RUN_NAME:-$(basename $0)/_$$}
    INDX=${1:-0}
    echo "/tmp/$RUN_NAME/game_${INDX}.xwg"
}

check_logs_done() {
    ERR=0
    for LOG in "$*"; do
        if ! grep -q XWPROTO_END_GAME $LOG; then
            ERR=1
            break
        fi
    done
    return $ERR
}
