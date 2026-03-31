#!/bin/bash

set -e -u

NO_RM=''
NO_UPLOAD=''

BRANCH=$(git branch --show-current)
DIR=/tmp/build_$$_dir

KNOWN_HOSTS=("eehouse.org"
			 "staging"
			 "eehouse"
			 "dev"
			 "pi4.liquidsugar.net"
			)
HOSTS=''

KNOWN_SRCS=("github"
            "eehouse"
            "staging"
            )
REMOTE=''

# REMOTE=https://github.com/eehouse/xwords.git
# REMOTE=ssh://prod@eehouse/home/prod/repos/xwords

usage() {
    [ $# -ge 1 ] && echo "Error: $1"
	echo "usage: $0 [--no-upload]     # do not remove build directory \\"
	echo "    [--no-rm]               # do not remove build directory \\"
	echo "    [--branch <git-branch>] # build this branch, not current \\"
	echo "    [--host <hostname>]* # add to list of upload targets \\"
    echo "builds debug variant from the current tip of github"
	echo "(last modified Jan 2024)"
    exit 1
}

while [ $# -ge 1 ]; do
    case $1 in
		--help)
			usage
			;;
		--host)
			HOSTS="${HOSTS} $2"
			shift
			;;
		--branch)
			BRANCH=$2
			shift
			;;
		--no-rm)
			NO_RM=1
			;;
		--no-upload)
			NO_UPLOAD=1
			;;
		*)
			usage "unexpected command $1"
			;;
	esac
	shift
done

if [ -z "$HOSTS" ]; then
	while :; do
		echo "Choose a host (by number); d when done; 'q' to exit: "
		for ii in "${!KNOWN_HOSTS[@]}"; do
			printf "[%d] %s\n" "$ii" "${KNOWN_HOSTS[$ii]}"
		done
		read CHOICE

		case $CHOICE in
			[0-9])
				HOSTS="${HOSTS} ${KNOWN_HOSTS[$CHOICE]}"
				;;
			q) break
			   exit 1
			   ;;
			d) break
			   ;;
		esac
	done
fi

while [ -z "$REMOTE" ]; do
	echo "Choose a git source host (by number); 'q' to exit: "
	for ii in "${!KNOWN_SRCS[@]}"; do
		printf "[%d] %s\n" "$ii" "${KNOWN_SRCS[$ii]}"
	done
	read CHOICE

	case $CHOICE in
		[0-9])
			REMOTE="${KNOWN_SRCS[$CHOICE]}"
            case "$REMOTE" in
                "github")
                    REMOTE="https://github.com/eehouse/xwords.git"
                    ;;
                "eehouse")
                    REMOTE="ssh://prod@eehouse/home/prod/repos/xwords"
                    ;;
                "staging")
                    REMOTE="ssh://prod@staging/home/prod/repos/xwords"
                    ;;
                *) usage "bad host???"
                   ;;
            esac
			;;
		q) break
		   exit 1
		   ;;
	esac
done

[ -z "${HOSTS}" ] && usage "no host set"

mkdir -p $DIR
pushd $DIR

git clone --branch $BRANCH --recurse-submodules ${REMOTE}
cd xwords/xwords4/android

case $BRANCH in
	"main")
		TARGET=asXw4dDeb
		;;
	"gameref")
		TARGET=asXw4grdDeb
		;;
	*) fail
	   ;;
esac

if [ -z "$(git describe 2>/dev/null)" ]; then
	echo "git tags required but not found"
	echo "skipping build"
else
	./gradlew $TARGET

	APK="$(find . -name '*.apk')"
	# pull something like xw4d out of the path
	SERVER_DIR=$(basename $(dirname $(dirname $APK)))
	echo "APK: $APK; SERVER_DIR: ${SERVER_DIR}"

	if [ -n "${NO_UPLOAD}" ]; then
		: # do nothing
	elif [ -n "${HOSTS}" ]; then
		for HOST in ${HOSTS}; do
			echo "need to upload to $HOST"
			HOST_DIR="/var/www/html/android/${SERVER_DIR}/"
			ssh ${HOST} mkdir -p ${HOST_DIR}
			echo "doing: scp $APK ${HOST}:${HOST_DIR}"
			scp "$APK" "${HOST}:${HOST_DIR}"
			echo "uploaded $APK to ${HOST}/${HOST_DIR}/"
		done
	else
		echo "no host to upload to"
	fi
fi

popd
if [ -z "${NO_RM}" ]; then
	rm -rf $DIR
else
	echo "not removing: $DIR"
fi
