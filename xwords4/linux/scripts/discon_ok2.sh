#!/bin/bash
set -u -e

NGAMES=${NGAMES:-1}
NROOMS=${NROOMS:-1}
HOST=${HOST:-localhost}
PORT=${PORT:-10997}
TIMEOUT=${TIMEOUT:-$((NGAMES*60+500))}
DICTS=${DICTS:-dict.xwd}
SAVE_GOOD=${SAVE_GOOD:-YES}
RESIGN_RATIO=${RESIGN_RATIO:-$((NGAMES/3))}

declare -a DICTS_ARR
for DICT in $DICTS; do
    DICTS_ARR[${#DICTS_ARR[*]}]=$DICT
done

NAMES=(UNUSED Brynn Ariela Kati Eric)

LOGDIR=$(basename $0)_logs
[ -d $LOGDIR ] && mv $LOGDIR /tmp/${LOGDIR}_$$
mkdir -p $LOGDIR

if [ "$SAVE_GOOD" = YES ]; then
    DONEDIR=$LOGDIR/done
    mkdir -p $DONEDIR
fi
DEADDIR=$LOGDIR/dead
mkdir -p $DEADDIR

USE_GTK=${USE_GTK:-FALSE}

declare -A PIDS
declare -A CMDS
declare -A FILES
declare -A LOGS

PLAT_PARMS=""
if [ $USE_GTK = FALSE ]; then
    PLAT_PARMS="--curses --close-stdin"
fi

usage() {
    echo "usage: [env=val *] $0" 1>&2
    echo " current env variables and their values: " 1>&2
    for VAR in NGAMES NROOMS USE_GTK TIMEOUT HOST PORT DICTS SAVE_GOOD; do
        echo "$VAR:" $(eval "echo \$${VAR}") 1>&2
    done
    exit 1
}

connName() {
    LOG=$1
    grep 'got_connect_cmd: connName' $LOG | \
        sed 's,^.*connName: \"\(.*\)\"$,\1,' | \
        sort -u 
}

while [ "$#" -gt 0 ]; do
    case $1 in
        *) usage
            ;;
    esac
    shift
done

declare -A CHECKED_ROOMS
check_room() {
    ROOM=$1
    if [ -z ${CHECKED_ROOMS[$ROOM]:-""} ]; then
        NUM=$(echo "SELECT COUNT(*) FROM games "\
            "WHERE NOT dead "\
            "AND ntotal!=sum_array(nperdevice) "\
            "AND ntotal != -sum_array(nperdevice) "\
            "AND room='$ROOM'" |
            psql -q -t xwgames)
        NUM=$((NUM+0))
        if [ "$NUM" -gt 0 ]; then
            echo "$ROOM in the DB has unconsummated games.  Remove them."
            exit 1
        else
            CHECKED_ROOMS[$ROOM]=1
        fi
    fi
}

build_cmds() {
    COUNTER=0
    for GAME in $(seq 1 $NGAMES); do
        ROOM=ROOM_$((GAME % NROOMS))
        check_room $ROOM
        NDEVS=$(($RANDOM%3+2))
        DICT=${DICTS_ARR[$((GAME%${#DICTS_ARR[*]}))]}
        # make one in three games public
        PUBLIC=""
        [ $((RANDOM%3)) -eq 0 ] && PUBLIC="--make-public --join-public"

        OTHERS=""
        for II in $(seq 2 $NDEVS); do
            OTHERS="--remote-player $OTHERS"
        done

        for DEV in $(seq $NDEVS); do
            FILE="${LOGDIR}/GAME_${GAME}_${DEV}.xwg"
            LOG=${LOGDIR}/${GAME}_${DEV}_LOG.txt
            touch $LOG          # so greps won't show errors
            CMD="./obj_linux_memdbg/xwords --room $ROOM"
            CMD="$CMD --robot ${NAMES[$DEV]} --robot-iq=$((1 + (RANDOM%100))) "
            CMD="$CMD $OTHERS --dict=$DICT --port=$PORT --host=$HOST "
            CMD="$CMD --file=$FILE --slow-robot 1:3 $PLAT_PARMS"
            CMD="$CMD $PUBLIC"
            CMDS[$COUNTER]=$CMD
            FILES[$COUNTER]=$FILE
            LOGS[$COUNTER]=$LOG
            PIDS[$COUNTER]=0
            COUNTER=$((COUNTER+1))
        done
    done
    echo "finished creating $COUNTER commands"
} # build_cmds

launch() {
    LOG=${LOGS[$1]}
    CMD="${CMDS[$1]}"
    exec $CMD >/dev/null 2>>$LOG
}

close_device() {
    ID=$1
    MVTO=$2
    REASON="$3"
    if [ ${PIDS[$ID]} -ne 0 ]; then
        kill ${PIDS[$ID]} 2>/dev/null
        wait ${PIDS[$ID]}
    fi
    unset PIDS[$ID]
    unset CMDS[$ID]
    echo "closing game: $REASON" >> ${LOGS[$ID]}
    if [ -n "$MVTO" ]; then
        [ -f ${FILES[$ID]} ] && mv ${FILES[$ID]} $MVTO
        mv ${LOGS[$ID]} $MVTO
    else
        rm -f ${FILES[$ID]}
        rm -f ${LOGS[$ID]}
    fi
    unset FILES[$ID]
    unset LOGS[$ID]
}

kill_from_logs() {
    CMDS=""
    while [ $# -gt 0 ]; do
        LOG=${LOGS[$1]}
        RELAYID=$(./scripts/relayID.sh $LOG)
        if [ -n "$RELAYID" ]; then
            CMDS="$CMDS -d $RELAYID"
        fi
        shift
    done
    if [ -n "$CMDS" ]; then
        echo "../relay/rq $CMDS"
        ../relay/rq -a $HOST $CMDS 2>/dev/null || true
    fi
}

kill_from_log() {
    LOG=$1
    RELAYID=$(./scripts/relayID.sh $LOG)
    if [ -n "$RELAYID" ]; then
        ../relay/rq -a $HOST -d $RELAYID 2>/dev/null || true
        return 0                # success
    fi
    return 1
}

maybe_resign() {
    if [ "$RESIGN_RATIO" -gt 0 ]; then
        KEY=$1
        LOG=${LOGS[$KEY]}
        if grep -q XWRELAY_ALLHERE $LOG; then
            if [ 0 -eq $(($RANDOM % $RESIGN_RATIO)) ]; then
                echo "making $LOG $(connName $LOG) resign..."
                kill_from_log $LOG && close_device $KEY $DEADDIR "resignation forced"
            fi
        fi
    fi
}

check_game() {
    KEY=$1
    LOG=${LOGS[$KEY]}
    CONNNAME="$(connName $LOG)"
    OTHERS=""
    if [ -n "$CONNNAME" ]; then
        if grep -q '\[unused tiles\]' $LOG; then
            ALL_DONE=TRUE
            for INDX in ${!LOGS[*]}; do
                [ $INDX -eq $KEY ] && continue
                ALOG=${LOGS[$INDX]}
                CONNNAME2="$(connName $ALOG)"
                if [ "$CONNNAME2" = "$CONNNAME" ]; then
                    if ! grep -q '\[unused tiles\]' $ALOG; then
                        OTHERS=""
                        break
                    fi
                    OTHERS="$OTHERS $INDX"
                fi
            done
        fi
    fi

    if [ -n "$OTHERS" ]; then
        echo -n "Closing $CONNNAME: "
        # kill_from_logs $OTHERS $KEY
        for ID in $OTHERS $KEY; do
            echo -n "${LOGS[$ID]}, "
            kill_from_log ${LOGS[$ID]} || true
            close_device $ID $DONEDIR "game over"
        done
        date
    elif grep -q 'relay_error_curses(XWRELAY_ERROR_DELETED)' $LOG; then
        echo "deleting $LOG $(connName $LOG) b/c another resigned"
        kill_from_log $LOG || true
        close_device $KEY $DEADDIR "other resigned"
    else
        maybe_resign $KEY
    fi
}

run_cmds() {
    ENDTIME=$(($(date +%s) + TIMEOUT))
    while :; do
        COUNT=${#CMDS[*]}
        [ 0 -ge $COUNT ] && break
        [ $(date +%s) -ge $ENDTIME ] && break
        INDX=$(($RANDOM%COUNT))
        KEYS=( ${!CMDS[*]} )
        KEY=${KEYS[$INDX]}
        if [ 0 -eq ${PIDS[$KEY]} ]; then
            launch $KEY &
            PIDS[$KEY]=$!
        else
            sleep 2             # make sure it's had some time
            kill ${PIDS[$KEY]} || true
            PIDS[$KEY]=0
            check_game $KEY
        fi
    done

    # kill any remaining games
    if [ $COUNT -gt 0 ]; then
        mkdir -p ${LOGDIR}/not_done
        echo "processing unfinished games...."
        for KEY in ${!CMDS[*]}; do
            close_device $KEY ${LOGDIR}/not_done "unfinished game"
        done
    fi
}

print_stats() {
    :
}

echo "*********$0 starting: $(date)**************"
STARTTIME=$(date +%s)
build_cmds
run_cmds
print_stats

wait

SECONDS=$(($(date +%s)-$STARTTIME))
HOURS=$((SECONDS/3600))
SECONDS=$((SECONDS%3600))
MINUTES=$((SECONDS/60))
SECONDS=$((SECONDS%60))
echo "*********$0 finished: $(date) (took $HOURS:$MINUTES:$SECONDS)**************"
