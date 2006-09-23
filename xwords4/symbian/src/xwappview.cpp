/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 2005 by Eric House (xwords@eehouse.org).  (based on sample
 * app helloworldbasic "Copyright (c) 2002, Nokia. All rights
 * reserved.")
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

#if defined SERIES_60
# include <w32std.h>
# include <eikinfo.h>
# include "xwords_60.rsg"
#elif defined SERIES_80
# include <cknenv.h>
# include <ckninfo.h>
# include <eikcfdlg.h>
# include <eikfile.rsg>
# include "xwords_80.rsg"
#endif

#include <stringloader.h>
#include <stdlib.h>             // for srand
#include <s32file.h>
#include <eikapp.h>

#include "xwappview.h"
#include "xwappui.h"
#include "xwords.hrh"
#include "comtypes.h"
#include "symdraw.h"
#include "symaskdlg.h"
#include "symdict.h"
#include "symgamdl.h"
#include "symblnk.h"
#include "symgmdlg.h"
#include "symutil.h"
#include "symssock.h"

#include "LocalizedStrIncludes.h"

// Standard construction sequence
CXWordsAppView* CXWordsAppView::NewL(const TRect& aRect, CEikApplication* aApp )
{
    CXWordsAppView* self = CXWordsAppView::NewLC( aRect, aApp );
    CleanupStack::Pop(self);
    return self;
}

CXWordsAppView* CXWordsAppView::NewLC(const TRect& aRect, CEikApplication* aApp )
{
    CXWordsAppView* self = new (ELeave) CXWordsAppView( aApp );
    CleanupStack::PushL(self);
    self->ConstructL(aRect);
    return self;
}

CXWordsAppView::CXWordsAppView( CEikApplication* aApp )
    : iApp( aApp )
    , iHeartbeatCB( HeartbeatTimerCallback, this )
    , iHeartbeatDTE(iHeartbeatCB)
    , iHBQueued(XP_FALSE)
{
#ifdef DEBUG
    TInt processHandleCount, threadHandleCount;
    RThread thread;
    thread.HandleCount( processHandleCount, threadHandleCount );
    XP_LOGF( "startup: processHandleCount: %d; threadHandleCount: %d", 
             processHandleCount, threadHandleCount );
#endif

    iDraw = NULL;
    XP_MEMSET( &iGame, 0, sizeof(iGame) );

    iCurGameName.Copy( _L("") );

    /* CBase derivitaves are zero'd out, they say */
    XP_ASSERT( iTimerReasons[0] == 0 && iTimerReasons[1] == 0 );

#ifndef XWFEATURE_STANDALONE_ONLY
    comms_getInitialAddr( &iCommsAddr );
#endif
}

CXWordsAppView::~CXWordsAppView()
{
    DeleteGame();

    if ( iDraw ) {
        draw_destroyCtxt( iDraw );
    }

    XP_FREE( mpool, iUtil.vtable );
    vtmgr_destroy( MPPARM(mpool) iVtMgr );

    mpool_destroy( mpool );

    delete iDictList;
    delete iRequestTimer;
    delete iGamesMgr;
#ifndef XWFEATURE_STANDALONE_ONLY
    delete iSendSock;
    delete iNewPacketQueue;
#endif

#ifdef DEBUG
    TInt processHandleCount, threadHandleCount;
    RThread thread;
    thread.HandleCount( processHandleCount, threadHandleCount );
    XP_LOGF( "shutdown: processHandleCount: %d; threadHandleCount: %d", 
             processHandleCount, threadHandleCount );
#endif
}

void CXWordsAppView::ConstructL(const TRect& aRect)
{
    // Create a window for this application view
    CreateWindowL();

    // Set the windows size
    SetRect(aRect);

    // Indicate that the control is blank
    SetBlank();

    iRequestTimer = CIdle::NewL( CActive::EPriorityIdle );
    iTimerRunCount = 0;
    iDeltaTimer = CDeltaTimer::NewL( CActive::EPriorityStandard );


#ifndef XWFEATURE_STANDALONE_ONLY
    iSendSock = CSendSocket::NewL( PacketReceived, (void*)this );
    XP_LOGF( "iSendSock created" );
    iNewPacketQueue = new (ELeave)CDesC8ArrayFlat(2);
#endif

    // Set the control's border
//     SetBorder(TGulBorder::EFlatContainer);

    // Set the correct application view (Note:
    // ESkinAppViewWithCbaNoToolband is the default)
    
#if defined SERIES_80
    //CknEnv::Skin().SetAppViewType(ESkinAppViewWithCbaNoToolband);
    CknEnv::Skin().SetAppViewType(ESkinAppViewNoCbaNoToolband);
#endif

    // This doesn't do what I want
//     SetExtentToWholeScreen();

    // Activate the window, which makes it ready to be drawn
    ActivateL();

    // Now the xwords-specific stuff
    XP_LOGF( "starting xwords initialization" );

    iStartTime.HomeTime();
    TDateTime tdtime = iStartTime.DateTime();
    // seed random with current microseconds, or so....  whatever.
    TInt seed = (((tdtime.Minute() * 60) + tdtime.Second()) * 1000 )
        + tdtime.MicroSecond();
    XP_LOGF( "seeding srand with %d", seed );
    srand( seed );

#ifdef MEM_DEBUG
    mpool = mpool_make();
#endif

    iVtMgr = make_vtablemgr( MPPARM_NOCOMMA(mpool) );
    User::LeaveIfNull( iVtMgr );

    iUtil.vtable = (UtilVtable*)XP_MALLOC( mpool, sizeof( UtilVtable ) );
    User::LeaveIfNull( iUtil.vtable );
    SetUpUtil();

    TFileName basePath;
    GetXwordsRWDir( &basePath, EGamesLoc );
    iGamesMgr = CXWGamesMgr::NewL( MPPARM(mpool) iCoeEnv, &basePath );

    iDraw = sym_drawctxt_make( MPPARM(mpool) &SystemGc(), iCoeEnv, 
                               iEikonEnv, iApp );
    User::LeaveIfNull( iDraw );

    if ( !FindAllDicts() ) {
        UserErrorFromID( R_ALERT_NO_DICTS );
        User::Leave( -1 );
    }

    if ( !LoadPrefs() ) {
        InitPrefs();
    }
    MakeOrLoadGameL();

    PositionBoard();
    (void)server_do( iGame.server ); // get tiles assigned etc.
} // ConstructL

// Draw this application's view to the screen
void CXWordsAppView::Draw( const TRect& aRect ) const
{
    // Get the standard graphics context 
    CWindowGc& gc = SystemGc();
    gc.SetClippingRect( aRect );
    gc.Clear( aRect );

    if ( iGame.board ) {
        XP_Rect rect;

        rect.left = SC( XP_U16, aRect.iTl.iX );
        rect.top = SC( XP_U16, aRect.iTl.iY );
        rect.width = SC( XP_U16, aRect.Width() );
        rect.height = SC( XP_U16, aRect.Height() );

        board_invalRect( iGame.board, &rect );
        board_draw( iGame.board );
    }

    DrawGameName();
} // Draw

/* static */ VTableMgr*
CXWordsAppView::sym_util_getVTManager( XW_UtilCtxt* uc )
{
    CXWordsAppView* self = (CXWordsAppView*)uc->closure;
    return self->iVtMgr;
} /* sym_util_getVTManager */

#ifndef XWFEATURE_STANDALONE_ONLY
/*static*/ XWStreamCtxt*
CXWordsAppView::sym_util_makeStreamFromAddr( XW_UtilCtxt* uc,
                                             XP_U16 channelNo )
{
    XP_LOGF( "sym_util_makeStreamFromAddr called" );
    CXWordsAppView* self = (CXWordsAppView*)uc->closure;
    XP_ASSERT( self->iGame.comms );
    return self->MakeSimpleStream( self->sym_send_on_close,
                                   channelNo );
}
#endif

#define EM BONUS_NONE
#define DL BONUS_DOUBLE_LETTER
#define DW BONUS_DOUBLE_WORD
#define TL BONUS_TRIPLE_LETTER
#define TW BONUS_TRIPLE_WORD
    
static XWBonusType
sym_util_getSquareBonus( XW_UtilCtxt* /*uc*/, ModelCtxt* /*model*/,
                         XP_U16 col, XP_U16 row )
{
    XP_U16 index;
    XWBonusType result;
    const XP_U16 fourteen = 14;

    const char defaultBoard[8*8] = {
        TW,EM,EM,DL,EM,EM,EM,TW,
        EM,DW,EM,EM,EM,TL,EM,EM,

        EM,EM,DW,EM,EM,EM,DL,EM,
        DL,EM,EM,DW,EM,EM,EM,DL,
                            
        EM,EM,EM,EM,DW,EM,EM,EM,
        EM,TL,EM,EM,EM,TL,EM,EM,
                            
        EM,EM,DL,EM,EM,EM,DL,EM,
        TW,EM,EM,DL,EM,EM,EM,DW,
    }; /* defaultBoard */

    if ( col > 7 ) col = SC(XP_U16, fourteen - col);
    if ( row > 7 ) row = SC(XP_U16, fourteen - row);
    index = SC(XP_U16,(row*8) + col);
    if ( index >= 8*8 ) {
        result = (XWBonusType)EM;
    } else {
        result = (XWBonusType)defaultBoard[index];
    }
    return result;
} // sym_util_getSquareBonus

/* static */ void
CXWordsAppView::sym_util_userError( XW_UtilCtxt* uc, UtilErrID id )
{
    TInt resourceId = 0;

    switch( id ) {
    case ERR_TILES_NOT_IN_LINE:
        resourceId = R_TILES_IN_LINE_ALERT;
        break;
    case ERR_NO_EMPTIES_IN_TURN:
        resourceId = R_NO_EMPTIES_SEP_ALERT;
        break;
    case ERR_TWO_TILES_FIRST_MOVE:
        resourceId = R_TWO_TILES_FIRST_MOVE_ALERT;
        break;
    case ERR_TILES_MUST_CONTACT:
        resourceId = R_PLACED_MUST_CONTACT_ALERT;
        break;
    case ERR_TOO_FEW_TILES_LEFT_TO_TRADE:
        resourceId = R_TOO_FEW_TO_TRADE_ALERT;
        break;
    case ERR_NOT_YOUR_TURN:
        resourceId = R_NOT_YOUR_TURN_ALERT;
        break;
    case ERR_NO_PEEK_ROBOT_TILES:
        resourceId = R_NO_PEEK_ALERT;
        break;
#ifndef XWFEATURE_STANDALONE_ONLY
    case ERR_SERVER_DICT_WINS:
    case ERR_NO_PEEK_REMOTE_TILES:
    case ERR_REG_UNEXPECTED_USER:
#endif
    case ERR_CANT_TRADE_MID_MOVE:
        resourceId = R_REMOVE_FIRST_ALERT;
        break;
    case ERR_CANT_UNDO_TILEASSIGN:
        resourceId = R_NOTHING_TO_UNDO_ALERT;
        break;
    default:
        break;
    }

    CXWordsAppView* self = (CXWordsAppView*)uc->closure;
    self->UserErrorFromID( resourceId );
} // sym_util_userError

static XP_Bool
sym_util_userQuery( XW_UtilCtxt* uc, UtilQueryID aId,
                    XWStreamCtxt* aStream )
{
    CXWordsAppView* self = (CXWordsAppView*)uc->closure;
    return self->UserQuery( aId, aStream );
} /* sym_util_userQuery */

static XP_S16
sym_util_userPickTile( XW_UtilCtxt* /*uc*/, const PickInfo* /*pi*/, 
                       XP_U16 /*playerNum*/,
                       const XP_UCHAR4* texts, XP_U16 nTiles )
{
    TInt result = 0;
    TRAPD( error, CXWBlankSelDlg::UsePickTileDialogL( texts, nTiles, &result ) );
    XP_ASSERT( result >= 0 && result < nTiles );
    return static_cast<XP_S16>(result);
} /* sym_util_userPickTile */

static XP_Bool
sym_util_askPassword( XW_UtilCtxt* /*uc*/, const XP_UCHAR* /*name*/,
                      XP_UCHAR* /*buf*/, XP_U16* /*len*/ )
{
    return XP_TRUE;
}

static void
sym_util_trayHiddenChange(XW_UtilCtxt* /*uc*/, XW_TrayVisState /*newState*/,
                          XP_U16 /*nVisibleRows*/)
{
}

static void
sym_util_yOffsetChange(XW_UtilCtxt* /*uc*/, XP_U16 /*oldOffset*/, 
                       XP_U16 /*newOffset*/ )
{
}

/* static */ void
CXWordsAppView::sym_util_notifyGameOverL( XW_UtilCtxt* uc )
{
    CXWordsAppView* self = (CXWordsAppView*)uc->closure;
    self->DrawNow();
    self->DisplayFinalScoresL();
}

static XP_Bool
sym_util_hiliteCell( XW_UtilCtxt* /*uc*/, XP_U16 /*col*/, XP_U16 /*row*/ )
{
    return XP_TRUE;             // don't exit early
}

static XP_Bool
sym_util_engineProgressCallback( XW_UtilCtxt* /*uc*/ )
{
#ifdef SHOW_PROGRESS
#endif
    return XP_TRUE;
}

/*static*/ TInt
CXWordsAppView::HeartbeatTimerCallback( TAny* closure )
{
#ifdef BEYOND_IR
    CXWordsAppView* self = (CXWordsAppView*)closure;
    self->iHBQueued = XP_FALSE;

    TimerProc proc;
    void* hbclosure;
    self->GetHeartbeatCB( &proc, &hbclosure );
    (*proc)( hbclosure, TIMER_HEARTBEAT );
#endif
    return 0;
}

/* static */ void
CXWordsAppView::sym_util_setTimer( XW_UtilCtxt* uc, XWTimerReason why, 
                                   XP_U16 when,
                                   TimerProc proc, void* closure )
{
    CXWordsAppView* self = (CXWordsAppView*)uc->closure;

    self->SetHeartbeatCB( proc, closure );

    if ( self->iHBQueued ) {
        self->iDeltaTimer->Remove( self->iHeartbeatDTE );
        self->iHBQueued = XP_FALSE;
    }

    self->iDeltaTimer->Queue( when * 1000000, self->iHeartbeatDTE );
    self->iHBQueued = XP_TRUE;
}

/* static */ void
CXWordsAppView::sym_util_requestTime( XW_UtilCtxt* uc )
{
    CXWordsAppView* self = (CXWordsAppView*)uc->closure;
    self->StartIdleTimer( EUtilRequest );
}

/* static */ XP_U32
CXWordsAppView::sym_util_getCurSeconds( XW_UtilCtxt* uc )
{
    CXWordsAppView* self = (CXWordsAppView*)uc->closure;
	TTime currentTime;
	currentTime.HomeTime();

    TTimeIntervalSeconds interval;
	(void)currentTime.SecondsFrom( self->iStartTime, interval );
    
    return (XP_U32)interval.Int();
}

/* static */ DictionaryCtxt*
CXWordsAppView::sym_util_makeEmptyDict( XW_UtilCtxt* uc )
{
    CXWordsAppView* self = (CXWordsAppView*)uc->closure;

    DictionaryCtxt* dict = sym_dictionary_makeL( MPPARM(self->mpool)
                                                 NULL, NULL );
    return dict;
}

static XP_UCHAR*
sym_util_getUserString( XW_UtilCtxt* /*uc*/, XP_U16 stringCode )
{
    // These belong in resources.  But I haven't yet figured out how
    // to do 8-bit strings in resources.  Also, StringLoader does
    // allocations when loading strings rather than just returning
    // pointers into the resources as palm does.  So this method will
    // have to provide permanent storage for the strings, probably via
    // a string table that's filled in on demand.

    switch( stringCode ) {
    case STRD_REMAINING_TILES_ADD:
        return (XP_UCHAR*)"+ %d [all remaining tiles]";
    case STRD_UNUSED_TILES_SUB:
        return (XP_UCHAR*)"- %d [unused tiles]";
    case STR_BONUS_ALL:
        return (XP_UCHAR*)"Bonus for using all tiles: 50" XP_CR;
    case STRD_TURN_SCORE:
        return (XP_UCHAR*)"Score for turn: %d" XP_CR;
    case STR_COMMIT_CONFIRM:
        return (XP_UCHAR*)"Commit the current move?" XP_CR;
    case STR_LOCAL_NAME:
        return (XP_UCHAR*)"%s";
    case STR_NONLOCAL_NAME:
        return (XP_UCHAR*)"%s (remote)";
    case STRD_TIME_PENALTY_SUB:
        return (XP_UCHAR*)" - %d [time]";

    case STRD_CUMULATIVE_SCORE:
        return (XP_UCHAR*)"Cumulative score: %d" XP_CR;
    case STRS_MOVE_ACROSS:
        return (XP_UCHAR*)"move (from %s across)" XP_CR;
    case STRS_MOVE_DOWN:
        return (XP_UCHAR*)"move (from %s down)" XP_CR;
    case STRS_TRAY_AT_START:
        return (XP_UCHAR*)"Tray at start: %s" XP_CR;

    case STRS_NEW_TILES:
        return (XP_UCHAR*)"New tiles: %s" XP_CR;
    case STRSS_TRADED_FOR:
        return (XP_UCHAR*)"Traded %s for %s.";
    case STR_PASS:
        return (XP_UCHAR*)"pass" XP_CR;
    case STR_PHONY_REJECTED:
        return (XP_UCHAR*)"Illegal word in move; turn lost!" XP_CR;

    case STRD_ROBOT_TRADED:
        return (XP_UCHAR*)"Robot traded tiles %d this turn.";
    case STR_ROBOT_MOVED:
        return (XP_UCHAR*)"The robot made this move:" XP_CR;
    case STR_REMOTE_MOVED:
        return (XP_UCHAR*)"Remote player made this move:" XP_CR;

    case STR_PASSED: 
        return (XP_UCHAR*)"Passed";
    case STRSD_SUMMARYSCORED: 
        return (XP_UCHAR*)"%s:%d";
    case STRD_TRADED: 
        return (XP_UCHAR*)"Traded %d";
    case STR_LOSTTURN:
        return (XP_UCHAR*)"Lost turn";

    case STRS_VALUES_HEADER:
        return (XP_UCHAR*)"%s counts/values:" XP_CR;

    default:
        XP_LOGF( "stringCode=%d", stringCode );
        return (XP_UCHAR*)"unknown code";
    }
} // sym_util_getUserString

static XP_Bool
sym_util_warnIllegalWord( XW_UtilCtxt* /*uc*/, BadWordInfo* /*bwi*/, 
                          XP_U16 /*turn*/, XP_Bool /*turnLost*/ )
{
    return XP_FALSE;
}

#ifdef BEYOND_IR
/*static*/void 
CXWordsAppView::sym_util_listenPortChange( XW_UtilCtxt* uc, XP_U16 aPort )
{
/*     CXWordsAppView* self = (CXWordsAppView*)uc->closure; */
//     self->iReadSock->SetListenPort( aPort );
/*     self->iSendSock->Listen(); */
}

/*static*/void 
CXWordsAppView::sym_util_addrChange( XW_UtilCtxt* uc, 
                                     const CommsAddrRec* aOld,
                                     const CommsAddrRec* aNew )
{
    CXWordsAppView* self = (CXWordsAppView*)uc->closure;
    XP_LOGF( "util_addrChange: calling connect" );
    self->iSendSock->ConnectL( aNew );
    (void)self->iSendSock->Listen();
}
#endif

#ifdef XWFEATURE_SEARCHLIMIT
static XP_Bool
sym_util_getTraySearchLimits(XW_UtilCtxt* /*uc*/, XP_U16* /*min*/, XP_U16* /*max*/ )
{
    return XP_FALSE;
}
#endif


void
CXWordsAppView::SetUpUtil()
{
    UtilVtable* vtable = iUtil.vtable;
    iUtil.closure = (void*)this;
    iUtil.gameInfo = &iGi;
    MPASSIGN( iUtil.mpool, mpool );

    vtable->m_util_getVTManager = sym_util_getVTManager;
#ifndef XWFEATURE_STANDALONE_ONLY
    vtable->m_util_makeStreamFromAddr = sym_util_makeStreamFromAddr;
#endif
    vtable->m_util_getSquareBonus = sym_util_getSquareBonus;
    vtable->m_util_userError = sym_util_userError;
    vtable->m_util_userQuery = sym_util_userQuery;
    vtable->m_util_userPickTile = sym_util_userPickTile;
    vtable->m_util_askPassword = sym_util_askPassword;
    vtable->m_util_trayHiddenChange = sym_util_trayHiddenChange;
    vtable->m_util_yOffsetChange = sym_util_yOffsetChange;
    vtable->m_util_notifyGameOver = sym_util_notifyGameOverL;
    vtable->m_util_hiliteCell = sym_util_hiliteCell;
    vtable->m_util_engineProgressCallback = sym_util_engineProgressCallback;
    vtable->m_util_setTimer = sym_util_setTimer;
    vtable->m_util_requestTime = sym_util_requestTime;
    vtable->m_util_getCurSeconds = sym_util_getCurSeconds;
    vtable->m_util_makeEmptyDict = sym_util_makeEmptyDict;
    vtable->m_util_getUserString = sym_util_getUserString;
    vtable->m_util_warnIllegalWord = sym_util_warnIllegalWord;
#ifdef BEYOND_IR
    vtable->m_util_addrChange = sym_util_addrChange;
#endif
#ifdef XWFEATURE_SEARCHLIMIT
    vtable->m_util_getTraySearchLimits = sym_util_getTraySearchLimits;
#endif
}

// Load the current game from prefs or else create a new one.
// Eventually this will involve putting up the new game dialog.
void
CXWordsAppView::MakeOrLoadGameL()
{
    if ( iCurGameName.Length() > 0 && iGamesMgr->Exists( &iCurGameName ) ) {
        LoadOneGameL( &iCurGameName );
    } else {

        /***************************************************************
         * PENDING The code here duplicates DoNewGame.  Unify the two!!!!
         ***************************************************************/

        gi_initPlayerInfo( MPPARM(mpool) &iGi, (XP_UCHAR*)"Player %d" );

#ifndef XWFEATURE_STANDALONE_ONLY
        CommsAddrRec commsAddr;
        if ( iGame.comms != NULL ) {
            comms_getAddr( iGame.comms, &commsAddr );
        } else {
            commsAddr = iCommsAddr;
        }
#endif

        TGameInfoBuf gib( &iGi, 
#ifndef XWFEATURE_STANDALONE_ONLY
                          &commsAddr, 
#endif
                          iDictList );
        if ( !CXWGameInfoDlg::DoGameInfoDlgL( MPPARM(mpool) &gib, ETrue ) ) {
            User::Leave(-1);
        }

        gib.CopyToL( MPPARM(mpool) &iGi
#ifndef XWFEATURE_STANDALONE_ONLY
                     , &iCommsAddr
#endif
                     );

        TFileName path;
        GetXwordsRWDir( &path, EDictsLoc );        
        DictionaryCtxt* dict = sym_dictionary_makeL( MPPARM(mpool) 
                                                     &path, iGi.dictName );
        User::LeaveIfNull( dict );

        XP_U16 newGameID = SC( XP_U16, sym_util_getCurSeconds( &iUtil ) );
        game_makeNewGame( MPPARM(mpool) &iGame, &iGi, 
                          &iUtil, iDraw, newGameID, &iCp,
                          SYM_SEND, this );
        model_setDictionary( iGame.model, dict );

#ifndef XWFEATURE_STANDALONE_ONLY
        if ( iGame.comms ) {
            comms_setAddr( iGame.comms, &iCommsAddr );
            if ( iGi.serverRole == SERVER_ISCLIENT ) {
                XWStreamCtxt* stream = MakeSimpleStream( sym_send_on_close );
                server_initClientConnection( iGame.server, stream );
            }
        }
#endif

        iGamesMgr->MakeDefaultName( &iCurGameName );
        StoreOneGameL( &iCurGameName );
    }
} /* MakeOrLoadGameL */

void
CXWordsAppView::DeleteGame()
{
    game_dispose( &iGame );
    gi_disposePlayerInfo( MPPARM(mpool) &iGi );
}

void
CXWordsAppView::PositionBoard() 
{
    const TRect rect = Rect();
    const XP_U16 boardWidth = 15 * scaleBoardH;
    const XP_U16 scoreTop = 25;
    const XP_U16 scoreHt = 120;

    board_setPos( iGame.board, 2, 2, XP_FALSE );
    board_setScale( iGame.board, scaleBoardH, scaleBoardV );

    XP_U16 scoreLeft = 2 + boardWidth + 2 + 3; /* 2 for border, 3 for gap */
    XP_U16 scoreRight = SC(XP_U16, rect.iBr.iX - 2 - 1); /* 2 for border */
    board_setScoreboardLoc( iGame.board, scoreLeft, scoreTop,
                            SC( XP_U16, scoreRight - scoreLeft - 1),
                            scoreHt, XP_FALSE );
    board_setYOffset( iGame.board, 0 );

    board_setTrayLoc( iGame.board, 
                      2 + (15 * scaleBoardH) + 5, // to right of board
                      // make tray bottom same as board's
                      2 + (15*scaleBoardV) - scaleTrayV,
                      scaleTrayH, scaleTrayV,   // v and h scale
                      3 );      // divider width
    board_invalAll( iGame.board );

    iTitleBox.SetRect( scoreLeft, 2, scoreRight, scoreTop - 4 );
} // PositionBoard

int 
CXWordsAppView::HandleCommand( TInt aCommand )
{
    XP_Bool draw = XP_FALSE;
    XP_Bool notDone;

    switch ( aCommand ) {

    case XW_NEWGAME_COMMAND:
        draw = DoNewGame();
        break;

    case XW_SAVEDGAMES_COMMAND:
        draw = DoSavedGames();
        (void)server_do( iGame.server );
        break;
    case XW_PREFS_COMMAND:
    case XW_ABOUT_COMMAND:
        NotImpl();
        break;

    case XW_VALUES_COMMAND:
        if ( iGame.server != NULL ) {
            XWStreamCtxt* stream = MakeSimpleStream( NULL );
            if ( stream != NULL ) {
                server_formatDictCounts( iGame.server, stream, 7 ); /* 4: ncols */
                CXWAskDlg::DoInfoDlg(MPPARM(mpool) stream, ETrue);
            }
        }
        break;

    case XW_REMAIN_COMMAND:
        if ( iGame.board != NULL ) {
            XWStreamCtxt* stream = MakeSimpleStream( NULL );
            if ( stream ) {
                board_formatRemainingTiles( iGame.board, stream );
                CXWAskDlg::DoInfoDlg(MPPARM(mpool) stream, ETrue);
            }
        }
        break;

    case XW_CURINFO_COMMAND: {
        NotImpl();
/*         TGameInfoBuf gib( &iGi, iDictList ); */
/*         CXWGameInfoDlg::DoGameInfoDlgL( MPPARM(mpool) &gib, EFalse ); */
    }
        break;

    case XW_HISTORY_COMMAND:
        if ( iGame.server ) {
            XP_Bool gameOver = server_getGameIsOver( iGame.server );
            XWStreamCtxt* stream = MakeSimpleStream( NULL );

            model_writeGameHistory( iGame.model, stream, 
                                    iGame.server, gameOver );
            if ( stream_getSize( stream ) > 0 ) {
                CXWAskDlg::DoInfoDlg( MPPARM(mpool) stream, ETrue );
            }
        }
        
        break;

    case XW_FINALSCORES_COMMAND:
        if ( server_getGameIsOver( iGame.server ) ) {
            DisplayFinalScoresL();
        } else if ( AskFromResId( R_CONFIRM_END_GAME ) ) {
            server_endGame( iGame.server );
            draw = ETrue;
        }
        break;

    case XW_HINT_COMMAND:
#ifdef XWFEATURE_SEARCHLIMIT
    case XW_LIMHINT_COMMAND:
#endif
        break;
    case XW_NEXTHINT_COMMAND:
        draw = board_requestHint( iGame.board, 
#ifdef XWFEATURE_SEARCHLIMIT
                                  XP_FALSE,
#endif

                                  &notDone );
        break;
    case XW_UNDOCUR_COMMAND:
        draw = board_replaceTiles( iGame.board );
        break;
    case XW_UNDOLAST_COMMAND:
        draw = server_handleUndo( iGame.server );
        break;
    case XW_DONE_COMMAND:
        draw = board_commitTurn( iGame.board );
        break;
    case XW_JUGGLE_COMMAND:
        draw = board_juggleTray( iGame.board );
        break;

    case XW_TRADE_COMMAND:
        draw = board_beginTrade( iGame.board );
        break;

    case XW_HIDETRAY_COMMAND:
        if ( TRAY_REVEALED == board_getTrayVisState( iGame.board ) ) {
            draw = board_hideTray( iGame.board );
        } else {
            draw = board_showTray( iGame.board );
        }
        break;

    case XW_FLIP_COMMAND:
        draw = board_flip( iGame.board );
        break;
    case XW_TOGGLEVALS_COMMAND:
        draw = board_toggle_showValues( iGame.board );
        break;

#ifndef XWFEATURE_STANDALONE_ONLY
    case XW_RESEND_COMMAND:
        if ( iGame.comms != NULL ) {
            (void)comms_resendAll( iGame.comms );
        }
#endif

    default:
        return 0;
    }

    if ( draw ) {
        DoImmediateDraw();
    }

    return 1;
} // CXWordsAppView::HandleCommand

void 
CXWordsAppView::Exiting()
{
    StoreOneGameL( &iCurGameName );
    WritePrefs();
} /* Exiting */

TBool
CXWordsAppView::HandleKeyEvent( const TKeyEvent& aKeyEvent )
{
    XP_Bool draw = XP_FALSE;
    XP_Key key = XP_KEY_NONE;
    TChar chr(aKeyEvent.iCode);

    switch ( chr ) {

    case EKeyTab:  // 2                     // this is probably for laptop only! :-)
        key = XP_FOCUSCHANGE_KEY;
        break;

    case EKeyEnter:
        key = XP_RETURN_KEY;
        break;
    case EKeyBackspace:
        key = XP_CURSOR_KEY_DEL;
        break;
    case EKeyLeftArrow:         // 14
        key = XP_CURSOR_KEY_LEFT;
        break;
    case EKeyRightArrow:        // 15
        key = XP_CURSOR_KEY_RIGHT;
        break;
    case EKeyUpArrow:        // 16
        key = XP_CURSOR_KEY_UP;
        break;
    case EKeyDownArrow:           // 17
        key = XP_CURSOR_KEY_DOWN;
        break;

    default:
        key = (XP_Key)aKeyEvent.iScanCode;
        if ( key < 0x20 || key > 0x7f ) {
            key = XP_KEY_NONE;
        }
        break;
    }

    if ( iGame.board != NULL ) {
        if ( key != XP_KEY_NONE ) {
            draw = board_handleKey( iGame.board, key );
        }
    }

    if ( draw ) {
        DoImmediateDraw();
    }

    // handled if it's one we recognize.  This is probably too broad!!!
    return key != XP_KEY_NONE;
}

/* static */ TInt
CXWordsAppView::TimerCallback( TAny* aPtr )
{
    CXWordsAppView* me = (CXWordsAppView*)aPtr;
    
    if ( 0 ) {

#ifndef XWFEATURE_STANDALONE_ONLY
    /* Only do one per call.  Packets are higher priority */
    } else if ( me->iTimerReasons[EProcessPacket] > 0 ) {
        --me->iTimerReasons[EProcessPacket];
        XP_ASSERT( me->iTimerReasons[EProcessPacket] == 0 );
        
        XP_Bool draw = XP_FALSE;

        XWStreamCtxt* stream = me->MakeSimpleStream( NULL );
        
        TPtrC8 packet = (*me->iNewPacketQueue)[0];
        stream_putBytes( stream, (void*)packet.Ptr(), packet.Length() );
        me->iNewPacketQueue->Delete(0);
        XP_LOGF( "pulling packet off head of queue; there are %d left",
                 me->iNewPacketQueue->Count() );

        CommsAddrRec addr;
        addr.conType = COMMS_CONN_RELAY;
        if ( comms_checkIncomingStream( me->iGame.comms, stream, &addr ) ) {
            draw = server_receiveMessage( me->iGame.server, stream );
        }
        stream_destroy( stream );
        sym_util_requestTime( &me->iUtil );

        if ( draw ) {
            me->DoImmediateDraw();        
        }
#endif
    } else if ( me->iTimerReasons[EUtilRequest] > 0 ) {
        --me->iTimerReasons[EUtilRequest];
        if ( server_do( me->iGame.server ) ) {
            me->DoImmediateDraw();
        }
    }

    return --me->iTimerRunCount > 0;
}

XWStreamCtxt* 
CXWordsAppView::MakeSimpleStream( MemStreamCloseCallback cb,
                                  XP_U16 channelNo )
{
    return mem_stream_make( MPPARM(mpool)
                            iVtMgr, (void*)this, 
                            channelNo, cb );
} /* MakeSimpleStream */

void
CXWordsAppView::DisplayFinalScoresL()
{
    XWStreamCtxt* stream = MakeSimpleStream( NULL );
    User::LeaveIfNull( stream );

    server_writeFinalScores( iGame.server, stream );

    CXWAskDlg::DoInfoDlg( MPPARM(mpool) stream, ETrue );
} /* displayFinalScores */

TBool
CXWordsAppView::AskFromResId( TInt aResource )
{
    TBuf16<128> message;
    StringLoader::Load( message, aResource );

    return CXWAskDlg::DoAskDlg( MPPARM(mpool) &message );
}

static void
logOneFile( TPtrC name )
{
    TBuf8<128> tmpb;
    tmpb.Copy( name );
    XP_UCHAR buf[128];
    XP_MEMCPY( buf, (void*)(tmpb.Ptr()), tmpb.Length() );
    buf[tmpb.Length()] = '\0';
    XP_LOGF( "found file %s", buf );
} /* logOneFile */

TBool
CXWordsAppView::FindAllDicts()
{
    /* NOTE: CEikFileNameSelector might be the way to do this and the display
     * of the list in the game setup dialog.
     */
    TBool found = EFalse;
    RFs fs = iCoeEnv->FsSession();

    TFindFile file_finder( fs );
    CDir* file_list;
    _LIT( wildName, "*.xwd" );

    TFileName dir;
    GetXwordsRWDir( &dir, EDictsLoc );

    TInt err = file_finder.FindWildByDir( wildName, dir, file_list );
    if ( err == KErrNone ) {

        CleanupStack::PushL( file_list );
        iDictList = new (ELeave)CDesC16ArrayFlat( file_list->Count() );

        TInt i;
        for ( i = 0; i < file_list->Count(); i++ ) {
            TParse fullentry;
            fullentry.Set((*file_list)[i].iName,& file_finder.File(),NULL);
            logOneFile( fullentry.Name() );
            iDictList->AppendL( fullentry.Name() );
        }

        CleanupStack::PopAndDestroy( file_list );
        found = ETrue;
    }

    return found;
} /* FindAllDicts */

void
CXWordsAppView::UserErrorFromID( TInt aResource )
{
    if ( aResource != 0 ) {
        _LIT(title,"Oops");
        TBuf16<128> message;
        StringLoader::Load( message, aResource );
#if defined SERIES_60
        CEikInfoDialog::RunDlgLD( title, message );
#elif defined SERIES_80
        CCknInfoDialog::RunDlgLD( title, message );
#endif
    }
}

void
CXWordsAppView::NotImpl()
{
    UserErrorFromID( R_ALERT_FEATURE_PENDING );
} /* NotImpl */

void
CXWordsAppView::GetXwordsRWDir( TFileName* aPathRef, TDriveReason aWhy )
{
    TFileName fn = iApp->BitmapStoreName(); /* isn't the a method to just get
                                               the path? */
    TParse nameParser;
    nameParser.Set( fn, NULL, NULL );
    TPtrC path = nameParser.DriveAndPath();

    switch( aWhy ) {
    case EGamesLoc:
    case EDictsLoc:
        aPathRef->Copy( nameParser.DriveAndPath() );
        break;
    case EPrefsLoc:
        aPathRef->Copy( nameParser.Path() );
        break;                  /* don't want a drive */
    }
} /* GetXwordsRWDir */

_LIT(filename,"xwdata.dat");
TBool
CXWordsAppView::LoadPrefs()
{
    RFs fs = iCoeEnv->FsSession();

    TFileName nameD;
    GetXwordsRWDir( &nameD, EPrefsLoc );
    nameD.Append( filename );

    /* Read in prefs etc. */
    RFileReadStream reader;
    CleanupClosePushL(reader);
    TInt err = reader.Open( fs, nameD, EFileRead );

    TBool found = err == KErrNone;
    if ( found ) {
        iCp.showBoardArrow = reader.ReadInt8L();
        iCp.showRobotScores = reader.ReadInt8L();
        reader >> iCurGameName;
    }
    CleanupStack::PopAndDestroy( &reader ); // reader

    return found;
} /* LoadPrefs */

void
CXWordsAppView::WritePrefs()
{
    RFs fs = iCoeEnv->FsSession();

    TFileName nameD;
    GetXwordsRWDir( &nameD, EPrefsLoc );
    nameD.Append( filename );

    RFileWriteStream writer;
    CleanupClosePushL(writer);
    User::LeaveIfError( writer.Replace( fs, nameD, EFileWrite ) );

    writer.WriteInt8L( iCp.showBoardArrow );
    writer.WriteInt8L( iCp.showRobotScores );

    writer << iCurGameName;

    CleanupStack::PopAndDestroy( &writer ); // writer
} /* WritePrefs */

void
CXWordsAppView::InitPrefs()
{
    iCp.showBoardArrow = XP_TRUE; // default because no pen
    iCp.showRobotScores = XP_FALSE;

#ifndef XWFEATURE_STANDALONE_ONLY
    iGi.serverRole = SERVER_STANDALONE;
#endif
}

TBool
CXWordsAppView::DoNewGame()
{
    TBool draw = EFalse;

    TBool save = AskSaveGame();

#ifndef XWFEATURE_STANDALONE_ONLY
    CommsAddrRec commsAddr;
    if ( iGame.comms != NULL ) {
        comms_getAddr( iGame.comms, &commsAddr );
    } else {
        commsAddr = iCommsAddr;
    }
#endif

    TGameInfoBuf gib( &iGi, 
#ifndef XWFEATURE_STANDALONE_ONLY
                      &commsAddr, 
#endif
                      iDictList );
    if ( CXWGameInfoDlg::DoGameInfoDlgL( MPPARM(mpool) &gib, ETrue ) ) {

        if ( save ) {
            StoreOneGameL( &iCurGameName );
            iGamesMgr->MakeDefaultName( &iCurGameName );
        }

        gib.CopyToL( MPPARM(mpool) &iGi
#ifndef XWFEATURE_STANDALONE_ONLY
                     , &iCommsAddr
#endif
                     );
        XP_U16 newGameID = SC( XP_U16,sym_util_getCurSeconds( &iUtil ) );
        game_reset( MPPARM(mpool) &iGame, &iGi, &iUtil, newGameID,
                    &iCp, SYM_SEND, this );

        DictionaryCtxt* prevDict = model_getDictionary( iGame.model );
        if ( 0 != XP_STRCMP( dict_getName(prevDict), iGi.dictName ) ) {
            dict_destroy( prevDict );

            TFileName path;
            GetXwordsRWDir( &path, EDictsLoc );        
            DictionaryCtxt* dict = sym_dictionary_makeL( MPPARM(mpool) 
                                                         &path, iGi.dictName );
            model_setDictionary( iGame.model, dict );
        }
#ifndef XWFEATURE_STANDALONE_ONLY
        if ( iGame.comms ) {
            comms_setAddr( iGame.comms, &iCommsAddr );

            if ( iGi.serverRole == SERVER_ISCLIENT ) {
                XWStreamCtxt* stream = MakeSimpleStream( sym_send_on_close );
                // server deletes stream
                server_initClientConnection( iGame.server, stream );
            }
        }
#endif

        board_invalAll( iGame.board );
        (void)server_do( iGame.server ); // get tiles assigned etc.
        draw = ETrue;
    }
    return draw;
} /* DoNewGame */

TBool
CXWordsAppView::DoSavedGames()
{
    StoreOneGameL( &iCurGameName );

    TGameName openName;
    TBool confirmed = CXSavedGamesDlg::DoGamesPicker( MPPARM(mpool) 
                                                      this,
                                                      iGamesMgr,
                                                      &iCurGameName,
                                                      &openName );
    if ( confirmed ) {
        if ( 0 != iCurGameName.Compare( openName ) ) {

            iCurGameName = openName;

            game_dispose( &iGame );
            gi_disposePlayerInfo( MPPARM(mpool) &iGi );

            LoadOneGameL( &iCurGameName );
            Window().Invalidate( iTitleBox );
        }
    }
    return confirmed;
} /* DoSavedGames */

void
CXWordsAppView::LoadOneGameL( TGameName* aGameName )
{
    XWStreamCtxt* stream = MakeSimpleStream( NULL );
    DictionaryCtxt* dict;

    iGamesMgr->LoadGameL( aGameName, stream );
    
    XP_U16 len = stream_getU8( stream );
    XP_UCHAR dictName[32];
    stream_getBytes( stream, dictName, len );
    dictName[len] = '\0';

    TFileName path;
    GetXwordsRWDir( &path, EDictsLoc );        
    dict = sym_dictionary_makeL( MPPARM(mpool) &path, dictName );
    XP_ASSERT( !!dict );

    game_makeFromStream( MPPARM(mpool) stream, &iGame, 
                         &iGi, dict, &iUtil, iDraw, &iCp,
                         SYM_SEND, this );
    stream_destroy( stream );

    PositionBoard();
}

void
CXWordsAppView::StoreOneGameL( TGameName* aGameName )
{
    XWStreamCtxt* stream = MakeSimpleStream( NULL );

    /* save the dict.  NOTE: should be possible to do away with this! */
    XP_U32 len = XP_STRLEN(iGi.dictName);
    XP_ASSERT( len <= 0xFF );
    stream_putU8( stream, SC(XP_U8, len) );
    stream_putBytes( stream, iGi.dictName, SC(XP_U16, len) );

    /* Now the game */
    game_saveToStream( &iGame, &iGi, stream );

    iGamesMgr->StoreGameL( aGameName, stream );
    stream_destroy( stream );
}

XP_Bool
CXWordsAppView::UserQuery( UtilQueryID aId, XWStreamCtxt* aStream )
{
    TInt resID = 0;

    switch ( aId ) {
    case QUERY_ROBOT_MOVE:
    case QUERY_ROBOT_TRADE:
    case QUERY_COMMIT_TURN:
        XP_ASSERT( aStream );
        return CXWAskDlg::DoAskDlg( MPPARM(mpool) aStream, EFalse );
    case QUERY_COMMIT_TRADE:
        XP_ASSERT( !aStream );
        resID = R_CONFIRM_TRADE;
        break;
    case SYM_QUERY_CONFIRM_DELGAME:
        XP_ASSERT( !aStream );
        resID = R_CONFIRM_DELGAME;
        break;
    }
    if ( resID != 0 ) {
        return AskFromResId( resID );
    }
    XP_ASSERT( 0 );
    return XP_FALSE;
} /* sym_util_userQuery */

void
CXWordsAppView::DoImmediateDraw()
{
    ActivateGc();

    XP_ASSERT( iGame.board != NULL );
    board_draw( iGame.board );

    DeactivateGc();
} /* DoImmediateDraw */

void
CXWordsAppView::DrawGameName() const
{
    if ( iGi.dictName != NULL ) {
        TBuf16<66> buf;
        buf.Copy( TBuf8<32>(iGi.dictName) );
        buf.Insert( 0, _L("::") );
        buf.Insert( 0, iCurGameName );

        CWindowGc& gc = SystemGc();
        gc.SetPenStyle( CGraphicsContext::ESolidPen );
        gc.SetPenColor( KRgbBlack );
        gc.SetBrushColor( KRgbGray );
        gc.SetBrushStyle( CGraphicsContext::ESolidBrush );

        gc.UseFont( iCoeEnv->NormalFont() );
        gc.DrawText( buf, iTitleBox, iTitleBox.Height() - 4,/*TInt aBaselineOffset*/
                     CGraphicsContext::ECenter );
        gc.DiscardFont();
    }
}

#ifndef XWFEATURE_STANDALONE_ONLY

/*static*/ void
CXWordsAppView::PacketReceived( const TDesC8* aBuf, void* aClosure )
{
    CXWordsAppView* me = (CXWordsAppView*)aClosure;

    TInt count = me->iNewPacketQueue->Count();
    me->iNewPacketQueue->InsertL( count, *aBuf );
    XP_LOGF( "inserted packet: now number %d", count );

    me->StartIdleTimer( EProcessPacket );
} /* CXWordsAppView::PacketReceived */

/*static*/ XP_S16
CXWordsAppView::sym_send( XP_U8* aBuf, XP_U16 aLen, const CommsAddrRec* aAddr, 
                          void* aClosure )
{
    CommsAddrRec addr;
    XP_S16 result = -1;
    CXWordsAppView* self = (CXWordsAppView*)aClosure;

    if ( aAddr == NULL ) {
        comms_getAddr( self->iGame.comms, &addr );
        aAddr = &addr;
    }

    if ( self->iSendSock->SendL( aBuf, aLen, aAddr ) ) {
        result = aLen;
    }

    /* Can't call listen until we've sent something.... */
    self->iSendSock->Listen();

    return result;
} /* sym_send */

/*static*/ void
CXWordsAppView::sym_send_on_close( XWStreamCtxt* aStream, void* aClosure )
{
    XP_LOGF( "sym_send_on_close called" );
    CXWordsAppView* self = (CXWordsAppView*)aClosure;

    comms_send( self->iGame.comms, aStream );
}
#endif

void
CXWordsAppView::StartIdleTimer( XWTimerReason_symb aWhy )
{
    ++iTimerReasons[aWhy];

    if ( ++iTimerRunCount == 1 ) {
        iRequestTimer->Start( TCallBack( CXWordsAppView::TimerCallback, 
                                         (TAny*)this ) );
    }
}
