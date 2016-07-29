#!/bin/bash

set -u -e

EXCEPTS=" "
DRY_RUN=''

usage() {
    [ $# -ge 1 ] && echo "ERROR: $1"
    echo "usage: $0 [--dry-run] [--except path/to/file]*"
	echo "  Starting in current directory, recursively remove all non-git-controlled"
	echo "  files, excepting those named with --except flags."
    exit 1
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
		--dry-run)
			DRY_RUN=1
			;;
		--help)
			usage
			;;
        *)
            usage "unexpected param $1"
            ;;
    esac
    shift
done

for FILE in $(find $(pwd) -type f); do
    if [ ! "${EXCEPTS}" = "${EXCEPTS# $FILE }" ]; then
        continue
    elif git ls-files $FILE --error-unmatch 2>/dev/null 1>/dev/null; then
        continue
    else
        echo "$FILE not under git; removing..."
		[ -n "$DRY_RUN" ] || rm $FILE
    fi
done

echo "$0: done" >&2
