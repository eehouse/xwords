#!/bin/bash

NGAMES=${NGAMES:-1}
NROOMS=${NROOMS:-1}
HOST=${HOST:-localhost}
PORT=${PORT:-10997}
TIMEOUT=${TIMEOUT:-400}

NAMES=(Brynn Ariela Kati Eric)

LOGDIR=$(basename $0)_logs
[ -d $LOGDIR ] && mv $LOGDIR /tmp/${LOGDIR}_$$
mkdir -p $LOGDIR
DONEDIR=$LOGDIR/done
mkdir -p $DONEDIR

USE_GTK=${USE_GTK:-FALSE}

if [ $USE_GTK = FALSE ]; then
    PLAT_PARMS="-u -0"
fi

usage() {
    echo "usage: [env=val *] $0" 1>&2
    echo " current env variables and their values: " 1>&2
    for VAR in NGAMES NROOMS USE_GTK TIMEOUT HOST PORT; do
        echo "$VAR:" $(eval "echo \$${VAR}") 1>&2
    done
    exit 0
}

connName() {
    LOG=$1
    grep 'got_connect_cmd: connName' $LOG | \
        sed 's,^.*connName: \"\(.*\)\"$,\1,' | \
        sort -u 
}

do_device() {
    GAME=$1
    DEV=$2
    NDEVS=$3

    LOG=${LOGDIR}/${GAME}_${DEV}_LOG.txt
    rm -f $LOG
    ROOM=ROOM_$((GAME%NROOMS))

    FILE="GAME_${GAME}_${DEV}.xwg"
    rm -f $FILE
    for II in $(seq 2 $NDEVS); do
        OTHERS="-N $OTHERS"
    done

    STOPTIME=$(($(date "+%s") + TIMEOUT))
    while :; do
        sleep $((RANDOM%5))
        ./obj_linux_memdbg/xwords -C $ROOM -r ${NAMES[$DEV]} $OTHERS \
            -d dict.xwd -p $PORT -a $HOST -f $FILE -z 1:3 $PLAT_PARMS \
            >/dev/null 2>>$LOG &
        PID=$!
        sleep $((RANDOM%10+5))
        while :; do
            kill $PID 2>/dev/null
            [ -d /proc/$PID ] || break
            sleep 1
        done

        if grep -q 'all remaining tiles' $LOG; then
            echo -n "device $DEV in game $GAME succeeded ($LOG $(connName $LOG)) "
            date
            mv $LOG $DONEDIR
            break
        elif [ $(date "+%s") -ge $STOPTIME ]; then
            echo -n "timeout exceeded for device $DEV in game $GAME " 
            echo -n "($LOG $(connName $LOG)) "
            date
            break
        elif [ ! -d $LOGDIR ]; then
            break;
        fi
    done
}

do_game() {
    INDEX=$1
    NDEVS=$(($RANDOM%3+2))

    for DEV in $(seq $NDEVS); do
        do_device $INDEX $DEV $NDEVS &
    done
}

while [ -n "$1" ]; do
    case $1 in
        *) usage
            ;;
    esac
    shift
done

for GAME in $(seq 1 $NGAMES); do
    do_game $GAME
done

wait

# for LOG in $LOGDIR/*LOG.txt; do
#     echo -n "$LOG "
#     grep 'got_connect_cmd: connName' $LOG | \
#         sed 's,^.*connName: \"\(.*\)\"$,\1,' | \
#         sort -u 
# done
