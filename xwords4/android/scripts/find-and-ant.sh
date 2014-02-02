#!/bin/sh

set -e -u

MANIFEST=AndroidManifest.xml

usage() {
    [ $# -ge 1 ] && echo "ERROR: $1"
    echo "usage: $0 <cmds to ant>"
	echo "   default commands: $CMDS"
    exit 1
}

CMDS="clean debug install"
if [ $# -gt 0 ]; then
    case $1 in
	--help|-h|-help|-?)
	    usage
	    ;;
	*)
	    CMDS="$*"
	    break;
	    ;;
    esac
    shift
fi

while [ ! -e $MANIFEST ]; do
    [ '/' = $(pwd) ] && usage "reached root without finding $MANIFEST"
    cd ..
done

ant $CMDS
