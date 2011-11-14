#!/bin/sh

set -u -e


PARAMS="--curses --robot Kati --remote-player --game-dict dict.xwd --quit-after 1 --sort-tiles"


run() {
    SEED=$1
    LOG=LOG__${SEED}.txt
    ROOM=ROOM_${SEED}
    > $LOG
    ./obj_linux_memdbg/xwords $PARAMS --room $ROOM \
        --seed $SEED >/dev/null 2>>$LOG &
    sleep 1
    ./obj_linux_memdbg/xwords $PARAMS --room $ROOM \
        --seed $((SEED+1000)) >/dev/null 2>>$LOG &
}

for SEED in $(seq 1 1000); do
    echo "trying seed $SEED"
    run $SEED
    wait
done
