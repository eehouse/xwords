#!/bin/sh

set -u -e

RACKPIPE=/tmp/rack_pipe
BOARDPIPE=/tmp/board_pipe

DICT=./BasEnglish2to8.xwd

onsignals() {
    echo "onsignals called"
    rm -f $RACKPIPE $BOARDPIPE
}

trap onsignals TERM INT ABRT

if [ ! -e $RACKPIPE ]; then
    mkfifo $RACKPIPE && echo "created pipe $RACKPIPE"
fi
if [ ! -e $BOARDPIPE ]; then
    mkfifo $BOARDPIPE && echo "created pipe $RACKPIPE"
fi

# obj_linux_memdbg/xwords --board-pipe $BOARDPIPE  --rack-pipe $RACKPIPE \
#     --game-dict dict.xwd || true
obj_linux_memdbg/xwords --board-pipe $BOARDPIPE  --rack "ABCQXZW" \
    --game-dict $DICT || true

# rm -f $RACKPIPE $BOARDPIPE
