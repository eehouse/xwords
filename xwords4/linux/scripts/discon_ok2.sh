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

declare -A PIDS
declare -A CMDS
declare -A FILES
declare -A LOGS

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

while [ -n "$1" ]; do
    case $1 in
        *) usage
            ;;
    esac
    shift
done

build_cmds() {
    COUNTER=0
    for GAME in $(seq 1 $NGAMES); do
        ROOM=ROOM_$((GAME % NROOMS))
        NDEVS=$(($RANDOM%3+2))
        NDEVS=2

        unset OTHERS
        for II in $(seq 2 $NDEVS); do
            OTHERS="-N $OTHERS"
        done

        for DEV in $(seq $NDEVS); do
            FILE="${LOGDIR}/GAME_${GAME}_${DEV}.xwg"
            LOG=${LOGDIR}/${GAME}_${DEV}_LOG.txt
            CMD="./obj_linux_memdbg/xwords -C $ROOM -r ${NAMES[$DEV]} $OTHERS"
            CMD="$CMD -d dict.xwd -p $PORT -a $HOST -f $FILE -z 1:3 $PLAT_PARMS"
            CMDS[$COUNTER]=$CMD
            FILES[$COUNTER]=$FILE
            LOGS[$COUNTER]=$LOG
            PIDS[$COUNTER]=0
            COUNTER=$((COUNTER+1))
        done
    done
}

launch() {
    LOG=${LOGS[$1]}
    CMD="${CMDS[$1]}"
    exec $CMD >/dev/null 2>>$LOG
}

close_device() {
    ID=$1
    if [ ${PIDS[$ID]} -ne 0 ]; then
        kill ${PIDS[$ID]}
    fi
    unset PIDS[$ID]
    unset CMDS[$ID]
    mv ${FILES[$ID]} $DONEDIR
    unset FILES[$ID]
    mv ${LOGS[$ID]} $DONEDIR
    unset LOGS[$ID]
    echo "closed $ID"
}

check_game() {
    KEY=$1
    LOG=${LOGS[$KEY]}
    CONNNAME="$(connName $LOG)"
    unset OTHERS
    if [ -n "$CONNNAME" ]; then
        if grep -q 'all remaining tiles' $LOG; then
            ALL_DONE=TRUE
            for INDX in ${!LOGS[*]}; do
                [ $INDX -eq $KEY ] && continue
                ALOG=${LOGS[$INDX]}
                CONNNAME2="$(connName $ALOG)"
                if [ "$CONNNAME2" = "$CONNNAME" ]; then
                    if ! grep -q 'all remaining tiles' $ALOG; then
                        unset OTHERS
                        break
                    fi
                    OTHERS="$OTHERS $INDX"
                fi
            done
        fi
    fi

    if [ -n "$OTHERS" ]; then
        for ID in $OTHERS $KEY; do
            close_device $ID
        done
    fi
}

run_cmds() {
    while :; do
        COUNT=${#CMDS[*]}
        [ 0 -ge $COUNT ] && break
        INDX=$(($RANDOM%COUNT))
        KEYS=( ${!CMDS[*]} )
        KEY=${KEYS[$INDX]}
        if [ 0 -eq ${PIDS[$KEY]} ]; then
            launch $KEY &
            PIDS[$KEY]=$!
        else
            sleep 2             # make sure it's had some time
            kill ${PIDS[$KEY]}
            PIDS[$KEY]=0
            check_game $KEY
        fi
    done
}

print_stats() {
    :
}

build_cmds
run_cmds
print_stats

wait
