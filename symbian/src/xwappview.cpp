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

#if defined SERIES_60
# include <w32std.h>
# include <eikinfo.h>
#elif defined SERIES_80
# include <cknenv.h>
# include <ckninfo.h>
# include <eikcfdlg.h>
# include <eikfile.rsg>
#endif

#include <stringloader.h>
#include <stdlib.h>             // for srand
#include <s32file.h>

#include "xwords.rsg"

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
    XP_MEMSET( &iGame, 0, sizeof(iGame) );

    iCurGameName.Copy( _L("") );
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

    delete iDictList;
    delete iRequestTimer;
    delete iGamesMgr;
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

    iDraw = sym_drawctxt_make( MPPARM(mpool) &SystemGc(), iCoeEnv, iEikonEnv );
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
        // This must go!  Board needs a method to inval within a rect.
        // But without this when a menu or other obscuring window goes
        // away we get called to redraw but board thinks nothing needs
        // drawing.
        board_invalAll( iGame.board );

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
sym_util_trayHiddenChange(XW_UtilCtxt* /*uc*/, XW_TrayVisState /*newState*/ )
{
}

static void
sym_util_yOffsetChange(XW_UtilCtxt* /*uc*/, XP_U16 /*oldOffset*/, XP_U16 /*newOffset*/ )
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

static void
sym_util_setTimer( XW_UtilCtxt* /*uc*/, XWTimerReason /*why*/ )
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
                                                 NULL );
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
        gi_initPlayerInfo( MPPARM(mpool) &iGi, (XP_UCHAR*)"Player %d" );

        TGameInfoBuf gib( &iGi, iDictList );
        if ( !CXWGameInfoDlg::DoGameInfoDlgL( MPPARM(mpool) &gib, ETrue ) ) {
            User::Leave(-1);
        }

        gib.CopyToL( MPPARM(mpool) &iGi );

        DictionaryCtxt* dict = sym_dictionary_makeL( MPPARM(mpool) 
                                                     iGi.dictName );
        User::LeaveIfNull( dict );

        XP_U16 newGameID = SC( XP_U16, sym_util_getCurSeconds( &iUtil ) );
        game_makeNewGame( MPPARM(mpool) &iGame, &iGi, 
                          &iUtil, iDraw, newGameID, &iCp,
                          (TransportSend)NULL, this );
        model_setDictionary( iGame.model, dict );

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
    board_setPos( iGame.board, 2, 2, XP_FALSE );
    board_setScale( iGame.board, scaleBoardH, scaleBoardV );

    TInt scoreLeft = 2 + (15 * scaleBoardH) + 5;
    board_setScoreboardLoc( iGame.board, scoreLeft, 25,
                            200, 125, XP_FALSE );
    board_setYOffset( iGame.board, 0, XP_FALSE );

    board_setTrayLoc( iGame.board, 
                      (15 * scaleBoardH) + 5, // to right of board
                      // make tray bottom same as board's
                      2 + (15*scaleBoardV) - scaleTrayV,
                      scaleTrayH, scaleTrayV,   // v and h scale
                      3 );      // divider width
    board_invalAll( iGame.board );

    iTitleBox.SetRect( scoreLeft, 2, 
                       scoreLeft + (scaleTrayH * MAX_TRAY_TILES) + 3, 
                       2 + 20 );
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
                server_formatDictCounts( iGame.server, stream, 4 ); /* 4: ncols */
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
    CXWordsAppView* self = (CXWordsAppView*)aPtr;

    if ( server_do( self->iGame.server ) ) {
        self->DoImmediateDraw();
    }

    return --self->iTimerRunCount > 0;
}

XWStreamCtxt* 
CXWordsAppView::MakeSimpleStream( MemStreamCloseCallback cb )
{
    return mem_stream_make( MPPARM(mpool)
                            iVtMgr, (void*)this, 
                            CHANNEL_NONE, cb );
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
    RFs fileSession;
    User::LeaveIfError(fileSession.Connect());
    CleanupClosePushL(fileSession);

    TFindFile file_finder( fileSession ); // 1
    CDir* file_list;
    _LIT( aWildName, "*.xwd" );

    TFileName dir;
    GetXwordsRWDir( &dir, EDictsLoc );

    TInt err = file_finder.FindWildByDir( aWildName, dir, file_list );
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
    CleanupStack::PopAndDestroy( &fileSession ); 
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
    aPathRef->Delete( 0, aPathRef->Length() );

    switch( aWhy ) {
    case EGamesLoc:
        aPathRef->Append( _L("C:") ); /* read-write: must be on C */
        break;
    case EDictsLoc:
#if defined __WINS__
        aPathRef->Append( _L("Z:") );
#elif defined __MARM__
        aPathRef->Append( _L("C:") );
#endif
        break;
    case EPrefsLoc:
        break;                  /* don't want a drive */
    }

    _LIT( dir,"\\system\\apps\\XWORDS\\" );
    aPathRef->Append( dir );
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
    TInt err = reader.Open( fs, filename, EFileRead );

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
    User::LeaveIfError( writer.Replace( fs, filename, EFileWrite ) );

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
    iCurGameName.Delete( 0, 1000 );
}

TBool
CXWordsAppView::DoNewGame()
{
    TBool draw = EFalse;

    TBool save = AskSaveGame();

    TGameInfoBuf gib( &iGi, iDictList );
    if ( CXWGameInfoDlg::DoGameInfoDlgL( MPPARM(mpool) &gib, ETrue ) ) {

        if ( save ) {
            StoreOneGameL( &iCurGameName );
            iGamesMgr->MakeDefaultName( &iCurGameName );
        }

        gib.CopyToL( MPPARM(mpool) &iGi );
        XP_U16 newGameID = SC( XP_U16,sym_util_getCurSeconds( &iUtil ) );
        game_reset( MPPARM(mpool) &iGame, &iGi, newGameID,
                    &iCp, (TransportSend)NULL, this );

        DictionaryCtxt* prevDict = model_getDictionary( iGame.model );
        if ( 0 != XP_STRCMP( dict_getName(prevDict), iGi.dictName ) ) {
            dict_destroy( prevDict );
            DictionaryCtxt* dict = sym_dictionary_makeL( MPPARM(mpool) 
                                                         iGi.dictName );
            model_setDictionary( iGame.model, dict );
        }

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

    iGamesMgr->LoadGameL( aGameName, stream );
    
    XP_U16 len = stream_getU8( stream );
    XP_UCHAR dictName[32];
    stream_getBytes( stream, dictName, len );
    dictName[len] = '\0';

    DictionaryCtxt* dict = sym_dictionary_makeL( MPPARM(mpool) dictName );
    XP_ASSERT( !!dict );

    game_makeFromStream( MPPARM(mpool) stream, &iGame, 
                         &iGi, dict, &iUtil, iDraw, &iCp,
                         (TransportSend)NULL, this );
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
    CWindowGc& gc = SystemGc();

    gc.UseFont( iCoeEnv->NormalFont() );

    TBuf16<48> buf( _L( "Game: " ) );
    buf.Append( iCurGameName );
/*     gc.DrawText( buf, iGameNameLoc ); */

    gc.SetPenStyle( CGraphicsContext::ESolidPen );
    gc.SetPenColor( KRgbBlack );
    gc.SetBrushStyle( CGraphicsContext::ENullBrush );

    gc.DrawText( buf, iTitleBox, iTitleBox.Height() - 2,/*TInt aBaselineOffset*/
                 CGraphicsContext::ECenter );

    gc.DiscardFont();
}
