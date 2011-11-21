#!/bin/bash

#set -u
set -e -u

SEED=""
RELAY_LOG="../relay/xwrelay.log"
NDEVS=${NDEVS:-2}

usage() {
    echo "usage: $(basename $0) [--seed RANDOM_SEED]"
    exit 0
}

logname() {
    echo ${DIR}/game_${1}.log
}

PID=$$
echo "**********************************************************************"
echo "pid: $PID"
echo "**********************************************************************"

while [ $# -gt 0 ]; do
    case $1 in
        --seed)
            [ $# -gt 1 ] || usage
            shift
            SEED=$1
            ;;
        *) usage
        ;;
    esac
    shift
done

ROOM=ROOM_$PID
DIR=$(basename $0)_$PID
DICT=dict.xwd

APP=./obj_linux_memdbg/xwords
COMMON_ARGS="--room $ROOM --curses --robot Eric --remote-player --game-dict $DICT --quit-after 2"

mkdir -p $DIR

if [ -e $RELAY_LOG ]; then
    echo "removing xwrelay.log"
    rm $RELAY_LOG
else
    echo "xwrelay.log not found"
fi

# Run once to connect each with the relay
for NUM in $(seq 0 $((NDEVS-1))); do
    LOG="$(logname $NUM)"
    ARGS=$COMMON_ARGS
    if [ -n "$SEED" ]; then
        ARGS="$ARGS --seed $((SEED+NUM))"
    fi
    $APP $ARGS --file $DIR/game_${NUM}.xwg > /dev/null 2>>$LOG &
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
    for NUM in $(seq 0 $((NDEVS-1))); do
        LOG="$(logname $NUM)"
        RELAYID=$(./scripts/relayID.sh --short $LOG)
        MSG_COUNT=$(../relay/rq -m $RELAYID 2>/dev/null | sed 's,^.*-- ,,')
        if [ "$MSG_COUNT" -gt 0 ]; then
            WORK_DONE=true

            ARGS=$COMMON_ARGS
            if [ -n "$SEED" ]; then
                ARGS="$ARGS --seed $((SEED+NUM))"
            fi
            $APP $ARGS --file $DIR/game_${NUM}.xwg --with-nbs $NBS > /dev/null 2>>$LOG &
            PID1=$!

            ../relay/rq -f $RELAYID -b $NBS

            wait $PID1 || true
            sleep 1             # make it easy to see sequences in the logs
        fi
        sleep 1
    done
    if [ -z "$WORK_DONE" ]; then
        ZERO_COUNT=$((ZERO_COUNT+1))
    fi
done

# Check first if we got to the point where one device recognized that
# the game's over.  Strictly speaking we need to get beyond that, but
# reaching it is the first step.  Debug failure to get that far first.
ENDED=""
for NUM in $(seq 0 $((NDEVS-1))); do
    LOG="$(logname $NUM)"
    if grep -q 'waiting for server to end game' $LOG; then
        ENDED=1
        break;
    fi
done

if [ -z "$ENDED" ]; then
    for NUM in $(seq 0 $((NDEVS-1))); do
        LOG="$(logname $NUM)"
        if ! grep -q 'all remaining tiles' $LOG; then
            echo "$LOG didn't seem to end correctly"
            mv $RELAY_LOG $DIR
            break
        fi
    done
fi

echo "$0 done (pid: $PID)"
echo ""
