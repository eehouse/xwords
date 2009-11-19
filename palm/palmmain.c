/* -*-mode: C; fill-column: 77; c-basic-offset: 4; compile-command: "make ARCH=ARM_ONLY MEMDEBUG=TRUE"; -*- */
/* 
 * Copyright 1999 - 2009 by Eric House (xwords@eehouse.org).  All rights
 * reserved.
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

#include <PalmTypes.h>
#include <Event.h>
#include <SysEvtMgr.h>
#include <SystemMgr.h>
#include <Preferences.h>
#include <Window.h>
#include <Form.h>
#include <Menu.h>
#include <IrLib.h>
#include <Chars.h>
#include <TimeMgr.h>
#include <FeatureMgr.h>
#include <NotifyMgr.h>
#include <unix_stdarg.h>
#include <FileStream.h>
#ifdef FEATURE_SILK
# include <SonyCLIE.h>
#endif
#ifdef XWFEATURE_BLUETOOTH
# include <BtLibTypes.h>
#endif
#include "comtypes.h"
#include "comms.h"

#include "xwords4defines.h"
#include "palmmain.h"
#include "newgame.h"
#include "palmdbg.h"
#include "dbgutil.h"
#include "dictui.h"
#include "dictlist.h"
#include "palmutil.h"
#include "palmdict.h"
#include "palmsavg.h"
#include "memstream.h"
#include "strutils.h"
#include "palmir.h"
#include "palmip.h"
#include "palmbt.h"
#include "xwcolors.h"
#include "prefsdlg.h"
#include "connsdlg.h"
#include "gameutil.h"
#include "dictui.h"
#include "palmblnk.h"
#include "LocalizedStrIncludes.h"

#ifdef XWFEATURE_FIVEWAY
# include <Hs.h>
/* # include <HsKeyCommon.h> */
#endif

#include "callback.h"
#include "pace_man.h"           /* for crash() macro */

#ifdef SUPPORT_SONY_JOGDIAL
#include "SonyChars.h"
#endif

#define PALM_TIMER_DELAY 25

/*-------------------------------- defines and consts-----------------------*/
/*  #define COLORCHANGE_THRESHOLD 300 */

/*-------------------------------- prototypes ------------------------------*/
static XP_Bool startApplication( PalmAppGlobals** globalsP );
static void eventLoop( PalmAppGlobals* globals );
static void stopApplication( PalmAppGlobals* globals );
static Boolean applicationHandleEvent( EventPtr event );
static Boolean mainViewHandleEvent( EventPtr event );

static UInt16 romVersion( void );
static Boolean handleHintRequest( PalmAppGlobals* globals );
static XP_Bool timeForTimer( PalmAppGlobals* globals, XWTimerReason* why, 
                             XP_U32* when );
static XP_S16 palm_send( const XP_U8* buf, XP_U16 len, 
                         const CommsAddrRec* addr, void* closure );
#ifdef COMMS_HEARTBEAT
static void palm_reset( void* closure );
#endif
static void palm_relayStatus( void* closure, CommsRelayState newState );
static void palm_send_on_close( XWStreamCtxt* stream, void* closure );

/* callbacks */
static VTableMgr* palm_util_getVTManager( XW_UtilCtxt* uc );
static void palm_util_userError( XW_UtilCtxt* uc, UtilErrID id );
static XP_Bool palm_util_userQuery( XW_UtilCtxt* uc, UtilQueryID id,
                                    XWStreamCtxt* stream );
static XWBonusType palm_util_getSquareBonus( XW_UtilCtxt* uc, 
                                             const ModelCtxt* model,
                                             XP_U16 col, XP_U16 row );
static XP_S16 palm_util_userPickTile( XW_UtilCtxt* uc, const PickInfo* pi,
                                      XP_U16 playerNum, const XP_UCHAR** texts, 
                                      XP_U16 nTiles );
static XP_Bool palm_util_askPassword( XW_UtilCtxt* uc, const XP_UCHAR* name, 
                                      XP_UCHAR* buf, XP_U16* len );
static void palm_util_trayHiddenChange( XW_UtilCtxt* uc, 
                                        XW_TrayVisState newState,
                                        XP_U16 nVisibleRows );
static void palm_util_yOffsetChange( XW_UtilCtxt* uc, XP_U16 oldOffset, 
                                     XP_U16 newOffset );
static void palm_util_notifyGameOver( XW_UtilCtxt* uc );
static XP_Bool palm_util_hiliteCell( XW_UtilCtxt* uc, XP_U16 col, 
                                     XP_U16 row );
static XP_Bool palm_util_engineProgressCallback( XW_UtilCtxt* uc );
static void palm_util_setTimer( XW_UtilCtxt* uc, XWTimerReason why, XP_U16 when,
                                XWTimerProc proc, void* closure );
static void palm_util_clearTimer( XW_UtilCtxt* uc, XWTimerReason why );
static XP_Bool palm_util_altKeyDown( XW_UtilCtxt* uc );
static XP_U32 palm_util_getCurSeconds( XW_UtilCtxt* uc );
static void palm_util_requestTime( XW_UtilCtxt* uc );
static DictionaryCtxt* palm_util_makeEmptyDict( XW_UtilCtxt* uc );

#ifndef XWFEATURE_STANDALONE_ONLY
static XWStreamCtxt* palm_util_makeStreamFromAddr( XW_UtilCtxt* uc, 
                                                   XP_PlayerAddr channelNo );
#endif
static const XP_UCHAR* palm_util_getUserString( XW_UtilCtxt* uc, 
                                                XP_U16 stringCode );
static XP_Bool palm_util_warnIllegalWord( XW_UtilCtxt* uc, BadWordInfo* bwi, 
                                          XP_U16 turn, XP_Bool turnLost );
static void palm_util_remSelected(XW_UtilCtxt* uc);

#if defined XWFEATURE_BLUETOOTH || defined XWFEATURE_RELAY
static void palm_util_addrChange( XW_UtilCtxt* uc, const CommsAddrRec* oldAddr,
                                  const CommsAddrRec* newAddr );
#endif
#ifdef XWFEATURE_BLUETOOTH
static void btEvtHandler( PalmAppGlobals* globals, BtCbEvtInfo* evt );
#endif

#ifdef XWFEATURE_SEARCHLIMIT
static XP_Bool palm_util_getTraySearchLimits( XW_UtilCtxt* uc, XP_U16* min, 
                                              XP_U16* max );
#endif
static void userErrorFromStrId( PalmAppGlobals* globals, XP_U16 strID );
static XP_Bool askFromStream( PalmAppGlobals* globals, XWStreamCtxt* stream, 
                              XP_S16 titleID, Boolean closeAndDestroy );
static void displayFinalScores( PalmAppGlobals* globals );
static void updateScrollbar( PalmAppGlobals* globals, Int16 newValue );
static void askStartNewGame( PalmAppGlobals* globals );
static void palmSetCtrlsForTray( PalmAppGlobals* globals );
static void drawFormButtons( PalmAppGlobals* globals );
static MemHandle findXWPrefsRsrc( PalmAppGlobals* globals, UInt32 resType, 
                                  UInt16 resID );
#ifdef SHOW_PROGRESS
static void palm_util_engineStarting( XW_UtilCtxt* uc, XP_U16 nBlanks );
static void palm_util_engineStopping( XW_UtilCtxt* uc );
#endif

static void initAndStartBoard( PalmAppGlobals* globals, XP_Bool newGame );
#ifdef XWFEATURE_FIVEWAY
static XP_Bool isBoardObject( XP_U16 id );
#endif

/*-------------------------------- Globals ---------------------------------*/
/* NONE!!! */

/*****************************************************************************
 *
 ****************************************************************************/
#define XW_MOVE_EXG_TYPE "XwMv"
UInt32
PM2(PilotMain)( UInt16 cmd, MemPtr cmdPBP, UInt16 launchFlags)
{
    PalmAppGlobals* globals;
    if ( cmd == sysAppLaunchCmdNormalLaunch ) {
        if ( (launchFlags & sysAppLaunchFlagNewGlobals) != 0) {
#ifdef XW_TARGET_PNO
            /* SVN_REV isn't a string in ARM.  Fix that */
            XP_LOGF( "%s: arch=ARM", __func__ );
#else
            XP_LOGF( "%s: arch=68K, rev=%s", __func__, SVN_REV );
#endif
#ifdef MEM_DEBUG
            {
                char date[MAX_GAMENAME_LENGTH];
                makeDefaultGameName( date );
                XP_LOGF( "date: %s", date );
            }
#endif
            if ( startApplication( &globals ) ) {
                XP_ASSERT( (launchFlags & sysAppLaunchFlagNewGlobals) != 0 );
                // Initialize the application's global variables and database.
                eventLoop( globals );
            }
        }
        stopApplication( globals );

#ifdef XWFEATURE_IR
    } else if ( cmd == sysAppLaunchCmdExgAskUser ) {
        if ( (launchFlags & sysAppLaunchFlagSubCall) != 0 ) {
            ((ExgAskParamPtr)cmdPBP)->result = exgAskOk;
        }
    } else if ( cmd == sysAppLaunchCmdSyncNotify ) {
        if ( romVersion() >= 30 ) {
            ExgRegisterData( APPID, exgRegTypeID, XW_MOVE_EXG_TYPE );
        }
    } else if ( cmd == sysAppLaunchCmdExgReceiveData ) {
        if ( (launchFlags & sysAppLaunchFlagSubCall) != 0 ) {
            globals = getFormRefcon();
            palm_ir_receiveMove( globals, (ExgSocketPtr)cmdPBP );
        }
#endif
    }
    return 0;
} /* PilotMain */

/*****************************************************************************
 *
 ****************************************************************************/
static UInt16 
romVersion( void )
{
    UInt32 dwOSVer;
    UInt16 result;
    Err err;

    err = FtrGet(sysFtrCreator, sysFtrNumROMVersion, &dwOSVer );
    XP_ASSERT( errNone == err );
    /* should turn 3 and 5 into 35 */
    result = (sysGetROMVerMajor(dwOSVer)*10) + sysGetROMVerMinor(dwOSVer);

    return result;
} /* romVersion */

#ifdef COLOR_SUPPORT
/*****************************************************************************
 *
 ****************************************************************************/
static UInt32
cur_screen_depth( void )
{
    UInt32 curDepth;

    XP_ASSERT( romVersion() >= 30 ); /*  */

    WinScreenMode( winScreenModeGet, 0, 0, &curDepth, 0 );
    return curDepth;
} /* cur_screen_depth */
#endif

static void
getSizes( PalmAppGlobals* globals )
{
    XP_U16 width, height;
    width = 160;
    height = 160;

    if ( globals->hasHiRes ) {
        XP_U32 tmp;

        if ( WinScreenGetAttribute( winScreenWidth, &tmp ) == errNone ) {
            width = tmp;
        }
        if ( WinScreenGetAttribute( winScreenHeight, &tmp ) == errNone ) {
            height = tmp;
        }
    }

    if ( width == 320 ) {
        FormPtr form = FrmGetActiveForm();
        WinGetDisplayExtent( &width, &height );

        if ( !!form ) {
            RectangleType r;
            r.topLeft.x = 0;
            r.topLeft.y = 0;
            r.extent.x = width;
            r.extent.y = height;

            WinSetBounds( FrmGetWindowHandle(FrmGetActiveForm()), &r );
        }
        
        width *= 2;
        height *= 2;
        globals->useHiRes = width >= 320 && height >= 320;
    }

    globals->width = width;
    globals->height = height;
} /* getSizes */

/* The resources place the tray-related buttons for the high-res case.  If
 * the device is going to want them in the higher low-res position, move them
 * here.  And resize 'em too.
 */
static void
locateTrayButtons( PalmAppGlobals* globals )
{
    if ( !globals->useHiRes ) {
        /* we need to put the buttons into the old position and set their
           sizes for the larger tray. */
        XP_U16 buttonInfoTriplets[] = { XW_MAIN_HIDE_BUTTON_ID, 
                                        TRAY_BUTTONS_Y_LR, 

                                        XW_MAIN_JUGGLE_BUTTON_ID, 
                                        TRAY_BUTTONS_Y_LR, 

                                        XW_MAIN_TRADE_BUTTON_ID, 
                                        TRAY_BUTTONS_Y_LR
                                        + TRAY_BUTTON_HEIGHT_LR,

                                        XW_MAIN_DONE_BUTTON_ID,
                                        TRAY_BUTTONS_Y_LR
                                        + TRAY_BUTTON_HEIGHT_LR
        };
        XP_U16* ptr;
        XP_U16 i;

        for ( i = 0, ptr = buttonInfoTriplets; i < 4; ++i, ptr += 2 ) {
            RectangleType rect;
            getObjectBounds( ptr[0], &rect );
            rect.topLeft.y = ptr[1];
            rect.extent.y = TRAY_BUTTON_HEIGHT_LR;
            setObjectBounds( ptr[0], &rect );
        }
    }
} /* locateTrayButtons */

static XP_Bool
positionBoard( PalmAppGlobals* globals )
{
    XP_U16 bWidth = globals->width;
    XP_Bool erase = XP_FALSE;
    XP_Bool isLefty = globals->isLefty;
    XP_U16 nCols, leftEdge;
    XP_U16 scale = PALM_BOARD_SCALE;
    XP_U16 scaleH, scaleV;
    XP_U16 boardHeight, trayTop, trayScaleV;
    XP_U16 boardTop, scoreTop, scoreLeft, scoreWidth, scoreHeight;
    XP_U16 timerWidth, timerLeft;
    XP_U16 freeSpaceH;
    XP_Bool showGrid = globals->gState.showGrid;
    XP_U16 doubler = globals->useHiRes? 2 : 1;
#ifdef SHOW_PROGRESS
    RectangleType bounds;
#endif

    XP_ASSERT( !!globals->game.model );
    nCols = model_numCols( globals->game.model );
    XP_ASSERT( nCols <= PALM_MAX_ROWS );

    /* With the screen having variable width and height, we do away with
     * constants and calculate everything on the fly.  Horizontally, the
     * screen consists of the board and scrollbar/buttons.  Vertically, it's
     * the scoreboard, the board, and the tray.  If the board is square the
     * tray must overlap the board -- but not if the smallest font we can fit
     * in a cell allows the squares to squeeze down!
     *
     * Be careful with squeezing cells!  Custom chars won't allow it.  So
     * cell size needs to stay constant...  For now we only avoid overlap
     * cute when the screen is taller than it is wide.
     */

    /* since we only want the lines between cells one pixel wide, we can
       increase scale more than 2x when doubling. */
    if ( !showGrid ) {
        --scale;
    }
    scale = scale * doubler;
    scaleV = scaleH = scale;
    if ( globals->useHiRes ) {
        scaleV -= 2;
    }

    freeSpaceH = ((PALM_MAX_COLS-nCols)/2) * scaleH;
    if ( isLefty ) {
        leftEdge = bWidth - (nCols * scaleH) - freeSpaceH - 1;
    } else {
        leftEdge = PALM_BOARD_LEFT_RH + freeSpaceH;
    }

    /* position the timer.  There are really four cases: width depends on
       whether the grid's visible, and left edge depends on isLefty _and_
       width in the non-lefty case. */

    if ( showGrid ) {
        timerWidth = FntCharsWidth( "-00:00", 6 ); /* the ideal */
    } else {
        timerWidth = PALM_GRIDLESS_SCORE_WIDTH;
    }
    timerWidth *= doubler;

    if ( isLefty && !showGrid ) {
        timerLeft = 0;
    } else {
        timerLeft = bWidth - timerWidth;
    }
    board_setTimerLoc( globals->game.board, timerLeft, PALM_TIMER_TOP, 
                       timerWidth, PALM_TIMER_HEIGHT * doubler );

    if ( showGrid ) {
        boardTop = PALM_BOARD_TOP;
        scoreLeft = PALM_SCORE_LEFT;
        scoreTop = PALM_SCORE_TOP;
        scoreWidth = (bWidth/doubler) - PALM_SCORE_LEFT - (timerWidth/doubler);
        scoreHeight = PALM_SCORE_HEIGHT;
    } else {
        boardTop = PALM_GRIDLESS_BOARD_TOP;
        scoreLeft = isLefty? 0: PALM_GRIDLESS_SCORE_LEFT;
        scoreTop = PALM_GRIDLESS_SCORE_TOP;
        scoreWidth = PALM_GRIDLESS_SCORE_WIDTH;
        scoreHeight = PALM_TRAY_TOP - PALM_GRIDLESS_SCORE_TOP - 2;

        if ( !isLefty ) {
            leftEdge += doubler;		/* for the frame */
        }
    }

    boardTop *= doubler;
    scoreLeft *= doubler;
    scoreTop *= doubler;
    scoreWidth *= doubler;
    scoreHeight *= doubler;

    board_setPos( globals->game.board, leftEdge,
                  boardTop, isLefty );
    board_setScale( globals->game.board, scaleH, scaleV );

    board_setScoreboardLoc( globals->game.board, scoreLeft, scoreTop,
                            scoreWidth, scoreHeight, showGrid );

    board_setShowColors( globals->game.board, globals->gState.showColors );
    board_setYOffset( globals->game.board, 0 );

    /* figure location for the tray.  If possible, make it smaller than the
       ideal to avoid using a scrollbar.  Also, note at this point whether a
       scrollbar will be required. */
    globals->needsScrollbar = false; /* default */
    boardHeight = scaleV * nCols; 

    if ( globals->useHiRes ) {
        trayTop = ((160 - TRAY_HEIGHT_HR) * doubler) - 1;
        globals->needsScrollbar = false;
    } else {
        trayTop = 160 - TRAY_HEIGHT_LR;
        globals->needsScrollbar = showGrid && (nCols == PALM_MAX_COLS);
    }

    trayScaleV = 
        globals->useHiRes? (TRAY_HEIGHT_HR*doubler) + 1:
        TRAY_HEIGHT_LR;
    board_setTrayLoc( globals->game.board, 
                      (isLefty? PALM_TRAY_LEFT_LH:PALM_TRAY_LEFT_RH) * doubler,
                      trayTop,
                      PALM_TRAY_WIDTH * doubler, trayScaleV,
                      PALM_DIVIDER_WIDTH );

    board_prefsChanged( globals->game.board, &globals->gState.cp );

#ifdef SHOW_PROGRESS
    if ( showGrid ) {
        getObjectBounds( XW_MAIN_SCROLLBAR_ID, &bounds );

        bounds.topLeft.x += doubler;
        bounds.extent.x -= (doubler << 1);
    } else {
        bounds.topLeft.y = (PALM_TIMER_HEIGHT + 2) * doubler;
        bounds.topLeft.x = (globals->isLefty? FLIP_BUTTON_WIDTH+3:
            PALM_GRIDLESS_SCORE_LEFT+2) * doubler;

        bounds.extent.x = (RECOMMENDED_SBAR_WIDTH + 2) * doubler;
        bounds.extent.y = (PALM_GRIDLESS_SCORE_TOP - bounds.topLeft.y - 2) 
            * doubler;
    }
    globals->progress.boundsRect = bounds;
#endif

    updateScrollbar( globals, globals->scrollValue ); /* changing visibility? */
    palmSetCtrlsForTray( globals );
    drawFormButtons( globals );

    return erase;
} /* positionBoard */

