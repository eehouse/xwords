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
        COUNT=$(grep -w -c $ID $1)
        if [ 0 -eq "$COUNT" ]; then
            echo $ID not found in $1
        elif [ 1 -lt "$COUNT" ]; then
            echo "$ID found $COUNT times in $1"
        fi
        if ! grep -q $ID *.c; then
            echo $ID not used in any .c file
        fi
    done

    grep '^\s*IDS_.*' $1 | sed 's/^ *\(IDS[A-Z_]*\).*$/\1/' | sort -u > /tmp/ids_cur$$.txt
    if [ -e /tmp/ids_prev$$.txt ]; then
        echo -e "\ncomparing IDs in $1, $PREV"
        diff /tmp/ids_prev$$.txt /tmp/ids_cur$$.txt
    fi
    mv /tmp/ids_cur$$.txt /tmp/ids_prev$$.txt
    PREV=$1

    shift
done

#rm -f /tmp/ids_cur$$.txt /tmp/ids_prev$$.txt
