#!/bin/bash

NGAMES=${NGAMES:-1}
NROOMS=${NROOMS:-1}

USE_GTK=${USE_GTK:-FALSE}

if [ $USE_GTK = FALSE ]; then
    PLAT_PARMS="-u -0"
else
    PLAT_PARMS="-z 1:10"
fi

usage() {
    echo "usage: [env=val *] $0" 1>&2
    echo " current env variables and their values: " 1>&2
    for VAR in NGAMES NROOMS USE_GTK; do
        echo "$VAR:" $(eval "echo \$${VAR}") 1>&2
    done
    exit 0
}

do_device() {
    GAME=$1
    DEV=$2
    NDEVS=$3
    LOG=$4
    ROOM=ROOM_$((GAME%NROOMS))

    FILE="GAME_${GAME}_${DEV}.xwg"
    rm -f $FILE
    for II in $(seq 2 $NDEVS); do
        OTHERS="-N $OTHERS"
    done

    while :; do
        sleep $((RANDOM%5))
        ./obj_linux_memdbg/xwords -C $ROOM -r edd $OTHERS \
            -d dict.xwd -f $FILE $PLAT_PARMS >/dev/null 2>>$LOG &
        PID=$!
        sleep $((RANDOM%10+10))
        kill $PID 2>/dev/null

        [ $(grep -c 'all remaining tiles' $LOG) -eq $NDEVS ] && break
        pidof xwrelay > /dev/null || break
    done
}

do_game() {
    INDEX=$1
    NDEVS=$(($RANDOM%3+2))

    LOG="LOG_${INDEX}.txt"
    rm -f $LOG

    for DEV in $(seq $NDEVS); do
        do_device $INDEX $DEV $NDEVS $LOG &
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
