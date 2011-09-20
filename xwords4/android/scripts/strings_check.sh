#!/bin/bash

set -u -e

declare -A ENG_IDS
LOCS=""
LIST_ONLY=""
PAIRS_ONLY=""
SEARCH_SOURCE=1

# ENG=~/dev/git/ANDROID_BRANCH/xwords4/android/XWords4/res/values/strings.xml

usage() {
    echo "usage: $0 (--loc <path_to_strings.xml>)+ [--list-only] [--pairs-only]" >&2
    exit 1
}

list_ids() {
    XML_FILE=$1
    xmlstarlet sel -T -t -m "/resources/string" -v @name -n $XML_FILE
}

list_pairs() {
    XML_FILE=$1
    for NAME in $(list_ids $XML_FILE); do
        xmlstarlet sel -t -m "//string[@name='$NAME']" -v @name -o ':' \
            -v . $XML_FILE | tr -d '\n' | sed 's,  *, ,g'
        echo ""
    done
}

while [ $# -gt 0 ]; do
    case $1 in
        --loc)
            [ $# -gt 1 ] || usage
            [ -e $2 ] || usage
            LOCS="$LOCS $2"
            shift
            ;;
        --list-only)
            LIST_ONLY=1
            ;;
        --pairs-only)
            PAIRS_ONLY=1
            ;;
        *)
            usage
            ;;
    esac
    shift
done

[ -n "$LOCS" ] || usage

if [ -n "$LIST_ONLY" ]; then
    for LOC in $LOCS; do
        list_ids $LOC
    done
    exit 0
fi

if [ -n "$PAIRS_ONLY" ]; then
    for LOC in $LOCS; do
        list_pairs $LOC
    done
    exit 0
fi

# echo "checking $ENG for ids not in any .java file"
for LOC in $LOCS; do
    for ID in $(list_ids $LOC); do
        ENG_IDS[$ID]=1
    done
done

if [ -n "$SEARCH_SOURCE" ]; then
    IDS="${!ENG_IDS[*]}"
    for ID in $IDS; do
        if grep -qw R.string.$ID $(find . -name '*.java'); then
            :
        elif grep -qw "@string/$ID" $(find . -name '*.xml'); then
            :
        else
            echo "$ID appears to be unused"
        fi
    done
fi


for LOC in $LOCS; do
    IDS="${!ENG_IDS[*]}"
    for ID in $IDS; do
        grep -q $ID $LOC || echo "$ID not found in $LOC"
    done
done