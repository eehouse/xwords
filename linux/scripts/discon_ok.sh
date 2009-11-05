#!/bin/bash

HOST_COUNTER=0
NGAMES=1                        # games, not hosts
SAME_ROOM=""                    # unset means use different
DICT=./dict.xwd
HOST=localhost
PORT=10999
XWORDS=./obj_linux_memdbg/xwords
WAIT=10

RUN_NAME=$(basename $0)/_$$

. ./scripts/script_common.sh

exec_cmd() {
    CMD="$1"
    LOG="$2"
    [ -z "$LOG" ] && LOG=/dev/null
    echo "launching: $CMD"
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

    declare -a CMDS
    declare -a PIDS
    declare -a LOGS

    for JJ in $(seq ${#GAME_STR}); do
        GAME=$(game_name $((COUNTER+INDX)))
        CMD=$(cmd_for $GAME_STR $(($JJ-1)) "room" $GAME)
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

    # Now launch them again

    for JJ in $(seq 0 $((${#GAME_STR}-1))); do
        exec_cmd "${CMDS[$JJ]}" "${LOGS[$JJ]}" &
        PIDS[$JJ]=$!
    done

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

#     for JJ in $(seq ${#GAME_STR}); do
#         CMDS[$HOST_COUNTER]=$(cmd_for $GAME_STR $(($JJ-1)) $ROOM $LOG)
#         echo "${CMDS[$HOST_COUNTER]}"
#         HOST_COUNTER=$((HOST_COUNTER+1))
#     done
done

# for II in $(seq 0 $((HOST_COUNTER-1))); do
#     LOG=$(log_name $HOST_COUNTER)
#     exec_cmd "${CMDS[$II]}" $LOG &
#     PIDS[$II]=$!
#     sleep 2
# done

# sleep 10

# for II in $(seq 0 $((HOST_COUNTER-1))); do
#     echo "kill ${PIDS[$II]}"
#     kill ${PIDS[$II]}
# done

wait
