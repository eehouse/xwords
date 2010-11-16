#!/bin/sh

while [ $# -ge 1 ]; do
    LOG=$1
    while read LINE; do
        case "$LINE" in
            *got_connect_cmd:\ connName* )
                CONNNAME="$(echo $LINE | sed 's,^.*connName: "\(.*\)"$,\1,')"
                ;;
            *hostid* )
                HOSTID=$(echo $LINE | sed 's,^.*set hostid: \(.\)$,\1,')
                ;;
            *getChannelSeed:\ channelSeed:*)
                SEED=$(echo $LINE | sed 's,^.*getChannelSeed: channelSeed: \(.*\)$,\1,')
                ;;
        esac
    done < $LOG
    if [ -z "${CONNNAME}" ]; then
        echo "CONNNAME not found in $LOG" >&2
    elif [ -z "${HOSTID}" ]; then
        echo "HOSTID not found in $LOG" >&2
    elif [ -z "${SEED}" ]; then
        echo "SEED not found in $LOG" >&2
    else
        echo ${CONNNAME}/${HOSTID}/${SEED}
    fi

    shift
done
