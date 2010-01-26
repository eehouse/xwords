/* -*-mode: C; fill-column: 78; c-basic-offset: 4;-*- */ 
/* 
 * Copyright 2000-2009 by Eric House (xwords@eehouse.org).  All rights reserved.
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
#include "ceconnmg.h"

#define LCROSSWORDS_DIR_NODBG L"Crosswords"
#define CE_GAMEFILE_VERSION1 0x01  /* means draw gets to save/restore */
#define CE_GAMEFILE_VERSION2 0x02 /* save/restore includes width */
#define CE_GAMEFILE_VERSION CE_GAMEFILE_VERSION2
#ifdef DEBUG
# define CROSSWORDS_DIR "Cross_dbg"
# define LCROSSWORDS_DIR L"Cross_dbg"
#else
# define CROSSWORDS_DIR "Crosswords"
# define LCROSSWORDS_DIR LCROSSWORDS_DIR_NODBG
#endif

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
# define IS_SMARTPHONE(g) ((g) != (g)) /* make compiler warnings go away  */
#endif

enum { CE_BONUS0_COLOR,
       CE_BONUS1_COLOR,
       CE_BONUS2_COLOR,
       CE_BONUS3_COLOR,
       
       CE_BKG_COLOR, 
       CE_TILEBACK_COLOR,

       CE_FOCUS_COLOR,

       CE_PLAYER0_COLOR,
       CE_PLAYER1_COLOR,
       CE_PLAYER2_COLOR,
       CE_PLAYER3_COLOR,

       CE_BLACK_COLOR,     /* not editable by users */
       CE_WHITE_COLOR,

       CE_NUM_COLORS		/* last */
};

// #define CUR_CE_PREFS_FLAGS 0x0003 /* adds CE_FOCUS_COLOR */
#define CUR_CE_PREFS_FLAGS 0x0004 /* moves showColors into CommonPrefs */

/* This is what CEAppPrefs looked like for CUR_CE_PREFS_FLAGS == 0x0002 */
typedef struct CEAppPrefs0002 {
    XP_U16 versionFlags;
    CommonPrefs cp;
    COLORREF colors[12]; /* CE_FOCUS_COLOR wasn't there */
    XP_Bool showColors;
} CEAppPrefs0002;

/* This is what CEAppPrefs looked like for CUR_CE_PREFS_FLAGS == 0x0003 */
typedef struct _CEAppPrefs0003 {
    XP_U16 versionFlags;
    struct {
        XP_Bool         showBoardArrow;  /* applies to all games */
        XP_Bool         showRobotScores; /* applies to all games */
        XP_Bool         hideTileValues; 
        XP_Bool         skipCommitConfirm; /* applies to all games */
#ifdef XWFEATURE_SLOW_ROBOT
        XP_U16          robotThinkMin, robotThinkMax;
#endif
    } cp;
    COLORREF colors[13];
    XP_Bool showColors;
    XP_Bool fullScreen;
} CEAppPrefs0003;

typedef enum {
    SAB_NONE = 0
    ,SAB_PHONEOFF = 1 << 0
    ,SAB_NETFAILED = 1 << 1
    ,SAB_HOST_CONND = 1 << 2
    ,SAB_CLIENT_CONND = 1 << 3
    ,SAB_ALL_HERE = 1 << 4
    ,SAB_HEART_YOU = 1 << 5
    ,SAB_HEART_OTHER = 1 << 6
} SkipAlertBits;

typedef struct CEAppPrefs {
    XP_U16 versionFlags;
    CommonPrefs cp;
    COLORREF colors[CE_NUM_COLORS];
    XP_Bool fullScreen;
} CEAppPrefs;

enum { OWNED_RECT_LEFT
       ,OWNED_RECT_RIGHT
       ,OWNED_RECT_TOP
       ,OWNED_RECT_BOTTOM
       ,N_OWNED_RECTS
};

enum {
    MY_DOCS_CACHE,
    PROGFILES_CACHE,
    N_CACHED_PATHS
};

typedef struct _TimerData {
    XP_U32 id;
    XWTimerProc proc;
    void* closure;
    XP_U32 when;
} TimerData;