static XWStreamCtxt* 
makeSimpleStream( PalmAppGlobals* globals, MemStreamCloseCallback cb )
{
    return mem_stream_make( MPPARM(globals->mpool)
                            globals->vtMgr,
                            globals, 
                            CHANNEL_NONE, cb );
} /* makeSimpleStream */

static XWStreamCtxt*
gameRecordToStream( PalmAppGlobals* globals, XP_U16 index )
{
    XWStreamCtxt* recStream = NULL;
    LocalID id;
    MemHandle handle;
    Err err;

    id = DMFINDDATABASE( globals, CARD_0, XW_GAMES_DBNAME );
    if ( id != 0 ) {
        UInt16 numRecs;
        DmOpenRef dbP;

        dbP = DMOPENDATABASE( globals, CARD_0, id, dmModeReadOnly );
        numRecs = DmNumRecords( dbP );

        if ( index < numRecs ) {
            handle = DmGetRecord( dbP, index );

            recStream = makeSimpleStream( globals, NULL );
            stream_open( recStream );
            stream_putBytes( recStream, MemHandleLock(handle), 
                             MemHandleSize(handle) );
            MemHandleUnlock(handle);
            err = DmReleaseRecord( dbP, index, false );
            XP_ASSERT( err == 0 );
        }
        DMCLOSEDATABASE( dbP );
    }
    return recStream;
} /* gameRecordToStream */

static void
loadGamePrefs( /*PalmAppGlobals* globals, */XWStreamCtxt* stream )
{
    /* Keep in sync with games saved in prev version, which foolishly saved
       hintsNotAllowed separate from the current game's value for the same
       thing.  When the version changes get rid of this bit. PENDING */
    (void)stream_getBits( stream, 1 ); 
} /* loadGamePrefs */

static void
saveGamePrefs( /*PalmAppGlobals* globals, */XWStreamCtxt* stream )
{
    stream_putBits( stream, 1, 0 );
} /* saveGamePrefs */

static void
keySafeCustomAlert( PalmAppGlobals* globals, const XP_UCHAR* buf )
{
    /* Another gross hack to get around the OS sending a spurious keyDown
       event when a dialog is invoked while still processing a keyUp event.
       We just pull all events off the queue until the keyDown is found.  In
       practice that's always the first event, but let's leave logging on for
       a while in case this causes problems. */
    while ( globals->handlingKeyEvent ) {
        EventType event;
        EvtGetEvent( &event, 0 );
        XP_LOGF( "%s: consumed %s", __func__, eType_2str(event.eType) );
        if ( event.eType == keyDownEvent || event.eType == nilEvent ) {
            break;
        }
    }
    (void)FrmCustomAlert( XW_ERROR_ALERT_ID, (const char*)buf, " ", " " );
}

static void
reportMissingDict( PalmAppGlobals* globals, XP_UCHAR* name )
{
    /* FrmCustomAlert crashes on some OS versions when there's no form under
       it to "return" to. */
    if ( FrmGetActiveForm() != NULL ) {
        XP_UCHAR buf[48];
        const XP_UCHAR* str = getResString( globals, STRS_CANNOT_FIND_DICT );
        StrPrintF( buf, str, name );
        keySafeCustomAlert( globals, buf );
    }
} /* reportMissingDict */

static void
palmInitTProcs( PalmAppGlobals* globals, TransportProcs* procs )
{
    XP_MEMSET( procs, 0, sizeof(*procs) );
    procs->send = palm_send;
#ifdef COMMS_HEARTBEAT
    procs->reset = palm_reset;
#endif
#ifdef XWFEATURE_RELAY
    procs->rstatus = palm_relayStatus;
#endif

    procs->closure = globals;
}

static XP_Bool
loadCurrentGame( PalmAppGlobals* globals, XP_U16 gIndex,
                 XWGame* game, CurGameInfo* ginfo )
{
    XP_Bool hasDict;
    XWStreamCtxt* recStream;
    XP_Bool success = XP_FALSE;
    DictionaryCtxt* dict;

    recStream = gameRecordToStream( globals, gIndex );

    /* now read everything out of the stream */
    if ( !!recStream ) {
        char ignore[MAX_GAMENAME_LENGTH];

        /* skip the name */
        stream_getBytes( recStream, ignore, MAX_GAMENAME_LENGTH );

        loadGamePrefs( /*globals, */recStream );

        hasDict = stream_getU8( recStream );
        if ( hasDict ) {
            XP_UCHAR name[33];
            stringFromStreamHere( recStream, name, sizeof(name) );
            dict = palm_dictionary_make( MPPARM(globals->mpool) globals,
                                         name, globals->dictList );
            success = dict != NULL;

            if ( !success ) {
                reportMissingDict( globals, name );
                beep();
            }
        } else {
            dict = NULL;
            success = XP_TRUE;
        }

        if ( success ) {
            TransportProcs procs;
            palmInitTProcs( globals, &procs );
            success = game_makeFromStream( MEMPOOL recStream, game, ginfo, 
                                           dict, &globals->util, 
                                           globals->draw, &globals->gState.cp, 
                                           &procs );
        }

        stream_destroy( recStream );
    }

    return success;
} /* loadCurrentGame */

static void
initUtilFuncs( PalmAppGlobals* globals )
{
    UtilVtable* vtable = globals->util.vtable = 
        XP_MALLOC( globals->mpool, sizeof( UtilVtable ) );
    globals->util.closure = (void*)globals;
    globals->util.gameInfo = &globals->gameInfo;

    MPASSIGN( globals->util.mpool, globals->mpool );

    vtable->m_util_getVTManager = palm_util_getVTManager;
    vtable->m_util_userError = palm_util_userError;
    vtable->m_util_getSquareBonus = palm_util_getSquareBonus;
    vtable->m_util_userQuery = palm_util_userQuery;
    vtable->m_util_userPickTile = palm_util_userPickTile;
    vtable->m_util_askPassword = palm_util_askPassword;
    vtable->m_util_trayHiddenChange = palm_util_trayHiddenChange;
    vtable->m_util_yOffsetChange = palm_util_yOffsetChange;
    vtable->m_util_notifyGameOver = palm_util_notifyGameOver;
    vtable->m_util_hiliteCell = palm_util_hiliteCell;
    vtable->m_util_engineProgressCallback = palm_util_engineProgressCallback;
    vtable->m_util_setTimer = palm_util_setTimer;
    vtable->m_util_clearTimer = palm_util_clearTimer;
    vtable->m_util_requestTime = palm_util_requestTime;
    vtable->m_util_altKeyDown = palm_util_altKeyDown;
    vtable->m_util_getCurSeconds = palm_util_getCurSeconds;
    vtable->m_util_makeEmptyDict = palm_util_makeEmptyDict;
#ifndef XWFEATURE_STANDALONE_ONLY
    vtable->m_util_makeStreamFromAddr = palm_util_makeStreamFromAddr;
#endif
    vtable->m_util_getUserString = palm_util_getUserString;
    vtable->m_util_warnIllegalWord = palm_util_warnIllegalWord;
    vtable->m_util_remSelected = palm_util_remSelected;
#if defined XWFEATURE_BLUETOOTH || defined XWFEATURE_RELAY
    vtable->m_util_addrChange = palm_util_addrChange;
#endif
#ifdef XWFEATURE_SEARCHLIMIT
    vtable->m_util_getTraySearchLimits = palm_util_getTraySearchLimits;
#endif
#ifdef SHOW_PROGRESS
    vtable->m_util_engineStarting = palm_util_engineStarting;
    vtable->m_util_engineStopping = palm_util_engineStopping;
#endif
} /* initUtilFuncs */

#ifdef COLOR_SUPPORT
static void
loadColorsFromRsrc( DrawingPrefs* prefs, MemHandle colorH )
{
    RGBColorType color;
    UInt8* colorP;
    short index = 0;
    UInt32 count;

    count = MemHandleSize( colorH );
    XP_ASSERT( count < 0xFFFF );
    XP_ASSERT( (((XP_U16)count) % 3) == 0 );
    colorP = MemHandleLock( colorH );
        
    do {
        color.r = *colorP++;
        color.g = *colorP++;
        color.b = *colorP++;
        prefs->drawColors[index++] = WinRGBToIndex( &color );
    } while ( (count -= 3) != 0 );

#ifdef XWFEATURE_FIVEWAY
    prefs->drawColors[COLOR_CURSOR] 
        = UIColorGetTableEntryIndex( UIObjectSelectedFill );
#endif
    MemHandleUnlock( colorH );
} /* loadColorsFromRsrc */
#endif

static void
palmInitPrefs( PalmAppGlobals* globals )
{
    globals->gState.showGrid = true;
    globals->gState.versionNum = CUR_PREFS_VERS;
    globals->gState.cp.showBoardArrow = XP_TRUE;
    globals->gState.cp.showRobotScores = XP_TRUE;

#ifdef SHOW_PROGRESS
    globals->gState.showProgress = true;
#endif

} /* palmInitPrefs */

static void
openXWPrefsDB( PalmAppGlobals* globals )
{
    Err err;

    err = DmCreateDatabase( CARD_0, XW_PREFS_DBNAME,
                            APPID, XWORDS_PREFS_TYPE, true );
    XP_ASSERT( err == errNone || err == dmErrAlreadyExists );
    globals->boardDBID = DmFindDatabase( CARD_0, XW_PREFS_DBNAME );
    globals->boardDBP = DmOpenDatabase( CARD_0, globals->boardDBID,
                                        dmModeWrite );
} /* openXWPrefsDB */

static XP_Bool
setupBonusPtrs( PalmAppGlobals* globals )
{
    XP_U16 i;
    for ( i = 0; i < NUM_BOARD_SIZES; ++i ) {
        MemHandle hand = findXWPrefsRsrc( globals, BOARD_RES_TYPE, 
                                          BOARD_RES_ID + i );
        if ( !hand ) {
            return XP_FALSE;
        }

        XP_ASSERT( MemHandleLockCount(hand) == 0 );
        globals->bonusResPtr[i] = MemHandleLock( hand );
    }
    return XP_TRUE;
} /* setupBonusPtrs */

static void
unlockBonusPtrs( PalmAppGlobals* globals )
{
    XP_U16 i;
    for ( i = 0; i < NUM_BOARD_SIZES; ++i ) {
        MemPtrUnlock( (MemPtr)globals->bonusResPtr[i] );
    }
} /* unlockBonusPtrs */

static void
openGamesDB( PalmAppGlobals* globals )
{
    Err err;

    err = DmCreateDatabase( CARD_0, XW_GAMES_DBNAME,
                            APPID, XWORDS_GAMES_TYPE, false );
    globals->gamesDBID = DmFindDatabase( CARD_0, XW_GAMES_DBNAME );
    globals->gamesDBP = DmOpenDatabase( CARD_0, globals->gamesDBID,
                                        dmModeReadWrite );
    XP_ASSERT( !!globals->gamesDBP );
} /* openGamesDB */

static MemHandle
findXWPrefsRsrc( PalmAppGlobals* globals, UInt32 resType, UInt16 resID )
{
    Int16 index;
    MemHandle handle = NULL;
    Boolean beenThere = XP_FALSE;

    for ( ; ; ) {
        XP_ASSERT( !!globals->boardDBP );
        index = DmFindResource( globals->boardDBP, resType, resID, NULL );

        if ( index == -1 ) {	/* not found */
            MemHandle builtinH; 
            MemHandle newH;
            UInt32 size;

            if ( !beenThere ) {
                builtinH = DmGetResource( resType, resID );
                XP_ASSERT( !!builtinH );
                size = MemHandleSize( builtinH );
                newH = DmNewResource( globals->boardDBP, resType, 
                                      resID, size );
                XP_ASSERT( !!newH );
                DmWrite( MemHandleLock( newH ), 0, MemHandleLock(builtinH), 
                         size );
                MemHandleUnlock( newH );
                MemHandleUnlock( builtinH );
                DmReleaseResource( newH );
                DmReleaseResource( builtinH );

                beenThere = XP_TRUE;
                continue;
            }
            break;
        }

        handle = DmGetResourceIndex( globals->boardDBP, index );
        break;
    }

    return handle;
} /* findXWPrefsRsrc */

static XP_Bool
initResources( PalmAppGlobals* globals )
{
    /* strings */
    MemHandle hand;

    XP_ASSERT( !globals->stringsResPtr );

    hand = DmGetResource( STRL_RES_TYPE, STRL_RES_ID );
    XP_ASSERT( !!hand );
    XP_ASSERT( MemHandleLockCount(hand) == 0 );
    globals->stringsResPtr = (XP_UCHAR*)MemHandleLock( hand );

    /* bonus square and color values.  These live in a separate database,
       which we create if it doesn't already exist. */
    openXWPrefsDB( globals );
    if ( !setupBonusPtrs( globals ) ) {
        return XP_FALSE;
    }

    openGamesDB( globals );

    if ( globals->able == COLOR ) {
        hand = findXWPrefsRsrc( globals, COLORS_RES_TYPE, COLORS_RES_ID );
        if ( !hand ) {
            return XP_FALSE;
        }
        loadColorsFromRsrc( &globals->drawingPrefs, hand );
        DmReleaseResource( hand );
    }

    return XP_TRUE;
} /* initResources */

static void
freeAndUnlockPtr( MemPtr ptr )
{
    MemHandle hand;
    XP_ASSERT( !!ptr );
    hand = MemPtrRecoverHandle(ptr );
    XP_ASSERT( !!hand );
    MemHandleUnlock( hand );
    DmReleaseResource( hand );
} /* freeAndUnlockPtr */

static void
uninitResources( PalmAppGlobals* globals )
{
    XP_U16 i;

    /* strings */
    freeAndUnlockPtr( globals->stringsResPtr );
    globals->stringsResPtr = NULL;

    /* bonus square values */
    for ( i = 0; i < NUM_BOARD_SIZES; ++i ) {
        freeAndUnlockPtr( globals->bonusResPtr[i] );
    }

    XP_ASSERT( !!globals->boardDBP );
    DmCloseDatabase( globals->boardDBP );

} /* uninitResources */

const XP_UCHAR*
getResString( PalmAppGlobals* globals, XP_U16 strID )
{
    XP_ASSERT( !!globals->stringsResPtr );
    XP_ASSERT( strID < MemPtrSize( globals->stringsResPtr ) );
    XP_ASSERT( (strID == 0) || (globals->stringsResPtr[strID-1] == '\0') );
    XP_ASSERT( strID < STR_LAST_STRING );
    return &globals->stringsResPtr[strID];
} /* getResString */

static Err
volChangeEventProc( SysNotifyParamType* XP_UNUSED_SILK(notifyParamsP) )
{
#if 0    
    if ( notifyParamsP->notifyType == sysNotifyVolumeUnmountedEvent ) {

        DictListHandleUnmount( globals->dictList );

    } else if ( notifyParamsP->notifyType == sysNotifyVolumeMountedEvent ) {

        DictListHandleMount( &globals->dictList );

    } else {
        XP_ASSERT(0);
        return errNone;
    }
#endif

#ifdef FEATURE_SILK
    if ( notifyParamsP->notifyType == sysNotifyDisplayChangeEvent ) {
        postEmptyEvent( doResizeWinEvent );
        return errNone;
    }
#endif
    /* for now, just blow outta here!  Force the app to rebuild
       datastructures when it's relaunched.  This is a hack but I like
       it. :-) */
#ifndef REALLY_HANDLE_MEDIA
    postEmptyEvent( appStopEvent );
#endif

    return errNone;
} /* volChangeEventProc */

static void
doCallbackReg( PalmAppGlobals* globals, XP_Bool reg )
{
    /* The mounted/unmounted events aren't there unless we're PalmOS version
       4.0 or greater.  No need to use FtrGet to check for Notification Mgr
       here, as it's useless without these. */
    if ( globals->romVersion >= 40 ) {
        XP_U16 i;
        UInt32 notifyTypes[] = { sysNotifyVolumeUnmountedEvent
                                 , sysNotifyVolumeMountedEvent
#ifdef FEATURE_SILK
                                 , sysNotifyDisplayChangeEvent
#endif
        };


        for ( i = 0; i < VSIZE(notifyTypes); ++i ) {
            UInt32 notifyType = notifyTypes[i];        

            if ( reg ) {
                SysNotifyRegister( 0, 0, notifyType, volChangeEventProc,
                                   sysNotifyNormalPriority, globals );
            } else {
                SysNotifyUnregister( 0, 0, notifyType, 
                                     sysNotifyNormalPriority);
            }
        }
    }
} /* doCallbackReg */

