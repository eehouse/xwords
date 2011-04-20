#!/bin/sh

set -u -e

RACKPIPE=/tmp/rack_pipe
BOARDPIPE=/tmp/board_pipe

DICT=./all_words_2to15.xwd

if [ ! -e $RACKPIPE ]; then
    rm -rf $RACKPIPE
    mkfifo $RACKPIPE && echo "created pipe $RACKPIPE"
fi
if [ ! -e $BOARDPIPE ]; then
    rm -rf $BOARDPIPE
    mkfifo $BOARDPIPE && echo "created pipe $RACKPIPE"
fi

obj_linux_rel/xwords --board-pipe $BOARDPIPE \
    --rack-pipe $RACKPIPE \
    --board ./board.txt \
    --game-dict $DICT
