#!/bin/bash
set -u -e

LOGDIR=$(basename $0)_logs
APP_NEW=""
DO_CLEAN=""
APP_NEW_PARAMS=""
NGAMES=""
UPGRADE_ODDS=""
NROOMS=""
HOST=""
PORT=""
TIMEOUT=""
SAVE_GOOD=""
MINDEVS=""
MAXDEVS=""
ONEPER=""
RESIGN_RATIO=""
DROP_N=""
MINRUN=2
ONE_PER_ROOM=""                 # don't run more than one device at a time per room
USE_GTK=""
UNDO_PCT=0
ALL_VIA_RQ=${ALL_VIA_RQ:-FALSE}
SEED=""
BOARD_SIZES_OLD=(15)
BOARD_SIZES_NEW=(15)
NAMES=(UNUSED Brynn Ariela Kati Eric)
SEND_CHAT=''

declare -A PIDS
declare -A APPS
declare -A NEW_ARGS
declare -A ARGS
declare -A ARGS_DEVID
declare -A ROOMS
declare -A FILES
declare -A LOGS
declare -A MINEND
declare -A ROOM_PIDS
declare -a APPS_OLD
declare -a DICTS
declare -A CHECKED_ROOMS

function cleanup() {
    APP="$(basename $APP_NEW)"
    while pidof $APP; do
        echo "killing existing $APP instances..."
        killall -9 $APP
        sleep 1
    done
    echo "cleaning everything up...."
    if [ -d $LOGDIR ]; then
    mv $LOGDIR /tmp/${LOGDIR}_$$
    fi
    if [ -e $(dirname $0)/../../relay/xwrelay.log ]; then
    mkdir -p /tmp/${LOGDIR}_$$
    mv $(dirname $0)/../../relay/xwrelay.log /tmp/${LOGDIR}_$$
    fi

    echo "delete from games;" | psql -q -t xwgames
}

function connName() {
    LOG=$1
    grep 'got_connect_cmd: connName' $LOG | \
        tail -n 1 | \
        sed 's,^.*connName: \"\(.*\)\" (reconnect=.)$,\1,'
}

