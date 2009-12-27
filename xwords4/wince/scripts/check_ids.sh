#!/bin/sh

ID_FILE=${ID_FILE:-./strids.h}

if [ ! -f $ID_FILE ]; then
    echo "$0: $ID_FILE not found"
    exit 0;
elif [ -z "$1" ]; then
    echo "usage: <rc file> [<rc file>*]"
fi

while [ -n "$1" ]; do
    RC_FILE=$1
    cat $ID_FILE | grep '^# *define.* IDS' \
        | sed 's/^# *define *\(IDS_.*\) .*$/\1/' \
        | while read ID; do
        COUNT=$(grep -w -c $ID $RC_FILE)
        if [ 0 -eq "$COUNT" ]; then
            echo $ID not found in $RC_FILE
        elif [ 1 -lt "$COUNT" ]; then
            echo "$ID found $COUNT times in $RC_FILE"
        fi
        if ! grep -q $ID *.c; then
            echo $ID not used in any .c file
        fi
    done

    grep '^\s*IDS_.*' "$RC_FILE" | sed 's/^ *\(IDS[A-Z_]*\).*$/\1/' | \
        sort -u > /tmp/ids_cur$$.txt
    if [ -e /tmp/ids_prev$$.txt ]; then
        echo -e "\ncomparing IDs in $RC_FILE, $PREV"
        diff /tmp/ids_prev$$.txt /tmp/ids_cur$$.txt
    fi
    mv /tmp/ids_cur$$.txt /tmp/ids_prev$$.txt
    PREV=$RC_FILE

    shift
done

#rm -f /tmp/ids_cur$$.txt /tmp/ids_prev$$.txt
