#!exec sh

set -e -u

APP=${1:-APP}

mkdir -p /sdcard/${APP}_logs
cd /sdcard/${APP}_logs

FILENAME="$(date +%d-%m-%Y_%H%M%S).log"
logcat -v time >> $FILENAME
