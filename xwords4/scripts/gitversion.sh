#!/bin/sh

usage() {
    echo "usage: $0"
    exit 1
}

[ -z "$1" ] || usage

HASH=$(git log -1 --pretty=format:%H)

# mark it with a "+" if anything's changed
if git status --porcelain | grep -q '^[^\?]'; then
	HASH="${HASH}+M"
fi

echo $HASH
