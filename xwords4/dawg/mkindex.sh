#!/bin/sh

set -u -e

DO_MD5=""
DICT_PATH=.

usage() {
    [ $# -ge 1 ] && echo "ERROR: $1"
    echo "usage: $0 [--md5] --path /path/to/dicts"
    echo "   write to stdout an html file that serves up all .xwd files inside /path/to/dicts"
    echo "   optionally, write dictName.md5 in dir for every dict."
    exit 1
}

do_lang() {
    LANG=$1
    echo "<tr><td><a name=\"$LANG\">$LANG</a></td></tr>"

    cd $LANG
    for DICT in $(ls *.xwd); do
        echo "<tr>"
        echo "<td>&nbsp;&nbsp;<a href=\"./$LANG/$DICT\">${DICT%.xwd}</a></td>"

        HEXCOUNT=$(hd $DICT | head -n 1 | awk '{print $6 $7 $8 $9}' | \
            tr [a-f] [A-F])
        DECCOUNT=$(echo "ibase=16;$HEXCOUNT" | bc)
        echo "<td>${DECCOUNT}</td>"

        SIZE=$(ls -l $DICT | awk '{print $5}')
        SIZE=$(((SIZE+1024)/1024))
        echo "<td>${SIZE}K</td>"

        echo "</tr>"
        [ -n "$DO_MD5" ] && md5sum $DICT | awk '{print $1}' > $DICT.md5
    done

    cd ..
}

while [ $# -ge 1 ]; do
    echo $1
    case $1 in
	--md5)
	    DO_MD5=1
	    ;;
	--path)
	    shift
	    DICT_PATH=$1
	    ;;
	--help)
	    usage
	    ;;
	*)
	    usage "Unexpected param $1"
	    ;;
    esac
    shift
done

WD=$(pwd)
cd $DICT_PATH

DIRS=""
for DIR in $(ls); do
    if [ -d $DIR ] && ls $DIR/*.xwd >/dev/null 2>&1; then
        DIRS="$DIRS $DIR"
    fi
done


cat <<EOF
<html>
<head>
<link rel="stylesheet" type="text/css" href="/xw4mobile.css" />
</head>
<body>
EOF

echo "<p>Download dictionaries for:"
for DIR in $DIRS; do
    echo " <a href=\"#$DIR\">$DIR</a>"
done
echo ".</p>"

echo "<table>"
echo "<tr><th>Dictionary</th><th>Wordcount</th><th>Size</th></tr>"
for DIR in $DIRS; do
    do_lang $DIR
done
echo "</table>"

echo "</body></html>"

cd $WD