/* temp workarounds for some sony include file trouble */
# ifdef FEATURE_SILK
extern Err SilkLibEnableResizeFoo(UInt16 refNum)
				SILK_LIB_TRAP(sysLibTrapCustom+1);
extern Err VskSetStateFoo(UInt16 refNum, UInt16 stateType, UInt16 state)
				SILK_LIB_TRAP(sysLibTrapCustom+3+3);
# endif

static XP_Bool
isOnZodiac( void )
{
    // from http://tamspalm.tamoggemon.com/2006/03/02/determining-if-your-app-runs-on-a-zodiac/
    const XP_U32 twCreatorID = 'Tpwv';
    Err err;
    UInt32 manufacturer;
    XP_Bool result;
    err = FtrGet( sysFileCSystem, sysFtrNumOEMCompanyID, &manufacturer );
    result = (err == errNone) && (manufacturer == twCreatorID);
    LOG_RETURNF( "%d", (int)result );
    return result;
}

static void
initHighResGlobals( PalmAppGlobals* globals )
{
    Err err;
    XP_U32 vers;

    err = FtrGet( sysFtrCreator, sysFtrNumWinVersion, &vers );
    XP_ASSERT( err == errNone );
    globals->hasHiRes = (err == errNone) && (vers >= 4) && !globals->isZodiac;
    XP_LOGF( "hasHiRes = %d", (XP_U16)globals->hasHiRes );
    globals->oneDotFiveAvail = globals->hasHiRes
        && (err == errNone) && (vers >= 5);

#ifdef XWFEATURE_FIVEWAY
# ifndef hsFtrIDNavigationSupported
# define hsFtrIDNavigationSupported 14
# endif
    /* sysFtrNumUIHardwareFlags unavailable on PalmOS 4 */
    err = FtrGet( sysFtrCreator, sysFtrNumUIHardwareFlags, &vers );
    globals->generatesKeyUp = ( (err == errNone) && 
                                ((vers & sysFtrNumUIHardwareHasKbd) != 0) )
        || globals->isZodiac;
    globals->hasTreoFiveWay = (err == errNone)
        && ((vers & sysFtrNumUIHardwareHas5Way) != 0) && !globals->isZodiac;

    err = FtrGet( hsFtrCreator, hsFtrIDNavigationSupported, &vers );
    if ( errNone == err ) {
        XP_ASSERT( vers == 1 || vers == 2 );
        globals->isTreo600 = (err == errNone) && (vers == 1);
    }
#endif

#ifdef FEATURE_SILK
    if ( globals->hasHiRes ) {
        XP_U16 ref;

        err = SysLibFind(sonySysLibNameSilk, &ref );
        if ( err == sysErrLibNotFound ) {
            err = SysLibLoad( 'libr', sonySysFileCSilkLib, &ref );
        }

        if ( err == errNone ) {
            XP_U32 tmp;
            globals->sonyLibRef = ref;

            err = FtrGet( sonySysFtrCreator, sonySysFtrNumVskVersion, &tmp );
            if ( err == errNone ) {
                globals->doVSK = XP_TRUE;
                if ( VskOpen( ref ) == errNone ) {
                    VskSetStateFoo( ref, vskStateEnable, 1 );
                }
            } else {
                if ( SilkLibOpen( ref ) == errNone ) {
                    SilkLibEnableResizeFoo( ref );
                }
            }
        }
    }
#endif
} /* initHighResGlobals */

static void
uninitHighResGlobals( PalmAppGlobals* XP_UNUSED_SILK(globals) )
{
#ifdef FEATURE_SILK
    if ( globals->hasHiRes && globals->sonyLibRef != 0 ) {
        if ( globals->doVSK ) {
            VskClose( globals->sonyLibRef );
        } else {
            SilkLibClose( globals->sonyLibRef );
        }
    }
#endif
} /* uninitHighResGlobals */

static XP_Bool
canConvertPrefs( XWords4PreferenceType* prefs, UInt16 prefSize, XP_S16 vers )
{
    XP_Bool success = XP_FALSE;

    if ( vers == VERSION_NUM_405 ) {
        if ( prefSize < sizeof(XWords4PreferenceType) ) {
            XP_U8* newRgn = ((XP_U8*)prefs) + prefSize;
            XP_MEMSET( newRgn, 0, sizeof(XWords4PreferenceType) - prefSize );
        }
        success = XP_TRUE;
    }

    return success;
} /* canConvertPrefs */

/*****************************************************************************
 *
 ****************************************************************************/
static XP_Bool
startApplication( PalmAppGlobals** globalsP )
{
    UInt16 prefSize;
    Boolean prefsFound;
    XWords4PreferenceType prefs;
    PalmAppGlobals* globals;
    Boolean leftyFlag;
    Int16 vers;
    UInt32 ignore;
#if defined XWFEATURE_BLUETOOTH
    Err err;
#endif
    MPSLOT;

#if defined FOR_GREMLINS
    SysRandom( 1 );
#else
    SysRandom( TimGetTicks() );		/* initialize */
#endif

#ifdef MEM_DEBUG
    mpool = mpool_make();
#endif

    globals = (PalmAppGlobals*)XP_MALLOC( mpool, sizeof( PalmAppGlobals ) );
    *globalsP = globals;
    setFormRefcon( globals );
    XP_MEMSET( globals, 0, sizeof(PalmAppGlobals) );
    MPASSIGN( globals->mpool, mpool );

    globals->isZodiac = isOnZodiac();

    initHighResGlobals( globals );
    getSizes( globals );

    globals->runningOnPOSE = FtrGet( 'pose', 0, &ignore) != ftrErrNoSuchFeature;

#if defined XWFEATURE_BLUETOOTH
    err = FtrGet( btLibFeatureCreator, btLibFeatureVersion, &ignore );
    /* could expand the test to skip version 1 and the Treo650 :-) */
    globals->hasBTLib = ftrErrNoSuchFeature != err;
# ifdef MEM_DEBUG 
    if ( errNone == err ) {
        /* Sprint Treo650 is returning 0036 */
        /* Treo 700 on VWZ: sysFileCBtLib version: 00000003 */
        /* Treo 650 on Sprint: sysFileCBtLib version: 00000001 */
        XP_LOGF( "sysFileCBtLib version: %lx", ignore );
    } else {
        XP_LOGF( "no sysFileCBtLib via FtrGet: OS too old?" );
    }
    /* Make the UI elements easier to test */
    if ( globals->runningOnPOSE ) {
        globals->hasBTLib = XP_TRUE;
    }
# endif
#endif

    globals->vtMgr = make_vtablemgr( MPPARM_NOCOMMA(globals->mpool) );

    globals->romVersion = romVersion();

    globals->isFirstLaunch = true;

    leftyFlag = 0;
    if ( !PrefGetAppPreferencesV10('Lfty', 1, &leftyFlag, 
                                   sizeof(leftyFlag) )) {
        leftyFlag = 0;
    }
    globals->isLefty = leftyFlag != 0;

#ifdef COLOR_SUPPORT
    if ( (globals->romVersion >= 35) && (cur_screen_depth() >= 8) ) {
        globals->able = COLOR;
    } else {
        globals->able = ONEBIT;
    }
#else
    globals->able = ONEBIT;
#endif

    if ( !initResources( globals ) ) {
        return XP_FALSE;
    }

#ifdef XWFEATURE_RELAY
    palm_ip_setup( globals );
#endif

    doCallbackReg( globals, XP_TRUE );

    initUtilFuncs( globals );

    offerConvertOldDicts( globals );

    globals->dictList = DictListMake( MPPARM_NOCOMMA(globals->mpool) );
    if ( DictListCount( globals->dictList ) == 0 ) {
        userErrorFromStrId( globals, STR_NO_DICT_INSTALLED );
        return XP_FALSE;
    }

    prefSize = sizeof( prefs );
    vers = PrefGetAppPreferences( AppType, PrefID, &prefs, &prefSize, true);
    if ( vers == VERSION_NUM ) {
        prefsFound = XP_TRUE;
    } else if ( vers != noPreferenceFound ) {
        prefsFound = canConvertPrefs( &prefs, prefSize, vers );
    } else {
        prefsFound = XP_FALSE;
    }

    if ( prefsFound ) {
        prefs.versionNum = XP_NTOHS( prefs.versionNum );
        prefs.curGameIndex = XP_NTOHS( prefs.curGameIndex );
        prefs.focusItem = XP_NTOHS( prefs.focusItem );

        MemMove( &globals->gState, &prefs, sizeof(prefs) );
    }

    globals->draw = palm_drawctxt_make( MPPARM(globals->mpool) 
                                        globals->able, 
                                        globals, 
                                        getResString,
                                        &globals->drawingPrefs );

    FrmGotoForm( XW_MAIN_FORM );

    /* do this first so players who don't exist have default names */
    gi_initPlayerInfo( MEMPOOL &globals->gameInfo,
                       getResString( globals, STR_DEFAULT_NAME ) );

    if ( prefsFound && loadCurrentGame( globals, globals->gState.curGameIndex,
                                        &globals->game, &globals->gameInfo) ) {
        postEmptyEvent( loadGameEvent );
        globals->isFirstLaunch = false;
    } else {
        TransportProcs procs;
        DictListEntry* dlep;

        /* if we're here because dict missing, don't re-init all prefs! */
        if ( !prefsFound ) {
            palmInitPrefs( globals );
        } else {
            /* increment count so we get a new game rather than replace
               existing one.  We want it still there if somebody puts the
               missing dict back. */
            globals->gState.curGameIndex = countGameRecords( globals );
        }
        globals->isNewGame = true;

        getNthDict( globals->dictList, 0, &dlep );
        globals->gameInfo.dictName = copyString( globals->mpool,
                                                 dlep->baseName );

        palmInitTProcs( globals, &procs );
        game_makeNewGame( MEMPOOL &globals->game, &globals->gameInfo,
                          &globals->util, globals->draw, 0, 
                          &globals->gState.cp, &procs );

        FrmPopupForm( XW_NEWGAMES_FORM );
    }

    return XP_TRUE;
} /* startApplication */

/* save the stream's contents to a database. */
static void
writeToDb( XWStreamCtxt* stream, void* closure )
{
    PalmAppGlobals* globals = (PalmAppGlobals*)closure;
    Err err;

    err = DmCreateDatabase( CARD_0, XW_GAMES_DBNAME,
                            APPID, XWORDS_GAMES_TYPE, false );

    streamToGameRecord( globals, stream, globals->gState.curGameIndex );
} /* writeToDb */

static void
saveOpenGame( PalmAppGlobals* globals ) 
{
    if ( !!globals->game.server ) {
        XWStreamCtxt* memStream;
        DictionaryCtxt* dict;
        const XP_UCHAR* dictName;
        char namebuf[MAX_GAMENAME_LENGTH];

        if ( gi_countLocalHumans( &globals->gameInfo ) > 1 ) {
            board_hideTray( globals->game.board ); /* so won't be visible when
                                                      next opened */
        }
        memStream = makeSimpleStream( globals, writeToDb );
        stream_open( memStream );

        /* write the things's name.  Name is first because we want to be able
           to manipulate it without knowing about the other stuff. */
        nameFromRecord( globals, globals->gState.curGameIndex, namebuf );
        stream_putBytes( memStream, namebuf, MAX_GAMENAME_LENGTH );

        saveGamePrefs( /*globals, */memStream );

        /* the dictionary */
        dict = model_getDictionary( globals->game.model );
        dictName = !!dict? dict_getName( dict ) : NULL;
        stream_putU8( memStream, !!dictName );
        if ( !!dictName ) {
            stringToStream( memStream, dictName );
        }

        game_saveToStream( &globals->game, &globals->gameInfo, memStream );

        stream_destroy( memStream );
    }
} /* saveOpenGame */

/*****************************************************************************
 *
 ****************************************************************************/
static void 
stopApplication( PalmAppGlobals* globals )
{
    if ( globals != NULL ) {
#ifdef XWFEATURE_FIVEWAY
        Int16 focusItem = getFocusOwner();
#endif
        MPSLOT;

        saveOpenGame( globals );

        FrmCloseAllForms();
	
        uninitResources( globals );

#ifdef XWFEATURE_BLUETOOTH
        palm_bt_close( globals );
#endif

        /* Write the state information -- once we're ready to read it in.
           But skip the save if user cancelled launching the first time. */
        if ( !globals->isFirstLaunch ) {
            XWords4PreferenceType prefs;
            /* temporarily don't save prefs since we crash on opening
               them. */
            XP_MEMCPY( &prefs, &globals->gState, sizeof(prefs) );
            prefs.versionNum = XP_HTONS( prefs.versionNum );
            prefs.curGameIndex = XP_HTONS( prefs.curGameIndex );

#ifdef XWFEATURE_FIVEWAY
            prefs.focusItem = XP_HTONS(focusItem);
#endif

            PrefSetAppPreferences( AppType, PrefID, VERSION_NUM, 
                                   &prefs, sizeof(prefs), true );
        }

        if ( !!globals->draw ) {
            draw_destroyCtxt( globals->draw );
        }

        game_dispose( &globals->game );
        gi_disposePlayerInfo( MEMPOOL &globals->gameInfo );

#ifdef XWFEATURE_RELAY
        palm_ip_close( globals );
#endif

        if ( !!globals->dictList ) {
            DictListFree( MPPARM(globals->mpool) globals->dictList );
        }

        if ( !!globals->util.vtable ) {
            XP_FREE( globals->mpool, globals->util.vtable );
        }

        if ( !!globals->prefsDlgState ) {
            XP_FREE( globals->mpool, globals->prefsDlgState );
        }

        if ( !!globals->savedGamesState && !globals->isFirstLaunch ) {
            freeSavedGamesData( MPPARM(globals->mpool)
                                globals->savedGamesState );
            XP_FREE( globals->mpool, globals->savedGamesState );
        }

        uninitHighResGlobals( globals );

        XP_ASSERT( !!globals->gamesDBP );
        DmCloseDatabase( globals->gamesDBP );

        if ( !!globals->vtMgr ) {
            vtmgr_destroy( MPPARM(globals->mpool) globals->vtMgr );
        }

        doCallbackReg( globals, XP_FALSE );

        MPASSIGN( mpool, globals->mpool );
        XP_FREE( globals->mpool, globals );
        mpool_destroy( mpool );
    }
} /* stopApplication */

static Int32
figureWaitTicks( PalmAppGlobals* globals )
{
    Int32 result = evtWaitForever;
    XP_U32 when;
    XWTimerReason why;

    if ( 0 ) {
#ifdef XWFEATURE_RELAY
    } else if ( ipSocketIsOpen(globals) ) {
/*         we'll do our sleeping in NetLibSelect */
        result = 0;
#endif
    } else if ( globals->timeRequested || globals->hintPending ) {
        result = 0;
    } else if ( timeForTimer( globals, &why, &when ) ) {
        result = when - TimGetTicks();
        if ( result < 0 ) {
            result = 0;
        }
    } else {
        /* leave it */
    }
    /*     XP_DEBUGF( "figureWaitTicks returning %d", result ); */

# ifdef XWFEATURE_BLUETOOTH
    if ( !!globals->mainForm ) {
        palm_bt_amendWaitTicks( globals, &result );
    }
# endif

    return result;
} /* figureWaitTicks */

static XP_Bool
closeNonMainForms( PalmAppGlobals* globals )
{
#if 1
    return FrmGetActiveForm() == globals->mainForm;
#else
    /* This doesn't work.  If there's a form in front of the main form
       sending it the close event closes it, but then FrmGetActiveForm()
       returns null the next time called.*/
    FormPtr prevActive;
    FormPtr curActive = NULL;

    for ( ; ; ) {
        EventType event;

        prevActive = curActive;
        curActive = FrmGetActiveForm();
        if ( prevActive == curActive ) {
            return XP_FALSE;
        }

        if ( curActive == globals->mainForm ) {
            return XP_TRUE;
        }
        event.eType = frmCloseEvent;
        event.data.frmClose.formID = FrmGetFormId(curActive);
        FrmDispatchEvent( &event );
    }
#endif
} /* closeNonMainForms */

/*****************************************************************************
 *
 ****************************************************************************/
static void
eventLoop( PalmAppGlobals* globals )
{
    EventType event;
	
    do {
#ifdef XWFEATURE_RELAY
        if ( !!globals->game.comms 
             && (comms_getConType(globals->game.comms) == COMMS_CONN_RELAY) ) {
            checkHandleNetEvents( globals );
        }
#endif

        /* 	EvtGetEvent( &event, evtWaitForever ); */
        EvtGetEvent( &event, figureWaitTicks(globals) );

        if ( event.eType == keyDownEvent ) {
            if ( 0 ) {
#ifdef FOR_GREMLINS
            } else if ( event.data.keyDown.chr == findChr ) {
                continue;
#endif
            } else if ( (event.data.keyDown.modifiers & commandKeyMask) != 0
                        && ( (event.data.keyDown.chr == autoOffChr)
                             || (event.data.keyDown.chr == hardPowerChr) )
                        && !!globals->game.board ) {
                if ( !globals->menuIsDown /* hi Marcus :-) */
                     && closeNonMainForms(globals)
                     && gi_countLocalHumans( &globals->gameInfo ) > 1
                     && board_hideTray( globals->game.board ) ) {
                    board_draw( globals->game.board );
                }
            }
        }

        /* Give the system a chance to handle the event. */
        if ( !SysHandleEvent(&event)) {
            UInt16 error;
            if ( !MenuHandleEvent( NULL, &event, &error)) {
                if ( !applicationHandleEvent( &event )) {
                    FrmDispatchEvent(&event);
                }
            }
        }
    } while (event.eType != appStopEvent);
} /* eventLoop */

/**********************************************************************
 * applicationHandleEvent
 **********************************************************************/
