#!/bin/bash

NRUNS=${NRUNS:-4}
XWORDS=${XWORDS:-"./obj_linux_memdbg/xwords"}
DICT=${DICT:-dict.xwd}
HOST=${HOST:-localhost}
PORT=${PORT:-10999}
QUIT=${QUIT:-"-q 2"}
USE_CURSES=${USE_CURSES:="yes"}

usage() {
    echo "usage: $0 <no params>"
    cat <<EOF
The goal of this script is to simulate real-world loads on the relay,
with games starting, stopping, moves happening, etc. over a long time.
It uses ENV variables rather than commandline parms for configuration.
EOF
    echo "    env: NRUNS: number of simultaneous games; default 4"
    echo "    env: DICT: dictionary; default: dict.xwd"
    echo "    env: HOST: remote host; default: localhost"
    echo "    env: PORT: remote port; default: 10999"
    exit 0
}


random() {
    # RANDOM is a bashism
#     RAND=$(dd if=/dev/urandom count=1 2>/dev/null | cksum | cut -f1 -d" ")
#     echo $RAND
    echo $RANDOM
}

do_one() {
    COOKIE=${1:-$(exec sh -c 'echo $PPID')}

    while :; do                 # loop forever (eventually)

        TODO=$(($COOKIE % 3))
        TODO=$((TODO+2))
        COUNT=0
        for NAME in Bbbbb Aaaaa Kkkkk Eeeee; do
            [ $COUNT = $TODO ] && break
            while :; do
                RAND=$(random)
                INDEX=$(( $RAND % $TODO ))
                WAIT=$(( $RAND % 60 ))
                case $INDEX in
                    0)
                        if [ -z "$ZERO_DONE" ]; then
                            REMOTES=""
                            for JJ in $(seq $(($TODO-1))); do
                                REMOTES="$REMOTES -N"; 
                            done
                            ZERO_DONE=1
                            echo $COOKIE:ZERO
                            if [ "$USE_CURSES" = "yes" ]; then
                                $XWORDS -d $DICT -r $NAME -s $REMOTES \
                                    -a $HOST -p $PORT -C $COOKIE $QUIT -u \
                                    -z 0:$WAIT >/dev/null \
                                    2>/tmp/log_${COOKIE}_${INDEX}.txt &

                            else
                                $XWORDS -d $DICT -r $NAME -s $REMOTES \
                                    -a $HOST -p $PORT -C $COOKIE $QUIT &
                            fi
                            break
                        fi
                        ;;
                    1)
                        if [ -z "$ONE_DONE" ]; then
                            ONE_DONE=1
                            echo $COOKIE:ONE
                            if [ "$USE_CURSES" = "yes" ]; then
                                $XWORDS -d $DICT -r $NAME -a $HOST -p $PORT \
                                    -C $COOKIE $QUIT -u \
                                    -z 0:$WAIT >/dev/null \
                                    2>/tmp/log_$COOKIE_${INDEX}.txt &
                            else
                                $XWORDS -d $DICT -r $NAME -a $HOST -p $PORT \
                                    -C $COOKIE $QUIT &
                            fi
                            break
                        fi
                        ;;
                    2)
                        if [ -z "$TWO_DONE" ]; then
                            TWO_DONE=1
                            echo $COOKIE:TWO
                            if [ "$USE_CURSES" = "yes" ]; then
                                $XWORDS -d $DICT -r $NAME -a $HOST -p $PORT \
                                    -C $COOKIE $QUIT -u \
                                    -z 0:$WAIT >/dev/null \
                                    2>/tmp/log_$COOKIE_${INDEX}.txt &

                            else
                                $XWORDS -d $DICT -r $NAME -a $HOST -p $PORT \
                                    -C $COOKIE $QUIT &
                            fi
                            break
                        fi
                        ;;
                    3)
                        if [ -z "$THREE_DONE" ]; then
                            THREE_DONE=1
                            echo $COOKIE:THREE
                            if [ "$USE_CURSES" = "yes" ]; then
                                $XWORDS -d $DICT -r $NAME -a $HOST -p $PORT \
                                    -C $COOKIE $QUIT -u \
                                    -z 0:$WAIT >/dev/null \
                                    2>/tmp/log_$COOKIE_${INDEX}.txt &
                            else
                                $XWORDS -d $DICT -r $NAME -a $HOST -p $PORT \
                                    -C $COOKIE $QUIT &
                            fi
                            break
                        fi
                        ;;
                esac
            done
            COUNT=$((COUNT+1))
        done
        wait
        break
    done
}

############################################################################
# main

[ -n "$1" ] && usage

for II in $(seq $NRUNS); do
    do_one $II &
done

wait
