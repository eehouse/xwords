/* -*-mode: C; fill-column: 77; c-basic-offset: 4; -*- */
/* 
 * Copyright 1999 - 2002 by Eric House (fixin@peak.org).  All rights reserved.
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
#ifdef FEATURE_HIGHRES
# include <FileStream.h>
# include <SonyCLIE.h>
#endif

#include "comtypes.h"
#include "comms.h"

#include "xwords4defines.h"
#include "palmmain.h"
#include "newgame.h"
#include "dictui.h"
#include "dictlist.h"
#include "palmutil.h"
#include "palmdict.h"
#include "palmsavg.h"
#include "memstream.h"
#include "strutils.h"
#include "palmir.h"
#include "xwcolors.h"
#include "prefsdlg.h"
#include "connsdlg.h"
#include "gameutil.h"
#include "dictui.h"
#include "LocalizedStrIncludes.h"

#include "callback.h"
#include "pace_man.h"           /* for crash() macro */

#ifdef SUPPORT_SONY_JOGDIAL
#include "SonyChars.h"
#endif

#define TIMER_OFF 0L
#define PALM_TIMER_DELAY 25

#ifdef IR_SUPPORT
# ifndef IR_EXCHMGR
/* These are globals!  But the ptrs must remain valid until they're removed
   from the IR lib, and is seems stupid to alloc them on the heap when they
   have to live in the code anyway */
UInt8 deviceInfo[] = { IR_HINT_PDA, 
                       IR_CHAR_ASCII,
                       'X','W','O','R','D','S','4' };
UInt8 deviceName[] = { 
    IAS_ATTRIB_USER_STRING, IR_CHAR_ASCII, 
    7,'X','W','O','R','D','S','4'};

UInt8 xwordsIRResult[] = { 
    0x01, /* Type for Integer is 1 */ 
    0x00,0x00,0x00,0x02 /* Assumed Lsap */ 
}; 

/* IrDemo attribute */ 
const IrIasAttribute xwordsIRAttribs = { 
    (UInt8*) "IrDA:IrLMP:LsapSel",18, 0,
    (UInt8*)xwordsIRResult, sizeof(xwordsIRResult)
}; 

static IrIasObject xwordsIRObject = { 
    (UInt8*)"Xwords4", 7, 1, 
    NULL
    /*     (IrIasAttribute*)&irdemoAttribs  */
}; 
# endif
# else
# define ir_setup(g)
#endif

/*-------------------------------- defines and consts-----------------------*/
/*  #define COLORCHANGE_THRESHOLD 300 */

/*-------------------------------- prototypes ------------------------------*/
static XP_Bool startApplication( PalmAppGlobals** globalsP );
static void eventLoop( PalmAppGlobals* globals );
static void stopApplication( PalmAppGlobals* globals );
static Boolean applicationHandleEvent( PalmAppGlobals* globals, 
                                       EventPtr event );
static Boolean mainViewHandleEvent( EventPtr event );

static UInt16 romVersion( void );
static Boolean handleHintRequest( PalmAppGlobals* globals );

/* callbacks */
static VTableMgr* palm_util_getVTManager( XW_UtilCtxt* uc );
static void palm_util_userError( XW_UtilCtxt* uc, UtilErrID id );
static XP_Bool palm_util_userQuery( XW_UtilCtxt* uc, UtilQueryID id,
                                    XWStreamCtxt* stream );
static XWBonusType palm_util_getSquareBonus( XW_UtilCtxt* uc, 
                                             ModelCtxt* model,
                                             XP_U16 col, XP_U16 row );
static XP_S16 palm_util_userPickTile( XW_UtilCtxt* uc, PickInfo* pi,
                                      XP_U16 playerNum, XP_UCHAR4* texts, 
                                      XP_U16 nTiles );
static XP_Bool palm_util_askPassword( XW_UtilCtxt* uc, const XP_UCHAR* name, 
                                      XP_UCHAR* buf, XP_U16* len );
static void palm_util_trayHiddenChange( XW_UtilCtxt* uc, 
                                        XW_TrayVisState newState );
static void palm_util_yOffsetChange( XW_UtilCtxt* uc, XP_U16 oldOffset, 
                                     XP_U16 newOffset );
static void palm_util_notifyGameOver( XW_UtilCtxt* uc );
static XP_Bool palm_util_hiliteCell( XW_UtilCtxt* uc, XP_U16 col, 
                                     XP_U16 row );
static XP_Bool palm_util_engineProgressCallback( XW_UtilCtxt* uc );
static void palm_util_setTimer( XW_UtilCtxt* uc, XWTimerReason why );
static XP_U32 palm_util_getCurSeconds( XW_UtilCtxt* uc );
static DictionaryCtxt* palm_util_makeEmptyDict( XW_UtilCtxt* uc );
#ifndef XWFEATURE_STANDALONE_ONLY
static XWStreamCtxt* palm_util_makeStreamFromAddr( XW_UtilCtxt* uc, 
                                                   XP_U16 channelNo );
#ifdef BEYOND_IR
static void palm_util_listenPortChange( XW_UtilCtxt* uc, XP_U16 newPort );
#endif
#endif
static XP_UCHAR* palm_util_getUserString( XW_UtilCtxt* uc, XP_U16 stringCode );
static XP_Bool palm_util_warnIllegalWord( XW_UtilCtxt* uc, BadWordInfo* bwi, 
                                          XP_U16 turn, XP_Bool turnLost );
#ifdef XWFEATURE_SEARCHLIMIT
static XP_Bool palm_util_getTraySearchLimits( XW_UtilCtxt* uc, XP_U16* min, 
                                              XP_U16* max );
#endif
static void userErrorFromStrId( PalmAppGlobals* globals, XP_U16 strID );
static Boolean askFromStream( PalmAppGlobals* globals, XWStreamCtxt* stream, 
                              XP_S16 titleID, Boolean closeAndDestroy );
static void displayFinalScores( PalmAppGlobals* globals );
static void updateScrollbar( PalmAppGlobals* globals, Int16 newValue );
static void askStartNewGame( PalmAppGlobals* globals );
static void palmSetCtrlsForTray( PalmAppGlobals* globals );
static void drawFormButtons( PalmAppGlobals* globals );
static MemHandle findXWPrefsRsrc( PalmAppGlobals* globals, UInt32 resType, 
                                  UInt16 resID );
#ifdef SHOW_PROGRESS
static void palm_util_engineStarting( XW_UtilCtxt* uc );
static void palm_util_engineStopping( XW_UtilCtxt* uc );
#endif
static void initAndStartBoard( PalmAppGlobals* globals, XP_Bool newGame );

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
        if ( ((launchFlags & sysAppLaunchFlagNewGlobals) != 0)
             && startApplication( &globals ) ) {
            XP_ASSERT( (launchFlags & sysAppLaunchFlagNewGlobals) != 0 );
            // Initialize the application's global variables and database.
            eventLoop( globals );
        }
        stopApplication( globals );

#ifdef IR_EXCHMGR
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

    FtrGet(sysFtrCreator, sysFtrNumROMVersion, &dwOSVer );
    /* should turn 3 and 5 into 35 */
    return (sysGetROMVerMajor(dwOSVer)*10) + sysGetROMVerMinor(dwOSVer);
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

#ifdef FEATURE_HIGHRES
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
    }

    globals->width = width;
    globals->height = height;
} /* getSizes */
#else
# define getSizes(g)
#endif

