#!/bin/bash

#set -u
set -e -u

PID=$$
echo "**********************************************************************"
echo "pid: $PID"
echo "**********************************************************************"

ROOM=ROOM_$PID
DIR=$(basename $0)_$PID
DICT=dict.xwd

APP=./obj_linux_memdbg/xwords
COMMON_ARGS="--room $ROOM --curses --robot Eric --remote-player --game-dict $DICT --quit-after 2"

mkdir -p $DIR

# Run once to connect each with the relay
for NUM in $(seq 1 2); do
    LOG=$DIR/game_${NUM}.log
    $APP $COMMON_ARGS --file $DIR/game_${NUM}.xwg > /dev/null 2>$LOG &
    PID1=$!
    sleep 4
    kill $PID1
    wait $PID1
done

# run apps until done
NBS=$DIR/nbs
ZERO_COUNT=0
while [ $ZERO_COUNT -lt 2 ]; do
    WORK_DONE=""
    for NUM in $(seq 1 2); do
        LOG=$DIR/game_${NUM}.log
        RELAYID=$(./scripts/relayID.sh --short $LOG)
        MSG_COUNT=$(../relay/rq -m $RELAYID 2>/dev/null | sed 's,^.*-- ,,')
        if [ "$MSG_COUNT" -gt 0 ]; then
            echo "*** $MSG_COUNT messages available ***"
            WORK_DONE=true

            $APP $COMMON_ARGS --file $DIR/game_${NUM}.xwg \
                --with-nbs $NBS > /dev/null &
            PID1=$!

            ../relay/rq -f $RELAYID -b $NBS

            wait $PID1 || true
        else
            sleep 1
        fi
    done
    if [ -z "$WORK_DONE" ]; then
        ZERO_COUNT=$((ZERO_COUNT+1))
    fi
done

echo "$0 done (pid: $PID)"
