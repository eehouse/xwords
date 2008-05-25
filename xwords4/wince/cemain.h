/* -*-mode: C; fill-column: 78; c-basic-offset: 4;-*- */ 
/* 
 * Copyright 2000-2007 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#ifdef _WIN32_WCE
# include <aygshell.h>
#endif
#include "draw.h"
#include "game.h"
#include "util.h"
#include "mempool.h"
#include "cesockwr.h"

#ifdef _WIN32_WCE
typedef enum {
    WINCE_UNKNOWN
    , WINCE_PPC_V1
    , WINCE_PPC_2003
    , WINCE_PPC_2005
    , _LAST_PPC                 /* so can test for PPC */
    , WINCE_SMARTPHONE_V1
    , WINCE_SMARTPHONE_2003
    , WINCE_SMARTPHONE_2005
} XW_WinceVersion;

# define IS_SMARTPHONE(g) ((g)->winceVersion > _LAST_PPC)
#else
# define IS_SMARTPHONE(g) XP_FALSE
#endif

enum { CE_BONUS1_COLOR,
       CE_BONUS2_COLOR,
       CE_BONUS3_COLOR,
       CE_BONUS4_COLOR,
       
       CE_BKG_COLOR, 
       CE_TILEBACK_COLOR,

       CE_FOCUS_COLOR,

       CE_USER_COLOR1,
       CE_USER_COLOR2,
       CE_USER_COLOR3,
       CE_USER_COLOR4,

       CE_BLACK_COLOR,     /* not editable by users */
       CE_WHITE_COLOR,

       CE_NUM_COLORS		/* last */
};

#define CUR_CE_PREFS_FLAGS 0x0003 /* adds CE_FOCUS_COLOR */

/* This is what CEAppPrefs looked like for CUR_CE_PREFS_FLAGS == 0x0002 */
typedef struct CEAppPrefs0002 {
    XP_U16 versionFlags;
    CommonPrefs cp;
    COLORREF colors[12]; /* CE_FOCUS_COLOR wasn't there */
    XP_Bool showColors;
} CEAppPrefs0002;

typedef struct CEAppPrefs {
    XP_U16 versionFlags;
    CommonPrefs cp;
    COLORREF colors[CE_NUM_COLORS];
    XP_Bool showColors;
    XP_Bool fullScreen;
} CEAppPrefs;

#define NUM_BUTTONS 4

typedef struct CEAppGlobals {
    HINSTANCE hInst;
    HDC hdc;			/* to pass drawing ctxt to draw code */
    HWND hWnd;
#if defined TARGET_OS_WINCE
    HWND hwndCB;
#endif

    HWND buttons[NUM_BUTTONS];

#ifdef _WIN32_WCE
    SHACTIVATEINFO sai;
    XW_WinceVersion winceVersion;
#else
    /* Store location of dummy button */
    HMENU dummyMenu;
    XP_U16 dummyPos;
#endif

    struct {
        HMENU oldMenu;          /* menu whose item is now on left button */
        XP_U16 oldId;           /* id of item now on left button */
        XP_U16 oldPos;          /* position of prev item within oldMenu */
        wchar_t oldName[32];    /* name of previous item */
    } softkey;

    DrawCtx* draw;
    XWGame game;
    CurGameInfo gameInfo;
    XP_UCHAR* curGameName;      /* path to storage for current game */
    XW_UtilCtxt util;
    VTableMgr* vtMgr;
    XP_U16* bonusInfo;

    XP_U32 timerIDs[NUM_TIMERS_PLUS_ONE];
    XWTimerProc timerProcs[NUM_TIMERS_PLUS_ONE];
    void* timerClosures[NUM_TIMERS_PLUS_ONE];
    XP_U32 timerWhens[NUM_TIMERS_PLUS_ONE];

    XP_U16 flags;               /* bits defined below */

#ifdef CEFEATURE_CANSCROLL
    HWND scrollHandle;
#endif

    CeSocketWrapper* socketWrap;

    CEAppPrefs appPrefs;

    XP_Bool isNewGame;
    XP_Bool penDown;
    XP_Bool hintPending;
    XP_Bool doGlobalPrefs;

#if defined DEBUG && !defined _WIN32_WCE
    int dbWidth, dbHeight;
#endif

#ifdef XWFEATURE_SEARCHLIMIT
    XP_Bool askTrayLimits;
#endif
    MPSLOT

} CEAppGlobals;

#ifdef DICTS_MOVED_ALERT
# define FLAGS_BIT_SHOWN_NEWDICTLOC 0x0001
#endif

#define GAME_IN_PROGRESS(g) ((g)->gameInfo.dictName != 0)

enum {
    XWWM_TIME_RQST = WM_APP
    ,XWWM_PACKET_ARRIVED

};

#define CE_NUM_EDITABLE_COLORS CE_BLACK_COLOR

typedef enum { 
    RFONTS_TRAY
    ,RFONTS_TRAYVAL
    ,RFONTS_CELL
    ,RFONTS_PTS

    ,N_RESIZE_FONTS
} RFIndex;

typedef struct CEDrawCtx {
    DrawCtxVTable* vtable;
    
    HWND mainWin;
    CEAppGlobals* globals;

    COLORREF prevBkColor;

    HBRUSH brushes[CE_NUM_COLORS];

    HFONT selPlayerFont;
    HFONT playerFont;
    HFONT setFont[N_RESIZE_FONTS];
    XP_U16 setFontHt[N_RESIZE_FONTS];

    HBITMAP rightArrow;
    HBITMAP downArrow;
    HBITMAP origin;

    XP_U16 trayOwner;
    XP_U16 miniLineHt;
    XP_Bool scoreIsVertical;

    MPSLOT
} CEDrawCtx;

DrawCtx* ce_drawctxt_make( MPFORMAL HWND mainWin, CEAppGlobals* globals );
void ce_drawctxt_update( DrawCtx* dctx );

int messageBoxChar( CEAppGlobals* globals, XP_UCHAR* str, wchar_t* title, 
                    XP_U16 buttons );

#ifdef DEBUG
void logLastError( const char* comment );
void messageToBuf( UINT message, char* buf, int bufSize );
#else
# define logLastError(c)
#endif

/* These allow LISTBOX and COMBOBOX to be used by the same code */
#ifdef _WIN32_WCE
# define SETCURSEL LB_SETCURSEL
# define GETCURSEL LB_GETCURSEL
# define ADDSTRING LB_ADDSTRING
# define GETLBTEXTLEN LB_GETTEXTLEN
# define GETLBTEXT LB_GETTEXT
#else
# define SETCURSEL CB_SETCURSEL
# define GETCURSEL CB_GETCURSEL
# define ADDSTRING CB_ADDSTRING
# define GETLBTEXTLEN CB_GETLBTEXTLEN
# define GETLBTEXT CB_GETLBTEXT
#endif

#endif /* _CEMAIN_H_ */
