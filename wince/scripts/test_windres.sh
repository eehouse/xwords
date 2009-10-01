#!/bin/sh

# Make sure the windres we have is correctly converting utf-8 to
# utf-16.  We'll cons up a fake .rc file, compile it, hexdump it and
# look for a pattern.  Use Polish for grins.

usage() {
    echo "usage: $0 path/to/windres"
}

STR="Wartości i ilości klocków"
RC_FILE=/tmp/test_$$.rc
DOT_O_FILE=/tmp/test_$$.o
WINDRES=$1
which $WINDRES || usage

# Create the .rc file
cat > $RC_FILE <<EOF
STRINGTABLE DISCARDABLE 
BEGIN
1000 "$STR"
END
EOF

# compile it
${WINDRES} -c 65001 $RC_FILE -o $DOT_O_FILE

NEEDLE=$(echo -n "$STR" | iconv -t UTF-16LE | hexdump -v -e '1/1 "%02X"')
HAYSTACK=$(hexdump -v -e '1/1 "%02X"' < $DOT_O_FILE)

rm -f $RC_FILE $DOT_O_FILE

if echo $HAYSTACK | grep -q $NEEDLE; then
    exit 0
else
    echo "failed!!!"
    echo $NEEDLE
    echo $HAYSTACK
    exit 1
fi
