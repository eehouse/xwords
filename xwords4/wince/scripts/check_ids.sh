#!/bin/sh

ID_FILE=${ID_FILE:-./strids.h}

if [ ! -f $ID_FILE ]; then
    echo "$ID_FILE not found"
    exit 0;
elif [ -z "$1" ]; then
    echo "usage: <rc file> [<rc file>*]"
fi

while [ -n "$1" ]; do
    cat $ID_FILE | grep '^# *define.* IDS' \
        | sed 's/^# *define *\(IDS_.*\) .*$/\1/' \
        | while read ID; do
        if ! grep -q $ID $1; then
            echo $ID not found in $1
        fi
    done
    shift
done

