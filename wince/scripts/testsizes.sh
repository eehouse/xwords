#!/bin/bash

PLAT=win32
DBG=dbg

SIZES=( 
    240x214                     # Treo in full-screen mode
    240x188                     # Treo in normal mode

    176x200                     # 6.1 standard
    200x176                     # flipped
    
    480x536                     # VGA, non full-screen
    536x480
    
    400x240
    240x400
    440x240
    240x440

    320x186                     # WM 6.1 Standard landscape
    186x320

    240x266                     # WM 6.1 Standard QVGA
    266x240                     # flipped

    266x320                     # WM 6.1 Standard square w/title bar
    320x266                     # flipped
    320x320                     # full-screen mode

    320x250                     # WM 6 Pro 320x320 short for some reason
    320x285                     # with tile bar
    # 320x320                     # full-screen mode
)

cd $(dirname $0)
EXES=$(ls -c ../obj_${PLAT}_${DBG}/built/xwords4_*.exe)

if ls ../obj_${PLAT}_${DBG}/built/*.xwd >/dev/null 2>&1;  then
    : # nothing to do
elif [ -s "$XWDICT" ]; then
   cp $XWDICT ../obj_${PLAT}_${DBG}
else
    cp ../../dawg/English/BasEnglish2to8.xwd ../obj_${PLAT}_${DBG}
fi

for SIZE in ${SIZES[*]}; do
    WIDTH=${SIZE%x*}
    HEIGHT=${SIZE#*x}

    for EXE in $EXES; do        # 
        CMD="wine $EXE width=$WIDTH height=$HEIGHT"
        echo $CMD
        eval "$CMD"
        break                   # -c sorts by date, so quit after running newest
    done

done