#ifdef FEATURE_HIGHRES
static void
locateTrayButtons( PalmAppGlobals* globals, XP_U16 trayTop, XP_U16 trayHt )
{
    if ( FrmGetActiveForm() != NULL ) {
        RectangleType rect;
        XP_S16 diff;

        getObjectBounds( XW_MAIN_HIDE_BUTTON_ID, &rect );
        diff = trayTop - rect.topLeft.y;

        if ( diff != 0 ) {
            XP_U16 i;
            XP_U16 ids[] = {XW_MAIN_SHOWTRAY_BUTTON_ID,
                            XW_MAIN_HIDE_BUTTON_ID,
                            XW_MAIN_DONE_BUTTON_ID,
                            XW_MAIN_TRADE_BUTTON_ID,
                            XW_MAIN_JUGGLE_BUTTON_ID
            };

            for ( i = 0; i < sizeof(ids)/sizeof(ids[0]); ++i ) {
                getObjectBounds( ids[i], &rect );
                rect.topLeft.y += diff;
                setObjectBounds( ids[i], &rect );
            }
        }
    }
} /* locateTrayButtons */
#else
# define locateTrayButtons(g,t,h)
#endif

static XP_Bool
positionBoard( PalmAppGlobals* globals )
{
#ifdef FEATURE_HIGHRES
    XP_U16 bWidth = globals->width;
    XP_U16 bHeight = globals->height;
#else
# define  bWidth  160
# define  bHeight 160
#endif
    XP_Bool canDouble = bWidth >= 320 && bHeight >= 320;
    XP_Bool erase = XP_FALSE;
    XP_Bool isLefty = globals->isLefty;
    XP_U16 nCols, leftEdge;
    XP_U16 scale = PALM_BOARD_SCALE;
    XP_U16 boardHeight, trayTop, trayScaleV, trayScaleH;
    XP_U16 boardTop, scoreTop, scoreLeft, scoreWidth, scoreHeight;
    XP_U16 timerWidth, timerLeft;
    XP_U16 freeSpace;
    XP_Bool showGrid = globals->gState.showGrid;
    XP_U16 doubler = canDouble? 2:1;
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

    freeSpace = ((PALM_MAX_ROWS-nCols)/2) * scale;
    if ( isLefty ) {
        leftEdge = bWidth - (nCols * scale) - freeSpace - 1;
    } else {
        leftEdge = PALM_BOARD_LEFT_RH + freeSpace;
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
                       timerWidth, PALM_TIMER_HEIGHT );

    if ( showGrid ) {
        boardTop = PALM_BOARD_TOP;
        scoreLeft = PALM_SCORE_LEFT;
        scoreTop = PALM_SCORE_TOP;
        scoreWidth = (bWidth/doubler) - PALM_SCORE_LEFT - timerWidth;
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
    board_setScale( globals->game.board, scale, scale );

    board_setScoreboardLoc( globals->game.board, scoreLeft, scoreTop,
                            scoreWidth, scoreHeight, showGrid );

    board_setShowColors( globals->game.board, globals->gState.showColors );
    board_setYOffset( globals->game.board, 0, XP_FALSE /* why bother */ );

    /* figure location for the tray.  If possible, make it smaller than the
       ideal to avoid using a scrollbar.  Also, note at this point whether a
       scrollbar will be required. */
    globals->needsScrollbar = false; /* default */
    boardHeight = scale * nCols;
    trayTop = boardHeight + boardTop + 1;
    if ( trayTop < PALM_TRAY_TOP ) { 
        trayTop = PALM_TRAY_TOP;/* we want it this low even if not
                                   necessary */
    } else if ( bHeight >= 450) {
        ++trayTop;              /* just for grins */
         /* hack: leave it */
    } else {
        while ( trayTop > (PALM_TRAY_TOP_MAX*doubler) ) {
            trayTop -= scale;
            globals->needsScrollbar = true;
        }
    }
    trayScaleH = PALM_TRAY_SCALEH * doubler;
    trayScaleV = bHeight - trayTop;
    if ( trayScaleV > trayScaleH ) {
        trayScaleV = trayScaleH;
    }
    board_setTrayLoc( globals->game.board, 
                      (isLefty? PALM_TRAY_LEFT_LH:PALM_TRAY_LEFT_RH) * doubler,
                      trayTop,
                      trayScaleH, trayScaleV,
                      PALM_DIVIDER_WIDTH * doubler );

    board_prefsChanged( globals->game.board, &globals->gState.cp );

    locateTrayButtons( globals, trayTop/doubler, trayScaleV );

#ifdef SHOW_PROGRESS
    if ( showGrid ) {
        getObjectBounds( XW_MAIN_SCROLLBAR_ID, &bounds );

        bounds.topLeft.x += doubler;
        bounds.extent.x -= 2 * doubler;
    } else {
        bounds.topLeft.y = (PALM_TIMER_HEIGHT + 2) * doubler;
        bounds.topLeft.x = (globals->isLefty? FLIP_BUTTON_WIDTH+3:
            PALM_GRIDLESS_SCORE_LEFT+2) * doubler;

        bounds.extent.x = (RECOMMENDED_SBAR_WIDTH + 2) * doubler;
        bounds.extent.y = (PALM_GRIDLESS_SCORE_TOP - bounds.topLeft.y - 2) * doubler;
    }
    globals->progress.boundsRect = bounds;
#endif

    updateScrollbar( globals, globals->scrollValue ); /* changing visibility? */
    palmSetCtrlsForTray( globals );
    drawFormButtons( globals );

    return erase;
} /* positionBoard */

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

            recStream = mem_stream_make( MEMPOOL globals->vtMgr, globals,
                                         0, NULL );
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
       thing.  When the version changes get rid of this bit. */
    (void)stream_getBits( stream, 1 ); 
} /* loadGamePrefs */

static void
saveGamePrefs( /*PalmAppGlobals* globals, */XWStreamCtxt* stream )
{
    stream_putBits( stream, 1, 0 );
} /* saveGamePrefs */

