#!/bin/bash

set -u -e

EXCEPTS=" "

echo "$0 $*"
pwd

usage() {
    [ $# -ge 1 ] && echo "ERROR: $1"
    echo "usage: $0 [--except path/to/file]*"
    exit 1
}

# rm_not_excepted() {
#     FILE=$1
#     for EXCEPT in $EXCEPTS; do
#         if [ $EXCEPT = $FILE ]; then
#             echo "skipping delete of $FILE"
#             FILE=""
#             break
#         fi
#     done

#     [ -n "$FILE" ] && rm $FILE
# }

rm_in() {
    echo "rm_in $1"
    for FILE in $(ls $1); do
        FILE=$1/$FILE
        echo "FILE: $FILE"
        if [ ! "${EXCEPTS}" = "${EXCEPTS# $FILE }" ]; then
            echo "$FILE is in $EXCEPTS"
            continue
        elif [ -d $FILE ]; then
            rm_in $FILE
        elif git ls-files $FILE --error-unmatch 2>/dev/null; then
            echo "$FILE is a git file"
            continue
        else
            echo "rm $FILE"
            rm $FILE
        fi
    done
}


while [ $# -ge 1 ]; do
    case $1 in
        --except)
            FILE=$2
            shift
            if [ $FILE != ${FILE#/} ]; then          # starts with / ?
                :                                    # leave it alone
            elif [ "$FILE" != "${FILE#\./}" ]; then  # starts with ./ ?
                :                                    # leave it alone
            else
                FILE="./${FILE}"                     # prepend ./ to match ls output
            fi
            EXCEPTS=" $EXCEPTS $FILE "
            ;;
        *)
            usage "unexpected param $1"
            ;;
    esac
    shift
done

echo "EXCEPTS: $EXCEPTS"
exit 0

rm_in "."
echo "$0: exiting cleanly"
