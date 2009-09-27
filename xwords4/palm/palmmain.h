/* -*-mode: C; fill-column: 76; c-basic-offset: 4; -*- */
/* 
 * Copyright 1999 - 2007 by Eric House (xwords@eehouse.org).  All rights reserved.
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
#define VERSION_NUM_405 1
#define VERSION_NUM     2           /* 1 to 2 moving to ARM */

#include <PalmTypes.h>
#include <DataMgr.h>
#include <Window.h>
#include <List.h>
#include <Form.h>
#include <IrLib.h>
#ifdef XWFEATURE_RELAY
# include <NetMgr.h>
#endif

#include "game.h"
#include "util.h"
#include "mempool.h"
#include "nwgamest.h"

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

typedef const XP_UCHAR* (*GetResStringFunc)( PalmAppGlobals* globals, 
                                             XP_U16 strID );

typedef struct {
    XP_S16 topOffset;           /* how many pixels from the top of the
                                   drawing area is the first pixel set in
                                   the glyph */
    XP_U16 height;              /* How many rows tall is the image? */
} PalmFontHtInfo;

typedef struct PalmDrawCtx {
    DrawCtxVTable* vtable;
    PalmAppGlobals* globals;

    void (*drawBitmapFunc)( DrawCtx* dc, Int16 resID, Int16 x, Int16 y );
    GetResStringFunc getResStrFunc;

    DrawingPrefs* drawingPrefs;

    RectangleType oldScoreClip;
    RectangleType oldTrayClip;

    XP_S16 trayOwner;
    XP_U16 fntHeight;

    GraphicsAbility able;

    UInt16 oldCoord;
    XP_Bool doHiRes;
    XP_Bool oneDotFiveAvail;
    XP_Bool topFocus;

    XP_LangCode fontLangCode;
    PalmFontHtInfo* fontHtInfo;

    union {
        struct {
            XP_U8 reserved;     /* make CW compiler happy */
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
    Boolean reserved; /* was oneTimeShown */
    Boolean reserved1[4];       /* pad out to 12 for ARM */

    /* New for 0x0405 */
    CommonPrefs cp;

    Int16 focusItem;
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
    XP_S16 curLine;
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
    XP_Bool allowPickTiles;
    XP_Bool allowHintRect;
#ifdef XWFEATURE_BLUETOOTH
    XP_Bool confirmBTConnect;
#endif
} PrefsDlgState;

typedef struct DictState {
    ListPtr dictList;
    ListData sLd;
    XP_U16 nDicts;
} DictState;

typedef struct PalmNewGameState {
    FormPtr form;
    ListPtr playerNumList;
    NewGameCtx* ngc;
    XP_U16 nXPorts;
    XP_UCHAR passwds[MAX_PASSWORD_LENGTH+1][MAX_NUM_PLAYERS];
    XP_UCHAR* dictName;
    XP_UCHAR shortDictName[32]; /* as long as a dict name can be */

    XP_Bool forwardChange;
    DeviceRole curServerHilite;
#ifndef XWFEATURE_STANDALONE_ONLY
    CommsAddrRec addr;
#endif
} PalmNewGameState;

typedef struct PalmDictList PalmDictList;

#ifdef XWFEATURE_RELAY
typedef struct NetLibStuff {
    UInt16 netLibRef;
    NetSocketRef socket;
    XP_Bool ipAddrInval;
} NetLibStuff;
#define ipSocketIsOpen(g) ((g)->nlStuff.socket != -1)
#endif

#define MAX_DLG_PARAMS 2

#ifdef XWFEATURE_BLUETOOTH
typedef enum {
    BTUI_NOBT
    , BTUI_NONE
    , BTUI_LISTENING
    , BTUI_CONNECTING
    , BTUI_CONNECTED            /* slave */
    , BTUI_SERVING              /* master */
} BtUIState;
#endif

#ifdef XWFEATURE_BLUETOOTH
# define TIMER_ACL_BACKOFF NUM_TIMERS_PLUS_ONE
# define NUM_PALM_TIMERS (NUM_TIMERS_PLUS_ONE + 1)
#else
# define NUM_PALM_TIMERS NUM_TIMERS_PLUS_ONE
#endif

struct PalmAppGlobals {
    FormPtr mainForm;
    PrefsDlgState* prefsDlgState;
    SavedGamesState* savedGamesState;
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

#ifdef XWFEATURE_RELAY
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
    XP_Bool runningOnPOSE;    /* Needed for NetLibSelect */
#ifdef XWFEATURE_FIVEWAY
    XP_Bool isTreo600;
#endif
#ifdef XWFEATURE_BLUETOOTH
    XP_Bool userCancelledBT;
    XP_Bool hasBTLib;
#endif

    GraphicsAbility able;
    XP_U16 prevScroll;		/* for scrolling in 'ask' dialog */
    UInt16 romVersion;

    XP_U8 scrollValue;		/* 0..2: scrolled position of board */

#ifdef SHOW_PROGRESS
    ProgressCtxt progress;
#endif

    XP_U16 width, height;
    XP_U16 sonyLibRef;
    XP_Bool doVSK;
    XP_Bool hasHiRes;
    XP_Bool oneDotFiveAvail;
    XP_Bool useHiRes;
    XP_Bool hasTreoFiveWay;
    XP_Bool generatesKeyUp;
    XP_Bool isZodiac;
    XP_Bool keyDownReceived;
    XP_Bool initialTakeDropped; /* work around apparent OS bug */
    /* PalmOS seems pretty broken w.r.t. key events.  If I put up a modal
       dialog while in the process of handling a keyUp, that form gets a
       keyDown (and not with the repeat bit set either.)  Hack around it. */
    XP_Bool handlingKeyEvent;
    XP_Bool ignoreFirstKeyDown;

    XP_U16 lastKeyDown;

#ifdef XWFEATURE_SEARCHLIMIT
    XP_Bool askTrayLimits;
#endif

    CurGameInfo gameInfo;	/* for the currently open, or new, game */

    /* dialog/forms state */
    PalmNewGameState newGameState;

    DictState dictState;

    struct ConnsDlgState* connState;

    XWTimerProc timerProcs[NUM_PALM_TIMERS];
    void* timerClosures[NUM_PALM_TIMERS];
    XP_U32 timerFireAt[NUM_PALM_TIMERS];

#ifdef XWFEATURE_RELAY
    NetLibStuff nlStuff;
    XP_U32 heartTimerFireAt;
    XP_Bool lastSendGood;
#endif

#if defined XWFEATURE_BLUETOOTH || defined XWFEATURE_RELAY
    XP_U16 lastNetStatusRes;

# ifdef XWFEATURE_BLUETOOTH
    struct PalmBTStuff* btStuff;
    XP_Bool suspendBT;
# endif

    union {
# ifdef XWFEATURE_BLUETOOTH
        BtUIState btUIState;          /* For showing user what's up */
# endif
# ifdef XWFEATURE_RELAY
        CommsRelayState relayState;
# endif
    } netState;
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
enum { noopEvent = firstUserEvent /* 0x6000 */
       ,dictSelectedEvent
       ,newGameOkEvent
       ,newGameCancelEvent
       ,loadGameEvent
       ,prefsChangedEvent
       ,openSavedGameEvent
#ifdef XWFEATURE_FIVEWAY
       ,updateAfterFocusEvent
#endif
#if defined XWFEATURE_BLUETOOTH
       ,closeBtLibEvent
#endif
#ifdef FEATURE_SILK
       ,doResizeWinEvent
#endif
};

enum {
    PNOLET_STORE_FEATURE = 1    /* where FtrPtr to pnolet code lives */
    , GLOBALS_FEATURE           /* for passing globals to form handlers */
    , LOG_FILE_FEATURE            /* these three for debugging */
    , LOG_MEMO_FEATURE
    , LOG_SCREEN_FEATURE
#ifdef FEATURE_DUALCHOOSE
    , FEATURE_WANTS_68K         /* support for (pre-ship) ability to choose
                                   armlet or 68K */
#endif
#ifdef XWFEATURE_COMBINEDAWG
    , DAWG_STORE_FEATURE
#endif
    , PACE_BT_CBK_FEATURE
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

XP_Bool askPassword( PalmAppGlobals* globals, const XP_UCHAR* name, 
                     XP_Bool isNew, XP_UCHAR* retbuf, XP_U16* len );
XP_Bool palmaskFromStrId( PalmAppGlobals* globals, XP_U16 strId, 
                          XP_S16 titleID );
void freeSavedGamesData( MPFORMAL SavedGamesState* state );

void writeNameToGameRecord( PalmAppGlobals* globals, XP_S16 index, 
                            char* newName, XP_U16 len );

const XP_UCHAR* getResString( PalmAppGlobals* globals, XP_U16 strID );
XP_Bool palmask( PalmAppGlobals* globals, const XP_UCHAR* str, 
                 const XP_UCHAR* altButton, XP_S16 titleID );
void checkAndDeliver( PalmAppGlobals* globals, const CommsAddrRec* addr, 
                      XWStreamCtxt* instream, CommsConnType conType );

#ifdef XW_TARGET_PNO
# define READ_UNALIGNED16(n) read_unaligned16((unsigned char*)(n))
#else
# define READ_UNALIGNED16(n) *(n)
#endif

#define IS_T600(g) (g)->isTreo600

#endif /* _PALMMAIN_H_ */
