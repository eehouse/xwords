/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 2005 by Eric House (fixin@peak.org).  (based on sample
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

#include <cknenv.h>
#include <ckninfo.h>
#include <StringLoader.h>
#include <eikcfdlg.h>
#include <eikfile.rsg>
#include <stdlib.h>             // for srand

#include <xwords.rsg>

#include "xwappview.h"
#include "xwappui.h"
#include "xwords.hrh"
#include "comtypes.h"
#include "symdraw.h"
#include "symaskdlg.h"
#include "symdict.h"

#include "LocalizedStrIncludes.h"

// Standard construction sequence
CXWordsAppView* CXWordsAppView::NewL(const TRect& aRect)
{
    CXWordsAppView* self = CXWordsAppView::NewLC(aRect);
    CleanupStack::Pop(self);
    return self;
}

CXWordsAppView* CXWordsAppView::NewLC(const TRect& aRect)
{
    CXWordsAppView* self = new (ELeave) CXWordsAppView;
    CleanupStack::PushL(self);
    self->ConstructL(aRect);
    return self;
}

CXWordsAppView::CXWordsAppView()
{
    // no implementation required
    iDraw = NULL;
    iBoardPosInval = XP_TRUE;
    XP_MEMSET( &iGame, 0, sizeof(iGame) );
}

CXWordsAppView::~CXWordsAppView()
{
    // no implementation required
    if ( iDraw ) {
        draw_destroyCtxt( iDraw );
        iDraw = NULL;
    }

    DeleteGame();

    XP_FREE( mpool, iUtil.vtable );
    vtmgr_destroy( MPPARM(mpool) iVtMgr );

    mpool_destroy( mpool );

    delete iRequestTimer;
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

    // Set the control's border
//     SetBorder(TGulBorder::EFlatContainer);

    // Set the correct application view (Note:
    // ESkinAppViewWithCbaNoToolband is the default)
    
    //CknEnv::Skin().SetAppViewType(ESkinAppViewWithCbaNoToolband);
    CknEnv::Skin().SetAppViewType(ESkinAppViewNoCbaNoToolband);

    // This doesn't do what I want
//     SetExtentToWholeScreen();

    // Activate the window, which makes it ready to be drawn
    ActivateL();

    // Now the xwords-specific stuff
    XP_LOGF( "starting xwords initialization" );

    iStartTime.HomeTime();
    TDateTime tdtime = iStartTime.DateTime();
    // seed random with current microseconds, or so....  whatever.
    srand( (((tdtime.Minute() * 60) + tdtime.Second()) * 1000 )
           + tdtime.MicroSecond() );

#ifdef MEM_DEBUG
    mpool = mpool_make();
#endif

    iVtMgr = make_vtablemgr( MPPARM_NOCOMMA(mpool) );
    User::LeaveIfNull( iVtMgr );

    iUtil.vtable = (UtilVtable*)XP_MALLOC( mpool, sizeof( UtilVtable ) );
    User::LeaveIfNull( iUtil.vtable );
    SetUpUtil();

    iDraw = sym_drawctxt_make( MPPARM(mpool) &SystemGc(), iCoeEnv );
    User::LeaveIfNull( iDraw );

    InitGameL();

    PositionBoard();
    (void)server_do( iGame.server ); // get tiles assigned etc.
} // ConstructL

// Draw this application's view to the screen
void CXWordsAppView::Draw(const TRect& aRect) const
{
    // Draw the parent control
    //CEikBorderedControl::Draw(aRect);

    // Get the standard graphics context 
    CWindowGc& gc = SystemGc();
    
    // Gets the control's extent - Don't encroach on the border
//     TRect rect = Border().InnerRect(Rect());
    TRect rect = Rect();

    // Ensure that the border is not overwritten by future drawing operations
    gc.SetClippingRect( rect );

    XP_LOGF( "Draw beginning" );

    XP_LOGF( "clipped rect : %d x %d", rect.Width(), rect.Height() );

    if ( iGame.board ) {
        // This must go!  Board needs a method to inval within a rect.
        // But without this when a menu or other obscuring window goes
        // away we get called to redraw but board thinks nothing needs
        // drawing.
        board_invalAll( iGame.board );

        board_draw( iGame.board );
    }

    XP_LOGF( "Draw done" );
} // Draw

/* static */ VTableMgr*
CXWordsAppView::sym_util_getVTManager( XW_UtilCtxt* uc )
{
    CXWordsAppView* self = (CXWordsAppView*)uc->closure;
    return self->iVtMgr;
} /* sym_util_getVTManager */

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

    if ( col > 7 ) col = fourteen - col;
    if ( row > 7 ) row = fourteen - row;
    index = (row*8) + col;
    if ( index >= 8*8 ) {
        result = (XWBonusType)EM;
    } else {
        result = (XWBonusType)defaultBoard[index];
    }
    return result;
} // sym_util_getSquareBonus

