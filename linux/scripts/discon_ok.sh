#!/bin/bash

HOST_COUNTER=0
NGAMES=5                        # games, not hosts
SAME_ROOM=""                    # unset means use different
DICT=./dict.xwd
HOST=localhost
PORT=10999
XWORDS=./obj_linux_memdbg/xwords
WAIT=3
CURSES_ARGS="-u -0"

RUN_NAME=$(basename $0)/_$$

. ./scripts/script_common.sh

exec_cmd() {
    CMD="$1"
    LOG="$2"
    [ -z "$LOG" ] && LOG=/dev/null
    exec $CMD </dev/null >/dev/null 2>>$LOG
    echo "got here???"
}

# Given game strings, build commands for them, run them all for a
# while, and then start running them one at a time so they can
# progress IFF the relay allows disconnected communication
run_game_set() {
    GAME_STR=$1
    INDX=$2

    COUNTER=0
    ROOM="ROOM_${INDX}"

    declare -a CMDS
    declare -a PIDS
    declare -a LOGS

    for JJ in $(seq ${#GAME_STR}); do
        GAME=$(game_name $((COUNTER+INDX)))
        CMD=$(cmd_for $GAME_STR $(($JJ-1)) $ROOM $GAME)
        CMDS[$COUNTER]="$CMD"

        LOG=$(log_name $((COUNTER+INDX)))
        LOGS[$COUNTER]="$LOG"

        exec_cmd "$CMD" $LOG &
        PIDS[$COUNTER]=$!

        COUNTER=$((COUNTER+1))
    done


    # Loop until all games in set have connected, then kill them
    while :; do
        sleep 2
        FOUND=0
        for LOG in ${LOGS[*]}; do
            if [ -f $LOG ]; then
                if grep -q "relayPreProcess: connName" $LOG; then
                    FOUND=$((FOUND+1))
                fi
            fi
        done
        [ $FOUND -lt ${#LOGS[*]} ] && continue

        echo "all games started!"
        kill ${PIDS[*]}
        break
    done

    # Now loop, running and killing each in turn, until the game is
    # finished.  Eventually I'll need to deal with games that don't
    # finish correctly!

    while [ -z "$GAME_DONE" ]; do
        for JJ in $(seq 0 $((${#GAME_STR}-1))); do
            exec_cmd "${CMDS[$JJ]}" "${LOGS[$JJ]}" &
            PID=$!

            sleep $((WAIT+2))

            kill $PID

            if check_logs_done ${LOGS[*]}; then
                echo "game looks done: ${LOGS[*]}"
                GAME_DONE=TRUE
                break
            fi
        done
    done

    # Finally, launch them so they can sync up and get the cref out of
    # the MSGONLY state
    for JJ in $(seq ${#GAME_STR}); do
        exec_cmd "${CMDS[$JJ]}" "${LOGS[$JJ]}" &
        PIDS[$JJ]=$!
    done

    sleep 2
    kill ${PIDS[*]}
}

echo "mkdir -p $(dirname $(log_name))"
mkdir -p $(dirname $(log_name))

INDX=0
for II in $(seq $NGAMES); do
    N_THIS_GAME=$(( 2 + $(($RANDOM % 3 ))))
    GAME_STR=$(make_game_str $N_THIS_GAME)
    echo $GAME_STR

    run_game_set $GAME_STR $INDX &
    INDX=$((INDX+4))
done

wait