static Boolean
applicationHandleEvent( EventPtr event ) 
{
    FormPtr frm = NULL;
    Int16 formId;
    Boolean result = false;
    FormEventHandlerType* handler = NULL;

    if ( event->eType == frmLoadEvent ) {
        /*Load the form resource specified in the event then activate the
          form.*/
        formId = event->data.frmLoad.formID;
        frm = FrmInitForm(formId);
        FrmSetActiveForm(frm);

        switch (formId)	{
        case XW_MAIN_FORM:
            handler = mainViewHandleEvent;
            break;
        case XW_NEWGAMES_FORM:
            handler = newGameHandleEvent;
            break;
        case XW_DICTINFO_FORM:
            handler = dictFormHandleEvent;
            break;
        case XW_PREFS_FORM:
            handler = PrefsFormHandleEvent;
            break;
#if defined XWFEATURE_RELAY || defined XWFEATURE_BLUETOOTH
        case XW_CONNS_FORM:
            handler = ConnsFormHandleEvent;
            break;
#endif
        case XW_SAVEDGAMES_DIALOG_ID:
            handler = savedGamesHandleEvent;
            break;
        }
        if ( !!handler ) {
            XP_ASSERT( !!frm );
            result = true;
            FrmSetEventHandler( frm, handler );
        }
    }

    return result;
} // applicationHandleEvent

#if 0
static void
destroy_on_close( XWStreamCtxt* p_stream )
{
    MemStreamCtxt* stream = (MemStreamCtxt*)p_stream;
    /*     PalmAppGlobals* globals = stream->globals; */
    MemHandle handle;

    XP_WARNF( "destroy_on_close called" );
    handle = stream->bufHandle;
    MemHandleFree( handle );
    stream_destroy( p_stream );
} /* destroy_on_close */
#endif

static void
palmFireTimer( PalmAppGlobals* globals, XWTimerReason why )
{
    XWTimerProc proc = globals->timerProcs[why];
    void* closure = globals->timerClosures[why];
    XP_ASSERT( TimGetTicks() >= globals->timerFireAt[why] );
    globals->timerProcs[why] = NULL;
    (*proc)( closure, why );
} /* fireTimer */

static XP_Bool
timeForTimer( PalmAppGlobals* globals, XWTimerReason* why, XP_U32* when )
{
    XP_U16 ii;
    XWTimerReason nextWhy = 0;
    XP_U32 nextWhen = 0xFFFFFFFF;
    XP_Bool found;

    for ( ii = 1; ii < NUM_PALM_TIMERS; ++ii ) {
        if ( (globals->timerProcs[ii] != NULL) && 
             (globals->timerFireAt[ii] < nextWhen) ) {
            nextWhy = ii;
            nextWhen = globals->timerFireAt[ii];
        }
    }

    found = nextWhy != 0;
    if ( found ) {
        *why = nextWhy;
        *when = nextWhen;
    }
    return found;
} /* timeForTimer */

#if defined XWFEATURE_BLUETOOTH || defined XWFEATURE_RELAY
static void
showConnState( PalmAppGlobals* globals )
{
    CommsCtxt* comms = globals->game.comms;
    Int16 resID = 0;
    if ( !!comms ) {
        CommsConnType typ = comms_getConType( comms );
        if ( 0 ) {
#ifdef XWFEATURE_BLUETOOTH
        } else if ( COMMS_CONN_BT == typ ){
            switch( globals->netState.btUIState ) {
            case BTUI_NOBT:
                break;
            case BTUI_NONE:
                resID = BTSTATUS_NONE_RES_ID; break;
            case BTUI_LISTENING:
                resID = BTSTATUS_LISTENING_RES_ID; break;
            case BTUI_CONNECTING:
                resID = BTSTATUS_SEEKING_RES_ID; break;
            case BTUI_CONNECTED:
            case BTUI_SERVING:
                resID = BTSTATUS_CONNECTED_RES_ID; break;
            }
#endif
#ifdef XWFEATURE_RELAY
        } else if ( COMMS_CONN_RELAY == typ ) {
            /* resID = RELAYSTATUS_NONE_RESID; */
            if ( globals->lastSendGood ) {
                switch( globals->netState.relayState ) {
                case COMMS_RELAYSTATE_UNCONNECTED:
                case COMMS_RELAYSTATE_CONNECT_PENDING:
                    resID = RELAYSTATUS_PENDING_RESID; 
                    break;
                case COMMS_RELAYSTATE_CONNECTED:
                case COMMS_RELAYSTATE_RECONNECTED:
                    resID = RELAYSTATUS_CONN_RESID;
                    break;
                case COMMS_RELAYSTATE_ALLCONNECTED:
                    resID = RELAYSTATUS_ALLCONN_RESID; 
                    break;
                }
            }
#endif
        }
    }
    if ( globals->lastNetStatusRes != resID ) {
        RectangleType bounds;
        getObjectBounds( XW_NETSTATUS_GADGET_ID, &bounds );
        if ( resID != 0 ) {
            draw_drawBitmapAt( globals->draw, resID,
                               bounds.topLeft.x, bounds.topLeft.y );
        } else {
            if ( globals->useHiRes ) {
                bounds.extent.x = (1 + bounds.extent.x) >> 1;
                bounds.extent.y = (1 + bounds.extent.y) >> 1;
            }
            WinEraseRectangle( &bounds, 0 );
        }
        globals->lastNetStatusRes = resID;
    }
} /* showConnState */
#endif

#ifdef XWFEATURE_BLUETOOTH
static void
btEvtHandler( PalmAppGlobals* globals, BtCbEvtInfo* evt )
{
    switch ( evt->evt ) {
    case BTCBEVT_CONFIRM:
        if ( globals->suspendBT ) {
            evt->u.confirm.confirmed = XP_FALSE;
        } else if ( globals->gameInfo.confirmBTConnect ) {
            const XP_UCHAR* fmt;
            char buf[256];      /* fmt is 182+ bytes in English */
            XP_ASSERT( !!globals->game.comms &&
                       !comms_getIsServer(globals->game.comms) );
            fmt = getResString( globals, STRS_BT_CONFIRM );
            XP_SNPRINTF( buf, sizeof(buf), fmt, evt->u.confirm.hostName );
            evt->u.confirm.confirmed = palmask( globals, buf, NULL, -1 );
            globals->suspendBT = !evt->u.confirm.confirmed;
        } else {
            evt->u.confirm.confirmed = XP_TRUE;
        }
        break;
    case BTCBEVT_CONN:
        if ( !!globals->game.comms ) {
            comms_resendAll( globals->game.comms );
        }
        break;
    case BTCBEVT_DATA:
        if ( COMMS_CONN_BT == comms_getConType( globals->game.comms ) ) {
            XWStreamCtxt* instream;
            instream = makeSimpleStream( globals, NULL );
            stream_putBytes( instream, evt->u.data.data, evt->u.data.len );
            checkAndDeliver( globals, evt->u.data.fromAddr, 
                             instream, COMMS_CONN_BT );
        } else {
            /* If we're no longer using BT (meaning somebody loaded a new game
               that doesn't use it), close it down.  We don't want to do it as
               part of unloading the old game since it's expensive to stop/start
               BT and the new game will probably use the same connection.  But if
               we get here, a non-bt game's been loaded and we should shut
               down.*/
            postEmptyEvent( closeBtLibEvent  );
        }
        break;
    default:
        XP_ASSERT(0);
    }
} /* btEvtHandler */
#endif

static Boolean
handleNilEvent( PalmAppGlobals* globals )
{
    Boolean handled = true;
    XP_U32 when;
    XWTimerReason why;

    if ( 0 ) {
#ifdef XWFEATURE_BLUETOOTH
    } else if ( (handled = (!globals->suspendBT)
                 && palm_bt_doWork( globals, btEvtHandler,
                                    &globals->netState.btUIState ) )
                ,showConnState( globals )
                ,handled ) {
        /* nothing to do */
#endif
    } else if ( timeForTimer( globals, &why, &when ) 
                && (when <= TimGetTicks()) ) {
        palmFireTimer( globals, why );
    } else if ( globals->menuIsDown ) {
        /* do nothing */
    } else if ( globals->hintPending ) {
        handled = handleHintRequest( globals );
    } else if ( globals->timeRequested ) {
        globals->timeRequested = false;
        if ( globals->msgReceivedDraw ) {
            XP_ASSERT ( !!globals->game.board );
            board_draw( globals->game.board );
            globals->msgReceivedDraw = XP_FALSE;
        }
        handled = server_do( globals->game.server ); 
    } else {
        handled = false;
    }

    return handled;
} /* handleNilEvent */

static Boolean
handleFlip( PalmAppGlobals* globals )
{
    XP_ASSERT( !!globals->game.board );
    return board_flip( globals->game.board );
} /* handle_flip_button */

static Boolean
handleValueToggle( PalmAppGlobals* globals )
{
    return board_toggle_showValues( globals->game.board );
} /* handleValueToggle */

static Boolean
handleHideTray( PalmAppGlobals* globals )
{
    Boolean draw;
    if ( TRAY_REVEALED == board_getTrayVisState( globals->game.board ) ) {
        draw = board_hideTray( globals->game.board );
    } else {
        draw = board_showTray( globals->game.board );
    }

    return draw;
} /* handleHideTray */

#ifdef XWFEATURE_SEARCHLIMIT
static Boolean 
popupLists( EventPtr event )
{
    Boolean handled = false;
    XP_U16 ctlID;
    ListPtr list = NULL;
    XP_S16 chosen;

    if ( event->eType == ctlSelectEvent ) {
        ctlID = event->data.ctlSelect.controlID;
        if ( ctlID == XW_HINTCONFIG_MINSELECTOR_ID ) {
            list = getActiveObjectPtr( XW_HINTCONFIG_MINLIST_ID );
        } else if ( ctlID == XW_HINTCONFIG_MAXSELECTOR_ID ) {
            list = getActiveObjectPtr( XW_HINTCONFIG_MAXLIST_ID );
        }

        if ( !!list ) {
            chosen = LstPopupList( list );
            if ( chosen >= 0 ) {
                setSelectorFromList( ctlID, list, chosen );
            }
            handled = true;
        }
    }

    return handled;
} /* popupLists */

static XP_Bool
doHintConfig( XP_U16* minP, XP_U16* maxP )
{
    FormPtr form, prevForm;
    ListPtr listMin, listMax;
    XP_Bool confirmed;

    prevForm = FrmGetActiveForm();
    form = FrmInitForm( XW_HINTCONFIG_FORM_ID );
    FrmSetEventHandler( form, popupLists );
    FrmSetActiveForm( form );
    
    listMin = getActiveObjectPtr( XW_HINTCONFIG_MINLIST_ID );
    LstSetSelection( listMin, *minP - 1 );
    setSelectorFromList( XW_HINTCONFIG_MINSELECTOR_ID,
                         listMin, *minP - 1 );

    listMax = getActiveObjectPtr( XW_HINTCONFIG_MAXLIST_ID );
    LstSetSelection( listMax, *maxP - 1 );
    setSelectorFromList( XW_HINTCONFIG_MAXSELECTOR_ID,
                         listMax, *maxP - 1 );

    confirmed = FrmDoDialog( form ) == XW_HINTCONFIG_OK_ID;
    if ( confirmed ) {
        *minP = LstGetSelection( listMin ) + 1;
        *maxP = LstGetSelection( listMax ) + 1;
    }

    FrmDeleteForm( form );
    FrmSetActiveForm( prevForm );

    return confirmed;
} /* doHintConfig */
#endif

static Boolean
handleHintRequest( PalmAppGlobals* globals )
{
    Boolean notDone;
    Boolean draw;

    XP_ASSERT( !!globals->game.board );

    draw = board_requestHint( globals->game.board, 
#ifdef XWFEATURE_SEARCHLIMIT
                              globals->askTrayLimits,
#endif

                              &notDone );
    globals->hintPending = notDone;
    return draw;
} /* handleHintRequest */

static Boolean
handleDone( PalmAppGlobals* globals )
{
    return board_commitTurn( globals->game.board );
} /* handleDone */

static Boolean
handleJuggle( PalmAppGlobals* globals )
{
    return board_juggleTray( globals->game.board );
} /* handleJuggle */

static Boolean
handleTrade( PalmAppGlobals* globals )
{
    return board_beginTrade( globals->game.board );
} /* handleJuggle */

static Boolean
buttonIsUsable( ControlPtr button )
{
    return CtlEnabled( button );
} /* buttonIsUsable */

static void 
drawBitmapButton( PalmAppGlobals* globals, UInt16 ctrlID, UInt16 resID, 
                  XP_Bool eraseIfDisabled )
{
    FormPtr form;
    UInt16 index;
    RectangleType bounds;

    form = FrmGetActiveForm();
    index = FrmGetObjectIndex( form, ctrlID );
    FrmGetObjectBounds( form, index, &bounds );

    if ( buttonIsUsable( getActiveObjectPtr( ctrlID ) ) ) {
        draw_drawBitmapAt( globals->draw, resID, bounds.topLeft.x, 
                           bounds.topLeft.y );
    } else if ( eraseIfDisabled ) {
        /* gross hack; the button represents a larger bitmap; erase the
           whole thing.*/
#ifndef EIGHT_TILES
        if ( ctrlID == XW_MAIN_HIDE_BUTTON_ID ) {
            bounds.extent.x += TRAY_BUTTON_WIDTH + 1;
            bounds.extent.y += TRAY_BUTTON_WIDTH + 1;
        }
#endif
        WinEraseRectangle( &bounds, 0 );
    }
} /* drawBitmapButton */

static void
drawFormButtons( PalmAppGlobals* globals )
{
#ifdef XWFEATURE_FIVEWAY
    Int16 focusItem;
#endif
    XP_U16 pairs[] = {
        XW_MAIN_FLIP_BUTTON_ID, FLIP_BUTTON_BMP_RES_ID, XP_TRUE,
        XW_MAIN_VALUE_BUTTON_ID, VALUE_BUTTON_BMP_RES_ID, XP_TRUE,
        XW_MAIN_HINT_BUTTON_ID, HINT_BUTTON_BMP_RES_ID, XP_TRUE,
#ifndef EIGHT_TILES
        XW_MAIN_HIDE_BUTTON_ID, TRAY_BUTTONS_BMP_RES_ID, XP_TRUE,
#endif
        XW_MAIN_SHOWTRAY_BUTTON_ID, SHOWTRAY_BUTTON_BMP_RES_ID, XP_FALSE,
        0,
    };
    XP_U16* pair = (XP_U16*)pairs;

    if ( FrmGetActiveFormID() == XW_MAIN_FORM ) {
        while ( !!*pair ) {
            drawBitmapButton( globals, pair[0], pair[1], pair[2] );
            pair += 3;
        }
    }

#ifdef XWFEATURE_FIVEWAY
    if ( globals->hasTreoFiveWay ) {
        focusItem = globals->gState.focusItem;
        if ( focusItem > 0 ) {
            if ( isFormObject( globals->mainForm, focusItem ) ) {
/*                 XP_WARNF( "setting focus: %s", frmObjId_2str(focusItem) ); */
                setFormFocus( globals->mainForm, focusItem );
                if ( !isBoardObject( focusItem )
                     && buttonIsUsable( getActiveObjectPtr(focusItem) ) ) {
                    drawFocusRingOnGadget( globals, focusItem, focusItem );
                }
            }
            globals->gState.focusItem = -1;
        } else {
            drawFocusRingOnGadget( globals, XW_MAIN_DONE_BUTTON_ID,
                                   XW_MAIN_HIDE_BUTTON_ID );
        }
    }
#endif
} /* drawFormButtons */

static void
updateScrollbar( PalmAppGlobals* globals, Int16 newValue )
{
    if ( FrmGetActiveFormID() == XW_MAIN_FORM ) {
        ScrollBarPtr scroll = getActiveObjectPtr( XW_MAIN_SCROLLBAR_ID );
        XW_TrayVisState state = board_getTrayVisState( globals->game.board );
        XP_U16 max, min;

        max = model_numRows( globals->game.model );
        min = max;
        if ( globals->needsScrollbar && (max == SBAR_MAX)
             && state != TRAY_HIDDEN ) {
            min -= 2;		/* fragile!!! PENDING */
        }

        SclSetScrollBar( scroll, newValue + min, min, max, SBAR_PAGESIZE );
    }
} /* updateScrollbar */

static void
palmSetCtrlsForTray( PalmAppGlobals* globals )
{
    XW_TrayVisState state = board_getTrayVisState( globals->game.board );
    FormPtr form = globals->mainForm;

    /* In rare circumstances, e.g. when an appStopEvent comes in while the
       prefs dialog is up, this'll get called when the main form's not on
       top.  In that case it's probably ok to just do nothing.  But if not
       I'll need to queue an event of some sort so it gets done later. */
    if ( FrmGetActiveFormID() == XW_MAIN_FORM ) {

        disOrEnable( form, XW_MAIN_HINT_BUTTON_ID, 
                     (state==TRAY_REVEALED) && 
                     !globals->gameInfo.hintsNotAllowed );

#ifndef EIGHT_TILES
        disOrEnable( form, XW_MAIN_DONE_BUTTON_ID, state==TRAY_REVEALED );
        disOrEnable( form, XW_MAIN_JUGGLE_BUTTON_ID, state==TRAY_REVEALED );
        disOrEnable( form, XW_MAIN_TRADE_BUTTON_ID, state==TRAY_REVEALED );
        disOrEnable( form, XW_MAIN_HIDE_BUTTON_ID, state!=TRAY_HIDDEN );
#endif
        disOrEnable( form, XW_MAIN_SHOWTRAY_BUTTON_ID, state==TRAY_HIDDEN
                     && globals->gameInfo.nPlayers > 0 );

        globals->scrollValue = board_getYOffset( globals->game.board );
        updateScrollbar( globals, globals->scrollValue );

        /* PENDING(ehouse) Can't the board just do this itself? */
        if ( state==TRAY_HIDDEN ) {
            board_setYOffset( globals->game.board, 0 );
        }
    }
} /* palmSetCtrlsForTray */