typedef struct _CEAppGlobals {
    HINSTANCE hInst;
    HINSTANCE locInst;  /* same as hInst if no l10n DLL in use */
    HDC hdc;			/* to pass drawing ctxt to draw code */
    HWND hWnd;
#ifdef _WIN32_WCE
    HWND hwndCB;
    SHACTIVATEINFO sai;
    XW_WinceVersion winceVersion;
#else
    /* Store location of dummy button */
    HMENU dummyMenu;
    XP_U16 dummyPos;
#endif

    XP_U16 softKeyId;           /* id of item now on left button */
#ifndef _WIN32_WCE
    HMENU softKeyMenu;          /* so can check/uncheck duplicated items */
#endif

#if defined _WIN32_WCE && ! defined CEGCC_DOES_CONNMGR
    HINSTANCE hcellDll;
    /* UINT connmgrMsg; */
    CMProcs cmProcs;
#endif

    struct CEDrawCtx* draw;
    XWGame game;
    CurGameInfo gameInfo;
    XP_UCHAR* curGameName;      /* path to storage for current game */
    XP_UCHAR* langFileName;     /* language file currently loaded or chosen */
    XW_UtilCtxt util;
    VTableMgr* vtMgr;
    XP_U16* bonusInfo;

    TimerData timerData[NUM_TIMERS_PLUS_ONE];

    RECT ownedRects[N_OWNED_RECTS];

    XP_U16 flags;               /* bits defined below */
    XP_U16 cellHt;              /* how tall is a cell given current layout */

#ifdef CEFEATURE_CANSCROLL
    HWND scrollHandle;
    WNDPROC oldScrollProc;
#ifdef _WIN32_WCE
    RECT scrollRects[2];        /* above and below the scroller */
#endif
    XP_Bool scrollerHasFocus;
#endif
#ifdef KEYBOARD_NAV
    XP_Bool keyDown;
#endif
    struct CeSocketWrapper* socketWrap;

    CEAppPrefs appPrefs;

    SkipAlertBits skipAlrtBits;         /* bit vector */

    XP_Bool isNewGame;
    XP_Bool penDown;
    XP_Bool hintPending;
    XP_Bool doGlobalPrefs;
    XP_Bool exiting;            /* are we in the process of shutting down? */

#ifdef XWFEATURE_RELAY
    CommsRelayState relayState;
    CeConnState socketState;
    RECT relayStatusR;
#endif

#ifndef _WIN32_WCE
    XP_U16 dbWidth, dbHeight;
#endif

#ifdef LOADSTRING_BROKEN
    void* resStrStorage;        /* used in ceresstr.c */
#endif

    wchar_t* specialDirs[N_CACHED_PATHS];     /* reserved for ceGetPath() */

#ifdef XWFEATURE_SEARCHLIMIT
    XP_Bool askTrayLimits;
#endif
    MPSLOT

} CEAppGlobals;

/* No longer used, but may need to keep set for backwards compatibility */
# define FLAGS_BIT_SHOWN_NEWDICTLOC 0x0001

#define GAME_IN_PROGRESS(g) ((g)->gameInfo.dictName != 0)

enum {
    XWWM_TIME_RQST = WM_APP
    ,XWWM_REM_SEL
    ,XWWM_HOSTNAME_ARRIVED
    ,XWWM_RELAY_REQ_NEW
    ,XWWM_RELAY_REQ_CONN
    ,XWWM_SOCKET_EVT
#ifdef _WIN32_WCE
    ,XWWM_CONNMGR_EVT
#endif
};

#define CE_NUM_EDITABLE_COLORS CE_BLACK_COLOR


XP_Bool queryBoxChar( CEAppGlobals* globals, HWND hWnd, 
                      const XP_UCHAR* msg );

/* These allow LISTBOX and COMBOBOX to be used by the same code */

#define INSERTSTRING(g) (IS_SMARTPHONE(g)?LB_INSERTSTRING:CB_INSERTSTRING)
#define SETCURSEL(g) (IS_SMARTPHONE(g)?LB_SETCURSEL:CB_SETCURSEL)
#define GETCURSEL(g) (IS_SMARTPHONE(g)?LB_GETCURSEL:CB_GETCURSEL)
#define ADDSTRING(g)  (IS_SMARTPHONE(g)?LB_ADDSTRING:CB_ADDSTRING)
#define GETLBTEXT(g)  (IS_SMARTPHONE(g)?LB_GETTEXT:CB_GETLBTEXT)
#define GETLBTEXTLEN(g)  (IS_SMARTPHONE(g)?LB_GETTEXTLEN:CB_GETLBTEXTLEN)
#define FINDSTRINGEXACT(g) \
    (IS_SMARTPHONE(g)?LB_FINDSTRINGEXACT:CB_FINDSTRINGEXACT)
#define LB_IF_PPC(g,id)  (IS_SMARTPHONE(g)?id:(id+2))


#define BACK_KEY_UP_MAYBE 0x1000
#define CE_MAX_PATH_LEN 256

#ifndef DM_RESETSCROLL
//http://www.nah6.com/~itsme/cvs-xdadevtools/itsutils/src/its_windows_message_list.txt
# define DM_RESETSCROLL 0x0402
#endif

#endif /* _CEMAIN_H_ */