static void
sym_util_userError( XW_UtilCtxt* uc, UtilErrID id )
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

    if ( resourceId != 0 ) {
        _LIT(title,"Oops");
        TBuf16<128> message;
        StringLoader::Load( message, resourceId );
        CCknInfoDialog::RunDlgLD( title, message );
    }
} // sym_util_userError

static XP_Bool
sym_util_userQuery( XW_UtilCtxt* uc, UtilQueryID id,
                    XWStreamCtxt* stream )
{
    XP_Bool clickedOk;

    CXWAskDlg* genericQuery = new(ELeave)CXWAskDlg( MPPARM(uc->mpool) 
                                                    stream, EFalse );
    clickedOk = genericQuery->ExecuteLD( R_XWORDS_CONFIRMATION_QUERY );
 
    return clickedOk;
}

static XP_S16
sym_util_userPickTile( XW_UtilCtxt* uc, PickInfo* pi, 
                       XP_U16 playerNum,
                       XP_UCHAR4* texts, XP_U16 nTiles )
{
    return 0;
}

static XP_Bool
sym_util_askPassword( XW_UtilCtxt* uc, const XP_UCHAR* name,
                      XP_UCHAR* buf, XP_U16* len )
{
    return XP_TRUE;
}

static void
sym_util_trayHiddenChange(XW_UtilCtxt* uc, XW_TrayVisState newState )
{
}

static void
sym_util_yOffsetChange(XW_UtilCtxt* uc, XP_U16 oldOffset, XP_U16 newOffset )
{
}

static void
sym_util_notifyGameOver( XW_UtilCtxt* uc )
{
}

static XP_Bool
sym_util_hiliteCell( XW_UtilCtxt* /*uc*/, XP_U16 /*col*/, XP_U16 /*row*/ )
{
    return XP_TRUE;             // don't exit early
}

static XP_Bool
sym_util_engineProgressCallback( XW_UtilCtxt* uc )
{
#ifdef SHOW_PROGRESS
#endif
    return XP_TRUE;
}

static void
sym_util_setTimer( XW_UtilCtxt* uc, XWTimerReason why )
{
}

/* static */ void
CXWordsAppView::sym_util_requestTime( XW_UtilCtxt* uc )
{
    XP_LOGF( "sym_util_requestTime called" );

    CXWordsAppView* self = (CXWordsAppView*)uc->closure;

    // Only start it if it's not already running!!
    if ( ++self->iTimerRunCount == 1 ) {
        self->iRequestTimer->Start( TCallBack( CXWordsAppView::TimerCallback, 
                                               (TAny*)self ) );
    }


//     TApaTaskList atl( self->iCoeEnv->WsSession() );
//     TApaTask curTask = atl.FindByPos( 0 );
// 	TInt i = curTask.SendMessage( XW_TIMEREQ_COMMAND, NULL );

//     XP_LOGF( "SendMessage=>%d", i );

    // Setting a timer is wrong here!  All that's needed is to post a
    // freaking event we'll handle once we unwind from the
    // stack handling the current event.
//     XP_LOGF( "sym_util_requestTime returning" );
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

static DictionaryCtxt*
sym_util_makeEmptyDict( XW_UtilCtxt* uc )
{
    return NULL;
}

static XP_UCHAR*
sym_util_getUserString( XW_UtilCtxt* uc, XP_U16 stringCode )
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
sym_util_warnIllegalWord( XW_UtilCtxt* uc, BadWordInfo* bwi, 
                          XP_U16 turn, XP_Bool turnLost )
{
    return XP_FALSE;
}

#ifdef XWFEATURE_SEARCHLIMIT
static XP_Bool
sym_util_getTraySearchLimits(XW_UtilCtxt* uc, XP_U16* min, XP_U16* max )
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
    vtable->m_util_getSquareBonus = sym_util_getSquareBonus;
    vtable->m_util_userError = sym_util_userError;
    vtable->m_util_userQuery = sym_util_userQuery;
    vtable->m_util_userPickTile = sym_util_userPickTile;
    vtable->m_util_askPassword = sym_util_askPassword;
    vtable->m_util_trayHiddenChange = sym_util_trayHiddenChange;
    vtable->m_util_yOffsetChange = sym_util_yOffsetChange;
    vtable->m_util_notifyGameOver = sym_util_notifyGameOver;
    vtable->m_util_hiliteCell = sym_util_hiliteCell;
    vtable->m_util_engineProgressCallback = sym_util_engineProgressCallback;
    vtable->m_util_setTimer = sym_util_setTimer;
    vtable->m_util_requestTime = sym_util_requestTime;
    vtable->m_util_getCurSeconds = sym_util_getCurSeconds;
    vtable->m_util_makeEmptyDict = sym_util_makeEmptyDict;
    vtable->m_util_getUserString = sym_util_getUserString;
    vtable->m_util_warnIllegalWord = sym_util_warnIllegalWord;
#ifdef XWFEATURE_SEARCHLIMIT
    vtable->m_util_getTraySearchLimits = sym_util_getTraySearchLimits;
#endif
}