static Boolean
scrollBoard( PalmAppGlobals* globals, Int16 newValue, Boolean fromBar )
{
    XP_Bool result = XP_FALSE;

    XP_ASSERT( !!globals->game.board );

    result = board_setYOffset( globals->game.board, newValue );

    if ( !fromBar ) {
        updateScrollbar( globals, newValue );
    }
    return result;
} /* scrollBoard */

/* We can't create the board back in newgame.c because the wrong form's
 * frontmost at that point.  So we do it here instead -- and must also call
 * server_do.
 */
static void
initAndStartBoard( PalmAppGlobals* globals, XP_Bool newGame )
{
    DictionaryCtxt* dict;
    XP_UCHAR* newDictName = globals->gameInfo.dictName;

    /* This needs to happen even when !newGame because it's how the dict
       slots in PlayerInfo get loaded.  That really ought to happen earlier,
       though. */
    XP_ASSERT( !!globals->game.model );
    dict = model_getDictionary( globals->game.model );

    if ( !!dict ) {
        const XP_UCHAR* dictName = dict_getName( dict );
        if ( !!newDictName && 0 != XP_STRCMP( (const char*)dictName, 
                                              (const char*)newDictName ) ) {
            dict_destroy( dict );
            dict = NULL;
        } else {
            replaceStringIfDifferent( globals->mpool, 
                                      &globals->gameInfo.dictName, dictName );
        }
    }

    if ( !dict ) {
        XP_ASSERT( !!newDictName );
        dict = palm_dictionary_make( MPPARM(globals->mpool) globals,
                                     newDictName, globals->dictList );
        XP_ASSERT( !!dict );	
        model_setDictionary( globals->game.model, dict );
    }

    if ( newGame ) {
        TransportProcs procs;
        palmInitTProcs( globals, &procs );
        game_reset( MEMPOOL &globals->game, &globals->gameInfo,
                    &globals->util, 0, &globals->gState.cp, &procs );

#ifndef XWFEATURE_STANDALONE_ONLY
        if ( !!globals->game.comms ) {
            comms_setAddr( globals->game.comms, 
                           &globals->newGameState.addr );
        } else if ( globals->gameInfo.serverRole != SERVER_STANDALONE ) {
            XP_ASSERT(0);
        }
#endif        
    }

    XP_ASSERT( !!globals->game.board );
    getSizes( globals );
    (void)positionBoard( globals );

#ifndef XWFEATURE_STANDALONE_ONLY
    if ( !!globals->game.comms ) {
        comms_start( globals->game.comms );
    }

    if ( newGame && globals->gameInfo.serverRole == SERVER_ISCLIENT ) {
        XWStreamCtxt* stream;
        XP_ASSERT( !!globals->game.comms );
        stream = makeSimpleStream( globals, palm_send_on_close );
        server_initClientConnection( globals->game.server, stream );
    }
#endif
    /* Used to call server_do here, but if it's a robot's turn it'll run
       without drawing the board first.  This allows work to get done almost
       as quickly.  If the board starts flashing on launch this is why;
       server_do might need to take a bool param skip-robot */
    palm_util_requestTime( &globals->util );

    board_invalAll( globals->game.board );
    board_draw( globals->game.board );

#if defined XWFEATURE_BLUETOOTH || defined XWFEATURE_RELAY
# ifdef XWFEATURE_BLUETOOTH
    globals->suspendBT = XP_FALSE;
# endif
    showConnState( globals );
#endif

    globals->isNewGame = false;
} /* initAndStartBoard */

#ifdef DEBUG
static void
toggleBoolFtr( XP_U16 ftr )
{
    UInt32 val;
    FtrGet( APPID, ftr, &val );
    val = !val;
    FtrSet( APPID, ftr, val );
    XP_WARNF( "Turned %s.", val==0? "OFF" : "ON" );
} /* toggleBoolFtr */

static void
askOnClose( XWStreamCtxt* stream, void* closure )
{
    PalmAppGlobals* globals = (PalmAppGlobals*)closure;

    (void)askFromStream( globals, stream, -1, false );
} /* askOnClose */
#endif

static void
updateForLefty( PalmAppGlobals* globals, FormPtr form )
{
    XP_S16 idsAndXs[] = {
        /* ButtonID,              x-coord-when-lefty, */
        XW_MAIN_FLIP_BUTTON_ID,   0,
        XW_MAIN_VALUE_BUTTON_ID,  0,
        XW_MAIN_HINT_BUTTON_ID,   0, 
        XW_MAIN_SCROLLBAR_ID,     0,
        XW_MAIN_SHOWTRAY_BUTTON_ID, 0,
	
#ifdef FOR_GREMLINS
        GREMLIN_BOARD_GADGET_IDAUTOID, 9,
        GREMLIN_TRAY_GADGET_IDAUTOID, 9,
#endif

#ifndef EIGHT_TILES
        XW_MAIN_HIDE_BUTTON_ID,   -1,
        XW_MAIN_JUGGLE_BUTTON_ID, TRAY_BUTTON_WIDTH-1,
        XW_MAIN_TRADE_BUTTON_ID,  -1,
        XW_MAIN_DONE_BUTTON_ID,   TRAY_BUTTON_WIDTH-1,
#endif
#if defined XWFEATURE_BLUETOOTH || defined XWFEATURE_RELAY
        XW_NETSTATUS_GADGET_ID,    0,
#endif
        0,
    };
    if ( globals->isLefty ) {
        UInt16 id;
        UInt16* idp = (UInt16*)idsAndXs;
        for ( id = *idp; !!id; id = *(idp+=2) ) {
            XP_S16 x, y;
            UInt16 objIndex = FrmGetObjectIndex( form, id );
            FrmGetObjectPosition( form, objIndex, &x, &y );

            FrmSetObjectPosition( form, objIndex, idp[1], y );
        }
    }
} /* updateForLefty */

static void
beamBoard( PalmAppGlobals* globals )
{
    Err err;
    XP_UCHAR prcName[50];

    unlockBonusPtrs( globals );
    DmCloseDatabase( globals->boardDBP );

    /* do we need to close the db first, and reopen after? */
    XP_SNPRINTF( prcName, sizeof(prcName), (XP_UCHAR*)"%s.prc", 
                 XW_PREFS_DBNAME );
    err = sendDatabase( CARD_0, globals->boardDBID, 
                        prcName, (XP_UCHAR*)"board prefs" );

    globals->boardDBP = DmOpenDatabase( CARD_0, globals->boardDBID,
                                        dmModeWrite );
    setupBonusPtrs( globals );
} /* beamBoard */

static XP_Bool
considerMenuShow( EventPtr event )
{
    XP_S16 y = event->screenY;
    XP_Bool penInRightPlace = (y < PALM_BOARD_TOP) && (y >= 0);

    if ( penInRightPlace ) {
        EventType menuEvent;
        XP_MEMSET( &menuEvent, 0, sizeof(menuEvent) );
        menuEvent.eType = keyDownEvent;
        menuEvent.data.keyDown.chr = menuChr;
        menuEvent.data.keyDown.keyCode = 0;
        menuEvent.data.keyDown.modifiers = commandKeyMask;
        EvtAddEventToQueue( &menuEvent );
    }

    return penInRightPlace;
} /* considerMenuShow */

/* Draw immediately, because we've made a change we need reflected
   immediately. */
static void
drawChangedBoard( PalmAppGlobals* globals )
{
    if ( !!globals->game.board && !globals->menuIsDown ) {
        board_draw( globals->game.board );
    }
} /* drawChangedBoard */

static XP_Bool
tryLoadSavedGame( PalmAppGlobals* globals, XP_U16 newIndex )
{
    XWGame tmpGame;
    CurGameInfo tmpGInfo;
    XP_Bool loaded;

    XP_MEMSET( &tmpGame, 0, sizeof(tmpGame) );
    XP_MEMSET( &tmpGInfo, 0, sizeof(tmpGInfo) );

    loaded = loadCurrentGame( globals, newIndex, &tmpGame, &tmpGInfo );

    /* Nuke the one we don't want */
    game_dispose( loaded? &globals->game : &tmpGame );
    gi_disposePlayerInfo( MEMPOOL (loaded? &globals->gameInfo : &tmpGInfo) );

    if ( loaded ) {
        XP_MEMCPY( &globals->game, &tmpGame, sizeof(globals->game) );
        XP_MEMCPY( &globals->gameInfo, &tmpGInfo, sizeof(globals->gameInfo) );
        globals->gState.curGameIndex = newIndex;
    }

    return loaded;
} /* tryLoadSavedGame */

static XP_U16
hresX( PalmAppGlobals* globals, XP_U16 screenX )
{
    if ( globals->useHiRes ) {
        screenX *= 2;
    }
    return screenX;
}

static XP_U16
hresY( PalmAppGlobals* globals, XP_U16 screenY )
{
    if ( globals->useHiRes ) {
        screenY *= 2;
    }
    return screenY;
}

static void
hresRect( PalmAppGlobals* globals, RectangleType* r )
{
    if ( globals->useHiRes ) {
        r->topLeft.x *= 2;
        r->topLeft.y *= 2;
        r->extent.x *= 2;
        r->extent.y *= 2;
    }
}

#ifdef XWFEATURE_FIVEWAY
static void
invalRectAroundButton( PalmAppGlobals* globals, XP_U16 objectID )
{
    RectangleType rect;
    getObjectBounds( objectID, &rect );

    rect.topLeft.x -= 3;
    rect.topLeft.y -= 3;
    rect.extent.x += 6;
    rect.extent.y += 6;
    hresRect( globals, &rect );

    board_invalRect( globals->game.board, (XP_Rect*)&rect );
}

static XP_Bool
isBoardObject( XP_U16 id )
{
    return id == XW_BOARD_GADGET_ID
        || id == XW_SCOREBOARD_GADGET_ID
        || id == XW_TRAY_GADGET_ID;
}

static XP_Bool
handleFocusEvent( PalmAppGlobals* globals, const EventType* event, 
                  XP_Bool* drawP )
{
    XP_U16 objectID = event->data.frmObjectFocusTake.objectID;
    XP_Bool isBoardObj = isBoardObject( objectID );
    XP_Bool take;
    BoardObjectType typ;

    XP_ASSERT( &event->data.frmObjectFocusTake.objectID
               == &event->data.frmObjectFocusLost.objectID );
    take = event->eType == frmObjectFocusTakeEvent;

/*     XP_LOGF( "%s(%s,%s)", __func__, frmObjId_2str(objectID), */
/*              (take? "take":"lost") ); */

    if ( take && !globals->initialTakeDropped && 
         (objectID == XW_SCOREBOARD_GADGET_ID) ) {
        /* Work around OS's insistence on sending initial take event. */
        globals->initialTakeDropped = XP_TRUE;
    } else {
        /* Need to invalidate the neighborhood of buttons on which palm draws
           the focus ring when they lose focus -- to redraw where the focus
           ring may have been.  No need unless we have the focus now,
           however, since we'll otherwise have drawn the object correctly
           (unfocussed). */

        if ( (!take) && (!isBoardObj) && isBoardObject( getFocusOwner() ) ) {
            EventType event;
            event.eType = updateAfterFocusEvent;
            event.data.generic.datum[0] = objectID;
            EvtAddEventToQueue( &event );
        }

        /* Board needs to know about any change involving it, including
           something else taking the focus it may think it has.  Why?
           Because takes preceed losses, yet the board must draw itself
           without focus before some button draws itself with focus and snags
           as part of the background the board in focussed state. */

        typ = isBoardObj? OBJ_BOARD + (objectID - XW_BOARD_GADGET_ID) : OBJ_NONE;
        *drawP = board_focusChanged( globals->game.board, typ, take );
        if ( isBoardObj && take ) {
            setFormFocus( globals->mainForm, objectID );
        }
    }
    return isBoardObj;
} /* handleFocusEvent */
#endif

#ifdef DO_TUNGSTEN_FIVEWAY
/* These are supposed to be defined in some SDK headers but I can't find 'em,
 * and if I could they're obscure enough that I wouldn't want the build to
 * depend on 'em since they're copyrighted and I couldn't distribute. */
# define vchrNavChange (vchrPalmMin + 3)
# define navBitUp         0x0001
# define navBitDown       0x0002
# define navBitLeft       0x0004
# define navBitRight      0x0008
# define navBitSelect     0x0010
# define navBitsAll       0x001F

# define navChangeUp      0x0100
# define navChangeDown    0x0200
# define navChangeLeft    0x0400
# define navChangeRight   0x0800
# define navChangeSelect  0x1000
# define navChangeBitsAll 0x1F00
#endif

static XP_Bool
handleKeyEvent( PalmAppGlobals* globals, const EventType* event, 
                XP_Bool* handledP )
{
    /* keyDownEvent: be very careful here.  keyUpEvent is only sent on
       devices with a hard keyboard.  Do not assume keyUpEvent or all
       non-Treos will be broken!!! */

    XP_Bool draw = XP_FALSE;
    XP_Key xpkey;
    XP_Bool handled = XP_FALSE;
    XP_Bool altOn = (event->data.keyUp.modifiers & shiftKeyMask) != 0;
    XP_Bool treatAsUp = !globals->generatesKeyUp
        || (event->eType == keyUpEvent);
    XP_U16 keyCode = event->data.keyDown.keyCode;
    Int16 chr;
    XP_Bool (*handler)( BoardCtxt*, XP_Key, XP_Bool* );
    BoardCtxt* board = globals->game.board;
    XP_S16 incr = 0; /* needed for tungsten and zodiac, but not treo since
                        the OS handled focus movement between objects. */

    globals->handlingKeyEvent = XP_TRUE;

#ifdef DO_TUNGSTEN_FIVEWAY
    if ( !globals->generatesKeyUp ) { /* this is the Tungsten case */
        if ( event->data.keyDown.chr == vchrNavChange ) {
            if ( (keyCode & (/* navBitUp |  */navChangeUp )) != 0 ) {
                keyCode = vchrRockerUp;
                incr = -1;
            } else if ( (keyCode & (/* navBitDown |  */navChangeDown )) != 0 ) {
                keyCode = vchrRockerDown;
                incr = 1;
            } else if ( (keyCode & (navBitLeft /* |navChangeLeft */ )) != 0 ) {
                keyCode = vchrRockerLeft;
                incr = -1;
            } else if ( (keyCode & ( navBitRight /*|navChangeRight*/ )) != 0 ) {
                keyCode = vchrRockerRight;
                incr = 1;
            } else if ( (keyCode & (navBitSelect /*|navChangeSelect*/)) != 0 ) {
                keyCode = vchrRockerCenter;
            }
        } else {
            keyCode = event->data.keyUp.chr;
        }
    }
#endif

    /* We're assuming the same layout for keyUp and keyDown event data.
       Let's make sure they're the same.... */
    XP_ASSERT( OFFSET_OF(EventType, data.keyUp.modifiers)
               == OFFSET_OF(EventType, data.keyDown.modifiers) );
    XP_ASSERT( OFFSET_OF(EventType, data.keyUp.keyCode)
               == OFFSET_OF(EventType, data.keyDown.keyCode) );

    if ( !globals->generatesKeyUp ) {
        handler = board_handleKey;
    } else if ( event->eType == keyUpEvent ) {
        handler = board_handleKeyUp;
        globals->lastKeyDown = XP_KEY_NONE;
    } else if ( (event->data.keyDown.modifiers & autoRepeatKeyMask) != 0 ) {
        handler = board_handleKeyRepeat;
    } else {
        handler = board_handleKeyDown;
        XP_ASSERT( globals->lastKeyDown == XP_KEY_NONE );
        globals->lastKeyDown = event->data.keyDown.keyCode;
    }

    /* Unlike Treo, zodiac doesn't use keyCode as documented */
    if ( globals->isZodiac ) {
        keyCode = event->data.keyDown.chr;
    }

    /* Treo gets at least one of these wrong in the chr field, but puts the
       right value in the keyCode.  So use that.  On other platforms must set
       it first. */
    switch ( keyCode ) {
#ifdef XWFEATURE_FIVEWAY
    case vchrRockerCenter:
        xpkey = XP_RETURN_KEY;
        break;
    case vchrRockerLeft:
        xpkey = altOn ? XP_CURSOR_KEY_ALTLEFT : XP_CURSOR_KEY_LEFT;
        incr = -1;
        break;
    case vchrRockerRight:
        xpkey = altOn ? XP_CURSOR_KEY_ALTRIGHT : XP_CURSOR_KEY_RIGHT;
        incr = 1;
        break;
    case vchrRockerUp:
        xpkey = altOn ? XP_CURSOR_KEY_ALTUP : XP_CURSOR_KEY_UP;
        incr = -1;
        break;
    case vchrRockerDown:
        xpkey = altOn ? XP_CURSOR_KEY_ALTDOWN : XP_CURSOR_KEY_DOWN;
        incr = 1;
        break;
    case chrSpace:
        xpkey = XP_RAISEFOCUS_KEY;
        break;
#endif
    default:
        /* Zodiac doesn't send keyUp events for printing chars, which somehow
           includes backspace */
        if ( globals->isZodiac ) {
            handler = board_handleKey;
        }

        xpkey = XP_KEY_NONE;
        chr = event->data.keyUp.chr;
        /* I'm not interested in being dependent on a particular version
           of the OS, (can't manage to link against the intl library
           anyway) and so don't want to use the 3.5-only text tests.  So
           let's give the board two shots at each char, one lower case
           and another upper. */
        if ( !!handler && (chr < 255) && (chr > ' ') ) { /* space is first
                                                            printing char */
            draw = (*handler)( board, chr, &handled );
            if ( !handled && chr >= 'a' ) {
                draw = (*handler)( board, chr - ('a' - 'A'), &handled );
            }
        } else {
            switch ( chr ) {
            case pageUpChr:
                draw = treatAsUp && scrollBoard( globals, 0, false );
                break;
            case pageDownChr:
                draw = treatAsUp && scrollBoard( globals, 2, false );
                break;
            case backspaceChr:
                xpkey = XP_CURSOR_KEY_DEL;
                break;
            case chrSpace:
                xpkey = XP_RAISEFOCUS_KEY;
                break;
            }
        }
    }

    if ( xpkey != XP_KEY_NONE ) {
        XP_ASSERT( !!handler );
        draw = (*handler)( board, xpkey, &handled );

        if ( 0 ) {
#ifdef DO_TUNGSTEN_FIVEWAY
            /* If it's a tungsten or zodiac, there's no built-in focus xfer
               so we do it here.  Don't do it for Treo, and don't do it on
               key-down for zodiac since it has keyUp too. */
        } else if ( !globals->hasTreoFiveWay && treatAsUp
                    && !handled && (incr != 0) ) {
            /* order'll be different if scoreboard is vertical */
            BoardObjectType typs[] = { OBJ_SCORE, OBJ_BOARD, OBJ_TRAY };
            BoardObjectType nxt = board_getFocusOwner( board );
            XP_U16 indx = 0;
            if ( nxt != OBJ_NONE ) {
                for ( ; indx < VSIZE(typs); ++indx ){
                    if ( nxt == typs[indx] ) {
                        indx = (indx + (VSIZE(typs) + incr));
                        indx %= VSIZE(typs);
                        break;
                    }
                }
            }
            draw = board_focusChanged( board, typs[indx], XP_TRUE ) || draw;
#endif
        } else if ( draw && !handled ) {
            /* If handled comes back false yet something changed (draw),
               we'll be getting another event shortly.  Put the draw off
               until then so we don't flash the tray focussed then not.  This
               is a hack, but I can't think of a way to integrate it into
               board.c logic without making too many palm-centric assumptions
               there. */
            draw = XP_FALSE;
        }
    } else {
        /* remove this and break focus drilldown.  Why? */
        handled = draw;
    }
    *handledP = handled;

    globals->handlingKeyEvent = XP_FALSE;

    return draw;
} /* handleKeyEvent */

