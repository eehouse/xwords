#!/bin/sh

COOKIE=$$

./obj_linux_memdbg/xwords -d dict.xwd -r Brynn -a localhost \
    -p 10999 -C $COOKIE &
./obj_linux_memdbg/xwords -d dict.xwd -r Ariela -a localhost \
    -p 10999 -C $COOKIE &
./obj_linux_memdbg/xwords -d dict.xwd -r Kati -a localhost \
    -p 10999 -C $COOKIE &
./obj_linux_memdbg/xwords -d dict.xwd -r Eric -s -N -N -N -a localhost \
    -p 10999 -C $COOKIE &
