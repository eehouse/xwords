/* -*-mode: C; fill-column: 78; c-basic-offset: 4;-*- */ 
/* 
 * Copyright 2000-2002 by Eric House (fixin@peak.org).  All rights reserved.
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

#ifndef _CEMAIN_H_
#define _CEMAIN_H_

#include "draw.h"
#include "game.h"
#include "util.h"
#include "mempool.h"

typedef struct CEAppPrefs {
    XP_U16 versionFlags;
    CommonPrefs cp;
    XP_Bool showColors;
} CEAppPrefs;

#define NUM_BUTTONS 4

typedef struct CEAppGlobals {
    HINSTANCE hInst;
    HDC hdc;			/* to pass drawing ctxt to draw code */
    HWND hWnd;
    HWND hwndCB;

    HWND buttons[NUM_BUTTONS];

    DrawCtx* draw;
    XWGame game;
    CurGameInfo gameInfo;
    XP_UCHAR* curGameName;      /* path to storage for current game */
    XW_UtilCtxt util;
    VTableMgr* vtMgr;
    XP_U16* bonusInfo;

    CEAppPrefs appPrefs;

    XP_Bool isNewGame;
    XP_Bool penDown;
    XP_Bool hintPending;
    XP_Bool doGlobalPrefs;

    MPSLOT

} CEAppGlobals;

#define GAME_IN_PROGRESS(g) ((g)->gameInfo.dictName != 0)

enum {
    XWWM_TIME_RQST = WM_USER,

    XW_TIME_RQST
};

enum { BKG_COLOR, 

       BONUS1_COLOR,
       BONUS2_COLOR,
       BONUS3_COLOR,
       BONUS4_COLOR,
       
       TILEBACK_COLOR,
       BLACK_COLOR,
       WHITE_COLOR,

       USER_COLOR1,
       USER_COLOR2,
       USER_COLOR3,
       USER_COLOR4,

       NUM_COLORS		/* last */
};

typedef struct CEDrawCtx {
    DrawCtxVTable* vtable;
    
    HWND mainWin;
    CEAppGlobals* globals;

    COLORREF prevBkColor;

    COLORREF colors[NUM_COLORS];
    HBRUSH brushes[NUM_COLORS];

    HFONT trayFont;
    HFONT selPlayerFont;
    HFONT playerFont;

    HBITMAP rightArrow;
    HBITMAP downArrow;
    HBITMAP origin;

    XP_U16 trayOwner;

    MPSLOT
} CEDrawCtx;

DrawCtx* ce_drawctxt_make( MPFORMAL HWND mainWin, CEAppGlobals* globals );

#endif /* _CEMAIN_H_ */