static void
showRemaining( PalmAppGlobals* globals )
{
    if ( !!globals->game.board ) {
        XWStreamCtxt* stream = makeSimpleStream( globals, NULL );
        board_formatRemainingTiles( globals->game.board, stream );
        (void)askFromStream( globals, stream, STR_REMAINS_TITLE, true );
    }
}

/*****************************************************************************
 *
 ****************************************************************************/
static Boolean
mainViewHandleEvent( EventPtr event )
{
    XP_Bool handled = XP_TRUE;
    XP_Bool draw = XP_FALSE;
    Boolean erase;
#if defined CURSOR_MOVEMENT && defined DEBUG
    CursorDirection cursorDir;
    Boolean movePiece;
#endif
    PalmAppGlobals* globals;
    OpenSavedGameData* savedGameData;
    char newName[MAX_GAMENAME_LENGTH];
    XP_U16 prevSize;
    XWStreamCtxt* stream;

    CALLBACK_PROLOGUE();

    globals = getFormRefcon();

/*     XP_LOGF( "%s(%s)", __func__, eType_2str(event->eType) ); */

    switch ( event->eType ) {

    case nilEvent:
        draw = handled = handleNilEvent( globals );
        break;


    case noopEvent:
        /* do nothing! Exists just to force EvtGetEvent to return */
        XP_ASSERT( handled );
        break;

    case newGameCancelEvent:
        /* If user cancelled the new game dialog that came up the first time
           he launched (i.e. when there's no game to fall back to) then just
           quit.  It's easier than dealing with everything that can go wrong
           in this state. */
        if ( globals->isFirstLaunch ) {
            postEmptyEvent( appStopEvent );
        }
        globals->isNewGame = false;
        break;

    case openSavedGameEvent:
        globals->postponeDraw = XP_FALSE;
        prevSize = globals->gameInfo.boardSize;
        savedGameData = (OpenSavedGameData*)&event->data.generic;

        if ( tryLoadSavedGame( globals, savedGameData->newGameIndex ) ) {
            if ( prevSize > globals->gameInfo.boardSize ) {
                WinEraseWindow();
            }
            initAndStartBoard( globals, XP_FALSE );	
        }
        draw = true;
        break;

    case newGameOkEvent:
        if ( globals->newGameIsNew ) {
            globals->gState.curGameIndex = countGameRecords( globals );
        }
        globals->postponeDraw = false;
        makeDefaultGameName( newName );
        writeNameToGameRecord( globals, globals->gState.curGameIndex, 
                               newName, XP_STRLEN(newName) );
        globals->isFirstLaunch = false;	/* so we'll save the game */
        /* FALLTHRU */
    case loadGameEvent:
        XP_ASSERT( !!globals->game.server );
        initAndStartBoard( globals, event->eType == newGameOkEvent );
        draw = true;
        XP_ASSERT( !!globals->game.board );
        break;

#ifdef XWFEATURE_BLUETOOTH
    case closeBtLibEvent:
        palm_bt_close( globals );
        break;
#endif

#ifdef FEATURE_SILK
    case doResizeWinEvent:
        getSizes( globals );
        positionBoard( globals );
        board_invalAll( globals->game.board );
        FrmUpdateForm( 0, frmRedrawUpdateCode );
        break;
#endif

    case prefsChangedEvent:
        erase = LocalPrefsToGlobal( globals );
        draw = board_prefsChanged( globals->game.board, &globals->gState.cp );
        server_prefsChanged( globals->game.server, &globals->gState.cp );
        /* watch out for short-circuiting.  Both must be called */
        erase = positionBoard( globals ) || erase;
        if ( erase ) {
            WinEraseWindow();
        }
        globals->postponeDraw = false;
        FrmUpdateForm( 0, frmRedrawUpdateCode ); /* <- why is this necessary? */
        break;

#ifdef XWFEATURE_FIVEWAY
    case updateAfterFocusEvent:
        invalRectAroundButton( globals, event->data.generic.datum[0] );
        draw = XP_TRUE;
        break;
#endif

    case winExitEvent:
		if ( event->data.winExit.exitWindow == (WinHandle)FrmGetActiveForm() ){
			globals->menuIsDown = true;
        }
        if ( globals->lastKeyDown != XP_KEY_NONE ) {
            EventType event;
            XP_Bool ignore;

            event.eType = keyUpEvent;
            event.data.keyUp.chr = event.data.keyUp.keyCode
                = globals->lastKeyDown;
            draw = handleKeyEvent( globals, &event, &ignore );
        }
        break;

    case winEnterEvent:
		// From PalmOS's "Knowledge base": In the current code, the menu
		// doesn't remove itself when it receives a winExitEvent so we need
		// an extra check to make sure that the window being entered is the
		// first form.  This may be different in your implementation (ie: if
		// the first form opened is not the one you are currently watching
		// for)
		if (event->data.winEnter.enterWindow == (WinHandle)FrmGetActiveForm() &&
			event->data.winEnter.enterWindow == (WinHandle)FrmGetFirstForm() ){
            globals->menuIsDown = false;
		}
        break;

    case frmOpenEvent:
        globals->mainForm = FrmGetActiveForm();
        locateTrayButtons( globals );
        updateForLefty( globals, globals->mainForm );
        FrmDrawForm( globals->mainForm );
        break;

    case frmUpdateEvent:
        FrmDrawForm( globals->mainForm ); /* on 3.5 and higher, this erases
                                             the window before drawing, so
                                             there's nothing to be done about
                                             the erase after user clicks OK
                                             in prefs dialog. */
        if ( !!globals->game.board ) {
            RectangleType clip;
            WinGetClip( &clip );
    
            drawFormButtons( globals );
            hresRect( globals, &clip );
            board_invalRect( globals->game.board, (XP_Rect*)&clip );
            draw = !globals->postponeDraw;
#ifdef XWFEATURE_RELAY
            showConnState( globals );
#endif
        }
        break;

    case penDownEvent:
        draw = board_handlePenDown( globals->game.board, 
                                    hresX(globals, event->screenX), 
                                    hresY(globals, event->screenY), 
                                    &handled );
        globals->penDown = handled;
        break;

    case penMoveEvent:
        if ( globals->penDown ) {
            handled = board_handlePenMove( globals->game.board, 
                                          hresX( globals, event->screenX ), 
                                          hresY( globals, event->screenY ));
            draw = handled;
        }
        break;

    case penUpEvent:
        if ( globals->penDown ) {
            draw = board_handlePenUp( globals->game.board, 
                                      hresX( globals, event->screenX),
                                      hresY( globals, event->screenY ) );
            handled = draw;     /* this is wrong!!!! */
            globals->penDown = false;

            if ( !handled ) {
                handled = considerMenuShow( event );
            }
        }
        break;

    case menuEvent:
        MenuEraseStatus(0);
        switch ( event->data.menu.itemID ) {

        case XW_TILEVALUES_PULLDOWN_ID: 
            if ( !!globals->game.server ) {
                stream = makeSimpleStream( globals, NULL );

                server_formatDictCounts( globals->game.server, stream, 
                                         4 ); /* 4: ncols */

                (void)askFromStream( globals, stream, STR_VALUES_TITLE, true );
            }
            break;

        case XW_TILESLEFT_PULLDOWN_ID:
            showRemaining( globals );
            break;

        case XW_HISTORY_PULLDOWN_ID:
            if ( !!globals->game.server ) {
                XP_Bool gameOver = server_getGameIsOver(globals->game.server);
                stream = makeSimpleStream( globals, NULL );

                model_writeGameHistory( globals->game.model, stream, 
                                        globals->game.server, gameOver );
                if ( stream_getSize( stream ) > 0 ) {
                    (void)askFromStream( globals, stream, STR_HISTORY_TITLE, 
                                         XP_FALSE );
                } else {
                    beep();
                }
                stream_destroy( stream );
            }
            break;

        case XW_NEWGAME_PULLDOWN_ID:
            askStartNewGame( globals );
            break;

        case XW_SAVEDGAMES_PULLDOWN_ID:
            saveOpenGame( globals );/* so it can be accurately duped */
            /* save game changes state; reflect on screen before
               popping up dialog */
            drawChangedBoard( globals );
            FrmPopupForm( XW_SAVEDGAMES_DIALOG_ID );
            break;

        case XW_FINISH_PULLDOWN_ID:
            if ( server_getGameIsOver( globals->game.server ) ) {
                displayFinalScores( globals );
            } else if ( palmaskFromStrId( globals, STR_CONFIRM_END_GAME, -1 ) ) {
                server_endGame( globals->game.server );
                draw = true;	    
            }
            break;

#ifndef XWFEATURE_STANDALONE_ONLY
            /* Would be better to beep when no remote players.... */
        case XW_RESENDIR_PULLDOWN_ID:
            if ( !!globals->game.comms ) {
#ifdef XWFEATURE_BLUETOOTH
                globals->suspendBT = XP_FALSE;
#endif
                (void)comms_resendAll( globals->game.comms );
            } else {
                userErrorFromStrId( globals, STR_RESEND_STANDALONE );
            }
            break;
#endif
        case XW_BEAMDICT_PULLDOWN_ID:
            globals->dictuiForBeaming = true;
            FrmPopupForm( XW_DICTINFO_FORM );
            break;

        case XW_BEAMBOARD_PULLDOWN_ID:
            beamBoard( globals );
            break;

#ifdef FEATURE_DUALCHOOSE
            /* This probably goes away at ship.... */
        case XW_RUN68K_PULLDOWN_ID:
        case XW_RUNARM_PULLDOWN_ID: {
            Err err;
            LocalID dbID;
            
            (void)FtrUnregister( APPID, FEATURE_WANTS_68K );
            err = FtrSet( APPID, FEATURE_WANTS_68K, 
                          event->data.menu.itemID == XW_RUN68K_PULLDOWN_ID?
                          WANTS_68K : WANTS_ARM );

            dbID = DmFindDatabase( CARD_0, APPNAME );
            if ( dbID != 0 ) {
                (void)SysUIAppSwitch( 0, dbID, 
                                      sysAppLaunchCmdNormalLaunch, NULL );
            }
        }
            break;
#endif

        case XW_PASSWORDS_PULLDOWN_ID:
            globals->isNewGame = false;
            FrmPopupForm( XW_NEWGAMES_FORM );
            break;

#ifdef COLOR_EDIT
        case XW_EDITCOLORS_PULLDOWN_ID:
            if ( globals->able == COLOR ) {
                FrmPopupForm( XW_COLORPREF_DIALOG_ID );
            }
            break;
# ifdef DEBUG
        case XW_DUMPCOLORS_PULLDOWN_ID:
            dumpColors( globals );
            break;
# endif
#endif

        case XW_PREFS_PULLDOWN_ID:
            globals->stateTypeIsGlobal = XP_TRUE;
            GlobalPrefsToLocal( globals );
            FrmPopupForm( XW_PREFS_FORM );
            break;

        case XW_ABOUT_PULLDOWN_ID:
            palmaskFromStrId( globals, STR_ABOUT_CONTENT, STR_ABOUT_TITLE );
            break;
	    
        case XW_HINT_PULLDOWN_ID:
            board_resetEngine( globals->game.board );
            globals->askTrayLimits = XP_FALSE;

        case XW_NEXTHINT_PULLDOWN_ID:
            draw = handleHintRequest( globals );
            break;

#ifdef XWFEATURE_SEARCHLIMIT
        case XW_HINTCONFIG_PULLDOWN_ID:
            board_resetEngine( globals->game.board );
            globals->askTrayLimits = XP_TRUE;
            draw = handleHintRequest( globals );
            break;
#endif

        case XW_UNDOCUR_PULLDOWN_ID:
            draw = board_replaceTiles( globals->game.board );
            break;

        case XW_UNDOLAST_PULLDOWN_ID:
            draw = server_handleUndo( globals->game.server );
            break;

        case XW_DONE_PULLDOWN_ID:
            draw = handleDone( globals );
            break;

        case XW_JUGGLE_PULLDOWN_ID:
            draw = handleJuggle( globals );
            break;
	
        case XW_TRADEIN_PULLDOWN_ID:
            draw = handleTrade( globals );
            break;
	    
        case XW_HIDESHOWTRAY_PULLDOWN_ID:
            draw = handleHideTray( globals );
            break;

#ifdef FOR_GREMLINS
        case XW_GREMLIN_DIVIDER_RIGHT:
            if ( !!globals->game.board ) {
                board_moveDivider( globals->game.board, XP_TRUE );
                draw = XP_TRUE;
            }
            break;
        case XW_GREMLIN_DIVIDER_LEFT:
            if ( !!globals->game.board ) {
                board_moveDivider( globals->game.board, XP_FALSE );
                draw = XP_TRUE;
            }
            break;
#endif

#ifdef DEBUG	 
        case XW_LOGFILE_PULLDOWN_ID:
            toggleBoolFtr( LOG_FILE_FEATURE );
            break;
        case XW_LOGMEMO_PULLDOWN_ID:
            toggleBoolFtr( LOG_MEMO_FEATURE );
            break;

        case XW_CLEARLOGS_PULLDOWN_ID:
            PalmClearLogs();
            break;
# if 0
        case XW_RESET_PULLDOWN_ID: {
            postEmptyEvent( appStopEvent );
        }

            globals->resetGame = true;
            break;
# endif

        case XW_NETSTATS_PULLDOWN_ID:
            if ( !!globals->game.comms ) {
                stream = makeSimpleStream( globals, askOnClose );
                comms_getStats( globals->game.comms, stream );
                stream_destroy( stream );
            }
            break;
#if defined XWFEATURE_BLUETOOTH && defined DEBUG
        case XW_BTSTATS_PULLDOWN_ID:
            stream = makeSimpleStream( globals, askOnClose );
            palm_bt_getStats( globals, stream );
            stream_destroy( stream );
            break;
#endif

#ifdef MEM_DEBUG
        case XW_MEMSTATS_PULLDOWN_ID :
            if ( !!globals->mpool ) {
                stream = makeSimpleStream( globals, askOnClose );
                mpool_stats( globals->mpool, stream );
                stream_destroy( stream );
            }
            break;
#endif

#endif
   
        default:
            break;
        }
        break;

#ifdef XWFEATURE_FIVEWAY
    case frmObjectFocusTakeEvent:
    case frmObjectFocusLostEvent:
        handled = globals->hasTreoFiveWay
            && handleFocusEvent( globals, event, &draw );
        break;
#endif

    case keyUpEvent:
        XP_ASSERT( globals->generatesKeyUp );
        /* work around not yet being able to set generatesKeyUp accurately
           using FtrGet */
        if ( !globals->generatesKeyUp ) {
            globals->generatesKeyUp = XP_TRUE;
            globals->keyDownReceived = XP_FALSE; /* drop the event this once */
        } else if ( globals->keyDownReceived ) {
            globals->keyDownReceived = XP_FALSE;
            draw = handleKeyEvent( globals, event, &handled );
        }
        break;
    case keyDownEvent:
        if ( !globals->menuIsDown ) {
            globals->keyDownReceived = XP_TRUE;
            draw = handleKeyEvent( globals, event, &handled );
        }
        break;

    case sclRepeatEvent:
        draw = scrollBoard( globals, event->data.sclRepeat.newValue-SBAR_MIN, 
                            true );
        handled = false;
        break;

    case ctlSelectEvent:
        handled = true;
        switch ( event->data.ctlEnter.controlID ) {
        case XW_MAIN_FLIP_BUTTON_ID:
            draw = handleFlip( globals );
            break;
        case XW_MAIN_VALUE_BUTTON_ID:
            draw = handleValueToggle( globals );
            break;
        case XW_MAIN_HINT_BUTTON_ID:
            draw = handleHintRequest( globals );
            break;
#ifndef EIGHT_TILES
        case XW_MAIN_DONE_BUTTON_ID:
            draw = handleDone( globals );
            break;
        case XW_MAIN_JUGGLE_BUTTON_ID:
            draw = handleJuggle( globals );
            break;
        case XW_MAIN_TRADE_BUTTON_ID:
            draw = handleTrade( globals );
            break;
        case XW_MAIN_HIDE_BUTTON_ID:
            draw = handleHideTray( globals );
            break;
#endif
        case XW_MAIN_SHOWTRAY_BUTTON_ID:
            draw = board_showTray( globals->game.board );
            break;

        default:
            handled = false;
            break;
        } /* switch event->data.ctlEnter.controlID */

    default:
        handled = false;
        break;
    }

    if ( draw && !!globals->game.board && !globals->menuIsDown ) {
        XP_Bool drewAll = board_draw( globals->game.board );
        if ( !drewAll ) {
            globals->msgReceivedDraw = XP_TRUE;
            palm_util_requestTime( &globals->util );
        }
    }

    CALLBACK_EPILOGUE();
    return handled;
} /* mainViewHandleEvent */

