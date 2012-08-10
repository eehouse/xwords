#!/bin/sh

set -u -e

HOST=eehouse.org
USER=eehouse
WEBROOT=/var/www/xw4sms
ACTION=""

usage() {
    [ $# -ge 1 ] && echo "ERROR: $1" 1>&2
    echo "usage: $0 --push|--pull \\"
    echo "          [--host <host>]   # default: $HOST \\"
    echo "          [--root <root>]   # default: $WEBROOT \\"
    echo "          [--user <user>]   # default: $USER \\"
    exit 1
}


while [ $# -ge 1 ]; do
    echo $1
    case $1 in
        --push)
            ACTION=push
            ;;
        --pull)
            ACTION=pull
            ;;
        --host)
            shift
            HOST=$1
            ;;
        --root)
            shift
            WEBROOT=$1
            ;;
        --user)
            shift
            USER=$1
            ;;
        *)
            usage "Unexpected param $1"
            ;;
    esac
    shift
done

cd $(dirname $0)
FILES=""
for FILE in $(find . -type f); do
    # echo "file: $FILE"
    case $FILE in
        */$(basename ${0}))
            ;;
        *~)
            ;;
        *)
            FILES="$FILES $FILE"
            ;;
    esac
done

echo $FILES
#exit 0

if [ "$ACTION" = push ]; then
    rsync -avz --exclude=*~ --exclude=$(basename $0) . ${USER}@${HOST}:${WEBROOT}/
elif [ "$ACTION" = pull ]; then
    rsync ${USER}@${HOST}: .
else
    usage "--push or --pull required"
fi

