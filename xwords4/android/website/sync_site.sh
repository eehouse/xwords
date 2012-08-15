#!/bin/sh

set -u -e

HOST=eehouse.org
USER=eehouse
WEBROOT=/var/www/xw4sms
ACTION=""

EXCLUDES="--exclude=*~ --exclude=*# --exclude=$(basename $0)"

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

if [ "$ACTION" = push ]; then
    echo rsync -avz "$EXCLUDES" . ${USER}@${HOST}:${WEBROOT}/
    rsync -avz "$EXCLUDES" . ${USER}@${HOST}:${WEBROOT}/
elif [ "$ACTION" = pull ]; then
    rsync ${USER}@${HOST}: .
else
    usage "--push or --pull required"
fi