static void
askStartNewGame( PalmAppGlobals* globals )
{
    if ( palmaskFromStrId( globals, STR_ASK_REPLACE_GAME, -1 )) {
        /* do nothing; popping up the NEWGAMES dlg will do it -- if not
           cancelled */
        globals->newGameIsNew = XP_FALSE;
    } else {
        saveOpenGame( globals );

        drawChangedBoard( globals );

        globals->newGameIsNew = XP_TRUE;
    }
    globals->isNewGame = true;
    globals->isFirstLaunch = false;
    FrmPopupForm( XW_NEWGAMES_FORM );
} /* askStartNewGame */

static void
displayFinalScores( PalmAppGlobals* globals )
{
    XWStreamCtxt* stream;

    stream = makeSimpleStream( globals, NULL );
    server_writeFinalScores( globals->game.server, stream );
    stream_putU8( stream, '\0' );

    (void)askFromStream( globals, stream, STR_FINAL_SCORES_TITLE, true );
} /* displayFinalScores */

XP_S16
palm_memcmp( XP_U8* p1, XP_U8* p2, XP_U16 nBytes )
{
    /* man memcmp: The memcmp() function compares the first n bytes of the
       memory areas s1 and s2.  It returns an integer less than, equal to, or
       greater than zero if s1 is found, respectively, to be less than, to
       match, or be greater than s2.*/
    XP_S16 result = 0;
    while ( result == 0 && nBytes-- ) {
        result = *p1++ - *p2++;
    }
    return result;
} /* palm_memcmp */

static void
askScrollbarAdjust( PalmAppGlobals* globals, FieldPtr field )
{
    UInt16 scrollPos;
    UInt16 textHeight;
    UInt16 fieldHeight;
    UInt16 maxValue;
    ScrollBarPtr scroll;

    FldGetScrollValues( field, &scrollPos, &textHeight, &fieldHeight );

    if ( textHeight > fieldHeight ) {
        maxValue = textHeight - fieldHeight;
    } else if ( scrollPos != 0 ) {
        maxValue = scrollPos;
    } else {
        maxValue = 0;
    }

    scroll = getActiveObjectPtr( XW_ASK_SCROLLBAR_ID );
    SclSetScrollBar( scroll, scrollPos, 0, maxValue, fieldHeight-1 );
    globals->prevScroll = scrollPos;
} /* askScrollbarAdjust */

static void
getWindowBounds( WinHandle wHand, RectangleType* r )
{
    WinHandle prev = WinSetDrawWindow( wHand );
    WinGetDrawWindowBounds( r );
    (void)WinSetDrawWindow( prev );
} /* getWindowBounds */

static void
tryGrowAskToFit( FormPtr form, FieldPtr field, const XP_UCHAR* str )
{
    RectangleType fieldRect, dlgRect;
    UInt16 scrollPos;
    UInt16 textHeight;
    UInt16 fieldHeight;
    WinHandle wHand;
    XP_S16 fldWidth, fldHeight, maxHeight, needsHeight;
    XP_U16 lineHeight;
    XP_U16 growthAmt, i;
    RectangleType objBounds;

    FldGetScrollValues( field, &scrollPos, &textHeight, &fieldHeight );

    getObjectBounds( XW_ASK_TXT_FIELD_ID, &fieldRect );
    fldWidth = fieldRect.extent.x;
    fldHeight = fieldRect.extent.y;

    /* max is cur height plus diff between dialog's height and what it could
       be */
    wHand = FrmGetWindowHandle( form );
    getWindowBounds( wHand, &dlgRect );
    maxHeight = fldHeight + (156 - dlgRect.extent.y);

    lineHeight = FntLineHeight();

    needsHeight = FldCalcFieldHeight((const char*)str, fldWidth) * lineHeight;

    if ( needsHeight > maxHeight ) {
        /* make window as large as it can be */
        needsHeight = maxHeight;
    }

    /* now round down to a multiple of lineHeight */
    needsHeight = (needsHeight / lineHeight) * lineHeight;

    growthAmt = needsHeight - fldHeight;

    /* now reflect the new size by moving things around.  Window first */
    dlgRect.topLeft.y -= growthAmt;
    dlgRect.extent.y += growthAmt;
    WinSetBounds( wHand, &dlgRect );

    /* then the field */
    fieldRect.extent.y += growthAmt;
    setObjectBounds( XW_ASK_TXT_FIELD_ID, &fieldRect );

    /* the scrollbar */
    getObjectBounds( XW_ASK_SCROLLBAR_ID, &objBounds );
    objBounds.extent.y = fieldRect.extent.y;
    setObjectBounds( XW_ASK_SCROLLBAR_ID, &objBounds );

    /* and the buttons */
    XP_ASSERT( XW_ASK_NO_BUTTON_ID - XW_ASK_YES_BUTTON_ID == 1 );
    for ( i = XW_ASK_YES_BUTTON_ID; i <= XW_ASK_NO_BUTTON_ID; ++i ){
        getObjectBounds( i, &objBounds );
        objBounds.topLeft.y += growthAmt;
        setObjectBounds( i, &objBounds );
    }
} /* tryGrowAskToFit */

static Boolean 
handleScrollInAsk( EventPtr event )
{
    UInt16 linesToScroll = 0;
    Boolean scrollFromButton = false;
    Boolean result = true;
    WinDirectionType direction = 5;
    PalmAppGlobals* globals = (PalmAppGlobals*)getFormRefcon();
    FieldPtr field;
    UInt16 endPosition;

    XP_ASSERT ( !!globals );

    field = getActiveObjectPtr( XW_ASK_TXT_FIELD_ID );

    switch ( event->eType ) {

    case penUpEvent:
        /* When user drags pen through text and causes a scroll the scrollbar
           will get out of sync.  So we listen to the event but don't claim to
           have handled it. */
        askScrollbarAdjust( globals, field );
        result = false;
        break;

    case keyDownEvent:
        if ( globals->ignoreFirstKeyDown ) {
            globals->ignoreFirstKeyDown = XP_FALSE;
            XP_ASSERT( result );
        } else if ( FrmGetWindowHandle( FrmGetActiveForm() ) 
                    == WinGetDrawWindow() ) {
            /* don't scroll a menu if open! */
            switch ( event->data.keyDown.chr ) {
            case pageUpChr:
            case vchrRockerUp:
                direction = winUp; 
                break;
            case pageDownChr:
            case vchrRockerDown:
                direction = winDown; 
                break;
            default:
                result = false;
            }
            linesToScroll = 3;
            scrollFromButton = true;
        }
        break;

    case sclRepeatEvent: {
        XP_S16 newVal = event->data.sclRepeat.newValue;
        XP_S16 tmp = newVal - globals->prevScroll;
        linesToScroll = XP_ABS( tmp );
        XP_ASSERT( linesToScroll != 0 );
        direction = newVal > globals->prevScroll? winDown: winUp;
        globals->prevScroll = newVal;
        scrollFromButton = false;
    }
        break;

    case menuEvent:
        MenuEraseStatus(0);
        result = true;
        switch ( event->data.menu.itemID ) {
        case ASK_COPY_PULLDOWN_ID:
            FldCopy( field );
            break;
        case ASK_SELECTALL_PULLDOWN_ID:
            endPosition = FldGetTextLength( field );
            FldSetSelection( field, 0, endPosition );
            break;
        }
        break;

    default:
        result = false;
    }

    if ( result && FldScrollable( field, direction ) ) {
        FldScrollField( field, linesToScroll, direction );
        if ( scrollFromButton ) {
            askScrollbarAdjust( globals, field );
        } else {
            result = false;	/* for some reason this is necessary to make
                               scrolbar work right. */
        }
    }

    return result;
} /* handleScrollInAsk */

/* Swap the two elements, preserving their outside borders */
static void
moveLeftOf( UInt16 rightID, UInt16 leftID )
{
    UInt16 leftIndex, rightIndex;
    UInt16 middleMargin;
    RectangleType leftBounds, rightBounds;
    FormPtr form = FrmGetActiveForm();

    leftIndex = FrmGetObjectIndex( form, leftID );
    rightIndex = FrmGetObjectIndex( form, rightID );

    FrmGetObjectBounds( form, rightIndex, &rightBounds );
    FrmGetObjectBounds( form, leftIndex, &leftBounds );

    XP_ASSERT( rightBounds.topLeft.y == leftBounds.topLeft.y );

    middleMargin = rightBounds.topLeft.x - 
        (leftBounds.topLeft.x+leftBounds.extent.x);

    FrmSetObjectPosition( form, rightIndex, leftBounds.topLeft.x,
                          leftBounds.topLeft.y );

    FrmSetObjectPosition( 
                         form, leftIndex, 
                         leftBounds.topLeft.x + rightBounds.extent.x + middleMargin,
                         leftBounds.topLeft.y );

} /* moveLeftOf */

XP_Bool
palmaskFromStrId( PalmAppGlobals* globals, XP_U16 strId, XP_S16 titleID )
{
    const XP_UCHAR* message;
    const XP_UCHAR* yes;
    message = getResString( globals, strId );
    XP_ASSERT( !!message );
    yes = titleID < 0? NULL: getResString( globals, STR_OK );
    return palmask( globals, message, yes, titleID );
} /* palmaskFromStrId */

XP_Bool
palmask( PalmAppGlobals* globals, const XP_UCHAR* str, 
         const XP_UCHAR* yesButton, XP_S16 titleID )
{
    FormPtr form, prevForm;
    FieldPtr field;
    const XP_UCHAR* title;
    UInt16 buttonHit;
    XP_U16 buttons[] = { XW_ASK_YES_BUTTON_ID, XW_ASK_NO_BUTTON_ID };
    XP_U16 nButtons;

    if ( !!globals->game.board ) {
        board_pushTimerSave( globals->game.board );
    }

    title = titleID >= 0? getResString( globals, titleID ): NULL;

    prevForm = FrmGetActiveForm();
    form = FrmInitForm( XW_ASK_FORM_ID );

    FrmSetActiveForm( form );

    if ( !!yesButton ) {
        CtlSetLabel( getActiveObjectPtr(XW_ASK_YES_BUTTON_ID), 
                     (const char*)yesButton );
        fitButtonToString( XW_ASK_YES_BUTTON_ID );
    }

    /* Hack: take advantage of fact that for now only non-queries should not
       have a cancel button. */
    if ( title == NULL ) {
        nButtons = 2;
    } else {
        FrmSetTitle( form, (char*)title );
        disOrEnable( form, XW_ASK_NO_BUTTON_ID, false );
        nButtons = 1;
    }
    /* always center: some localized buttons are bigger */
    centerControls( form, buttons, nButtons );

    /* If we're running OS5 (oneDotFiveAvail), then eat the first keyDown.
       If an earlier OS (Treo600) then we won't see that spurious event. */
    if ( globals->handlingKeyEvent && globals->oneDotFiveAvail ) {
        globals->ignoreFirstKeyDown = XP_TRUE;
    }

    FrmSetEventHandler( form, handleScrollInAsk );

    globals->prevScroll = 0;

    if ( globals->isLefty ) {
        moveLeftOf( XW_ASK_SCROLLBAR_ID, XW_ASK_TXT_FIELD_ID );
    }

    field = getActiveObjectPtr( XW_ASK_TXT_FIELD_ID );
    FldSetTextPtr( field, (char*)str );

    FldRecalculateField( field, true );

    if ( globals->romVersion >= 35 ) {
        /* I'm not sure how to do this pre 3.5 */
        tryGrowAskToFit( form, field, str );
    }

    askScrollbarAdjust( globals, field );

    FrmDrawForm( form );

    buttonHit = FrmDoDialog( form );

    FrmDeleteForm( form );
    FrmSetActiveForm( prevForm );

    if ( !!globals->game.board ) {
        board_popTimerSave( globals->game.board );
    }

    return buttonHit == XW_ASK_YES_BUTTON_ID;
} /* palmask */

static XP_Bool
askFromStream( PalmAppGlobals* globals, XWStreamCtxt* stream, XP_S16 titleID,
               Boolean closeAndDestroy )
{
    XP_U16 nBytes = stream_getSize( stream );
    XP_Bool result;
    XP_UCHAR* buffer;

    XP_ASSERT( nBytes < maxFieldTextLen );

    buffer = XP_MALLOC( globals->mpool, nBytes + 1 );
    stream_getBytes( stream, buffer, nBytes );
    /* nuke trailing <CR> chars to they don't extend length of field */
    while ( buffer[nBytes-1] == '\n' ) {
        --nBytes;
    }
    buffer[nBytes] = '\0';	/* just to be safe */
    
    result = palmask( globals, buffer, 
                      getResString( globals, STR_OK ), titleID );

    XP_FREE( globals->mpool, buffer );

    if ( closeAndDestroy ) {
        stream_destroy( stream );
    }

    return result;
} /* askFromStream */

XP_Bool
askPassword( PalmAppGlobals* globals, const XP_UCHAR* name, XP_Bool isNew, 
             XP_UCHAR* retbuf, XP_U16* len )
{
    XP_Bool result = XP_FALSE;
    FormPtr prevForm, form;
    FieldPtr field;
    UInt16 showMe;

    prevForm = FrmGetActiveForm();
    form = FrmInitForm( XW_PASSWORD_DIALOG_ID );
    FrmSetActiveForm( form );

    if ( isNew ) {
        showMe = XW_PASSWORD_NEWNAME_LABEL;
    } else {
        showMe = XW_PASSWORD_NAME_LABEL;
    }
    FrmShowObject( form, FrmGetObjectIndex( form, showMe ) );

    FrmDrawForm( form );

    if ( !!name ) {
        field = getActiveObjectPtr( XW_PASSWORD_NAME_FIELD );
        FldSetTextPtr( field, (char*)name );
        FldDrawField( field );
    }

    if ( !globals->hasTreoFiveWay ) {
        FrmSetFocus( form, FrmGetObjectIndex( form, XW_PASSWORD_PASS_FIELD ) );
    }

    if ( FrmDoDialog( form ) == XW_PASSWORD_OK_BUTTON ) {
        char* enteredPass;
        XP_U16 enteredLen;
        field = getActiveObjectPtr( XW_PASSWORD_PASS_FIELD );
        enteredPass = FldGetTextPtr( field );
        enteredLen = enteredPass? StrLen(enteredPass) : 0;
        if ( enteredLen < *len ) {
            result = XP_TRUE;
            if ( enteredLen > 0 ) {
                XP_MEMCPY( retbuf, enteredPass, enteredLen );
            }
            retbuf[enteredLen] = '\0';
            *len = enteredLen;
        }
    }

    FrmDeleteForm( form );
    FrmSetActiveForm( prevForm );

    return result;
} /* askPassword */

/*****************************************************************************
 * Callbacks
 ****************************************************************************/
static VTableMgr*
palm_util_getVTManager( XW_UtilCtxt* uc )
{
    PalmAppGlobals* globals = (PalmAppGlobals*)uc->closure;
    return globals->vtMgr;
} /* palm_util_getVTManager */

static void
palm_util_userError( XW_UtilCtxt* uc, UtilErrID id )
{
    PalmAppGlobals* globals;
    XP_U16 strID = STR_LAST_STRING;

    switch( id ) {
    case ERR_TILES_NOT_IN_LINE:
        strID = STR_ALL_IN_LINE_ERR;
        break;
    case ERR_NO_EMPTIES_IN_TURN:
        strID = STR_NO_EMPTIES_ERR;
        break;

    case ERR_TWO_TILES_FIRST_MOVE:
        strID = STR_FIRST_MOVE_ERR;
        break;
    case ERR_TILES_MUST_CONTACT:
        strID = STR_MUST_CONTACT_ERR;
        break;
    case ERR_NOT_YOUR_TURN:
        strID = STR_NOT_YOUR_TURN;
        break;
    case ERR_NO_PEEK_ROBOT_TILES:
        strID = STR_NO_PEEK_ROBOT_TILES;
        break;

#ifndef XWFEATURE_STANDALONE_ONLY
    case ERR_NO_PEEK_REMOTE_TILES:
        strID = STR_NO_PEEK_REMOTE_TILES;
        break;
    case ERR_SERVER_DICT_WINS:
        strID = STR_SERVER_DICT_WINS;
        break;
    case ERR_REG_UNEXPECTED_USER:
        strID = STR_REG_UNEXPECTED_USER;
        break;	
    case ERR_REG_SERVER_SANS_REMOTE:
        strID = STR_REG_NEED_REMOTE;
        break;	
    case STR_NEED_BT_HOST_ADDR:
        strID = STR_REG_BT_NEED_HOST;
        break;	
#endif

    case ERR_CANT_TRADE_MID_MOVE:
        strID = STR_CANT_TRADE_MIDTURN;
        break;

    case ERR_TOO_FEW_TILES_LEFT_TO_TRADE:
        strID = STR_TOO_FEW_TILES;
        break;

    case ERR_CANT_UNDO_TILEASSIGN:
        strID = STR_CANT_UNDO_TILEASSIGN;
        break;
        
    case ERR_CANT_HINT_WHILE_DISABLED:
        strID = STR_CANT_HINT_WHILE_DISABLED;
        break;

#ifdef XWFEATURE_RELAY
    case ERR_RELAY_BASE + XWRELAY_ERROR_TIMEOUT:
        strID = STR_RELAY_TIMEOUT;
        break;
    case ERR_RELAY_BASE + XWRELAY_ERROR_HEART_YOU:
        strID = STR_RELAY_GENERIC;
        break;
    case ERR_RELAY_BASE + XWRELAY_ERROR_HEART_OTHER:
    case ERR_RELAY_BASE + XWRELAY_ERROR_LOST_OTHER:
        strID = STR_RELAY_LOST_OTHER;
        break;
#endif

    default:
        XP_DEBUGF( "errcode=%d", id );
        break;
    }

    XP_LOGF( "%s(%d)", __func__, strID );

    XP_ASSERT( strID < STR_LAST_STRING );
    globals = (PalmAppGlobals*)uc->closure;
    userErrorFromStrId( globals, strID );
} /* palm_util_userError */

