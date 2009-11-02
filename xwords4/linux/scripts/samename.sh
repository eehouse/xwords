#!/bin/bash

NGAMES=${NGAMES:-20}
ROOM=${ROOM:-room}
DICT=./dict.xwd
HOST=localhost
PORT=10999
XWORDS=obj_linux_memdbg/xwords
WAIT=10
LAUNCH_WAIT=30

RUN_NAME=$(basename $0)/_$$

usage() {
    cat <<EOF
usage: $0 
      [--ngames <n>]  # default: $NGAMES
      [--room <s>]    # default: $ROOM
      [--port <n>]    # default: $PORT
      [--host <s>]    # default: $HOST
      [--wait <d>]    # max wait time between moves; default: $WAIT
      [--launch-wait <d>]    # max wait time before launching; default: $LAUNCH_WAIT
      [--nuke]        # remove old logs; default: $NUKE
      [--save-good]   # don't nuke logs for games that complete; default: $SAVE_GOOD
Run a bunch of games all with the same name.
EOF
    exit 0
}

while [ -n "$1" ]; do
    case $1 in
        --room)
            ROOM=$2
            shift
            ;;
        --ngames)
            NGAMES=$2
            shift
            ;;
        --port)
            PORT=$2
            shift
            ;;
        --host)
            HOST=$2
            shift
            ;;
        --wait)
            WAIT=$2
            shift
            ;;
        --launch-wait)
            LAUNCH_WAIT=$2
            shift
            ;;
        --nuke)
            NUKE=1
            ;;
        --save-good)
            SAVE_GOOD=1
            ;;
        *)
            usage
            ;;
    esac
    shift
done

launch_game() {
    ROOM=$1
    LOG=$2
    N_GUESTS=$3
    N_ON_HOST=$4

    [ "$LAUNCH_WAIT" -gt 0 ] && sleep $(($RANDOM % $LAUNCH_WAIT))

    G=""

    if [ "$N_ON_HOST" -gt 0 ]; then # I'm server
        S="-s"
        for II in $(seq $N_GUESTS); do G="$G -N"; done
        for II in $(seq $N_ON_HOST); do S="$S -r Eric"; done
    else
        S=""
        for II in $(seq $N_GUESTS); do G="$G -r Eric"; done
    fi

    exec $XWORDS -u -0 -z 0:$WAIT -d $DICT -C $ROOM -q 2 -a $HOST -p $PORT $S $G 2>$LOG >/dev/null < /dev/null
}

r_safe_mod() {
    NUM=$1
    [ $NUM -eq 0 ] || NUM=$(( RANDOM % $NUM ))
    echo $NUM
}

log_name() {
    INDX=${1:-0}
    echo "/tmp/$RUN_NAME/log_${INDX}.txt"
}

fill_room() {
    INDX=$1
    NPLAYERS=$2

    while [ $NPLAYERS -ge 2 ]; do
        NTHISGAME=$((2 + $(($RANDOM % 3))))
        [ $NTHISGAME -le $NPLAYERS ] || NTHISGAME=$NPLAYERS
        NPLAYERS=$(( $NPLAYERS - $NTHISGAME ))
        GAME_STR=""

        NPLAYERS_HOST=$(( 1+ $(r_safe_mod $((NTHISGAME - 1))) ))
        NPLAYERS_GUESTS=$(( NTHISGAME - NPLAYERS_HOST ))
        for II in $(seq $NPLAYERS_HOST); do GAME_STR="${GAME_STR}H"; done

        launch_game $ROOM $(log_name $INDX) $NPLAYERS_GUESTS $NPLAYERS_HOST &
        INDX=$((1+INDX))
        while [ $NPLAYERS_GUESTS -gt 0 ]; do
            GAME_STR="${GAME_STR}:"
            NTHISGUEST=$(( 1 + $( r_safe_mod $((NPLAYERS_GUESTS - 1))) ))
            for II in $(seq $NTHISGUEST); do GAME_STR="${GAME_STR}G"; done
            NPLAYERS_GUESTS=$((NPLAYERS_GUESTS - NTHISGUEST))
            launch_game $ROOM $(log_name $INDX) $NTHISGUEST 0 &
            INDX=$((1+INDX))
        done
        echo $GAME_STR
    done
}

# Problem is we don't know what games will wind up together.  We match
# by seeds: all the games together will have the same seed.  If all
# ended properly that's a good game.  Save it (for now; delete later
# when that's the norm.)
check_logs() {
    DIR=$(dirname $(log_name))

    mkdir -p $DIR/bad
    CONNAMES=$(grep 'relayPreProcess: connName' $DIR/* | sed 's,^.*connName: "\(.*\) eehouse.org.*$,\1,' | sort -u)
    for CONNAME in $CONNAMES; do
        NFILES=$((${#CONNAME}/4))
        SEEDS=""
        GOOD_FILES=""
        FOUND=0
        while [ -n "$CONNAME" ]; do
            SEED=${CONNAME:0:4}
            FPATH=$(grep "channelSeed: $SEED" $DIR/* | sed 's,:.*,,')
            if [ -f "$FPATH" ]; then
                if grep -q XWPROTO_END_GAME $FPATH; then
                    FOUND=$((FOUND+1))
                    GOOD_FILES="$GOOD_FILES $FPATH"
                fi
            fi
            SEEDS="$SEEDS ${CONNAME:0:4}"
            CONNAME=${CONNAME:4}
        done
        if [ $FOUND -eq $NFILES ]; then
            if [ -n "$SAVE_GOOD" ]; then
                mkdir -p $DIR/good
                mv $GOOD_FILES $DIR/good
            else
                rm -f $GOOD_FILES
            fi
            echo "game with${SEEDS} completed successfully" 1>&2
        fi
    done

    if [ 0 -eq $(ls -l $DIR | grep '\-rw-r--r--' | wc -l) ]; then
        echo "no more log files; done?"
        return 0
    else
        return 1
    fi
}

watch_and_wait() {
    while :; do
        sleep 10
        check_logs && break
    done
}


[ -n "$NUKE" ] && rm -rf $(dirname $(dirname $(log_name)))
mkdir -p $(dirname $(log_name))

INDX=0
fill_room $INDX $NGAMES
INDX=$((INDX+NGAMES))

watch_and_wait
