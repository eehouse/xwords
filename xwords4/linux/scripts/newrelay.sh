#!/bin/sh

DICT=./dict.xwd
COOKIE=${COOKIE:-foo}
NGAMES=${NGAMES:-1}
NPLAYERS=${NPLAYERS:-2}

usage() {
    echo "usage: [env=val *] $0" 1>&2
    echo " current env variables and their values: " 1>&2
    for VAR in COOKIE NGAMES NPLAYERS; do
        echo "$VAR:" $(eval "echo \$${VAR}") 1>&2
    done
    exit 0
}

while [ -n "$1" ]; do
    case $1 in
        *) usage
            ;;
    esac
    shift
done

for II in $(seq $NGAMES); do
    REMOTES=""
    for JJ in $(seq $((NPLAYERS-1))); do
        REMOTES="${REMOTES} -N"
    done
    for JJ in $(seq $NPLAYERS); do
        ./obj_linux_memdbg/xwords -u -0 -d $DICT -r Eric $REMOTES -C $COOKIE \
            -q 2 2>log_${II}_${JJ}.txt >/dev/null &
    done
done

wait

echo "$0 done"