// Load the current game from prefs or else create a new one.
// Eventually this will involve putting up the new game dialog.
void
CXWordsAppView::InitGameL()
{
    DictionaryCtxt* dict = NULL;
    iCp.showBoardArrow = XP_TRUE; // default because no pen
    iCp.showRobotScores = XP_FALSE;

    gi_initPlayerInfo( MPPARM(mpool) &iGi, (XP_UCHAR*)"Player %d" );

    game_makeNewGame( MPPARM(mpool) &iGame, &iGi, 
                      &iUtil, iDraw, &iCp,
                      (TransportSend)NULL, this );


    TFileName nameD;
    CEikFileOpenDialog* dictDlg = new(ELeave)CEikFileOpenDialog( &nameD );
    XP_LOGF( "setting required type" );
    dictDlg->SetRequiredExtension( &_L(".xwd") ); // it's ignoring this
    dictDlg->SetShowSystem( EFalse );
    if ( dictDlg->ExecuteLD( R_EIK_DIALOG_FILE_OPEN ) ) {
        TBuf8<256> buf8;
        buf8.Copy( nameD );
        char buf[257];
        TInt len = buf8.Length();
        XP_MEMCPY( buf, (void*)buf8.Ptr(), len );
        buf[len] = '\0';
        XP_LOGF( "got file %s", buf );
        
        dict = sym_dictionary_makeL( MPPARM(mpool) &nameD );
    }

#ifdef STUBBED_DICT
    if ( !dict ) {
        dict = make_stubbed_dict( MPPARM_NOCOMMA(mpool) );
    }
#endif
    User::LeaveIfNull( dict );

    model_setDictionary( iGame.model, dict ); // game_dispose kills this

//     CurGameInfo gameInfo;    
//     CNewGameDialog* dialog = new(ELeave) CNewGameDialog( &gameInfo );
//     User::Leave( -1 );
}

void
CXWordsAppView::DeleteGame()
{
    game_dispose( &iGame );
    gi_disposePlayerInfo( MPPARM(mpool) &iGi );
 
}

void
CXWordsAppView::PositionBoard() 
{
    if ( iBoardPosInval ) {
        iBoardPosInval = XP_FALSE;

        board_setPos( iGame.board, 2, 2, XP_FALSE );
        board_setScale( iGame.board, scaleBoardH, scaleBoardV );

        board_setScoreboardLoc( iGame.board, 2 + (15 * scaleBoardH) + 5, 2,
                                200, 130, XP_FALSE );
        board_setYOffset( iGame.board, 0, XP_FALSE );

        board_setTrayLoc( iGame.board, 
                          (15 * scaleBoardH) + 5, // to right of board
                          // make tray bottom same as board's
                          2 + (15*scaleBoardV) - scaleTrayV,
                          scaleTrayH, scaleTrayV,   // v and h scale
                          3 );      // divider width
    }

} // PositionBoard

int 
CXWordsAppView::HandleCommand( TInt aCommand )
{
    XP_Bool draw = XP_FALSE;
    XP_Bool notDone;

    switch ( aCommand ) {

//     case XW_TIMEREQ_COMMAND:
//         XP_LOGF( "got XW_TIMEREQ_COMMAND" );
//         draw = server_do( iGame.server ); // get tiles assigned etc.
//         break;

    case XW_NEWGAME_COMMAND:
    case XW_SAVEDGAMES_COMMAND:
    case XW_PREFS_COMMAND:
    case XW_ABOUT_COMMAND:

    case XW_VALUES_COMMAND:
    case XW_REMAIN_COMMAND:
    case XW_CURINFO_COMMAND:
    case XW_HISTORY_COMMAND:
    case XW_FINALSCORES_COMMAND:

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

    default:
        return 0;
    }

    if ( draw ) {
        DrawDeferred();
    }

    return 1;
} // CXWordsAppView::HandleCommand

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

//     XP_CURSOR_KEY_DEL,

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
        break;
    }

    if ( iGame.board != NULL ) {
        if ( key != XP_KEY_NONE ) {
            draw = board_handleKey( iGame.board, key );
        }
    }

    if ( draw ) {
        DrawNow();
    }

    // handled if it's one we recognize.  This is probably too broad!!!
    return key != XP_KEY_NONE;
}

/* static */ TInt
CXWordsAppView::TimerCallback( TAny* aPtr )
{
    CXWordsAppView* self = (CXWordsAppView*)aPtr;

    if ( server_do( self->iGame.server ) ) {
        self->DrawDeferred();
    }

    return --self->iTimerRunCount > 0;
}

