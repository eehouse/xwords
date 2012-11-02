#!/bin/sh

set -u -e

FILE=""

usage() {
    [ $# -ge 1 ] && echo "ERROR: $1"
    echo "usage: $0 <path/to/file.xml>"
    echo "print all string names in the .xml file"
    exit 1
}

while [ $# -ge 1 ]; do
    case $1 in
        --help)
            usage
            ;;
        *)
            FILE=$1
            ;;
    esac
    shift
done

[ -e $FILE ] || usage "File $FILE not found"

xmlstarlet sel -T -t -m "/resources/string" -v @name -n $FILE
