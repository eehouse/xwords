/* -*-mode: C; fill-column: 76; c-basic-offset: 4; -*- */
/* 
 * Copyright 1999 - 2003 by Eric House (fixin@peak.org).  All rights reserved.
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
#ifndef _PALMMAIN_H_
#define _PALMMAIN_H_

#define AppType APPID
#define PrefID 0
#define VERSION_NUM 1

#include <PalmTypes.h>
#include <DataMgr.h>
#include <Window.h>
#include <List.h>
#include <Form.h>
#include <IrLib.h>
#ifdef BEYOND_IR
# include <NetMgr.h>
#endif

#include "game.h"
#include "util.h"
#include "mempool.h"

/* #include "prefsdlg.h" */
#include "xwcolors.h"

#include "xwords4defines.h"

#ifdef MEM_DEBUG
# define MEMPOOL globals->mpool,
#else
# define MEMPOOL 
#endif


enum { ONEBIT, /*  GREYSCALE,  */COLOR };
typedef unsigned char GraphicsAbility; /* don't let above be 4 bytes */
typedef struct PalmAppGlobals PalmAppGlobals;

typedef XP_UCHAR* (*GetResStringFunc)( PalmAppGlobals* globals, 
				    XP_U16 strID );
typedef struct PalmDrawCtx {
    DrawCtxVTable* vtable;
    PalmAppGlobals* globals;

    void (*drawBitmapFunc)( DrawCtx* dc, Int16 resID, Int16 x, Int16 y );
    GetResStringFunc getResStrFunc;

    DrawingPrefs* drawingPrefs;

    RectangleType oldScoreClip;
    RectangleType oldTrayClip;
    WinHandle numberWin;

    XP_S16 trayOwner;

    GraphicsAbility able;

#ifdef FEATURE_HIGHRES
    UInt16 oldCoord;
    XP_Bool doHiRes;
#endif

    union {
        struct {
/* 	    IndexedColorType black; */
/* 	    IndexedColorType white; */
/* 	    IndexedColorType playerColors[MAX_NUM_PLAYERS]; */
/* 	    IndexedColorType bonusColors[BONUS_LAST-1]; */
        XP_U8 reserved;         /* make CW compiler happy */
        } clr;
        struct {
            CustomPatternType valuePatterns[4];
        } bnw;
    } u;
    MPSLOT
} PalmDrawCtx;

#define draw_drawBitmapAt(dc,id,x,y) \
     (*((((PalmDrawCtx*)dc))->drawBitmapFunc))((dc),(id),(x),(y))

typedef struct ListData {
    unsigned char** strings;
    unsigned char* storage;
    XP_U16 nItems;
    XP_U16 storageLen;
    XP_U16 nextIndex;
    XP_S16 selIndex;
#ifdef DEBUG
    XP_Bool choicesSet;    /* ARM hack: don't use strings after PACE
                              swaps.... */
#endif
} ListData;

typedef struct XWords4PreferenceType {
    Int16 versionNum;

    Int16 curGameIndex;		/* which game is currently open */

    /* these are true global preferences */
    Boolean showProgress;
    Boolean showGrid;
    Boolean showColors;
#ifdef DEBUG
    Boolean showDebugstrs;
    Boolean logToMemo;
    Boolean reserved1;
    Boolean reserved2;
#else
    Boolean reserved1;
#endif
    /* New for 0x0405 */
    CommonPrefs cp;
    
} XWords4PreferenceType;

typedef struct MyIrConnect {
    IrConnect irCon;
    PalmAppGlobals* globals;
} MyIrConnect;

typedef XP_U8 IR_STATE;		/* enums are in palmir.h */

#define IR_BUF_SIZE 256

typedef struct MyIrPacket MyIrPacket;

typedef struct ProgressCtxt {
    RectangleType boundsRect;
    XP_U16 curLine;
} ProgressCtxt;

/* I *hate* having to define these globally... */
typedef struct SavedGamesState {
    struct PalmAppGlobals* globals;
    FormPtr form;
    ListPtr gamesList;
    FieldPtr nameField;
    char** stringPtrs;
    Int16 nStrings;
    Int16 displayGameIndex;
} SavedGamesState;

typedef struct PrefsDlgState {
    ListPtr playerBdSizeList;
    ListPtr phoniesList;

    CommonPrefs cp;

    XP_U16 gameSeconds;
    XP_Bool stateTypeIsGlobal;

    XP_U8   phoniesAction;
    XP_U8   curBdSize;
    XP_Bool showColors;
    XP_Bool smartRobot;
    XP_Bool showProgress;
    XP_Bool showGrid;
    XP_Bool hintsNotAllowed;
    XP_Bool timerEnabled;
#ifdef FEATURE_TRAY_EDIT
    XP_Bool allowPickTiles;
#endif
#ifdef XWFEATURE_SEARCHLIMIT
    XP_Bool allowHintRect;
#endif
} PrefsDlgState;

typedef struct DictState {
    ListPtr dictList;
    ListData sLd;
    XP_U16 nDicts;
} DictState;

typedef struct ConnDlgAddrs {
    XP_U32 remoteIP;
    XP_U16 remotePort;
    XP_U16 localPort;
    CommsConnType conType;
} ConnDlgAddrs;

typedef struct PalmNewGameState {
    ListPtr playerNumList;
    XP_UCHAR* passwds[MAX_NUM_PLAYERS];
    XP_UCHAR* dictName;
    XP_Bool isLocal[MAX_NUM_PLAYERS];
    XP_Bool isRobot[MAX_NUM_PLAYERS];
    XP_UCHAR shortDictName[32]; /* as long as a dict name can be */
    XP_U8 curNPlayersLocal;
    XP_U8 curNPlayersTotal;
    XP_Bool forwardChange;
    Connectedness curServerHilite;
#ifdef BEYOND_IR
    ConnDlgAddrs connAddrs;
    XP_Bool connsSettingChanged;
#endif
} PalmNewGameState;

