#!/bin/bash
set -e -u

DB=xwgames.sqldb

usage() {
    cat <<EOF
usage: $0
      [--db <path>]  # default: $DB
Removes game and group data from a linux xwords db.
EOF
    exit 0
}

while [[ ! -z ${1+x} ]]; do
    case $1 in
		--db)
			DB=$2
			shift
			;;
		--help)
			usage
			;;
        *)
			echo "Unexpected parameter $1" >&2
            usage
            ;;
	esac
	shift
done

if [ -f $DB ]; then
	echo "delete from pairs where key like 'games/%';" | sqlite3 $DB
	echo "delete from pairs where key like 'groups/%';" | sqlite3 $DB
	echo "delete from pairs where key like 'gmgr/state';" | sqlite3 $DB
else
	echo "file $DB does not exist"
fi
