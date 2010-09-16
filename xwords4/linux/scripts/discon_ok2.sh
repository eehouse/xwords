#!/bin/bash

do_device() {
    GAME=$1
    DEV=$2
    NDEVS=$3
    LOG=$4

    FILE="GAME_${GAME}_${DEV}.xwg"
    rm -f $FILE
    for II in $(seq 2 $NDEVS); do
        OTHERS="-N $OTHERS"
    done

    while :; do
        ./obj_linux_memdbg/xwords -C foo -r edd $OTHERS \
            -d dict.xwd -f $FILE 2>>$LOG &
        PID=$!
        sleep $((RANDOM%5+1))
        kill $PID

        [ $(grep -c 'all remaining tiles' $LOG) -eq $NDEVS ] && break
        pidof xwrelay || break
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

for GAME in $(seq 1 1); do
    do_game $GAME
done