static void
userErrorFromStrId( PalmAppGlobals* globals, XP_U16 strID )
{
    const XP_UCHAR* message = getResString( globals, strID );
    keySafeCustomAlert( globals, message );
} /* userErrorFromStrId */

static XP_Bool
palm_util_userQuery( XW_UtilCtxt* uc, UtilQueryID id, XWStreamCtxt* stream )
{
    PalmAppGlobals* globals = (PalmAppGlobals*)uc->closure;
    XP_U16 strID = STR_LAST_STRING; /* error if not changed */

    switch( id ) {
    case QUERY_COMMIT_TURN:
        return askFromStream( globals, stream, -1, false );
        break;
    case QUERY_COMMIT_TRADE:
        strID = STR_CONFIRM_TRADE;
        break;
    case QUERY_ROBOT_MOVE:
    case QUERY_ROBOT_TRADE:
        return askFromStream( globals, stream, STR_ROBOT_TITLE, false );
        break;
    default:
        XP_ASSERT(0);
        break;
    }

    return (XP_Bool)palmaskFromStrId( globals, strID, -1 );
} /* palm_util_userQuery */

static XWBonusType
palm_util_getSquareBonus( XW_UtilCtxt* uc, const ModelCtxt* model,
                          XP_U16 col, XP_U16 row )
{
    PalmAppGlobals* globals = (PalmAppGlobals*)uc->closure;
    XP_U16 nCols, nRows;
    XP_U16 midCol, index, resIndex;
    XP_UCHAR* bonusResPtr;
    XP_U8 value;

    XP_ASSERT( !!model );

    nCols = model_numCols( model );
    nRows = model_numRows( model );
    midCol = nCols / 2;
    resIndex = (PALM_MAX_COLS - nCols) / 2;    
    bonusResPtr = globals->bonusResPtr[resIndex];

    if ( col > midCol ) col = nCols - 1 - col;
    if ( row > midCol ) row = nRows - 1 - row;
    index = (row*(midCol+1)) + col;

    XP_ASSERT( index/2 < MemPtrSize(bonusResPtr) );

    value = bonusResPtr[index/2];
    if ( index%2 == 0 ) {
        value >>= 4;
    }

    return value & 0x0F;
} /* palm_util_getSquareBonus */

static XP_S16
palm_util_userPickTile( XW_UtilCtxt* uc, const PickInfo* pi,
                        XP_U16 playerNum, const XP_UCHAR** texts, 
                        XP_U16 nTiles )
{
    PalmAppGlobals* globals = (PalmAppGlobals*)uc->closure;
    return askBlankValue( globals, playerNum, pi, nTiles, texts );
} /* palm_util_userPickTile */

static XP_Bool 
palm_util_askPassword( XW_UtilCtxt* uc, const XP_UCHAR* name, 
                       XP_UCHAR* buf, XP_U16* len )
{
    PalmAppGlobals* globals = (PalmAppGlobals*)uc->closure;
    return askPassword( globals, name, false, buf, len );
} /* palm_util_askPassword */

static void 
palm_util_trayHiddenChange( XW_UtilCtxt* uc, 
                            XW_TrayVisState XP_UNUSED(newState),
                            XP_U16 XP_UNUSED(nVisibleRows) )
{
    PalmAppGlobals* globals = (PalmAppGlobals*)uc->closure;
    palmSetCtrlsForTray( globals );

    drawFormButtons( globals );
} /* palm_util_trayHiddenChange */

static void 
palm_util_yOffsetChange( XW_UtilCtxt* uc, XP_U16 XP_UNUSED_DBG(oldOffset), 
                         XP_U16 newOffset )
{
    PalmAppGlobals* globals = (PalmAppGlobals*)uc->closure;
    XP_ASSERT( oldOffset != newOffset );
    updateScrollbar( globals, newOffset );
} /* palm_util_yOffsetChange */

static void 
palm_util_notifyGameOver( XW_UtilCtxt* uc )
{
    PalmAppGlobals* globals = (PalmAppGlobals*)uc->closure;
    board_draw( globals->game.board ); /* refresh scoreboard so it agrees
                                          with dialog */
    displayFinalScores( globals );
} /* palm_util_notifyGameOver */

static XP_Bool
palm_util_hiliteCell( XW_UtilCtxt* uc, XP_U16 col, XP_U16 row )
{
    /* EvtSysEventAvail, not EvtEventAvail, because the former ignores nil
       events, and it appears that when there's an IR connection up the
       system floods us with nil events.*/
    XP_Bool eventPending = EvtSysEventAvail( true );
#ifdef SHOW_PROGRESS
    if ( !eventPending ) {
        PalmAppGlobals* globals = (PalmAppGlobals*)uc->closure;
        if ( globals->progress.curLine >= 0 ) {
            board_hiliteCellAt( globals->game.board, col, row );
        }
    }
#endif
    
    return !eventPending;
} /* palm_util_hiliteCell */

static XP_Bool
palm_util_engineProgressCallback( XW_UtilCtxt* uc )
{
#ifdef SHOW_PROGRESS
    PalmAppGlobals* globals = (PalmAppGlobals*)uc->closure;
    if ( globals->gState.showProgress && globals->progress.curLine >= 0  ) {
        RectangleType rect = globals->progress.boundsRect;
        short line;
        Boolean draw;

        globals->progress.curLine %= rect.extent.y * 2;
        draw = globals->progress.curLine < rect.extent.y;

        line = globals->progress.curLine % (rect.extent.y) + 1;
        line = rect.topLeft.y + rect.extent.y - line;
        if ( draw ) {
            WinDrawLine( rect.topLeft.x, line,
                         rect.topLeft.x + rect.extent.x - 1, line);
        } else {
            WinEraseLine( rect.topLeft.x, line,
                          rect.topLeft.x + rect.extent.x - 1,
                          line );
        }
        ++globals->progress.curLine;
    }
#endif
    return !EvtSysEventAvail( true );
} /* palm_util_engineProgressCallback */

static void
palm_util_setTimer( XW_UtilCtxt* uc, XWTimerReason why, 
                    XP_U16 secsFromNow,
                    XWTimerProc proc, void* closure )
{
    PalmAppGlobals* globals = (PalmAppGlobals*)uc->closure;
    XP_U32 now = TimGetTicks();

    if ( why == TIMER_PENDOWN ) {
        now += PALM_TIMER_DELAY;
    } else if ( why == TIMER_TIMERTICK ) {
        now += SysTicksPerSecond();
#if defined XWFEATURE_RELAY || defined COMMS_HEARTBEAT
    } else if ( why == TIMER_COMMS ) {
        now += (secsFromNow * SysTicksPerSecond());
#endif
#ifdef XWFEATURE_BLUETOOTH
    } else if ( why == TIMER_ACL_BACKOFF ) {
        now += (secsFromNow * SysTicksPerSecond());
#endif
    } else {
        XP_ASSERT( 0 );
    }

    XP_ASSERT( why < VSIZE(globals->timerProcs) );
    globals->timerProcs[why] = proc;
    globals->timerClosures[why] = closure;
    globals->timerFireAt[why] = now;

    /* Post an event to force us back out of EvtGetEvent.  Required if this
     * is called from inside some BT callback. */
    postEmptyEvent( noopEvent );
} /* palm_util_setTimer */

static void
palm_util_clearTimer( XW_UtilCtxt* uc, XWTimerReason why )
{
    PalmAppGlobals* globals = (PalmAppGlobals*)uc->closure;
    globals->timerProcs[why] = NULL;
}

static XP_Bool
palm_util_altKeyDown( XW_UtilCtxt* XP_UNUSED(uc) )
{
    XP_LOGF( "%s unimplemented", __func__ );
    return XP_FALSE;
}

static void 
palm_util_requestTime( XW_UtilCtxt* uc )
{
    PalmAppGlobals* globals = (PalmAppGlobals*)uc->closure;
    globals->timeRequested = true;
} /* palm_util_requestTime */

static XP_U32
palm_util_getCurSeconds( XW_UtilCtxt* XP_UNUSED(uc) )
{
    return TimGetSeconds();
} /* palm_util_getCurSeconds */

static DictionaryCtxt*
palm_util_makeEmptyDict( XW_UtilCtxt* uc )
{
    PalmAppGlobals* globals = (PalmAppGlobals*)uc->closure;
    DictionaryCtxt* result = palm_dictionary_make( MPPARM(uc->mpool) 
                                                   globals, NULL, NULL );
    XP_ASSERT( !!result );
    return result;
} /* palm_util_makeEmptyDict */

#ifndef XWFEATURE_STANDALONE_ONLY
static XWStreamCtxt* 
palm_util_makeStreamFromAddr( XW_UtilCtxt* uc, XP_PlayerAddr channelNo )
{
    XWStreamCtxt* stream;
    PalmAppGlobals* globals = (PalmAppGlobals*)uc->closure;

    XP_ASSERT( !!globals->game.comms ); /* shouldn't be making stream in case
                                           where can't send -- or should I
                                           just be passing a null on-close
                                           function? */
    XP_LOGF( "making stream for channel %d", channelNo );
    stream = makeSimpleStream( globals, palm_send_on_close );
    stream_setAddress( stream, channelNo );
    return stream;
} /* palm_util_makeStreamFromAddr */
#endif

static void
palm_send_on_close( XWStreamCtxt* stream, void* closure )
{
    PalmAppGlobals* globals = (PalmAppGlobals*)closure;

    XP_ASSERT( !!globals->game.comms );
    comms_send( globals->game.comms, stream );
} /* palm_send_on_close */

#ifdef XWFEATURE_BLUETOOTH
static void
handleUserBTCancel( PalmAppGlobals* globals )
{
    XP_ASSERT( !globals->userCancelledBT );
    globals->userCancelledBT = XP_TRUE;
    userErrorFromStrId( globals, STR_BT_NOINIT );
}
#endif

static XP_S16
palm_send( const XP_U8* buf, XP_U16 len, 
           const CommsAddrRec* addr, void* closure )
{
    PalmAppGlobals* globals = (PalmAppGlobals*)closure;
    XP_S16 result = 0;

    XP_ASSERT( !!globals->game.comms );

    switch( comms_getConType( globals->game.comms ) ) {
#ifdef XWFEATURE_IR
    case COMMS_CONN_IR:
        result = palm_ir_send( buf, len, globals );
        break;
#endif
#ifdef XWFEATURE_RELAY
    case COMMS_CONN_RELAY:
        result = palm_ip_send( buf, len, addr, globals );
        globals->lastSendGood = result != -1;
        break;
#endif
#ifdef XWFEATURE_BLUETOOTH
    case COMMS_CONN_BT:
        if ( !!globals->mainForm && !globals->userCancelledBT ) {
            XP_Bool userCancelled;
            result = palm_bt_send( buf, len, addr, globals, &userCancelled );
            if ( userCancelled ) {
                handleUserBTCancel( globals );
            }
        }
        break;
#endif
    default:
        XP_ASSERT(0);
    }
    return result;
} /* palm_send */

#ifdef XWFEATURE_RELAY
static void
palm_relayStatus( void* closure, CommsRelayState newState )
{
    PalmAppGlobals* globals = (PalmAppGlobals*)closure;
    globals->netState.relayState = newState;
    showConnState( globals );
}
#endif

#ifdef COMMS_HEARTBEAT
static void
palm_reset( void* closure )
{
    PalmAppGlobals* globals = (PalmAppGlobals*)closure;
    XP_ASSERT( !!globals->game.comms );

    switch( comms_getConType( globals->game.comms ) ) {
#ifdef XWFEATURE_BLUETOOTH
    case COMMS_CONN_BT:
        palm_bt_reset( globals );
        break;
#endif
    default:
        XP_ASSERT(0);
        break;
    }
}
#endif

void
checkAndDeliver( PalmAppGlobals* globals, const CommsAddrRec* addr, 
                 XWStreamCtxt* instream, CommsConnType conType )
{
    /* For now we'll just drop incoming packets on transports not the same as
       the current game's.  We *could* however alert the user, or even
       volunteer to switch e.g. from BT to IR as two passengers board a
       plane.  That'd require significant changes. */
    CommsCtxt* comms = globals->game.comms;
    if ( !!comms && (conType == comms_getConType( comms )) ) {
        if ( comms_checkIncomingStream( comms, instream, addr ) ) {
            (void)server_receiveMessage( globals->game.server, instream );
            globals->msgReceivedDraw = true;
        }
        palm_util_requestTime( &globals->util );
    }
    stream_destroy( instream );
} /* checkAndDeliver */

static const XP_UCHAR* 
palm_util_getUserString( XW_UtilCtxt* uc, XP_U16 stringCode )
{
    PalmAppGlobals* globals = (PalmAppGlobals*)uc->closure;    
    const XP_UCHAR* str = getResString( globals, stringCode );
    return str;
} /* palm_util_getUserString */

static void
formatBadWords( BadWordInfo* bwi, char buf[] )
{
    XP_U16 i;

    for ( i = 0, buf[0] = '\0'; ; ) {
        char wordBuf[18];
        StrPrintF( wordBuf, "\"%s\"", bwi->words[i] );
        StrCat( buf, wordBuf );
        if ( ++i == bwi->nWords ) {
            break;
        }
        StrCat( buf, ", " );
    }
} /* formatBadWords */

static XP_Bool
palm_util_warnIllegalWord( XW_UtilCtxt* uc, BadWordInfo* bwi, 
                           XP_U16 XP_UNUSED(turn), 
                           XP_Bool turnLost )
{
    XP_Bool result = XP_TRUE;
    PalmAppGlobals* globals = (PalmAppGlobals*)uc->closure;

    const XP_UCHAR* resStr = getResString( globals, 
                                        turnLost? STR_PHONY_REJECTED : STR_ILLEGAL_WORD );
    if ( turnLost ) {
        keySafeCustomAlert( globals, resStr );
    } else {
        char wordsBuf[150];
        XP_UCHAR buf[200];
        formatBadWords( bwi, wordsBuf );
        StrPrintF( (char*)buf, (const char*)resStr, wordsBuf );
        result = palmask( globals, buf, NULL, -1 );
    }

    return result;
} /* palm_util_warnIllegalWord */

static void
palm_util_remSelected(XW_UtilCtxt* uc)
{
    PalmAppGlobals* globals = (PalmAppGlobals*)uc->closure;
    showRemaining( globals );
}

#if defined XWFEATURE_BLUETOOTH || defined XWFEATURE_RELAY
static void
palm_util_addrChange( XW_UtilCtxt* uc, 
                      const CommsAddrRec* XP_UNUSED_RELAY(oldAddr),
                      const CommsAddrRec* newAddr )
{
    PalmAppGlobals* globals = (PalmAppGlobals*)uc->closure;

# ifdef XWFEATURE_BLUETOOTH
    XP_Bool isBT = COMMS_CONN_BT == newAddr->conType;
    if ( !isBT ) {
        XP_ASSERT( !!globals->mainForm );
        palm_bt_close( globals );
        showConnState( globals );
    }
# endif

    if ( 0 ) {
# ifdef XWFEATURE_RELAY
    } else if ( COMMS_CONN_RELAY == newAddr->conType ) {
        ip_addr_change( globals, oldAddr, newAddr );
        showConnState( globals );
# endif
# ifdef XWFEATURE_BLUETOOTH
    } else if ( isBT && !globals->userCancelledBT ) {
        XP_Bool userCancelled;
        XP_ASSERT( !!globals->mainForm );
        if ( !palm_bt_init( globals, &userCancelled ) ) {
            if ( userCancelled ) {
                handleUserBTCancel( globals );
            }
        }
# endif
    }
} /* palm_util_addrChange */
#endif /* #if defined XWFEATURE_BLUETOOTH || defined XWFEATURE_RELAY */

#ifdef XWFEATURE_SEARCHLIMIT
static XP_Bool
palm_util_getTraySearchLimits( XW_UtilCtxt* XP_UNUSED(uc),
                               XP_U16* min, XP_U16* max )
{
    return doHintConfig( min, max );
} /* palm_util_getTraySearchLimits */
#endif

#ifdef SHOW_PROGRESS
static void
palm_util_engineStarting( XW_UtilCtxt* uc, XP_U16 nBlanks )
{
    PalmAppGlobals* globals = (PalmAppGlobals*)uc->closure;

    if ( globals->gState.showProgress 
#ifdef XW_TARGET_PNO
         && ( nBlanks > 0 )
#endif
         ) {
        RectangleType* bounds = &globals->progress.boundsRect;

        WinEraseRectangle( bounds, 0 );
        WinDrawRectangleFrame( rectangleFrame, bounds );
        
        globals->progress.curLine = 0;
    } else {
        globals->progress.curLine = -1;
    }
} /* palm_util_engineStarting */

static void
palm_util_engineStopping( XW_UtilCtxt* uc )
{
    PalmAppGlobals* globals = (PalmAppGlobals*)uc->closure;
    if ( globals->gState.showProgress && globals->progress.curLine >= 0 ) {

        WinEraseRectangle( &globals->progress.boundsRect, 0 );
        WinEraseRectangleFrame( rectangleFrame, 
                                &globals->progress.boundsRect );

        if ( globals->needsScrollbar ) { 
            SclDrawScrollBar( getActiveObjectPtr( XW_MAIN_SCROLLBAR_ID ) );
        }
    }
} /* palm_util_engineStopping */
#endif
