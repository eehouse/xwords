// -*-mode: C; fill-column: 78; c-basic-offset: 4; -*-
/* 
 * Copyright 1999-2000 by Eric House (xwords@eehouse.org).  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */


#ifndef _FRANKMAIN_H_
#define _FRANKMAIN_H_

#include <assert.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include "sys.h"
#include "gui.h"
#include "OpenDatabases.h"
/*  #include "FieldMgr.h" */

#include "ebm_object.h"

#define SCREEN_WIDTH 200

#define BOARD_LEFT 2

#define SCORE_LEFT BOARD_LEFT
#define SCORE_TOP 3
#define SCORE_WIDTH SCREEN_WIDTH
#define SCORE_HEIGHT 13

#define TIMER_WIDTH 36
#define TIMER_HEIGHT SCORE_HEIGHT

#define BOARD_TOP SCORE_HEIGHT+SCORE_TOP
#define BOARD_SCALE 12

#define TRAY_LEFT BOARD_LEFT
#define MIN_TRAY_SCALE 23
#define FRANK_DIVIDER_WIDTH 5

#define VERSION_STRING "4.0.7a1"

extern "C" {

    typedef struct FrankDrawCtx {
        DrawCtxVTable* vtable;
        CWindow* window;
        const FONT* scoreFnt;
        const FONT* scoreFntBold;
        const FONT* trayFont;
        const FONT* valFont;
        const IMAGE rightcursor;
        const IMAGE downcursor;
        const IMAGE startMark;
#ifdef USE_PATTERNS
        const IMAGE bonusImages[BONUS_LAST];
#endif
    } FrankDrawCtx;

    void debugf( char* format, ... );

} /* extern "C" */

#endif
