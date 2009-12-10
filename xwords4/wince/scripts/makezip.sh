#!/bin/sh

DICT=../dawg/English/BasEnglish2to8.xwd
TMPDIR=/tmp/_$$
README=$TMPDIR/README.txt

function usage() {
    echo "usage: $0 --exe exe_file "
    echo "         [--name exe_name_to_use]"
    echo "         [--dict dict_to_include]"
    echo "         [--out zipfile_to_produce]"
    exit 0
}

while [ -n "$1" ]; do
    [ -z "$2" ] && usage
    case "$1" in
        --exe)
            EXE="$2"
            ;;
        --name)
            NAME="$2"
            ;;
        --dict)
            DICT="$2"
            ;;
        --out)
            OUTFILE="$2"
            ;;
        *)
            usage
    esac
    shift 2
done

[ -z "$EXE" ] && usage
[ -z "$OUTFILE" ] && OUTFILE=${EXE%.exe}.zip

mkdir -p $TMPDIR

# If name's specified, we need to create that file.  Do it before
# catting text below so EXE will be correct

if [ ! -z "$NAME" ]; then
    NAME=$TMPDIR/$NAME
    echo "copying $EXE to $NAME"
    cp $EXE $NAME
    EXE=$NAME
fi

cat > $README <<EOF

Thanks for downloading Crosswords 4.4 Beta 8 for Smartphone and PocketPC.

To install, copy the enclosed executable file ($(basename $EXE)) and dictionary file ($(basename $DICT)) into the same directory on your device using File Explorer, then double-click on the executable to launch it.

For a users manual, dictionaries in other languages, upgrades, information on reporting bugs, etc., point your browser at http://xwords.sf.net.  See the "Smartphone" menu there for navigation tips.

Crosswords is free/open source software.  Share it with your friends.  If you develop software yourself, check out the code (at the above URL.)

Enjoy!

--Eric House (ehouse@users.sf.net)
EOF

# Make README readable on Wince
todos $README

rm -f $OUTFILE
CMD="zip -j $OUTFILE $EXE $DICT $README"
echo $CMD
eval $CMD

echo "Done: "
zipinfo $OUTFILE

rm -rf $TMPDIR