typedef struct PalmDictList PalmDictList;

#ifdef BEYOND_IR
typedef struct NetLibStuff {
    UInt16 netLibRef;
    XP_U16 listenPort;
    NetSocketRef socketRef;
} NetLibStuff;
#define socketIsOpen(g) ((g)->nlStuff.listenPort != 0)
#endif

#define MAX_DLG_PARAMS 2

struct PalmAppGlobals {
    FormPtr mainForm;
    PrefsDlgState* prefsDlgState;
#if defined OWNER_HASH || defined NO_REG_REQUIRED
    SavedGamesState* savedGamesState;
#endif
    XWGame game;
    DrawCtx* draw;
    XW_UtilCtxt util;

    XP_U32 dlgParams[MAX_DLG_PARAMS];

    VTableMgr* vtMgr;

    XWords4PreferenceType gState;

    DrawingPrefs drawingPrefs;

    PalmDictList* dictList;

    DmOpenRef boardDBP;
    LocalID boardDBID;

    DmOpenRef gamesDBP;
    LocalID gamesDBID;

#ifdef BEYOND_IR
    UInt16 exgLibraryRef;    /* what library did user choose for sending? */
#endif

    XP_UCHAR* stringsResPtr;
    XP_U8* bonusResPtr[NUM_BOARD_SIZES];
    Boolean penDown;
    Boolean isNewGame;
    Boolean stateTypeIsGlobal;
    Boolean timeRequested;
    Boolean hintPending;
    Boolean isLefty;
    Boolean dictuiForBeaming;
    Boolean postponeDraw;
    Boolean needsScrollbar;
    Boolean msgReceivedDraw;
    Boolean isFirstLaunch;
    Boolean menuIsDown;
    XP_Bool newGameIsNew;

    GraphicsAbility able;
    XP_U32 penTimerFireAt;
    XP_U32 timerTimerFireAt;
    XP_U16 prevScroll;		/* for scrolling in 'ask' dialog */
    UInt16 romVersion;

    XP_U8 scrollValue;		/* 0..2: scrolled position of board */

#ifdef SHOW_PROGRESS
    ProgressCtxt progress;
#endif

#ifdef FEATURE_HIGHRES
    XP_U16 width, height;
    XP_U16 sonyLibRef;
    XP_Bool doVSK;
    XP_Bool hasHiRes;
#endif

#ifdef XWFEATURE_SEARCHLIMIT
    XP_Bool askTrayLimits;
#endif

    CurGameInfo gameInfo;	/* for the currently open, or new, game */

    /* dialog/forms state */
    PalmNewGameState newGameState;

    DictState dictState;

    struct ConnsDlgState* connState;

#ifdef BEYOND_IR
    NetLibStuff nlStuff;
#endif

#ifdef DEBUG
    UInt8 save_rLsap;
    IR_STATE ir_state_prev;
    XP_U16 yCount;
/*     Boolean resetGame; */
#endif
    MPSLOT
}; /* PalmAppGlobals */

/* custom events */
enum { dictSelectedEvent = firstUserEvent /* 0x6000 */
       ,newGameOkEvent
       ,newGameCancelEvent
       ,loadGameEvent
       ,prefsChangedEvent
       ,openSavedGameEvent
#ifdef BEYOND_IR
       ,connsSettingChgEvent
#endif
#ifdef FEATURE_SILK
       ,doResizeWinEvent
#endif
};

enum {
    PNOLET_STORE_FEATURE = 1
    , GLOBALS_FEATURE
    , WANTS_ARM_FEATURE
};
enum { WANTS_68K, WANTS_ARM };


/* If we're calling the old PilotMain (in palmmain.c) from from the one in
   enter68k.c it needs a different name.  But if this is the 68K-only app
   then that is the entry point. */
#ifdef FEATURE_PNOAND68K
# define PM2(pm) pm2_ ## pm
UInt32 PM2(PilotMain)(UInt16 cmd, MemPtr cmdPBP, UInt16 launchFlags);
#else
# define PM2(pm) pm
#endif

DrawCtx* palm_drawctxt_make( MPFORMAL GraphicsAbility able,
			     PalmAppGlobals* globals,
			     GetResStringFunc getRSF,
			     DrawingPrefs* drawprefs );
void palm_drawctxt_destroy( DrawCtx* dctx );

void palm_warnf( char* format, ... );

Boolean askPassword( PalmAppGlobals* globals, const XP_UCHAR* name, 
                     Boolean isNew, XP_UCHAR* retbuf, XP_U16* len );
Boolean palmaskFromStrId( PalmAppGlobals* globals, XP_U16 strId, 
			  XP_S16 titleID, XP_S16 altID );
void freeSavedGamesData( MPFORMAL SavedGamesState* state );

void palm_util_requestTime( XW_UtilCtxt* uc ); /* so palmir can call */
void writeNameToGameRecord( PalmAppGlobals* globals, XP_S16 index, 
			    char* newName, XP_U16 len );

XP_UCHAR* getResString( PalmAppGlobals* globals, XP_U16 strID );
Boolean palmask( PalmAppGlobals* globals, XP_UCHAR* str, XP_UCHAR* altButton, 
                 XP_S16 titleID );

#ifdef XW_TARGET_PNO
# define READ_UNALIGNED16(n) read_unaligned16(n)
#else
# define READ_UNALIGNED16(n) *(n)
#endif

#endif /* _PALMMAIN_H_ */
