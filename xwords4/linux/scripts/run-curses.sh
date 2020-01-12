#!/bin/sh

# This script just runs the curses app with a set of params known to
# work. At least when it was last committed. :-)

WD=$(cd $(dirname $0)/.. && pwd)
cd $WD
./obj_linux_memdbg/xwords --curses --name Eric --robot Kati --dict-dir ./ --game-dict ../dict.xwd 2>/dev/null

