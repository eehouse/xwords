#!/bin/bash

set -u -e

VARIANT=Xw4Foss
TAGNAME=""
FILES=""
LIST_FILE=''
XW_WWW_PATH=${XW_WWW_PATH:-""}
XW_RELEASE_SCP_DEST=${XW_RELEASE_SCP_DEST:-""}

usage() {
    echo "Error: $*" >&2
    echo "usage: $0 [--tag <name>] [--apk-list path/to/out.txt] \\"
    echo "    [--variant VARIANT] # default value: $VARIANT \\"
    echo "    [<package-unsigned.apk>]" >&2
    exit 1
}

do_build() {
    (cd $(dirname $0)/../ && ./gradlew clean as${VARIANT}Rel)
}

while [ "$#" -gt 0 ]; do
    case $1 in
        --tag)
            TAGNAME=$2
            git describe $TAGNAME || usage "$TAGNAME not a valid git tag"
            shift
            ;;
		--apk-list)
			LIST_FILE=$2
			> $LIST_FILE
			shift
			;;
		--variant)
			VARIANT=${2^}
			shift
			;;
		--help)
			usage
			;;
        *)
            FILES="$1"
            ;;
    esac
    shift
done

if [ -n "$TAGNAME" ]; then
    git branch 2>/dev/null | grep '\* android_branch' \
        || usage "not currently at android_branch"
    git checkout $TAGNAME 2>/dev/null || usage "unable to checkout $TAGNAME"
    HASH=$(git log -1 --pretty=format:%H)
    CHECK_BRANCH=$(git describe $HASH 2>/dev/null)
    if [ "$CHECK_BRANCH" != $TAGNAME ]; then
        usage "tagname not found in repo or not as expected"
    fi
    git stash
fi

if [ -z "$FILES" ]; then
    do_build
	for f in $(ls $(dirname $0)/../app/build/outputs/apk/*/release/*-release-unsigned-*.apk); do
		$(dirname $0)/sign-align.sh --apk $f
	done
fi

if [ -n "$TAGNAME" ]; then
    git stash pop
    git checkout android_branch 2>/dev/null
fi