function check_room() {
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

print_cmdline() {
    local COUNTER=$1
    local LOG=${LOGS[$COUNTER]}
    echo -n "New cmdline: " >> $LOG
    echo "${APPS[$COUNTER]} ${NEW_ARGS[$COUNTER]} ${ARGS[$COUNTER]}" >> $LOG
}

function pick_ndevs() {
    local NDEVS=2
    local RNUM=$((RANDOM % 100))
    if [ $RNUM -gt 90 -a $MAXDEVS -ge 4 ]; then
        NDEVS=4
    elif [ $RNUM -gt 75 -a $MAXDEVS -ge 3 ]; then
        NDEVS=3
    fi
    if [ -n "$MINDEVS" -a "$NDEVS" -lt "$MINDEVS" ]; then
        NDEVS=$MINDEVS
    fi
    echo $NDEVS
}

# Given a device count, figure out how many local players per device.
# "1 1" would be a two-device game with 1 each.  "1 2 1" a
# three-device game with four players total
function figure_locals() {
    local NDEVS=$1
    local NPLAYERS=$(pick_ndevs)
    [ $NPLAYERS -lt $NDEVS ] && NPLAYERS=$NDEVS
    
    local EXTRAS=0
    if [ -z "$ONEPER" ]; then
        EXTRAS=$(($NPLAYERS - $NDEVS))
    fi

    local LOCALS=""
    for IGNORE in $(seq $NDEVS); do
        COUNT=1
        if [ $EXTRAS -gt 0 ]; then
            local EXTRA=$((RANDOM % $((1 + EXTRAS))))
            if [ $EXTRA -gt 0 ]; then
                COUNT=$((COUNT + EXTRA))
                EXTRAS=$((EXTRAS - EXTRA))
            fi
        fi
        LOCALS="$LOCALS $COUNT"
    done
    echo "$LOCALS"
}

function player_params() {
    local NLOCALS=$1
    local NPLAYERS=$2
    local NAME_INDX=$3
    local NREMOTES=$((NPLAYERS - NLOCALS))
    local PARAMS=""
    while [ $NLOCALS -gt 0 -o $NREMOTES -gt 0 ]; do
        if [ 0 -eq $((RANDOM%2)) -a 0 -lt $NLOCALS ]; then
            PARAMS="$PARAMS --robot ${NAMES[$NAME_INDX]} --robot-iq $((1 + (RANDOM%100))) "
            NLOCALS=$((NLOCALS-1))
            NAME_INDX=$((NAME_INDX+1))
        elif [ 0 -lt $NREMOTES ]; then
            PARAMS="$PARAMS --remote-player"
            NREMOTES=$((NREMOTES-1))
        fi
    done
    echo "$PARAMS"
}

function sum() {
    RESULT=0
    while [ $# -gt 0 ]; do
        RESULT=$((RESULT+$1))
        shift
    done
    echo $RESULT
}

build_cmds() {
    COUNTER=0
    local PLAT_PARMS=""
    if [ $USE_GTK = FALSE ]; then
        PLAT_PARMS="--curses --close-stdin"
    fi

    for GAME in $(seq 1 $NGAMES); do
        ROOM=$(printf "ROOM_%.3d" $((GAME % NROOMS)))
        ROOM_PIDS[$ROOM]=0
        check_room $ROOM
        NDEVS=$(pick_ndevs)
        LOCALS=( $(figure_locals $NDEVS) ) # as array
        NPLAYERS=$(sum ${LOCALS[@]})
        [ ${#LOCALS[*]} -eq $NDEVS ] || usage "problem with LOCALS"
        #[ $NDEVS -lt $MINDEVS ] && NDEVS=$MINDEVS
        DICT=${DICTS[$((GAME%${#DICTS[*]}))]}
        # make one in three games public
        local PUBLIC=""
        [ $((RANDOM%3)) -eq 0 ] && PUBLIC="--make-public --join-public"

        DEV=0
        for NLOCALS in ${LOCALS[@]}; do
            DEV=$((DEV + 1))
            FILE="${LOGDIR}/GAME_${GAME}_${DEV}.xwg"
            LOG=${LOGDIR}/${GAME}_${DEV}_LOG.txt
            > $LOG # clear the log

            APPS[$COUNTER]="$APP_NEW"
            NEW_ARGS[$COUNTER]="$APP_NEW_PARAMS"
            BOARD_SIZE="--board-size ${BOARD_SIZES_NEW[$((RANDOM%${#BOARD_SIZES_NEW[*]}))]}"
            if [ xx = "${APPS_OLD+xx}" ]; then
                # 50% chance of starting out with old app
                NAPPS=$((1+${#APPS_OLD[*]}))
                if [ 0 -lt $((RANDOM%$NAPPS)) ]; then
                    APPS[$COUNTER]=${APPS_OLD[$((RANDOM%${#APPS_OLD[*]}))]}
                    BOARD_SIZE="--board-size ${BOARD_SIZES_OLD[$((RANDOM%${#BOARD_SIZES_OLD[*]}))]}"
                    NEW_ARGS[$COUNTER]=""
                fi
            fi

            PARAMS="$(player_params $NLOCALS $NPLAYERS $DEV)"
            PARAMS="$PARAMS $BOARD_SIZE --room $ROOM --trade-pct 20 --sort-tiles "
            [ $UNDO_PCT -gt 0 ] && PARAMS="$PARAMS --undo-pct $UNDO_PCT "
            PARAMS="$PARAMS --game-dict $DICT --port $PORT --host $HOST "
            PARAMS="$PARAMS --file $FILE --slow-robot 1:3 --skip-confirm"
            PARAMS="$PARAMS --drop-nth-packet $DROP_N $PLAT_PARMS"
            # PARAMS="$PARAMS --split-packets 2"
            if [ -n $SEND_CHAT ]; then
                   PARAMS="$PARAMS --send-chat $SEND_CHAT"
            fi
            # PARAMS="$PARAMS --savefail-pct 10"
            [ -n "$SEED" ] && PARAMS="$PARAMS --seed $RANDOM"
            PARAMS="$PARAMS $PUBLIC"
            ARGS[$COUNTER]=$PARAMS
            ROOMS[$COUNTER]=$ROOM
            FILES[$COUNTER]=$FILE
            LOGS[$COUNTER]=$LOG
            PIDS[$COUNTER]=0
            ARGS_DEVID[$COUNTER]=""
            update_devid_cmd $COUNTER

            print_cmdline $COUNTER

            COUNTER=$((COUNTER+1))
        done
    done
    echo "finished creating $COUNTER commands"
} # build_cmds

read_resume_cmds() {
    COUNTER=0
    for LOG in $(ls $LOGDIR/*.txt); do
        echo "need to parse cmd and deal with changes"
        exit 1
        CMD=$(head -n 1 $LOG)

        ARGS[$COUNTER]=$CMD
        LOGS[$COUNTER]=$LOG
        PIDS[$COUNTER]=0

        set $CMD
        while [ $# -gt 0 ]; do
            case $1 in
                --file)
                    FILES[$COUNTER]=$2
                    shift
                    ;;
                --room)
                    ROOMS[$COUNTER]=$2
                    shift
                    ;;
            esac
            shift
        done
        COUNTER=$((COUNTER+1))
    done
    ROOM_PIDS[$ROOM]=0
}

launch() {
    LOG=${LOGS[$1]}
    APP="${APPS[$1]}"
    PARAMS="${NEW_ARGS[$1]} ${ARGS[$1]} ${ARGS_DEVID[$1]}"
    exec $APP $PARAMS >/dev/null 2>>$LOG
}

# launch_via_rq() {
#      KEY=$1
#      RELAYID=$2
#      PIPE=${PIPES[$KEY]}
#      ../relay/rq -f $RELAYID -o $PIPE &
#      CMD="${CMDS[$KEY]}"
#      exec $CMD >/dev/null 2>>$LOG
# }

close_device() {
    ID=$1
    MVTO=$2
    REASON="$3"
    PID=${PIDS[$ID]}
    if [ $PID -ne 0 ]; then
        kill ${PIDS[$ID]} 2>/dev/null
        wait ${PIDS[$ID]}
        ROOM=${ROOMS[$ID]}
        [ ${ROOM_PIDS[$ROOM]} -eq $PID ] && ROOM_PIDS[$ROOM]=0
    fi
    unset PIDS[$ID]
    unset ARGS[$ID]
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
    unset ROOMS[$ID]
    unset APPS[$ID]
    unset ARGS_DEVID[$ID]
}

OBITS=""

kill_from_log() {
    LOG=$1
    RELAYID=$(./scripts/relayID.sh --long $LOG)
    if [ -n "$RELAYID" ]; then
        OBITS="$OBITS -d $RELAYID"
        if [ 0 -eq $(($RANDOM%2)) ]; then
            ../relay/rq -a $HOST $OBITS 2>/dev/null || true
            OBITS=""
        fi
        return 0                # success
    fi
    echo "unable to send kill command for $LOG"
    return 1
}

maybe_resign() {
    if [ "$RESIGN_RATIO" -gt 0 ]; then
        KEY=$1
        LOG=${LOGS[$KEY]}
        if grep -q XWRELAY_ALLHERE $LOG; then
            if [ 0 -eq $(($RANDOM % $RESIGN_RATIO)) ]; then
                echo "making $LOG $(connName $LOG) resign..."
                kill_from_log $LOG && close_device $KEY $DEADDIR "resignation forced" || true
            fi
        fi
    fi
}

try_upgrade() {
    KEY=$1
    if [ xx = "${APPS_OLD+xx}" ]; then
        if [ $APP_NEW != ${APPS[$KEY]} ]; then
            # one in five chance of upgrading
            if [ 0 -eq $((RANDOM % UPGRADE_ODDS)) ]; then
                APPS[$KEY]=$APP_NEW
                NEW_ARGS[$KEY]="$APP_NEW_PARAMS"
                print_cmdline $KEY
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
        echo -n "Closing $CONNNAME [$(date)]: "
        # kill_from_logs $OTHERS $KEY
        for ID in $OTHERS $KEY; do
            echo -n "${LOGS[$ID]}, "
            kill_from_log ${LOGS[$ID]} || true
            close_device $ID $DONEDIR "game over"
        done
        echo ""
        # XWRELAY_ERROR_DELETED may be old
    elif grep -q 'relay_error_curses(XWRELAY_ERROR_DELETED)' $LOG; then
        echo "deleting $LOG $(connName $LOG) b/c another resigned"
        kill_from_log $LOG || true
        close_device $KEY $DEADDIR "other resigned"
    elif grep -q 'relay_error_curses(XWRELAY_ERROR_DEADGAME)' $LOG; then
        echo "deleting $LOG $(connName $LOG) b/c another resigned"
        kill_from_log $LOG || true
        close_device $KEY $DEADDIR "other resigned"
    else
        maybe_resign $KEY
    fi
}

increment_drop() {
    KEY=$1
    CMD=${ARGS[$KEY]}
    if [ "$CMD" != "${CMD/drop-nth-packet//}" ]; then
        DROP_N=$(echo $CMD | sed 's,^.*drop-nth-packet \(-*[0-9]*\) .*$,\1,')
        if [ $DROP_N -gt 0 ]; then
            NEXT_N=$((DROP_N+1))
            ARGS[$KEY]=$(echo $CMD | sed "s,^\(.*drop-nth-packet \)$DROP_N\(.*\)$,\1$NEXT_N\2,")
        fi
    fi
}

get_relayid() {
    KEY=$1
    RELAY_ID=$(grep 'deviceRegistered: new id: ' ${LOGS[$KEY]} | tail -n 1)
    if [ -n "$RELAY_ID" ]; then
        RELAY_ID=$(echo $RELAY_ID | sed 's,^.*new id: ,,')
    else
        usage "new id string not in $LOG"
    fi
    echo $RELAY_ID
}

update_devid_cmd() {
    KEY=$1
    HELP="$(${APPS[$KEY]} --help 2>&1 || true)"
    if echo $HELP | grep -q '\-\-devid'; then
        CMD="--devid LINUX_TEST_$(printf %.5d ${KEY})"
        LOG=${LOGS[$KEY]}
        if [ -z "${ARGS_DEVID[$KEY]}" -o ! -e $LOG ]; then # upgrade or first run
            :
        else
            # otherwise, we should have successfully registered.  If
            # we have AND the reg has been rejected, make without
            # --rdevid so will reregister.
            LAST_GOOD=$(grep -h -n 'linux_util_deviceRegistered: new id' $LOG | tail -n 1 | sed 's,:.*$,,')
            LAST_REJ=$(grep -h -n 'linux_util_deviceRegistered: id rejected' $LOG | tail -n 1 | sed 's,:.*$,,')
            # echo "LAST_GOOD: $LAST_GOOD; LAST_REJ: $LAST_REJ"
            if [ -z "$LAST_GOOD" ]; then # not yet registered
                :
            elif [ -z "$LAST_REJ" ]; then
                CMD="$CMD --rdevid $(get_relayid $KEY)"
            elif [ "$LAST_REJ" -lt "$LAST_GOOD" ]; then # registered and not more recently rejected
                CMD="$CMD --rdevid $(get_relayid $KEY)"
            fi
        fi
        # echo $CMD
        ARGS_DEVID[$KEY]=$CMD
    fi
}

run_cmds() {
    ENDTIME=$(($(date +%s) + TIMEOUT))
    while :; do
        COUNT=${#ARGS[*]}
        [ 0 -ge $COUNT ] && break
        NOW=$(date '+%s')
        [ $NOW -ge $ENDTIME ] && break
        INDX=$(($RANDOM%COUNT))
        KEYS=( ${!ARGS[*]} )
        KEY=${KEYS[$INDX]}
        ROOM=${ROOMS[$KEY]}
        if [ 0 -eq ${PIDS[$KEY]} ]; then
            if [ -n "$ONE_PER_ROOM" -a 0 -ne ${ROOM_PIDS[$ROOM]} ]; then
                continue
            fi
            try_upgrade $KEY
            launch $KEY &
            PID=$!
            renice +1 $PID >/dev/null
            PIDS[$KEY]=$PID
            ROOM_PIDS[$ROOM]=$PID
            MINEND[$KEY]=$(($NOW + $MINRUN))
        else
            PID=${PIDS[$KEY]}
            if [ -d /proc/$PID ]; then
                SLEEP=$((${MINEND[$KEY]} - $NOW))
                [ $SLEEP -gt 0 ] && sleep $SLEEP
                kill $PID || true
                wait $PID
            fi
            PIDS[$KEY]=0
            ROOM_PIDS[$ROOM]=0
            [ "$DROP_N" -ge 0 ] && increment_drop $KEY
            update_devid_cmd $KEY
            check_game $KEY
        fi
    done

    [ -n "$OBITS" ] && ../relay/rq -a $HOST $OBITS 2>/dev/null || true

    # kill any remaining games
    if [ $COUNT -gt 0 ]; then
        mkdir -p ${LOGDIR}/not_done
        echo "processing unfinished games...."
        for KEY in ${!ARGS[*]}; do
            close_device $KEY ${LOGDIR}/not_done "unfinished game"
        done
    fi
}

run_via_rq() {
    # launch then kill all games to give chance to hook up
    for KEY in ${!ARGS[*]}; do
        echo "launching $KEY"
        launch $KEY &
        PID=$!
        sleep 1
        kill $PID
        wait $PID
        # add_pipe $KEY
    done

    echo "now running via rq"
    # then run them
    while :; do
        COUNT=${#ARGS[*]}
        [ 0 -ge $COUNT ] && break

        INDX=$(($RANDOM%COUNT))
        KEYS=( ${!ARGS[*]} )
        KEY=${KEYS[$INDX]}
        CMD=${ARGS[$KEY]}
            
        RELAYID=$(./scripts/relayID.sh --short ${LOGS[$KEY]})
        MSG_COUNT=$(../relay/rq -a $HOST -m $RELAYID 2>/dev/null | sed 's,^.*-- ,,')
        if [ $MSG_COUNT -gt 0 ]; then
            launch $KEY &
            PID=$!
            sleep 2
            kill $PID || true
            wait $PID
        fi
        [ "$DROP_N" -ge 0 ] && increment_drop $KEY
        check_game $KEY
    done
} # run_via_rq

function getArg() {
    [ 1 -lt "$#" ] || usage "$1 requires an argument"
    echo $2
}

function usage() {
    [ $# -gt 0 ] && echo "Error: $1" >&2
    echo "Usage: $(basename $0)                                       \\" >&2
    echo "    [--clean-start]                                         \\" >&2
    echo "    [--game-dict <path/to/dict>]*                           \\" >&2
    echo "    [--old-app <path/to/app]*                               \\" >&2
    echo "    [--new-app <path/to/app]                                \\" >&2
    echo "    [--new-app-args [arg*]]  # passed only to new app       \\" >&2
    echo "    [--min-devs <int>]                                      \\" >&2
    echo "    [--max-devs <int>]                                      \\" >&2
    echo "    [--one-per]              # force one player per device  \\" >&2
    echo "    [--num-games <int>]                                     \\" >&2
    echo "    [--num-rooms <int>]                                     \\" >&2
    echo "    [--host <hostname>]                                     \\" >&2
    echo "    [--port <int>]                                          \\" >&2
    echo "    [--seed <int>]                                          \\" >&2
    echo "    [--undo-pct <int>]                                      \\" >&2
    echo "    [--send-chat <interval-in-seconds>                      \\" >&2
    echo "    [--resign-ratio <0 <= n <=1000 >                        \\" >&2
    echo "    [--help]                                                \\" >&2

    exit 1
}

#######################################################
##################### MAIN begins #####################
#######################################################

while [ "$#" -gt 0 ]; do
    case $1 in
        --clean-start)
            DO_CLEAN=1
            ;;
        --num-games)
            NGAMES=$(getArg $*)
            shift
            ;;
        --num-rooms)
            NROOMS=$(getArg $*)
            shift
            ;;
        --old-app)
            APPS_OLD[${#APPS_OLD[@]}]=$(getArg $*)
            shift
            ;;
        --new-app)
            APP_NEW=$(getArg $*)
            shift
            ;;
        --new-app-args)
            APP_NEW_PARAMS="${2}"
            echo "got $APP_NEW_PARAMS"
            shift
            ;;
        --game-dict)
            DICTS[${#DICTS[@]}]=$(getArg $*)
            shift
            ;;
        --min-devs)
            MINDEVS=$(getArg $*)
            shift
            ;;
        --max-devs)
            MAXDEVS=$(getArg $*)
            shift
            ;;
        --one-per)
            ONEPER=TRUE
            ;;
        --host)
            HOST=$(getArg $*)
            shift
            ;;
        --port)
            PORT=$(getArg $*)
            shift
            ;;
        --seed)
            SEED=$(getArg $*)
            shift
            ;;
        --undo-pct)
            UNDO_PCT=$(getArg $*)
            shift
            ;;
    --send-chat)
            SEND_CHAT=$(getArg $*)
            shift
            ;;
    --resign-ratio)
        RESIGN_RATIO=$(getArg $*)
        shift
        ;;
        --help)
            usage
            ;;
        *) usage "unrecognized option $1"
            ;;
    esac
    shift
done

# Assign defaults
#[ 0 -eq ${#DICTS[@]} ] && DICTS=(dict.xwd)
[ xx = "${DICTS+xx}" ] || DICTS=(dict.xwd)
[ -z "$APP_NEW" ] && APP_NEW=./obj_linux_memdbg/xwords
[ -z "$MINDEVS" ] && MINDEVS=2
[ -z "$MAXDEVS" ] && MAXDEVS=4
[ -z "$NGAMES" ] && NGAMES=1
[ -z "$NROOMS" ] && NROOMS=$NGAMES
[ -z "$HOST" ] && HOST=localhost
[ -z "$PORT" ] && PORT=10997
[ -z "$TIMEOUT" ] && TIMEOUT=$((NGAMES*60+500))
[ -z "$SAVE_GOOD" ] && SAVE_GOOD=YES
[ -z "$RESIGN_RATIO" -a "$NGAMES" -gt 1 ] && RESIGN_RATIO=1000 || RESIGN_RATIO=0
[ -z "$DROP_N" ] && DROP_N=0
[ -z "$USE_GTK" ] && USE_GTK=FALSE
[ -z "$UPGRADE_ODDS" ] && UPGRADE_ODDS=10
#$((NGAMES/50))
[ 0 -eq $UPGRADE_ODDS ] && UPGRADE_ODDS=1
[ -n "$SEED" ] && RANDOM=$SEED
[ -z "$ONEPER" -a $NROOMS -lt $NGAMES ] && usage "use --one-per if --num-rooms < --num-games"

[ -n "$DO_CLEAN" ] && cleanup

RESUME=""
for FILE in $(ls $LOGDIR/*.{xwg,txt} 2>/dev/null); do
    if [ -e $FILE ]; then
        echo "Unfinished games found in $LOGDIR; continue with them (or discard)?"
        read -p "<yes/no> " ANSWER
        case "$ANSWER" in
            y|yes|Y|YES)
                RESUME=1
                ;;
            *)
                ;;
        esac
    fi
    break
done

if [ -z "$RESUME" -a -d $LOGDIR ]; then
    mv $LOGDIR /tmp/${LOGDIR}_$$
fi
mkdir -p $LOGDIR

if [ "$SAVE_GOOD" = YES ]; then
    DONEDIR=$LOGDIR/done
    mkdir -p $DONEDIR
fi
DEADDIR=$LOGDIR/dead
mkdir -p $DEADDIR

for VAR in NGAMES NROOMS USE_GTK TIMEOUT HOST PORT SAVE_GOOD \
    MINDEVS MAXDEVS ONEPER RESIGN_RATIO DROP_N ALL_VIA_RQ SEED \
    APP_NEW; do
    echo "$VAR:" $(eval "echo \$${VAR}") 1>&2
done
echo "DICTS: ${DICTS[*]}"
echo -n "APPS_OLD: "; [ xx = "${APPS_OLD[*]+xx}" ] && echo "${APPS_OLD[*]}" || echo ""

echo "*********$0 starting: $(date)**************"
STARTTIME=$(date +%s)
[ -z "$RESUME" ] && build_cmds || read_resume_cmds
if [ TRUE = "$ALL_VIA_RQ" ]; then
    run_via_rq
else
    run_cmds
fi

wait

SECONDS=$(($(date +%s)-$STARTTIME))
HOURS=$((SECONDS/3600))
SECONDS=$((SECONDS%3600))
MINUTES=$((SECONDS/60))
SECONDS=$((SECONDS%60))
echo "*********$0 finished: $(date) (took $HOURS:$MINUTES:$SECONDS)**************"
