#!/bin/sh

set -u -e

usage() {
    echo "usage: $0 /path/to/dicts"
    echo "   write to stdout an html file that serves up all .xwd files inside /path/to/dicts"
    exit 1
}

do_lang() {
    LANG=$1
    echo "<tr><td>$LANG</td></tr>"

    cd $LANG
    for DICT in $(ls *.xwd); do
        echo "<tr>"
        echo "<td><a href=\"./$LANG/$DICT\">${DICT#.xwd}</a></td>"
        SIZE=$(ls -l $DICT | awk '{print $5}')
        echo "<td>${SIZE}</td>"
        HEXCOUNT=$(hd $DICT | head -n 1 | awk '{print $6 $7 $8 $9}' | tr [a-f] [A-F])
        DECCOUNT=$(echo "ibase=16;$HEXCOUNT" | bc)
        echo "<td>${DECCOUNT}</td>"
        echo "</tr>"
    done

    cd ..
}

[ $# -eq 1 ] || usage

WD=$(pwd)
cd $1

DIRS=""
for DIR in $(ls); do
    if [ -d $DIR ] && ls $DIR/*.xwd >/dev/null 2>&1; then
        DIRS="$DIRS $DIR"
    fi
done


echo "<html><body>"
echo "<table>"
echo "<tr><th>File</th><th>Size</th><th>Wordcount</th></tr>"
for DIR in  $DIRS; do
    do_lang $DIR
done
echo "</table>"
echo "</body></html>"


cd $WD