static Boolean
loadCurrentGame( PalmAppGlobals* globals, XP_U16 gIndex,
                 XWGame* game, CurGameInfo* ginfo )
{
    XP_Bool hasDict;
    XWStreamCtxt* recStream;
    Boolean success = false;
    DictionaryCtxt* dict;

    recStream = gameRecordToStream( globals, gIndex );

    /* now read everything out of the stream */
    XP_ASSERT(!!recStream);
    if ( !!recStream ) {
        char ignore[MAX_GAMENAME_LENGTH];

        /* skip the name */
        stream_getBytes( recStream, ignore, MAX_GAMENAME_LENGTH );

        loadGamePrefs( /*globals, */recStream );

        hasDict = stream_getU8( recStream );
        if ( hasDict ) {
            XP_UCHAR* name = stringFromStream( MPPARM(globals->mpool)
                                               recStream );
            dict = palm_dictionary_make( MPPARM(globals->mpool) name, 
                                         globals->dictList );
            success = dict != NULL;

            if ( !success ) {
                /* I don't know why having this here crashes later... */
/*                 userErrorFromStrId( globals, STR_DICT_NOT_FOUND ); */
                XP_FREE( globals->mpool, name );
                beep();
            }
        } else {
            dict = NULL;
            success = true;
        }

        if ( success ) {
            game_makeFromStream( MEMPOOL recStream, game, ginfo, dict,
                                 &globals->util, globals->draw, 
                                 &globals->gState.cp, palm_send, globals );
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
    vtable->m_util_requestTime = palm_util_requestTime;
    vtable->m_util_getCurSeconds = palm_util_getCurSeconds;
    vtable->m_util_makeEmptyDict = palm_util_makeEmptyDict;
#ifndef XWFEATURE_STANDALONE_ONLY
    vtable->m_util_makeStreamFromAddr = palm_util_makeStreamFromAddr;
#ifdef BEYOND_IR
    vtable->m_util_listenPortChange = palm_util_listenPortChange;
#endif
#endif
    vtable->m_util_getUserString = palm_util_getUserString;
    vtable->m_util_warnIllegalWord = palm_util_warnIllegalWord;
#ifdef XWFEATURE_SEARCHLIMIT
    vtable->m_util_getTraySearchLimits = palm_util_getTraySearchLimits;
#endif
#ifdef SHOW_PROGRESS
    vtable->m_util_engineStarting = palm_util_engineStarting;
    vtable->m_util_engineStopping = palm_util_engineStopping;
#endif
} /* initUtilFuncs */

#ifdef IR_SUPPORT
#ifndef IR_EXCHMGR
/* Make the IR library available if possible.  Or should this be put off
 * until we know we're not playing stand-alone? 
 */
static void
ir_setup( PalmAppGlobals* globals )
{
    Err err;
    UInt16 refNum;

    if ( globals->irLibRefNum == 0 ) { /* been inited before? */

        err = SysLibFind( irLibName, &refNum ); 
        XP_ASSERT( !err );

        if ( err == 0 ) {		/* we have the library */
            err = IrOpen( refNum, irOpenOptSpeed9600 );
            XP_ASSERT( !err );

            if ( err == 0 ) {
                /* 	    IrSetConTypeLMP( &globals->irC_in.irCon ); */
                IrSetConTypeLMP( &globals->irC_out.irCon );

                /* 	    globals->irC_in.globals = globals; */
                /* 	    err = IrBind( refNum, (IrConnect*)&globals->irC_in,  */
                /* 			  ir_callback_in );  */
                /* 	    XP_ASSERT( !err ); */

                globals->irC_out.globals = globals;
                err = IrBind( refNum, &globals->irC_out.irCon, 
                              ir_callback_out);
                XP_ASSERT( !err );

                if ( err == 0 ) {

                    XP_DEBUGF( "con->lLsap == %d", 
                               globals->irC_out.irCon.lLsap );

                    if (IR_STATUS_SUCCESS ==
                        IrSetDeviceInfo( refNum, deviceInfo,
                                         sizeof(deviceInfo) ) ) { 
#if 1

                        xwordsIRObject.attribs =
                            (IrIasAttribute*)&xwordsIRAttribs;

                        IrIAS_SetDeviceName( refNum, 
                                             deviceName,
                                             sizeof(deviceName)); 
                        IrIAS_Add( refNum, &xwordsIRObject ); 
#endif
                        globals->irLibRefNum = refNum;
                    } else {
                        IrUnbind( refNum, &globals->irC_out.irCon ); 
                        IrClose( refNum ); 
                    }
                } else {
                    IrClose( refNum ); 
                }
            }
        }
    }

    ir_cleanup( globals );
} /* ir_setup */
# endif
#endif

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

    MemHandleUnlock( colorH );
} /* loadColorsFromRsrc */
#endif

static void
palmInitPrefs( PalmAppGlobals* globals )
{
    globals->gState.showGrid = true;
    globals->gState.versionNum = CUR_PREFS_VERS;
    globals->gState.cp.showBoardArrow = XP_TRUE;

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
    Boolean beenThere = false;

    for ( ; ; ) {
        XP_ASSERT( !!globals->boardDBP );
        index = DmFindResource( globals->boardDBP, resType, resID, NULL );

        if ( index == -1 ) {	/* not found */
            MemHandle builtinH; 
            MemHandle newH;
            UInt32 size;

            if ( beenThere ) {
                return NULL;
            }

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

            beenThere = true;

            continue;
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

XP_UCHAR*
getResString( PalmAppGlobals* globals, XP_U16 strID )
{
    XP_ASSERT( !!globals->stringsResPtr );
    XP_ASSERT( strID < MemPtrSize( globals->stringsResPtr ) );
    XP_ASSERT( (strID == 0) || (globals->stringsResPtr[strID-1] == '\0') );
    XP_ASSERT( strID < STR_LAST_STRING );
    return &globals->stringsResPtr[strID];
} /* getResString */

static Err
volChangeEventProc( SysNotifyParamType* notifyParamsP )
{
#ifndef REALLY_HANDLE_MEDIA
    EventType eventToPost;
#endif

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

#ifdef FEATURE_HIGHRES
    if ( notifyParamsP->notifyType == sysNotifyDisplayChangeEvent ) {
        eventToPost.eType = doResizeWinEvent;
        EvtAddEventToQueue( &eventToPost );
        return errNone;
    }
#endif
    /* for now, just blow outta here!  Force the app to rebuild
       datastructures when it's relaunched.  This is a hack but I like
       it. :-) */
#ifndef REALLY_HANDLE_MEDIA
    eventToPost.eType = appStopEvent;
    EvtAddEventToQueue( &eventToPost );
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
#ifdef FEATURE_HIGHRES                                 
                                 , sysNotifyDisplayChangeEvent
#endif
        };


        for ( i = 0; i < sizeof(notifyTypes) / sizeof(notifyTypes[0]); ++i ) {
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

#ifdef FEATURE_HIGHRES
/* temp workarounds for some sony include file trouble */
extern Err SilkLibEnableResizeFoo(UInt16 refNum)
				SILK_LIB_TRAP(sysLibTrapCustom+1);
extern Err VskSetStateFoo(UInt16 refNum, UInt16 stateType, UInt16 state)
				SILK_LIB_TRAP(sysLibTrapCustom+3+3);
static void
initHighResGlobals( PalmAppGlobals* globals )
{
    Err err;
    XP_U32 vers;

    err = FtrGet( sysFtrCreator, sysFtrNumWinVersion, &vers );
    globals->hasHiRes = ( err == errNone && vers >= 4 );

    XP_LOGF( "hasHiRes = %d", globals->hasHiRes );

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
} /* initHighResGlobals */

static void
uninitHighResGlobals( PalmAppGlobals* globals )
{
    if ( globals->hasHiRes && globals->sonyLibRef != 0 ) {
        if ( globals->doVSK ) {
            VskClose( globals->sonyLibRef );
        } else {
            SilkLibClose( globals->sonyLibRef );
        }
    }
} /* uninitHighResGlobals */
#else
# define initHighResGlobals(g)
# define uninitHighResGlobals(g)
#endif

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
    MPSLOT;

#if defined FOR_GREMLINS || defined FEATURE_PNOAND68K || defined XW_TARGET_PNO
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

    initHighResGlobals( globals );
    getSizes( globals );

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

    doCallbackReg( globals, XP_TRUE );

    initUtilFuncs( globals );

    offerConvertOldDicts( globals );

    globals->dictList = DictListMake( MPPARM_NOCOMMA(globals->mpool) );
    if ( DictListCount( globals->dictList ) == 0 ) {
        userErrorFromStrId( globals, STR_NO_DICT_INSTALLED );
        return XP_FALSE;
    }

    prefSize = sizeof( prefs );
    prefsFound = PrefGetAppPreferences( AppType, PrefID, &prefs, &prefSize, 
                                        true) == VERSION_NUM;
    if ( prefsFound ) {
        
        prefs.versionNum = XP_NTOHS( prefs.versionNum );
        prefs.curGameIndex = XP_NTOHS( prefs.curGameIndex );

        if ( (prefSize == sizeof(prefs)) 
             && (prefs.versionNum == CUR_PREFS_VERS) ) {
            /* all's well */
        } else if ( prefs.versionNum < CUR_PREFS_VERS ) {
            /* Init the new guys.  Later this may get more complex!! */
            prefs.cp.showBoardArrow = XP_TRUE;
            prefs.cp.showRobotScores = XP_FALSE;
        } else {
            prefsFound = XP_FALSE;
        }

        if ( prefsFound ) {
            MemMove( &globals->gState, &prefs, sizeof(prefs) );
        }
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
        EventType eventToPost;
        eventToPost.eType = loadGameEvent;
        EvtAddEventToQueue( &eventToPost );
    } else {
        DictListEntry* dlep;
	
        /* if we're here because dict missing, don't re-init all prefs! */
        if ( !prefsFound ) {
            palmInitPrefs( globals );
        } else if ( checkUserName() ) {
            /* increment count so we get a new game rather than replace
               existing one.  We want it still there if somebody puts the
               missing dict back. */
            globals->gState.curGameIndex = countGameRecords( globals );
        }
        globals->isNewGame = true;

        getNthDict( globals->dictList, 0, &dlep );
        globals->gameInfo.dictName = copyString( MPPARM(globals->mpool)
                                                 dlep->baseName );

        game_makeNewGame( MEMPOOL &globals->game, &globals->gameInfo,
                          &globals->util, globals->draw, &globals->gState.cp,
                          palm_send, globals );
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
        XP_UCHAR* dictName;
        char namebuf[MAX_GAMENAME_LENGTH];

/*         if ( server_countNonRobotPlayers() > 1 ) { */
            board_hideTray( globals->game.board ); /* so won't be visible when
                                                      next opened */
/*         } */
        memStream = mem_stream_make( MEMPOOL globals->vtMgr, globals, 0, 
                                     writeToDb );
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
        MPSLOT;

        saveOpenGame( globals );

        FrmCloseAllForms();
	
        uninitResources( globals );

        /* Write the state information -- once we're ready to read it in.
           But skip the save if user cancelled launching the first time. */
        if ( !globals->isFirstLaunch ) {
            XWords4PreferenceType prefs;
            /* temporarily don't save prefs since we crash on opening
               them. */
            XP_MEMCPY( &prefs, &globals->gState, sizeof(prefs) );
            prefs.versionNum = XP_HTONS( prefs.versionNum );
            prefs.curGameIndex = XP_HTONS( prefs.curGameIndex );

            PrefSetAppPreferences( AppType, PrefID, VERSION_NUM, 
                                   &prefs, sizeof(prefs), true );
        }

        if ( !!globals->draw ) {
            palm_drawctxt_destroy( globals->draw );
        }

        game_dispose( &globals->game );
        gi_disposePlayerInfo( MEMPOOL &globals->gameInfo );

#ifdef IR_SUPPORT
# ifndef IR_EXCHMGR
        /* close down the IR library if it's there */
        if ( globals->irLibRefNum != 0 ) {
            if ( IrIsIrLapConnected( globals->irLibRefNum ) ) {
                IrDisconnectIrLap( globals->irLibRefNum );
            }
	    
            IrClose( globals->irLibRefNum );
        }
# endif
#ifdef BEYOND_IR
        palm_ip_close( globals );
#endif
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

#if defined OWNER_HASH || defined NO_REG_REQUIRED
        if ( !!globals->savedGamesState && !globals->isFirstLaunch ) {
            freeSavedGamesData( MPPARM(globals->mpool)
                                globals->savedGamesState );
            XP_FREE( globals->mpool, globals->savedGamesState );
        }
#endif

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
    XP_U32 fireTime;

    if ( 0 ) {
#ifdef BEYOND_IR
    } else if ( socketIsOpen(globals) ) {
/*         we'll do our sleeping in NetLibSelect */
        result = 0;
#endif
    } else if ( globals->timeRequested || globals->hintPending ) {
        result = 0;
#ifdef IR_SUPPORT
# ifndef IR_EXCHMGR
    } else if ( ir_work_exists(globals) ) {
        /* 	XP_DEBUGF( "message pending" ); */
        result = 0;
    } else if ( globals->ir_state == IR_STATE_MESSAGE_RECD ) {
        /* 	XP_DEBUGF( "message recd" ); */
        result = 0;
# endif
#endif
    } else if ( (fireTime = globals->penTimerFireAt) != TIMER_OFF
                || (fireTime = globals->timerTimerFireAt ) != TIMER_OFF ) {
        result = fireTime - TimGetTicks();
        if ( result < 0 ) {
            result = 0;
        }
    } else {
        /* leave it */
    }
    /*     XP_DEBUGF( "figureWaitTicks returning %d", result ); */
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
#ifdef BEYOND_IR
        if ( !!globals->game.comms 
             && (comms_getConType(globals->game.comms) == COMMS_CONN_IP) ) {
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
                     && board_hideTray( globals->game.board ) ) {
                    board_draw( globals->game.board );
                }
            }
        }

        /* Give the system a chance to handle the event. */
        if ( !SysHandleEvent(&event)) {
            UInt16 error;
            if ( !MenuHandleEvent( NULL, &event, &error)) {
                if ( !applicationHandleEvent( globals, &event )) {
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
applicationHandleEvent( PalmAppGlobals* globals, EventPtr event ) 
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
#ifdef BEYOND_IR
        case XW_CONNS_FORM:
            handler = ConnsFormHandleEvent;
            break;
#endif
#if defined OWNER_HASH || defined NO_REG_REQUIRED
        case XW_SAVEDGAMES_DIALOG_ID:
            handler = savedGamesHandleEvent;
            break;
#endif
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

static Boolean
handleNilEvent( PalmAppGlobals* globals, EventPtr event )
{
    Boolean handled = true;

    if ( false ) {
    } else if ( globals->menuIsDown ) {
        /* do nothing */
#ifdef IR_SUPPORT
# ifndef IR_EXCHMGR
    } else if ( ir_work_exists(globals) ) {
        handled = ir_do_work( globals );
        ir_show_status( globals );
        /*     } else if ( globals->ir_state == IR_STATE_MESSAGE_RECD ) { */
        /* 	handled = do_ir_work( globals ); */
# endif
#endif
    } else if ( globals->hintPending ) {
        handled = handleHintRequest( globals );

    } else if ( globals->penTimerFireAt != TIMER_OFF &&
                globals->penTimerFireAt <= TimGetTicks() ) {
        globals->penTimerFireAt = TIMER_OFF;
        board_timerFired( globals->game.board, TIMER_PENDOWN );

    } else if ( globals->timeRequested ) {
        globals->timeRequested = false;
        if ( globals->msgReceivedDraw ) {
            XP_ASSERT ( !!globals->game.board );
            board_draw( globals->game.board );
            globals->msgReceivedDraw = XP_FALSE;
        }
        handled = server_do( globals->game.server ); 

    } else if ( globals->timerTimerFireAt != TIMER_OFF &&
                globals->timerTimerFireAt <= TimGetTicks() ) {
        globals->timerTimerFireAt = TIMER_OFF;
        board_timerFired( globals->game.board, TIMER_TIMERTICK );
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
    XW_TrayVisState curState = board_getTrayVisState( globals->game.board );

    if ( curState != TRAY_HIDDEN ) {
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
    FormPtr form = FrmGetActiveForm();
    UInt16 index = FrmGetObjectIndex( form, ctrlID );
    RectangleType bounds;

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
            board_setYOffset( globals->game.board, 0, XP_TRUE );
        }
    }
} /* palmSetCtrlsForTray */

static Boolean
scrollBoard( PalmAppGlobals* globals, Int16 newValue, Boolean fromBar )
{
    XP_Bool result = XP_FALSE;
    XP_U16 curYOffset;

    XP_ASSERT( !!globals->game.board );
    curYOffset = board_getYOffset( globals->game.board );

    result = curYOffset != newValue;
    if ( result ) {
        result = board_setYOffset( globals->game.board, newValue, XP_FALSE );
    }

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
        XP_UCHAR* dictName = dict_getName( dict );
        if ( !!newDictName && 0 != XP_STRCMP( (const char*)dictName, 
                                              (const char*)newDictName ) ) {
            dict_destroy( dict );
            dict = NULL;
        } else {
            replaceStringIfDifferent( MEMPOOL &globals->gameInfo.dictName,
                                      dictName );
        }
    }

    if ( !dict ) {
        XP_ASSERT( !!newDictName );
        dict = palm_dictionary_make( MPPARM(globals->mpool) 
                                     copyString( MEMPOOL newDictName ),
                                     globals->dictList );
        XP_ASSERT( !!dict );	
        model_setDictionary( globals->game.model, dict );
    }

    if ( newGame ) {
        XP_U32 newGameID = TimGetSeconds();
        game_reset( MEMPOOL &globals->game, &globals->gameInfo,
                    newGameID, &globals->gState.cp, palm_send, globals );
    }

    XP_ASSERT( !!globals->game.board );
    getSizes( globals );
    (void)positionBoard( globals );

#ifdef IR_SUPPORT
# ifndef IR_EXCHMGR
    if ( globals->gameInfo.serverRole == SERVER_STANDALONE ) {
        ir_cleanup( globals );
    } else {
        ir_setup( globals );
    }
# endif

    if ( newGame && globals->gameInfo.serverRole == SERVER_ISCLIENT ) {
        XWStreamCtxt* stream;
        XP_ASSERT( !!globals->game.comms );
        stream = mem_stream_make( MEMPOOL globals->vtMgr, globals, 
                                  CHANNEL_NONE, palm_send_on_close );
        server_initClientConnection( globals->game.server, stream );
    }
#endif

    /* do this before drawing the board.  If it assigns tiles, for example,
       that'll make a difference on the screen. */
    (void)server_do( globals->game.server );

    board_invalAll( globals->game.board );
    board_draw( globals->game.board );

    globals->isNewGame = false;
} /* initAndStartBoard */

#ifdef DEBUG
static void
askOnClose( XWStreamCtxt* stream, void* closure )
{
    PalmAppGlobals* globals = (PalmAppGlobals*)closure;

    askFromStream( globals, stream, -1, false );
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
considerMenuShow( PalmAppGlobals* globals, EventPtr event )
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
    if ( loaded ) {
        game_dispose( &globals->game );
        gi_disposePlayerInfo( MEMPOOL &globals->gameInfo );
        globals->game = tmpGame;
        /* we leaking dictName here? PENDING(ehouse) */
        XP_MEMCPY( &globals->gameInfo, &tmpGInfo, sizeof(globals->gameInfo) );
        globals->gState.curGameIndex = newIndex;
    } else {
        game_dispose( &tmpGame );
        gi_disposePlayerInfo( MEMPOOL &tmpGInfo );
    }

    return loaded;
} /* tryLoadSavedGame */

#ifdef FEATURE_HIGHRES
static XP_U16
hresX( PalmAppGlobals* globals, XP_U16 screenX )
{
    if ( globals->hasHiRes && globals->width >= 320 ) {
        screenX *= 2;
    }
    return screenX;
}

static XP_U16
hresY( PalmAppGlobals* globals, XP_U16 screenY )
{
    if ( globals->hasHiRes && globals->width >= 320 ) {
        screenY *= 2;
    }
    return screenY;
}

static void
hresRect( PalmAppGlobals* globals, RectangleType* r )
{
    if ( globals->hasHiRes && globals->width >= 320 ) {
        r->topLeft.x *= 2;
        r->topLeft.y *= 2;
        r->extent.x *= 2;
        r->extent.y *= 2;
    }
}

#else
# define hresX( g, n ) (n)
# define hresY( g, n ) (n)
# define hresRect( g, r )
#endif

/*****************************************************************************
 *
 ****************************************************************************/
static Boolean
mainViewHandleEvent( EventPtr event )
{
    Boolean handled = true;
    Boolean draw = false;
    Boolean erase;
#if defined CURSOR_MOVEMENT && defined DEBUG
    CursorDirection cursorDir;
    Boolean movePiece;
#endif
    PalmAppGlobals* globals;
    OpenSavedGameData* savedGameData;
    char newName[MAX_GAMENAME_LENGTH];
    XP_U16 prevSize;

    CALLBACK_PROLOGUE();

    globals = getFormRefcon();

    switch ( event->eType ) {

    case nilEvent:
        draw = handled = handleNilEvent( globals, event );
        break;

    case newGameCancelEvent:
        /* If user cancelled the new game dialog that came up the first time
           he launched (i.e. when there's no game to fall back to) then just
           quit.  It's easier than dealing with everything that can go wrong
           in this state. */
        if ( globals->isFirstLaunch ) {
            EventType eventToPost;
            eventToPost.eType = appStopEvent;
            EvtAddEventToQueue( &eventToPost );
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

    case loadGameEvent:
        XP_ASSERT( !!globals->game.server );
        initAndStartBoard( globals, event->eType == newGameOkEvent );
        draw = true;
        XP_ASSERT( !!globals->game.board );
        break;

#ifdef FEATURE_HIGHRES
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

    case winExitEvent:
		if ( event->data.winExit.exitWindow == (WinHandle)FrmGetActiveForm() ) {
			globals->menuIsDown = true;
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
			event->data.winEnter.enterWindow == (WinHandle)FrmGetFirstForm() ) {
            globals->menuIsDown = false;
		}
        break;

    case frmOpenEvent:
        globals->gState.windowAvail = true;
        globals->mainForm = FrmGetActiveForm();
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
        }
        break;

    case penDownEvent:
        globals->penDown = handled;
        draw = board_handlePenDown( globals->game.board, 
                                    hresX(globals, event->screenX), 
                                    hresY(globals, event->screenY), 
                                    0, &handled );
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
                                      hresY( globals, event->screenY ), 
                                      0 );
            handled = draw;     /* this is wrong!!!! */
            globals->penDown = false;

            if ( !handled ) {
                handled = considerMenuShow( globals, event );
            }
        }
        break;

    case menuEvent:
        MenuEraseStatus(0);
        switch ( event->data.menu.itemID ) {

        case XW_TILEVALUES_PULLDOWN_ID: 
            if ( !!globals->game.server ) {
                XWStreamCtxt* stream = mem_stream_make( MEMPOOL 
                                                        globals->vtMgr, 
                                                        globals, 
                                                        CHANNEL_NONE, 
                                                        NULL );
                XP_UCHAR* s = getResString( globals, STR_VALUES_HEADER );
                stream_putBytes( stream, s, XP_STRLEN((const char*)s) );

                server_formatPoolCounts( globals->game.server, stream, 
                                         3 ); /* 3: ncols */

                askFromStream( globals, stream, STR_VALUES_TITLE, true );
            }
            break;

        case XW_HISTORY_PULLDOWN_ID:
            if ( !!globals->game.server ) {
                XP_Bool gameOver = server_getGameIsOver(globals->game.server);
                XWStreamCtxt* stream = mem_stream_make( MEMPOOL 
                                                        globals->vtMgr,
                                                        globals, 
                                                        CHANNEL_NONE, NULL );

                model_writeGameHistory( globals->game.model, stream, 
                                        globals->game.server, gameOver );

                askFromStream( globals, stream, STR_HISTORY_TITLE, true );
            }
            break;

        case XW_NEWGAME_PULLDOWN_ID:
            askStartNewGame( globals );
            break;

        case XW_SAVEDGAMES_PULLDOWN_ID:
            if ( 0 ) {
#if defined OWNER_HASH || defined NO_REG_REQUIRED
            } else if ( checkUserName() ) { 
                saveOpenGame( globals );/* so it can be accurately duped */
                /* save game changes state; reflect on screen before
                   popping up dialog */
                drawChangedBoard( globals );
                FrmPopupForm( XW_SAVEDGAMES_DIALOG_ID );
#endif
#ifndef NO_REG_REQUIRED
            } else {
                userErrorFromStrId( globals, STR_NOT_UNREG_VERS );
#endif
            }
            break;

        case XW_FINISH_PULLDOWN_ID:
            if ( server_getGameIsOver( globals->game.server ) ) {
                displayFinalScores( globals );
            } else if ( palmaskFromStrId(globals, STR_CONFIRM_END_GAME,
                                         -1, -1) ) {
                server_endGame( globals->game.server );
                draw = true;	    
            }
            break;

#ifndef XWFEATURE_STANDALONE_ONLY
        case XW_RESENDIR_PULLDOWN_ID:
            if ( !!globals->game.comms ) {
                (void)comms_resendAll( globals->game.comms );
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

#ifdef FEATURE_PNOAND68K
            /* This probably goes away at ship.... */
        case XW_RUN68K_PULLDOWN_ID:
        case XW_RUNARM_PULLDOWN_ID: {
            UInt32 newVal, val;
            Err err;
            if ( event->data.menu.itemID == XW_RUN68K_PULLDOWN_ID ) {
                newVal = WANTS_68K;
            } else {
                newVal = WANTS_ARM;
            }
            (void)FtrUnregister( APPID, WANTS_ARM_FEATURE );
            err = FtrSet( APPID, WANTS_ARM_FEATURE, newVal );
            XP_ASSERT( err == errNone );
            err = FtrGet( APPID, WANTS_ARM_FEATURE, &val );
            XP_ASSERT( err == errNone );
            XP_ASSERT( val == newVal );
            XP_LOGF( "WANTS_ARM_FEATURE now %ld", val );
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
            palmaskFromStrId(globals, STR_ABOUT_CONTENT, STR_ABOUT_TITLE,-1);
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
        case XW_DEBUGSHOW_PULLDOWN_ID:
            globals->gState.showDebugstrs = true;
            break;
        case XW_DEBUGHIDE_PULLDOWN_ID:
            globals->gState.showDebugstrs = false;
            board_invalAll( globals->game.board );
            draw = true;
            break;

        case XW_DEBUGMEMO_PULLDOWN_ID:
            globals->gState.logToMemo = true;
            break;
        case XW_DEBUGSCREEN_PULLDOWN_ID:
            globals->gState.logToMemo = false;
            break;


# if 0
        case XW_RESET_PULLDOWN_ID: {
            EventType eventToPost;
            eventToPost.eType = appStopEvent;
            EvtAddEventToQueue( &eventToPost );
        }

            globals->resetGame = true;
            break;
# endif

        case XW_NETSTATS_PULLDOWN_ID:
            if ( !!globals->game.comms ) {
                XWStreamCtxt* stream = mem_stream_make( MEMPOOL 
                                                        globals->vtMgr,
                                                        globals, 
                                                        CHANNEL_NONE,
                                                        askOnClose );
                comms_getStats( globals->game.comms, stream );
                stream_destroy( stream );
            }
            break;
#ifdef MEM_DEBUG
        case XW_MEMSTATS_PULLDOWN_ID :
            if ( !!globals->mpool ) {
                XWStreamCtxt* stream = mem_stream_make( MEMPOOL 
                                                        globals->vtMgr,
                                                        globals, 
                                                        CHANNEL_NONE,
                                                        askOnClose );
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

    case keyDownEvent: {
        XP_Key xpkey = XP_KEY_NONE;
        Int16 ch = event->data.keyDown.chr;

        switch ( ch ) {
        case pageUpChr:
            draw = scrollBoard( globals, 0, false );
            break;
        case pageDownChr:
            draw = scrollBoard( globals, 2, false );
            break;
        case backspaceChr:
            xpkey = XP_CURSOR_KEY_DEL;
            break;
        default:
            /* I'm not interested in being dependent on a particular version
               of the OS, (can't manage to link against the intl library
               anyway) and so don't want to use the 3.5-only text tests.  So
               let's give the board two shots at each char, one lower case
               and another upper. */
            if ( ch < 255 && ch > ' ' ) {
                draw = board_handleKey( globals->game.board, ch );
                if ( !draw && ch >= 'a' ) {
                    draw = board_handleKey( globals->game.board, ch - ('a' - 'A') );
                }
            }
        }
        if ( xpkey != XP_KEY_NONE ) {
            draw = board_handleKey( globals->game.board, xpkey );
        }
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
    if ( palmaskFromStrId( globals, STR_ASK_REPLACE_GAME, -1, STR_NO)) {
        /* do nothing; popping up the NEWGAMES dlg will do it -- if not
           cancelled */
        globals->newGameIsNew = XP_FALSE;
    } else if ( checkUserName() ) {
        saveOpenGame( globals );

        drawChangedBoard( globals );

        globals->newGameIsNew = XP_TRUE;
    } else {
        return;
    }
    globals->isNewGame = true;
    globals->isFirstLaunch = false;
    FrmPopupForm( XW_NEWGAMES_FORM );
} /* askStartNewGame */

static void
displayFinalScores( PalmAppGlobals* globals )
{
    XWStreamCtxt* stream;

    stream = mem_stream_make( MEMPOOL globals->vtMgr, globals, 
                              CHANNEL_NONE, NULL );
    server_writeFinalScores( globals->game.server, stream );
    stream_putU8( stream, '\0' );

    askFromStream( globals, stream, STR_FINAL_SCORES_TITLE, true );
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
tryGrowAskToFit( FormPtr form, FieldPtr field, XP_UCHAR* str )
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
    XP_ASSERT( XW_ASK_CANCEL_BUTTON_ID - XW_ASK_OK_BUTTON_ID == 1 );
    for ( i = XW_ASK_OK_BUTTON_ID; i <= XW_ASK_CANCEL_BUTTON_ID; ++i ){
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
        switch ( event->data.keyDown.chr ) {
        case pageUpChr:
            direction = winUp; 
            break;
        case pageDownChr:
            direction = winDown; 
            break;
        default:
            result = false;
        }
        linesToScroll = 3;
        scrollFromButton = true;
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

Boolean
palmaskFromStrId( PalmAppGlobals* globals, XP_U16 strId, XP_S16 titleID, 
                  XP_S16 altButtonID )
{
    XP_UCHAR* message;
    XP_UCHAR* alt;
    message = getResString( globals, strId );
    XP_ASSERT( !!message );
    alt = altButtonID < 0? NULL: getResString( globals, altButtonID );
    return palmask( globals, message, alt, titleID );
} /* palmaskFromStrId */

Boolean
palmask( PalmAppGlobals* globals, XP_UCHAR* str, XP_UCHAR* altButton, 
         XP_S16 titleID )
{
    FormPtr form, prevForm;
    FieldPtr field;
    XP_UCHAR* title;
    UInt16 buttonHit;

    if ( !!globals->game.board ) {
        board_pushTimerSave( globals->game.board );
    }

    title = titleID >= 0? getResString( globals, titleID ): NULL;

    prevForm = FrmGetActiveForm();
    form = FrmInitForm( XW_ASK_FORM_ID );

    FrmSetActiveForm( form );

    if ( title != NULL ) {
        FrmSetTitle( form, (char*)title );
        /* Hack: take advantage of fact that for now only non-queries should
           not have a cancel button. */
        disOrEnable( form, XW_ASK_CANCEL_BUTTON_ID, false );
        centerControl( form, XW_ASK_OK_BUTTON_ID );
    } else if ( !!altButton ) {
        CtlSetLabel( getActiveObjectPtr(XW_ASK_CANCEL_BUTTON_ID), 
                     (const char*)altButton );
    }

    /* if we're running before the first form goes up, globals won't be
       available via the refcon, so don't install handler than requires 'em.
       That or move globals to a Ftr.... */
    if ( !!prevForm ) {
        FrmSetEventHandler( form, handleScrollInAsk );
    }
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

    return buttonHit == XW_ASK_OK_BUTTON_ID;
} /* palmask */

static Boolean
askFromStream( PalmAppGlobals* globals, XWStreamCtxt* stream, XP_S16 titleID,
               Boolean closeAndDestroy )
{
    XP_U16 nBytes = stream_getSize( stream );
    Boolean result;
    XP_UCHAR* buffer;

    XP_ASSERT( nBytes < maxFieldTextLen );

    buffer = MemPtrNew( nBytes + 1 );
    stream_getBytes( stream, buffer, nBytes );
    /* nuke trailing <CR> chars to they don't extend length of field */
    while ( buffer[nBytes-1] == '\n' ) {
        --nBytes;
    }
    buffer[nBytes] = '\0';	/* just to be safe */
    
    result = palmask( globals, buffer, NULL, titleID );
    MemPtrFree( buffer );

    if ( closeAndDestroy ) {
        stream_destroy( stream );
    }

    return result;
} /* askFromStream */

Boolean
askPassword( PalmAppGlobals* globals, const XP_UCHAR* name, Boolean isNew,
             XP_UCHAR* retbuf, XP_U16* len )
{
    Boolean result = false;
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

    FrmSetFocus( form, FrmGetObjectIndex( form, XW_PASSWORD_PASS_FIELD ) );
    field = getActiveObjectPtr( XW_PASSWORD_PASS_FIELD );

    if ( FrmDoDialog( form ) == XW_PASSWORD_OK_BUTTON ) {
        char* enteredPass = FldGetTextPtr( field );
        XP_U16 enteredLen = !enteredPass? 0: StrLen(enteredPass);
        if ( enteredLen < *len ) {
            result = true;
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

static Boolean 
handleKeysInBlank( EventPtr event )
{
    Boolean handled = false;

    if ( event->eType == keyDownEvent ) {
        char ch = event->data.keyDown.chr;

        if ( ch >= 'a' && ch <= 'z' ) {
            ch += 'A' - 'a';
        }
        if ( ch >= 'A' && ch <= 'Z' ) {
            ListPtr lettersList = getActiveObjectPtr( XW_BLANK_LIST_ID );
            XP_U16 nItems;
            XP_U16 i;

            XP_ASSERT( !!lettersList );
            nItems = LstGetNumberOfItems( lettersList );

            for ( i = 0; i < nItems; ++i ) {
                XP_UCHAR* itext = LstGetSelectionText( lettersList, i );

                if ( !!itext && (itext[0] == ch) ) {
                    LstSetSelection( lettersList, i );
                    LstMakeItemVisible( lettersList, i );
                    handled = true;
                    break;
                }
            }
        } else if ( ch == '\n' ) {
            EventType eventToPost;

            eventToPost.eType = ctlSelectEvent;
            eventToPost.data.ctlSelect.controlID = XW_BLANK_OK_BUTTON_ID;
            eventToPost.data.ctlSelect.pControl = 
                getActiveObjectPtr( XW_BLANK_OK_BUTTON_ID );
            EvtAddEventToQueue( &eventToPost );
        }
    }

    return handled;
} /* handleKeysInBlank */

static XP_S16
askBlankValue( PalmAppGlobals* globals, XP_U16 playerNum, PickInfo* pi,
               XP_U16 nTiles, XP_UCHAR4* texts )
{
    FormPtr form, prevForm;
    ListPtr lettersList;
    ListData ld;
    XP_U16 i;
    XP_S16 chosen;
    XP_UCHAR labelBuf[64];
    XP_UCHAR* name;
    XP_UCHAR* labelFmt;
    FieldPtr fld;
    XP_U16 tapped;
#ifdef FEATURE_TRAY_EDIT
    XP_Bool forBlank = pi->why == PICK_FOR_BLANK;
#endif

    initListData( MEMPOOL &ld, nTiles );

    for ( i = 0; i < nTiles; ++i ) {	
        addListTextItem( MEMPOOL &ld, (XP_UCHAR*)texts[i] );
    }

    prevForm = FrmGetActiveForm();
    form = FrmInitForm( XW_BLANK_DIALOG_ID );
    FrmSetActiveForm( form );

#ifdef FEATURE_TRAY_EDIT
    disOrEnable( form, XW_BLANK_PICK_BUTTON_ID, !forBlank );
    disOrEnable( form, XW_BLANK_BACKUP_BUTTON_ID, 
                 !forBlank && pi->nCurTiles > 0 );
#endif

    lettersList = getActiveObjectPtr( XW_BLANK_LIST_ID );
    setListChoices( &ld, lettersList, NULL );

    LstSetSelection( lettersList, 0 );

    name = globals->gameInfo.players[playerNum].name;
    labelFmt = getResString( globals, 
#ifdef FEATURE_TRAY_EDIT
                             !forBlank? STRS_PICK_TILE:
#endif
                             STR_PICK_BLANK );
    XP_SNPRINTF( labelBuf, sizeof(labelBuf), labelFmt, name );

#ifdef FEATURE_TRAY_EDIT
    if ( !forBlank ) {
        XP_U16 lenSoFar;
        XP_U16 i;

        lenSoFar = XP_STRLEN(labelBuf);
        lenSoFar += XP_SNPRINTF( labelBuf + lenSoFar, 
                                 sizeof(labelBuf) - lenSoFar,
                                 " (%d/%d)\nCur", pi->thisPick+1, pi->nTotal );

        for ( i = 0; i < pi->nCurTiles; ++i ) {
            lenSoFar += XP_SNPRINTF( labelBuf+lenSoFar, 
                                     sizeof(labelBuf)-lenSoFar, "%s%s",
                                     i==0?": ":", ", pi->curTiles[i] );
        }
    }
#endif

    fld = getActiveObjectPtr( XW_BLANK_LABEL_FIELD_ID );
    FldSetTextPtr( fld, labelBuf );
    FldRecalculateField( fld, false );

    FrmDrawForm( form );

    FrmSetEventHandler( form, handleKeysInBlank );
    tapped = FrmDoDialog( form );

    if ( 0 ) {
#ifdef FEATURE_TRAY_EDIT
    } else if ( tapped == XW_BLANK_PICK_BUTTON_ID ) {
        chosen = PICKER_PICKALL;
    } else if ( tapped == XW_BLANK_BACKUP_BUTTON_ID ) {
        chosen = PICKER_BACKUP;
#endif
    } else {
        chosen = LstGetSelection( lettersList );
    }

    FrmDeleteForm( form );
    FrmSetActiveForm( prevForm );

    freeListData( MEMPOOL &ld );

    return chosen;
} /* askBlankValue */

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

    default:
        XP_DEBUGF( "errcode=%d", id );
        break;
    }

    XP_ASSERT( strID < STR_LAST_STRING );
    globals = (PalmAppGlobals*)uc->closure;
    userErrorFromStrId( globals, strID );
} /* palm_util_userError */

static void
userErrorFromStrId( PalmAppGlobals* globals, XP_U16 strID )
{
    XP_UCHAR* message = getResString( globals, strID );
    (void)FrmCustomAlert( XW_ERROR_ALERT_ID, (const char*)message, " ", " " );
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

    return (XP_Bool)palmaskFromStrId( globals, strID, -1, -1 );
} /* palm_util_userQuery */

static XWBonusType
palm_util_getSquareBonus( XW_UtilCtxt* uc, ModelCtxt* model,
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
palm_util_userPickTile( XW_UtilCtxt* uc, PickInfo* pi,
                        XP_U16 playerNum, XP_UCHAR4* texts, XP_U16 nTiles )
{
    PalmAppGlobals* globals = (PalmAppGlobals*)uc->closure;
    return askBlankValue( globals, playerNum, pi, nTiles, texts );
} /* palm_util_userPickTile */

static XP_Bool 
palm_util_askPassword( XW_UtilCtxt* uc, const XP_UCHAR* name, XP_UCHAR* buf, 
                       XP_U16* len )
{
    PalmAppGlobals* globals = (PalmAppGlobals*)uc->closure;
    return askPassword( globals, name, false, buf, len );
} /* palm_util_askPassword */

static void 
palm_util_trayHiddenChange( XW_UtilCtxt* uc, XW_TrayVisState newState )
{
    PalmAppGlobals* globals = (PalmAppGlobals*)uc->closure;
    palmSetCtrlsForTray( globals );

    drawFormButtons( globals );
} /* palm_util_trayHiddenChange */

static void 
palm_util_yOffsetChange( XW_UtilCtxt* uc, XP_U16 oldOffset, XP_U16 newOffset )
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
        board_hiliteCellAt( globals->game.board, col, row );
    }
#endif
    
    return !eventPending;
} /* palm_util_hiliteCell */

static XP_Bool
palm_util_engineProgressCallback( XW_UtilCtxt* uc )
{
#ifdef SHOW_PROGRESS
    PalmAppGlobals* globals = (PalmAppGlobals*)uc->closure;
    if ( globals->gState.showProgress ) {
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
palm_util_setTimer( XW_UtilCtxt* uc, XWTimerReason why )
{
    PalmAppGlobals* globals = (PalmAppGlobals*)uc->closure;
    if ( why == TIMER_PENDOWN ) {
        globals->penTimerFireAt = TimGetTicks() + PALM_TIMER_DELAY;
    } else {
        globals->timerTimerFireAt = TimGetTicks() + SysTicksPerSecond();
    }
} /* palm_util_setTimer */

void 
palm_util_requestTime( XW_UtilCtxt* uc )
{
    PalmAppGlobals* globals = (PalmAppGlobals*)uc->closure;
    globals->timeRequested = true;
} /* palm_util_requestTime */

static XP_U32
palm_util_getCurSeconds( XW_UtilCtxt* uc )
{
    return TimGetSeconds();
} /* palm_util_getCurSeconds */

static DictionaryCtxt*
palm_util_makeEmptyDict( XW_UtilCtxt* uc )
{
    DictionaryCtxt* result = palm_dictionary_make( MPPARM(uc->mpool) NULL, NULL );
    XP_ASSERT( !!result );
    return result;
} /* palm_util_makeEmptyDict */

#ifndef XWFEATURE_STANDALONE_ONLY
static XWStreamCtxt* 
palm_util_makeStreamFromAddr( XW_UtilCtxt* uc, XP_U16 channelNo )
{
    XWStreamCtxt* stream;
    PalmAppGlobals* globals = (PalmAppGlobals*)uc->closure;

    XP_ASSERT( !!globals->game.comms ); /* shouldn't be making stream in case
                                           where can't send -- or should I
                                           just be passing a null on-close
                                           function? */
    XP_LOGF( "making stream for channel %d", channelNo );
    stream = mem_stream_make( MEMPOOL globals->vtMgr, globals, 
                              channelNo, palm_send_on_close );
    return stream;
} /* palm_util_makeStreamFromAddr */

#ifdef BEYOND_IR
static void
palm_util_listenPortChange( XW_UtilCtxt* uc, XP_U16 newPort )
{
    PalmAppGlobals* globals = (PalmAppGlobals*)uc->closure;    
    palm_bind_socket( globals, newPort );
} /* palm_util_getListeningPort */
#endif
#endif

static XP_UCHAR* 
palm_util_getUserString( XW_UtilCtxt* uc, XP_U16 stringCode )
{
    PalmAppGlobals* globals = (PalmAppGlobals*)uc->closure;    
    XP_UCHAR* str = getResString( globals, stringCode );
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
palm_util_warnIllegalWord( XW_UtilCtxt* uc, BadWordInfo* bwi, XP_U16 turn, 
                           XP_Bool turnLost )
{
    XP_Bool result = XP_TRUE;
    PalmAppGlobals* globals = (PalmAppGlobals*)uc->closure;
    XP_UCHAR buf[200];
    char wordsBuf[150];
    XP_UCHAR* format = getResString( globals, STR_ILLEGAL_WORD );
    formatBadWords( bwi, wordsBuf );
    StrPrintF( (char*)buf, (const char*)format, wordsBuf );
    if ( turnLost ) {
        (void)FrmCustomAlert( XW_ERROR_ALERT_ID, (const char*)buf, " ", " " );
    } else {
        result = palmask( globals, buf, NULL, -1 );
    }
    return result;
} /* palm_util_warnIllegalWord */

#ifdef XWFEATURE_SEARCHLIMIT
static XP_Bool
palm_util_getTraySearchLimits( XW_UtilCtxt* uc, XP_U16* min, XP_U16* max )
{
    return doHintConfig( min, max );
} /* palm_util_getTraySearchLimits */
#endif

#ifdef SHOW_PROGRESS
static void
palm_util_engineStarting( XW_UtilCtxt* uc )
{
    PalmAppGlobals* globals = (PalmAppGlobals*)uc->closure;
    if ( globals->gState.showProgress ) {
        RectangleType* bounds = &globals->progress.boundsRect;

        WinEraseRectangle( bounds, 0 );
        WinDrawRectangleFrame( rectangleFrame, bounds );

        globals->progress.curLine = 0;
    }
} /* palm_util_engineStarting */

static void
palm_util_engineStopping( XW_UtilCtxt* uc )
{
    PalmAppGlobals* globals = (PalmAppGlobals*)uc->closure;
    if ( globals->gState.showProgress ) {

        WinEraseRectangle( &globals->progress.boundsRect, 0 );
        WinEraseRectangleFrame( rectangleFrame, 
                                &globals->progress.boundsRect );

        if ( globals->needsScrollbar ) { 
            SclDrawScrollBar( getActiveObjectPtr( XW_MAIN_SCROLLBAR_ID ) );
        }
    }
} /* palm_util_engineStopping */
#endif
