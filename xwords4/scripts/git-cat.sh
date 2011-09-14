#!/bin/sh

set -u -e

REV=HEAD
BRANCH=""
FILES=""

usage() {
    echo "usage: $0 [--branch BRANCH | --rev REV ] path/to/file [path/to/file]*"
    exit 0
}

while [ $# -ge 1 ]; do
    case $1 in
        --branch)
            shift
            [ $# -gt 1 ] || usage "--branch requires a parameter"
            BRANCH=$1
            ;;
        --rev)
            shift
            [ $# -gt 1 ] || usage "--rev requires a parameter"
            REV=$1
            ;;
        --help)
            usage
            ;;
        *)
            FILES="$FILES $1"
            ;;
    esac
    shift
done

[ -n "$FILES" ] || usage

if [ -n "$BRANCH" ]; then
    REV=$(git log $BRANCH | grep '^commit' | head -n 1 | awk '{print $2}')
fi

for FILE in $FILES; do
    FILE=$(git ls-files --full-name $FILE)
    git show $REV:$FILE
done
