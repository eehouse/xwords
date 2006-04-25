/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 1999-2002 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#define TEST_CPP 1

#include <assert.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <timer.h> /* for time_get_onOS */
#include <sys/time.h>

#include "sys.h"
#include "gui.h"
#include "OpenDatabases.h"
#include "comms.h"		/* for CHANNEL_NONE */
#include "LocalizedStrIncludes.h"
//  #include "FieldMgr.h"

#include "ebm_object.h"

extern "C" {
#include <evnt_fun.h> /* jonathan_yavner@franklin.com says put in extern "C" */
#include "xptypes.h"
#include "game.h"
#include "vtabmgr.h"
#include "dictnry.h"
#include "util.h"
#include "memstream.h"
#include "strutils.h"
}

#include "frankmain.h"
#include "frankdraw.h"
#include "frankdict.h"
#include "frankpasswd.h"
#include "frankdlist.h"
#include "frankshowtext.h"
/* #include "frankask.h" */
#include "frankletter.h"
#include "frankplayer.h"
#include "frankgamesdb.h"
#include "franksavedgames.h"
#include "bmps_includes.h"
/* #include "browser.h" */

extern "C" {
#include "lcd.h"
#include "ereader_hostio.h"
}

#include "frankids.h"
class CXWordsWindow;

enum { HINT_REQUEST, SERVER_TIME_REQUEST, NEWGAME_REQUEST, 
       FINALSCORE_REQUEST };

CXWordsWindow *MainWindow;
/* CLabel *Repeat_Label; */
CMenuBar MainMenuBar( MENUBAR_WINDOW_ID, 23 );
/* CPushButton *Edit_Button; */

/* Addrtest variables */
/* class CRecordWindow; */
/* COpenDB DBase; */
//  CFMDatabase FMgr;
/* CRecordWindow *AddrWindow; */


/* callbacks */
static VTableMgr* frank_util_getVTManager( XW_UtilCtxt* uc );
static DictionaryCtxt* frank_util_makeEmptyDict( XW_UtilCtxt* uc );
static void frank_util_userError( XW_UtilCtxt* uc, UtilErrID id );
static XP_Bool frank_util_userQuery( XW_UtilCtxt* uc, UtilQueryID id,
                                     XWStreamCtxt* stream );
static XP_S16 frank_util_userPickTile( XW_UtilCtxt* uc, const PickInfo* pi, 
                                       XP_U16 playerNum,
                                       const XP_UCHAR4* texts, XP_U16 nTiles );
static XP_Bool frank_util_askPassword( XW_UtilCtxt* uc, const XP_UCHAR* name, 
                                       XP_UCHAR* buf, XP_U16* len );
static void frank_util_trayHiddenChange( XW_UtilCtxt* uc, 
                                         XW_TrayVisState newState );
static void frank_util_notifyGameOver( XW_UtilCtxt* uc );
static XP_Bool frank_util_hiliteCell( XW_UtilCtxt* uc, 
                                      XP_U16 col, XP_U16 row );
static XP_Bool frank_util_engineProgressCallback( XW_UtilCtxt* uc );
static void frank_util_setTimer( XW_UtilCtxt* uc, XWTimerReason why, 
                                 XP_U16 when,
                                 TimerProc proc, void* closure );
static void frank_util_requestTime( XW_UtilCtxt* uc );
static XP_U32 frank_util_getCurSeconds( XW_UtilCtxt* uc );
static XWBonusType frank_util_getSquareBonus( XW_UtilCtxt* uc, 
                                              ModelCtxt* model,
                                              XP_U16 col, XP_U16 row );
static XP_UCHAR* frank_util_getUserString( XW_UtilCtxt* uc, XP_U16 stringCode );
static XP_Bool frank_util_warnIllegalWord( XW_UtilCtxt* uc, BadWordInfo* bwi, 
                                           XP_U16 turn, XP_Bool turnLost );
static void frank_util_engineStarting( XW_UtilCtxt* uc, XP_U16 nBlanks );
static void frank_util_engineStopping( XW_UtilCtxt* uc );


typedef struct FrankSavedState {
    U32 magic;
    U16 curGameIndex;
    XP_Bool showProgress;
} FrankSavedState;


CXWordsWindow
class CXWordsWindow : public CWindow {
 public:    /* so util functions can access */
    XWGame fGame;
    CurGameInfo fGameInfo;
    VTableMgr* fVTableMgr;

 private:
    FrankDrawCtx* draw;
    DictionaryCtxt* dict;
    XW_UtilCtxt util;
    FrankDictList* fDictList;

    FrankSavedState fState;

    CGamesDB* gamesDB;

    CommonPrefs cp;

    RECT fProgressRect;
    U16 fProgressCurLine;

    /* There's a wasted slot here, but it simplifies indexing */
    TimerProc fTimerProcs[NUM_TIMERS_PLUS_ONE];
    void* fTimerClosures[NUM_TIMERS_PLUS_ONE];

    XP_U8 phoniesAction;
    BOOL penDown;
    BOOL drawInProgress;
    BOOL userEventPending;
    XP_Bool fRobotHalted;
    XP_Bool fAskTrayLimits;

public:
    CXWordsWindow( MPFORMAL FrankDictList* dlist );
    ~CXWordsWindow();
//      void init();
    void Draw();
    S32 MsgHandler( MSG_TYPE type, CViewable *from, S32 data );

    void setUserEventPending() { this->userEventPending = TRUE; }
    void clearUserEventPending() { this->userEventPending = FALSE; }
    BOOL getUserEventPending() { return this->userEventPending; }
    void setTimerImpl( XWTimerReason why, 
                       TimerProc proc, void* closure );
    void setTimerIfNeeded();
    void fireTimer( XWTimerReason why );
    XP_Bool robotIsHalted() { return fRobotHalted; }
    void updateCtrlsForTray( XW_TrayVisState newState );
    void startProgressBar();
    void advanceProgressBar();
    void finishProgressBar();

 private:
    CButton* addButtonAt( short id, short x, short y, char* str );
    CButton* addButtonAtBitmap( short id, short x, short y, const char* c,
				IMAGE* img );
    void initUtil();
    void initPrefs();
    void loadPrefs();
    void loadGameFromStream( XWStreamCtxt* inStream );
    void makeNewGame( U16 newIndex );
    void loadCurrentGame();
    void saveCurrentGame();
    void resetCurrentGame();
    void positionBoard();
    void disOrEnableFrank( U16 id, XP_Bool enable );
    
    void writeGameToStream( XWStreamCtxt* stream, U16 index );
    XWStreamCtxt* makeMemStream();

 public:
    void doCommit();
    void doHint( XP_Bool reset );
    void doUndo();
    void doHideTray();
    void doHeapDump();

    BOOL newGame( XP_Bool allowCancel );
    void gameInfo();
    void doNewGameMenu();
#ifndef HASBRO_EBM
    void doSavedGames();
#endif
    void doAbout();
    void doEndGame();
    void doTileValues();
    void doGameHistory();

    void fni();
    void displayFinalScores();
    XP_U16 displayTextFromStream( XWStreamCtxt* stream, const char* title,
				  BOOL killStream = TRUE,
				  BOOL includeCancel = FALSE );
    void wrappedEventLoop( CWindow* window );

    MPSLOT
}; /* class CXWordsWindow */

#ifdef MEM_DEBUG
#define MEMPOOL(t) (t)->mpool,
#define MEMPOOL_NOCOMMA(t) (t)->mpool
#else
#define MEMPOOL_NOCOMMA(t)
#define MEMPOOL(t)
#endif

#define V_BUTTON_SPACING 17
#define BUTTON_LEFT 183

CXWordsWindow::CXWordsWindow(MPFORMAL FrankDictList* dlist )
    : CWindow(MAIN_WINDOW_ID, 0, 0, 200, 240, 
			  "Crosswords 4" 
)
{
    short buttonTop = BOARD_TOP;
    MPASSIGN( this->mpool, mpool );
    fDictList = dlist;

    fVTableMgr = make_vtablemgr(MPPARM_NOCOMMA(mpool));

    this->gamesDB = new CGamesDB( MEMPOOL(this) (XP_UCHAR*)"xwords_games" );

    this->penDown = FALSE;
    this->drawInProgress = FALSE;
    fRobotHalted = XP_FALSE;

    this->cp.showBoardArrow = XP_TRUE;
    this->cp.showRobotScores = XP_FALSE; /* No ui to turn on/off yet!! */

    CButton* button;
    button = addButtonAtBitmap( MAIN_FLIP_BUTTON_ID, BUTTON_LEFT, buttonTop, 
                                "F", (IMAGE*)&flip );
    buttonTop += button->GetHeight() + 2;
    button = addButtonAtBitmap( MAIN_VALUE_BUTTON_ID, BUTTON_LEFT, buttonTop, 
                                "V", (IMAGE*)&valuebutton );
    buttonTop += button->GetHeight() + 2;
    button = addButtonAtBitmap( MAIN_HINT_BUTTON_ID, BUTTON_LEFT,
                                buttonTop, "?", (IMAGE*)&lightbulb );
    buttonTop += button->GetHeight() + 2;
    (void)addButtonAtBitmap( MAIN_UNDO_BUTTON_ID, BUTTON_LEFT, buttonTop,
                             "U", (IMAGE*)&undo );
    
    fProgressRect.y = buttonTop + V_BUTTON_SPACING + 2;

    /* now start drawing from the bottom */
    buttonTop = 205;
    button = addButtonAt( MAIN_COMMIT_BUTTON_ID,
                          BUTTON_LEFT, buttonTop, "D" );
    (void)addButtonAt( MAIN_HIDE_BUTTON_ID, 
                       BUTTON_LEFT - button->GetWidth(), buttonTop, "H" );
    buttonTop -= V_BUTTON_SPACING;
    (void)addButtonAt( MAIN_TRADE_BUTTON_ID, BUTTON_LEFT, buttonTop, "T" );
    buttonTop -= V_BUTTON_SPACING;
    (void)addButtonAt( MAIN_JUGGLE_BUTTON_ID, BUTTON_LEFT, buttonTop, "J" );

    this->draw = (FrankDrawCtx*)frank_drawctxt_make( MEMPOOL(this) this );

    fProgressRect.x = BUTTON_LEFT + 2;
    fProgressRect.width = 10;
    fProgressRect.height = buttonTop - fProgressRect.y - 2;

    this->initUtil();

    fGame.model = (ModelCtxt*)NULL;
    fGame.server = (ServerCtxt*)NULL;
    fGame.board = (BoardCtxt*)NULL;

    fAskTrayLimits = XP_FALSE;

    gi_initPlayerInfo( MEMPOOL(this) &fGameInfo, (XP_UCHAR*)"Player %d" );

    U16 nRecords = gamesDB->countRecords();
    if ( nRecords == 0 ) {	/* 1 for prefs, 1 for first game */
        initPrefs();

        fGameInfo.serverRole = SERVER_STANDALONE;
        /* 	fGameInfo.timerEnabled = XP_TRUE; */
        makeNewGame( fState.curGameIndex );

        GUI_EventMessage( MSG_USER, this, NEWGAME_REQUEST );
    } else {
        XP_ASSERT( nRecords >= 2 );
        loadPrefs();

        /* there needs to be a "game" for the saved one to be loaded into. */
        game_makeNewGame( MPPARM(mpool) &fGame, &fGameInfo, &this->util, 
                          (DrawCtx*)this->draw, 0, &this->cp,
                          (TransportSend)NULL, NULL);
        loadCurrentGame();

        positionBoard();
        server_do( fGame.server ); /* in case there's a robot */
        board_invalAll( fGame.board );
    }
} /* CXWordsWindow::CXWordsWindow */

XWStreamCtxt*
CXWordsWindow::makeMemStream()
{
    XWStreamCtxt* stream = mem_stream_make( MEMPOOL(this) 
                                            fVTableMgr,
                                            this, 
                                            CHANNEL_NONE, 
                                            (MemStreamCloseCallback)NULL );
    return stream;
} /* makeMemStream */

void
CXWordsWindow::saveCurrentGame()
{
    XWStreamCtxt* stream = makeMemStream();
    writeGameToStream( stream, fState.curGameIndex );

    U16 len = stream_getSize( stream );
    void* ptr = XP_MALLOC( this->mpool, len );
    stream_getBytes( stream, ptr, len );
    this->gamesDB->putNthRecord( fState.curGameIndex, ptr, len );
    XP_FREE( this->mpool, ptr );

    stream_destroy( stream );
} /* saveCurrentGame */

void
CXWordsWindow::positionBoard()
{
    board_setPos( fGame.board, BOARD_LEFT, BOARD_TOP, 
                  XP_FALSE );
    board_setScale( fGame.board, BOARD_SCALE, BOARD_SCALE );

    board_setScoreboardLoc( fGame.board, SCORE_LEFT, SCORE_TOP,
                            this->GetWidth()-SCORE_LEFT-TIMER_WIDTH, 
                            SCORE_HEIGHT, XP_TRUE );

    U16 trayTop = BOARD_TOP + (BOARD_SCALE * 15) + 2;
    board_setTrayLoc( fGame.board, TRAY_LEFT, trayTop, 
                      MIN_TRAY_SCALE, MIN_TRAY_SCALE, 
                      FRANK_DIVIDER_WIDTH );

    board_setTimerLoc( fGame.board, 
                       this->GetWidth() - TIMER_WIDTH, 
                       SCORE_TOP, TIMER_WIDTH, TIMER_HEIGHT );
} /* positionBoard */

void
CXWordsWindow::makeNewGame( U16 newIndex )
{
    XP_U32 gameID = frank_util_getCurSeconds( &this->util );
    if ( !!fGame.model ) {
        saveCurrentGame();
        game_reset( MEMPOOL(this) &fGame, &fGameInfo, &this->util, 
                    gameID, &this->cp, (TransportSend)NULL, NULL );
    } else {
        game_makeNewGame( MPPARM(mpool) &fGame, &fGameInfo, &this->util, 
                          (DrawCtx*)this->draw, gameID, &this->cp, 
                          (TransportSend)NULL, NULL);
        positionBoard();
    }
    this->gamesDB->putNthName( newIndex, (XP_UCHAR*)"untitled game" );
    fState.curGameIndex = newIndex;
} /* makeNewGame */

CButton*
CXWordsWindow::addButtonAt( short id, short x, short y, char* str )
{
    CButton* button = new CButton( id, 0, 0, str );
    this->AddChild( button, x, y );
    return button;
} /* addButtonAt */

CButton* 
CXWordsWindow::addButtonAtBitmap( short id, short x, short y, const char* c,
				  IMAGE* img )
{
    CButton* button = new CButton( id, 0, 0, (const char*)NULL,
                                   img, (IMAGE*)NULL, (IMAGE*)NULL );
    this->AddChild( button, x+1, y );

    return button;
} /* addButtonAtBitmap */

void
CXWordsWindow::initUtil()
{
    UtilVtable* vtable = this->util.vtable = new UtilVtable;
    this->util.closure = (void*)this;
    this->util.gameInfo = &fGameInfo;
    MPASSIGN( this->util.mpool, mpool );

/*     vtable->m_util_makeStreamFromAddr = NULL; */
    vtable->m_util_getVTManager = frank_util_getVTManager;
    vtable->m_util_makeEmptyDict = frank_util_makeEmptyDict;
/*     vtable->m_util_yOffsetChange = NULL <--no scrolling */
    vtable->m_util_userError = frank_util_userError;
    vtable->m_util_userQuery = frank_util_userQuery;
    vtable->m_util_userPickTile = frank_util_userPickTile;
    vtable->m_util_askPassword = frank_util_askPassword;
    vtable->m_util_trayHiddenChange = frank_util_trayHiddenChange;
    vtable->m_util_notifyGameOver = frank_util_notifyGameOver;
    vtable->m_util_hiliteCell = frank_util_hiliteCell;
    vtable->m_util_engineProgressCallback = frank_util_engineProgressCallback;
    vtable->m_util_setTimer = frank_util_setTimer;
    vtable->m_util_requestTime = frank_util_requestTime;
    vtable->m_util_getCurSeconds = frank_util_getCurSeconds;

    vtable->m_util_getSquareBonus = frank_util_getSquareBonus;
    vtable->m_util_getUserString = frank_util_getUserString;
    vtable->m_util_warnIllegalWord = frank_util_warnIllegalWord;
#ifdef SHOW_PROGRESS
    vtable->m_util_engineStarting = frank_util_engineStarting;
    vtable->m_util_engineStopping = frank_util_engineStopping;
#endif
} /* initUtil */

CXWordsWindow::~CXWordsWindow()
{
    XP_WARNF( "~CXWordsWindow(this=%p) called", this );

    if ( !!this->gamesDB ) {
        this->gamesDB->putNthRecord( 0, &fState, sizeof(fState) );
        saveCurrentGame();

        delete this->gamesDB;
        this->gamesDB = (CGamesDB*)NULL;
    }
    delete fDictList;
}

void CXWordsWindow::Draw()
{
    if ( !this->drawInProgress ) {
        this->drawInProgress = TRUE;
        // don't call CWindow::Draw();  It erases the entire board
        board_draw( fGame.board );

        this->DrawChildren();
        this->drawInProgress = FALSE;
    }
} // CXWordsWindow::Draw

S32
CXWordsWindow::MsgHandler( MSG_TYPE type, CViewable *from, S32 data )
{
    S32 result = 0;
    XP_Key xpkey;
    S16 drag_x;
    S16 drag_y;
    XP_Bool handled;

    drag_x = (S16) (data >> 16);
    drag_y = (S16) data;

    GUI_DisableTimers();
    switch (type) {

    case MSG_USER:
        switch ( data ) {
        case HINT_REQUEST:
            doHint( XP_FALSE );	/* will reset if fails */
            break;
        case SERVER_TIME_REQUEST:
            this->clearUserEventPending(); /* clear before calling server! */
            if ( server_do( fGame.server ) ) {
                GUI_NeedUpdate();
            }
            break;
        case NEWGAME_REQUEST:
            this->newGame( XP_FALSE );
            break;
        case FINALSCORE_REQUEST:
            this->displayFinalScores();
            break;
        }
        break;

    case MSG_PEN_DOWN:
        this->penDown = TRUE;
        if ( board_handlePenDown( fGame.board, drag_x, drag_y, &handled ) ) {
            GUI_NeedUpdate();
            result = 1;
        }
        board_pushTimerSave( fGame.board );
        break;

    case MSG_PEN_TRACK:
        if ( this->penDown ) {
            if ( board_handlePenMove( fGame.board, drag_x, drag_y ) ) {
                GUI_NeedUpdate();
                result = 1;
            }
        }
        break;

    case MSG_PEN_UP:
        if ( this->penDown ) {
            board_popTimerSave( fGame.board );
            if ( board_handlePenUp( fGame.board, drag_x, drag_y, 0 ) ) {
                GUI_NeedUpdate();
                result = 1;
            }
            this->penDown = FALSE;
        }
        break;

    case MSG_TIMER:
        fireTimer( (XWTimerReason)data );
        setTimerIfNeeded();
        GUI_NeedUpdate();	/* Needed off-emulator? PENDING */
        break;

    case MSG_BUTTON_SELECT:
        result = 1;
        switch (from->GetID()) {
        case MAIN_FLIP_BUTTON_ID:
            if ( board_flip( fGame.board ) ) {
                GUI_NeedUpdate();
            }
            break;
	    
        case MAIN_VALUE_BUTTON_ID:
            if ( board_toggle_showValues( fGame.board ) ) {
                GUI_NeedUpdate();
            }
            break;

        case MAIN_HINT_BUTTON_ID:
            this->doHint( XP_FALSE );
            break;

        case MAIN_UNDO_BUTTON_ID:
            this->doUndo();
            break;

        case MAIN_COMMIT_BUTTON_ID:
            this->doCommit();
            break;

        case MAIN_TRADE_BUTTON_ID:
            if ( board_beginTrade( fGame.board ) ) {
                GUI_NeedUpdate();
            }
            break;

        case MAIN_JUGGLE_BUTTON_ID:
            if ( board_juggleTray( fGame.board ) ) {
                GUI_NeedUpdate();
            }
            break;
        case MAIN_HIDE_BUTTON_ID:
            this->doHideTray();
            break;
        default:
            result = 0;
        }
        break;

    case MSG_KEY:
        xpkey = XP_KEY_NONE;
        switch( data ) {

        case K_JOG_ENTER:
            xpkey = XP_RETURN_KEY;
            break;

        case K_JOG_DOWN:
            xpkey = XP_CURSOR_KEY_RIGHT;
            break;

        case K_JOG_UP:
            xpkey = XP_CURSOR_KEY_LEFT;
            break;

        case K_DELETE:
        case K_BACKSPACE:
            xpkey = XP_CURSOR_KEY_DEL;
            break;

        default:
            if ( isalpha( data ) ) {
                xpkey = (XP_Key)toupper(data);
            }
            break;
	    
        }

        if ( xpkey != XP_KEY_NONE ) {
            if ( board_handleKey( fGame.board, xpkey ) ) {
                GUI_NeedUpdate();
                result = 1;
            }
        }

        break; /* MSG_KEY */

    default:
        break;
    }
    GUI_EnableTimers();

    if ( result == 0 ) {
        result = CWindow::MsgHandler( type, from, data );
    } 
    return result;
} // CXWordsWindow::MsgHandler

void
CXWordsWindow::setTimerIfNeeded()
{
    U32 mSeconds;
    XWTimerReason why;

    if ( fTimerProcs[TIMER_PENDOWN] != NULL ) { /* faster, so higher priority */
        mSeconds = (U32)450;
        why = TIMER_PENDOWN;
    } else if ( fTimerProcs[TIMER_TIMERTICK] != NULL ) {
        mSeconds = (U32)1000;
        why = TIMER_TIMERTICK;
    } else {
        return;
    }

    SetTimer( mSeconds, XP_FALSE, why );
} /* setTimerIfNeeded */

void 
CXWordsWindow::fireTimer( XWTimerReason why )
{
    TimerProc proc = fTimerProcs[why];
    fTimerProcs[why] = (TimerProc)NULL; /* clear now; board may set it again */

    (*proc)( fTimerClosures[why], why );
}

void 
CXWordsWindow::setTimerImpl( XWTimerReason why, TimerProc proc, void* closure )
{
    XP_ASSERT( why == TIMER_PENDOWN ||
               why == TIMER_TIMERTICK );

    fTimerProcs[why] = proc;
    fTimerClosures[why] = closure;
    setTimerIfNeeded();
}

void
CXWordsWindow::disOrEnableFrank( U16 id, XP_Bool enable )
{
    CButton* button = (CButton*)GetChildID( id );
    if ( enable ) {
        button->Enable();
    } else {
        button->Disable();	
    }
} /* disOrEnableFrank */

void
CXWordsWindow::updateCtrlsForTray( XW_TrayVisState newState )
{
    XP_ASSERT( newState != TRAY_HIDDEN );
    XP_Bool isRevealed = newState == TRAY_REVEALED;

    disOrEnableFrank( MAIN_HINT_BUTTON_ID, isRevealed );
    disOrEnableFrank( MAIN_UNDO_BUTTON_ID, isRevealed );
    disOrEnableFrank( MAIN_COMMIT_BUTTON_ID, isRevealed );
    disOrEnableFrank( MAIN_TRADE_BUTTON_ID, isRevealed );
    disOrEnableFrank( MAIN_JUGGLE_BUTTON_ID, isRevealed );
    disOrEnableFrank( MAIN_HIDE_BUTTON_ID, isRevealed );
} /* updateCtrlsForTray */

void
CXWordsWindow::startProgressBar()
{
    if ( fState.showProgress ) {

        DrawRectFilled( &fProgressRect, COLOR_WHITE );
        DrawRect( &fProgressRect, COLOR_BLACK );

        fProgressCurLine = 0;
    }
} /* startProgressBar */

void
CXWordsWindow::finishProgressBar()
{
    if ( fState.showProgress ) {
        DrawRectFilled( &fProgressRect, COLOR_WHITE );
    }
} /* finishProgressBar */

void
CXWordsWindow::advanceProgressBar()
{
    if ( fState.showProgress ) {
        U16 line;
        U16 height = fProgressRect.height - 2; /* don't overwrite top and
                                                  bottom */
        XP_Bool draw;
        COLOR color;

        fProgressCurLine %= height * 2;
        draw = fProgressCurLine < height;

        line = fProgressCurLine % (height) + 1;
        line = fProgressRect.y + height - line + 1;
        if ( draw ) {
            color = COLOR_BLACK;
        } else {
            color = COLOR_WHITE;
        }

        DrawLine( fProgressRect.x+1, line, 
                  fProgressRect.x + fProgressRect.width - 1, line, color );

        ++fProgressCurLine;
    }
} /* advanceProgressBar */

void
CXWordsWindow::wrappedEventLoop( CWindow* window )
{
    XP_Bool robotHalted = fRobotHalted;
    fRobotHalted = XP_TRUE;
    board_pushTimerSave( fGame.board );

    GUI_EventLoop( window );

    board_popTimerSave( fGame.board );
    fRobotHalted = robotHalted;
} /* wrappedEventLoop */

void
CXWordsWindow::gameInfo()
{
    BOOL ignore;
    wrappedEventLoop( new CPlayersWindow( MEMPOOL(this) &fGameInfo, fDictList, 
                                          FALSE, FALSE, &ignore ) );
} /* gameInfo */

BOOL
CXWordsWindow::newGame( XP_Bool allowCancel )
{
    BOOL cancelled;
    wrappedEventLoop( new CPlayersWindow( MEMPOOL(this) &fGameInfo, fDictList, 
                                          TRUE, allowCancel, &cancelled ) );

    XP_ASSERT( allowCancel || !cancelled );	/* can't clear cancelled if not
											   allowed to */
    if ( !cancelled ) {
        XP_U32 gameID = frank_util_getCurSeconds( &this->util );
        game_reset( MPPARM(mpool) &fGame, &fGameInfo, &this->util, gameID,
                    &this->cp, (TransportSend)NULL, NULL );
        if ( !!fGameInfo.dictName ) {
            DictionaryCtxt* dict = model_getDictionary( fGame.model );
            if ( !!dict && 0==strcmp( (char*)dict_getName(dict), 
                                      (char*)fGameInfo.dictName)){
                /* do nothing; this dict's fine */
            } else {
                if ( !!dict ) {
                    dict_destroy( dict );
                }
                dict = frank_dictionary_make( MPPARM(mpool)
                                              fGameInfo.dictName);
                model_setDictionary( fGame.model, dict );
            }
        }
        server_do( fGame.server );

        GUI_NeedUpdate();
    }

    return !cancelled;
} /* newGame */

/* ======================================================================== */
void
Init_Window( MPFORMAL FrankDictList* dlist )
{
    MainWindow = new CXWordsWindow( MPPARM(mpool) dlist );
}

void
Init_Menu()
{
    short row;
    CMenu* menu;

    menu = new CMenu( FILEMENU_WINDOW_ID );
    menu->SetNumRows( 4 );	/* 4 with preferences */
    row = 0;
    menu->SetRow( row++, FILEMENU_NEWGAME, "New game", 'n' );
    menu->SetRow( row++, FILEMENU_SAVEDGAMES, "Saved games..." );
    menu->SetSeparatorRow( row++ );
/*     menu->SetRow( row++, FILEMENU_PREFS, "Preferences..." ); */
#ifdef HASBRO_EBM
    menu->SetRow( row++, FILEMENU_ABOUT, "About Franklin Scrabble(tm)..." );
#else
    menu->SetRow( row++, FILEMENU_ABOUT, "About Crosswords..." );
#endif
    MainMenuBar.AddButton( new CPushButton(FILEMENU_BUTTON_ID,0,0,"File"),
			   menu );

    menu = new CMenu( GAMEMENU_WINDOW_ID );
    menu->SetNumRows( 4 );
    row = 0;
    menu->SetRow( row++, GAMEMENU_TVALUES, "Tile values", 'v' );
    menu->SetRow( row++, GAMEMENU_GAMEINFO, "Current game info", 'i' );
    menu->SetRow( row++, GAMEMENU_HISTORY, "Game history", 's' );
    menu->SetRow( row++, GAMEMENU_FINALSCORES, "Final scores", 'f' );
    MainMenuBar.AddButton( new CPushButton(GAMEMENU_BUTTON_ID,0,0,"Game"),
                           menu );

    menu = new CMenu( MOVEMENU_WINDOW_ID );
    menu->SetNumRows( 9 );
    row = 0;
    menu->SetRow( row++, MOVEMENU_HINT, "Hint", 'h' );
    menu->SetRow( row++, MOVEMENU_NEXTHINT, "Next hint", 'n' );
    menu->SetRow( row++, MOVEMENU_REVERT, "Revert move", 'r' );
    menu->SetRow( row++, MOVEMENU_UNDO, "Undo prev. move", 'u' );
    menu->SetSeparatorRow( row++ );
    menu->SetRow( row++, MOVEMENU_DONE, "Done", 'd' );
    menu->SetRow( row++, MOVEMENU_JUGGLE, "Juggle", 'j' );
    menu->SetRow( row++, MOVEMENU_TRADE, "Trade", 't' );
    menu->SetRow( row++, MOVEMENU_HIDETRAY, "Hide tray", 'h' );

    MainMenuBar.AddButton( new CPushButton(MOVEMENU_BUTTON_ID,0,0,"Move"),
			   menu );

#ifdef MEM_DEBUG
    menu = new CMenu( MOVEMENU_WINDOW_ID );
    menu->SetNumRows( 1 );
    row = 0;
    menu->SetRow( row++, DEBUGMENU_HEAPDUMP, "Heap dump" );

    MainMenuBar.AddButton( new CPushButton(DEBUGMENU_BUTTON_ID,0,0,"Debug"),
			   menu );
#endif

}

void
MyErrorHandler( const char *filename, int lineno, const char *failexpr )
{
    if (lineno != -1 || strcmp( failexpr, "Out of memory" )) {
        return;
    }

    GUI_Alert( ALERT_WARNING, 
               "Operation cancelled - insufficient memory" );
    GUI_SetMallocReserve( 1536 );
    GUI_ClearStack();
}

S32
GUI_main( MSG_TYPE type, CViewable *object, S32 data )
{
    switch (type) {
    case MSG_APP_START: {
        FrankDictList* dlist;
        if (OS_is_present && hostIO_am_I_the_current_task()) {
            HOSTIO_INLINE_BREAKPOINT();
        }

        struct timeval tv;
        gettimeofday( &tv, (struct timezone *)NULL );
        srand( tv.tv_sec /*20*/ /*TIMER_GetTickCountUSecs()*/ );

#ifdef MEM_DEBUG
        MemPoolCtx* mpool = mpool_make();
#endif

        dlist = new FrankDictList( MPPARM_NOCOMMA(mpool) );
        if ( dlist->GetDictCount() > 0 ) {
            Init_Window( MPPARM(mpool) dlist );
            Init_Menu();
            GUI_SetErrorHandler( MyErrorHandler );
        } else {
            delete dlist;
#ifdef MEM_DEBUG
            mpool_destroy( mpool );        
#endif
            GUI_Alert( ALERT_WARNING, 
                       "Crosswords requires at least one dictionary." );
            GUI_Exit();
        }
        return 1;
    }

    case MSG_APP_STOP:
        delete MainWindow;	/* trigger save */
        return 1;

    case MSG_KEY:
        if ( data == K_MENU ) {
            MainMenuBar.Show();
            return 1;
        }
        break;

    case MSG_MENU_SELECT:
        if (data == -1) {
            /* We don't care about menu cancellations */
            return 1;
        }
        switch ((U16) data) {

        case FILEMENU_NEWGAME:
            MainWindow->doNewGameMenu();
            return 1;

        case FILEMENU_SAVEDGAMES:
#ifdef HASBRO_EBM
            GUI_Alert( ALERT_WARNING, "Feature not available in demo version." );
#else
            MainWindow->doSavedGames();
#endif
            return 1;

            /* 	case FILEMENU_PREFS: */
        case FILEMENU_ABOUT:
            MainWindow->doAbout();
            return 1;

        case GAMEMENU_TVALUES:
            MainWindow->doTileValues();
            return 1;

        case GAMEMENU_FINALSCORES:
            MainWindow->doEndGame();
            return 1;
            break;

        case GAMEMENU_GAMEINFO:
            MainWindow->gameInfo();
            return 1;

        case GAMEMENU_HISTORY:
            MainWindow->doGameHistory();
            return 1;

        case MOVEMENU_HINT:
        case MOVEMENU_NEXTHINT:
            MainWindow->doHint( (U16)data == MOVEMENU_HINT );
            break;

        case MOVEMENU_UNDO:
            MainWindow->doUndo();
            break;

        case MOVEMENU_REVERT:
            break;
        case MOVEMENU_DONE:
            MainWindow->doCommit();
            break;
        case MOVEMENU_JUGGLE:
        case MOVEMENU_TRADE:
            break;
        case MOVEMENU_HIDETRAY:
            MainWindow->doHideTray();
            break;
#ifdef MEM_DEBUG
        case DEBUGMENU_HEAPDUMP:
            MainWindow->doHeapDump();
            break;
#endif
        }
        break;
    default:
        fallthru;
    }
    return 0;
} // GUI_main

void
CXWordsWindow::doHint( XP_Bool reset )
{
    XP_Bool workRemains = XP_FALSE;
    XP_Bool done;

    if ( reset ) {
        board_resetEngine( fGame.board );
    }
    done = board_requestHint( fGame.board, 
                              fAskTrayLimits, &workRemains );
    if ( done ) {
        GUI_NeedUpdate();
    }
    if ( workRemains ) {
        GUI_EventMessage( MSG_USER, this, HINT_REQUEST );
    }
} /* handleHintMenu */

void
CXWordsWindow::doUndo()
{
    if ( server_handleUndo( fGame.server ) ) {
        GUI_NeedUpdate();
    }
} /* doUndo */

void
CXWordsWindow::doHideTray()
{
    if ( board_hideTray( fGame.board ) ) {
        GUI_NeedUpdate();
    }
} /* doHideTray */

#ifdef MEM_DEBUG
void
CXWordsWindow::doHeapDump()
{
    XWStreamCtxt* stream = makeMemStream();
    mpool_stats( mpool, stream );
    
    XP_U16 size = stream_getSize( stream );
    char* buf = (char*)malloc(size+1);
    stream_getBytes( stream, buf, size );
    buf[size] = '\0';
    perror(buf);                /* XP_DEBUGF has 256 byte limit */
    free( buf );
} /* CXWordsWindow::doHeapDump */
#endif

void
CXWordsWindow::doCommit()
{
    if ( board_commitTurn( fGame.board ) ) {
        GUI_NeedUpdate();
    }
} /* doCommit */

/* If there's a game in progress (and there always is, I think), ask user if
 * wants to save.  If does, save that game and make a new one, with a new
 * index, to call the gui on.  Else reset the existing one and call the gui.*/
void
CXWordsWindow::doNewGameMenu()
{
    XP_Bool doit = XP_TRUE;
    /* OK returns 1 */
    if ( 0 == GUI_Alert( ALERT_OK, 
						 "Click \"OK\" to replace the current game with a new "
						 "one, or \"Cancel\" to add without deleting the current "
						 "game." ) ) {
		makeNewGame( this->gamesDB->countRecords() );
    } else if ( 0 == GUI_Alert( ALERT_OK,
                                "Are you sure you want to replace"
                                " the existing game?" ) ) {
        doit = XP_FALSE;
    }

    if ( doit && newGame( XP_FALSE ) ) { /* don't let user cancel; too late! */
		positionBoard();
    }

    board_invalAll( fGame.board );
    GUI_NeedUpdate();
} /* doNewGameMenu */

#ifndef HASBRO_EBM
void
CXWordsWindow::doSavedGames()
{
    U16 openIndex;		/* what index am I to open? */
    U16 curIndex = fState.curGameIndex; /* may change if lower-index
                                           game deleted */
    saveCurrentGame();		/* so can be duped */
    wrappedEventLoop( new CSavedGamesWindow( this->gamesDB, &openIndex, 
                                             &curIndex) );

    fState.curGameIndex = curIndex;
    if ( curIndex != openIndex ) {
        fState.curGameIndex = openIndex;
        loadCurrentGame();
        positionBoard();
        server_do( fGame.server ); /* in case there's a robot */
        board_invalAll( fGame.board );
        GUI_NeedUpdate();
    }
} /* doSavedGames */
#endif

void
CXWordsWindow::doAbout()
{
    XP_U16 ignore;
    XWStreamCtxt* stream;

    stream = makeMemStream();
    char* txt = "Crosswords " VERSION_STRING "\n"
        "Copyright 2000-2004 by Eric House (xwords@eehouse.org).\n"
        "All rights reserved.\n"
        "For further information see www.peak.org/~fixin/xwords/ebm.html.";
    stream_putString( stream, txt );
    stream_putU8( stream, '\0' );

    wrappedEventLoop( new CShowTextWindow( MEMPOOL(this) stream, 
                                           "About Crosswords", 
                                           true, false, 
                                           &ignore ) );
} /* doAbout */

void
CXWordsWindow::doEndGame()
{
    if ( server_getGameIsOver( fGame.server ) ) {
        this->displayFinalScores();
    } else if ( GUI_Alert( ALERT_OK, 
                           "Are you sure you want to end the game now?" ) 
                != 0 ) {
        server_endGame( fGame.server );
    }
    GUI_NeedUpdate();
} /* doEndGame */

void
CXWordsWindow::doTileValues()
{
    XWStreamCtxt* stream;

    stream = makeMemStream();
    server_formatDictCounts( fGame.server, stream, 2 /* cols */ );

    displayTextFromStream( stream, "Tile counts and values" );
} /* doTileValues */

void
CXWordsWindow::doGameHistory()
{
    XP_Bool gameOver = server_getGameIsOver( fGame.server );
    XWStreamCtxt* stream = makeMemStream();
    model_writeGameHistory( fGame.model, stream, fGame.server, gameOver );

    displayTextFromStream( stream, "Game history" );
} /* doGameHistory */

void 
CXWordsWindow::initPrefs()
{
    fState.magic = 0x12345678;
    fState.curGameIndex = 1; /* 0 is prefs record */
    fState.showProgress = XP_TRUE;

    /* save 'em now so we can save 1st game at expected index. */
    this->gamesDB->putNthRecord( 0, &fState, sizeof(fState) );
} /* initPrefs */

void 
CXWordsWindow::loadPrefs()
{
    CGamesDB* gamesDB = this->gamesDB;
    XP_ASSERT( gamesDB->countRecords() > 0 );

    U16 len;
    void* recordP = gamesDB->getNthRecord( 0, &len );

    XP_ASSERT( len == sizeof(fState) );
    XP_ASSERT( !!recordP );
    XP_MEMCPY( &fState, recordP, len );
    gamesDB->recordRelease(0);

    XP_ASSERT( fState.magic == 0x12345678 );
} /* loadPrefs */

void
CXWordsWindow::loadCurrentGame()
{
    U16 len;
    void* recordP = gamesDB->getNthRecord( fState.curGameIndex, &len );

    XWStreamCtxt* inStream = makeMemStream();
    stream_putBytes( inStream, recordP, len );
    gamesDB->recordRelease( fState.curGameIndex );

    loadGameFromStream( inStream );
    stream_destroy( inStream );
} /* loadCurrentGame */

void
CXWordsWindow::writeGameToStream( XWStreamCtxt* stream, U16 index )
{
    /* the dictionary */
    DictionaryCtxt* dict = model_getDictionary( fGame.model );
    XP_UCHAR* dictName = dict_getName( dict );
    stream_putU8( stream, !!dictName );
    if ( !!dictName ) {
        stringToStream( stream, dictName );
    }

    game_saveToStream( &fGame, &fGameInfo, stream );
} /* writeGameToStream */

void
CXWordsWindow::loadGameFromStream( XWStreamCtxt* inStream )
{
    /* the dictionary */
    XP_U8 hasDictName = stream_getU8( inStream );
    DictionaryCtxt* dict = (DictionaryCtxt*)NULL;
    if ( hasDictName ) {
        XP_UCHAR* name = stringFromStream(MEMPOOL(this) inStream);
        dict = frank_dictionary_make( MPPARM(mpool) name );
    }

    game_makeFromStream( MPPARM(mpool) inStream, &fGame, &fGameInfo, 
                         dict, &this->util, (DrawCtx*)this->draw, &this->cp,
                         (TransportSend)NULL, NULL );
} /* loadGameFromStream */

void
CXWordsWindow::fni()
{
    GUI_Alert( ALERT_WARNING, "Feature pending" );
    board_invalAll( fGame.board );
    GUI_NeedUpdate();
} /* fni */

void 
CXWordsWindow::displayFinalScores()
{
    XWStreamCtxt* stream;

    stream = makeMemStream();
    server_writeFinalScores( fGame.server, stream );

    displayTextFromStream( stream, "Final scores" );
} /* displayFinalScores */

XP_U16
CXWordsWindow::displayTextFromStream( XWStreamCtxt* stream, 
                                      const char* title, /* should be ID!!! */
                                      BOOL killStream,
                                      BOOL includeCancel )
{
    XP_U16 result = 0;
    wrappedEventLoop( new CShowTextWindow( MEMPOOL(this) stream, title, 
                                           killStream, includeCancel, 
                                           &result ) );
    return result;
} /* displayTextFromStream */

extern "C" {

    int
    frank_snprintf( XP_UCHAR* buf, XP_U16 len, XP_UCHAR* format, ... )
    {
        va_list ap;	
    
        va_start(ap, format);

        vsnprintf((char*)buf, len, (char*)format, ap);

        va_end(ap);

        return strlen((char*)buf);
    } /* frank_snprintf */

    void
    frank_debugf( char* format, ... )
    {
        char buf[256];
        va_list ap;
    
        va_start(ap, format);

        vsprintf(buf, format, ap);

        va_end(ap);
    
        perror(buf);
    } // debugf

    XP_UCHAR* 
    frankCopyStr( MPFORMAL const XP_UCHAR* buf )
    {
        XP_U16 len = XP_STRLEN(buf) + 1;
        XP_UCHAR* result = (XP_UCHAR*)XP_MALLOC( mpool, len );
        XP_MEMCPY( result, buf, len );
        return result;
    } /* frankCopyStr */

    unsigned long 
    frank_flipLong( unsigned long l )
    {
        unsigned long result =
            ((l & 0x000000FF) << 24) | 
            ((l & 0x0000FF00) << 8) | 
            ((l & 0x00FF0000) >> 8) | 
            ((l & 0xFF000000) >> 24);
        return result;
    } /* frank_flipLong */

    unsigned short
    frank_flipShort(unsigned short s)
    {
        unsigned short result = 
            ((s & 0x00FF) << 8) | 
            ((s & 0xFF00) >> 8);
    
        return result;
    } /* frank_flipShort */

}
/*****************************************************************************
 * These are the callbacks intstalled in the util vtable
 ****************************************************************************/
static VTableMgr* 
frank_util_getVTManager( XW_UtilCtxt* uc )
{
    CXWordsWindow* self = (CXWordsWindow*)uc->closure;
    return self->fVTableMgr;
} /* frank_util_getVTManager */

static DictionaryCtxt* 
frank_util_makeEmptyDict( XW_UtilCtxt* uc )
{
    return frank_dictionary_make( MPPARM(uc->mpool) (XP_UCHAR*)NULL );
} /* frank_util_makeEmptyDict */

static void
frank_util_userError( XW_UtilCtxt* uc, UtilErrID id )
{
    const char *message;
/*
    BOOL GUI_Alert( ALERT type, const char *text ); 
        Puts up an alert window, which is a small window containing an icon, some text, and one or more buttons. 
        These types of alerts are offered: 
            ALERT_BUG: Insect icon and "Abort" button (click terminates application).  Does not return. 
            ALERT_FATAL: Octagonal icon and "Stop" button (click terminates application).  Does not return. 
            ALERT_ERROR: Exclamation-point icon and "Cancel" button (click returns 0). 
            ALERT_WARNING: Info icon and "OK" button (click returns 0). 
            ALERT_OK: Question-mark icon, buttons "OK" (click returns 1) and "Cancel" (click returns 0). 
            ALERT_RETRY: Exclamation-point icon and buttons "Try again" (returns 1) and "Exit" (returns 0). 
*/
    switch( id ) {
    case ERR_TILES_NOT_IN_LINE:
        message = "All tiles played must be in a line.";
        break;
    case ERR_NO_EMPTIES_IN_TURN:
        message = "Empty squares cannot separate tiles played.";
        break;

    case ERR_TWO_TILES_FIRST_MOVE:
        message = "Must play two or more pieces on the first move.";
        break;
    case ERR_TILES_MUST_CONTACT:
        message = "New pieces must contact others already in place (or "
            "the middle square on the first move).";
        break;
    case ERR_NOT_YOUR_TURN:
        message = "You can't do that; it's not your turn!";
        break;
    case ERR_NO_PEEK_ROBOT_TILES:
        message = "No peeking at the robot's tiles!";
        break;
    case ERR_CANT_TRADE_MID_MOVE:
        message = "Remove played tiles before trading.";
        break;
    case ERR_TOO_FEW_TILES_LEFT_TO_TRADE:
        message = "Too few tiles left to trade.";
        break;
    default:
        message = "unknown errorcode ID!!!";
        break;
    }

    (void)GUI_Alert( ALERT_ERROR, message ); 
    CXWordsWindow* self = (CXWordsWindow*)uc->closure;
    board_invalAll( self->fGame.board );
    GUI_NeedUpdate();
} /* frank_util_userError */

static XP_Bool
frank_util_userQuery( XW_UtilCtxt* uc, UtilQueryID id, XWStreamCtxt* stream )
{
    char* question;
    XP_U16 askResult;
    CXWordsWindow* self = (CXWordsWindow*)uc->closure;

    switch( id ) {
    case QUERY_COMMIT_TURN:
        askResult = self->displayTextFromStream( stream, "Query",
                                                 FALSE, TRUE );
        return askResult;
    case QUERY_COMMIT_TRADE:
        question = "Really trade the selected tiles?";
        break;
    case QUERY_ROBOT_MOVE:
    case QUERY_ROBOT_TRADE:
        XP_LOGF( "handling robot info" );
        askResult = self->displayTextFromStream( stream, "Robot move",
                                                 FALSE, FALSE );
        return askResult;
        break;
    default:
        question = "Unimplemented query code!!!";
        break;
    }

    askResult = GUI_Alert( ALERT_OK, question ); 
    board_invalAll( self->fGame.board );
    GUI_NeedUpdate();
    return askResult != 0;
} /* frank_util_userQuery */

static XP_S16
frank_util_userPickTile( XW_UtilCtxt* uc, const PickInfo* pi, 
                         XP_U16 playerNum,
                         const XP_UCHAR4* texts, XP_U16 nTiles )
{
    CXWordsWindow* self = (CXWordsWindow*)uc->closure;
    XP_S16 result;
    self->wrappedEventLoop( new CAskLetterWindow( pi, playerNum,
                                                  texts, nTiles, &result ) );
    return result;
    /* doesn't need to inval because CAskLetterWindow saves bits behind */
} /* frank_util_askBlankFace */

static XP_Bool
frank_util_askPassword( XW_UtilCtxt* uc, const XP_UCHAR* name, XP_UCHAR* buf, 
			XP_U16* lenp )
{
    XP_Bool ok;
    CXWordsWindow* self = (CXWordsWindow*)uc->closure;
    self->wrappedEventLoop( new CAskPasswdWindow( name, buf, lenp, &ok ) );
    return ok;
} /* frank_util_askPassword */

static void 
frank_util_trayHiddenChange( XW_UtilCtxt* uc, XW_TrayVisState newState )
{
    CXWordsWindow* self = (CXWordsWindow*)uc->closure;
    self->updateCtrlsForTray( newState );
} /* frank_util_trayHiddenChange */

static void
frank_util_notifyGameOver( XW_UtilCtxt* uc )
{
    CXWordsWindow* self = (CXWordsWindow*)uc->closure;
    board_invalAll( self->fGame.board );
    GUI_NeedUpdate();
    GUI_EventMessage( MSG_USER, self, FINALSCORE_REQUEST );
} /* frank_util_notifyGameOver */

static XP_Bool
frank_util_hiliteCell( XW_UtilCtxt* uc, XP_U16 col, XP_U16 row )
{
    CXWordsWindow* self = (CXWordsWindow*)uc->closure;
    XP_Bool halted = self->robotIsHalted();
    if ( !halted ) {
        board_hiliteCellAt( self->fGame.board, col, row );
    }
    BOOL waiting = EVNT_IsWaiting();
    return !waiting && !halted;
} /* frank_util_hiliteCell */

/* Return false to get engine to abort search.
 */
static XP_Bool
frank_util_engineProgressCallback( XW_UtilCtxt* uc )
{
    CXWordsWindow* self = (CXWordsWindow*)uc->closure;

    self->advanceProgressBar();

    BOOL waiting = EVNT_IsWaiting();
    return !waiting && !self->robotIsHalted();
} /* frank_util_engineProgressCallback */

static void
frank_util_setTimer( XW_UtilCtxt* uc, XWTimerReason why, XP_U16 when,
                     TimerProc proc, void* closure )
{
    CXWordsWindow* self = (CXWordsWindow*)uc->closure;
    self->setTimerImpl( why, proc, closure );
} /* frank_util_setTimer */

static void
frank_util_requestTime( XW_UtilCtxt* uc )
{
    CXWordsWindow* self = (CXWordsWindow*)uc->closure;
    if ( !self->getUserEventPending() ) {
        GUI_EventMessage( MSG_USER, self, SERVER_TIME_REQUEST );
        self->setUserEventPending();
    }
} /* frank_util_requestTime */

static XP_U32
frank_util_getCurSeconds( XW_UtilCtxt* uc )
{
    struct timeval tv;
    gettimeofday( &tv, (struct timezone *)NULL );
    return tv.tv_sec;
} /* frank_util_getCurSeconds */

#define EM BONUS_NONE
#define DL BONUS_DOUBLE_LETTER
#define DW BONUS_DOUBLE_WORD
#define TL BONUS_TRIPLE_LETTER
#define TW BONUS_TRIPLE_WORD

static XWBonusType
frank_util_getSquareBonus( XW_UtilCtxt* uc, ModelCtxt* model,
			   XP_U16 col, XP_U16 row )
{
    XP_U16 index;

    const char scrabbleBoard[8*8] = {
        TW,EM,EM,DL,EM,EM,EM,TW,
        EM,DW,EM,EM,EM,TL,EM,EM,

        EM,EM,DW,EM,EM,EM,DL,EM,
        DL,EM,EM,DW,EM,EM,EM,DL,
                            
        EM,EM,EM,EM,DW,EM,EM,EM,
        EM,TL,EM,EM,EM,TL,EM,EM,
                            
        EM,EM,DL,EM,EM,EM,DL,EM,
        TW,EM,EM,DL,EM,EM,EM,DW,
    }; /* scrabbleBoard */

    if ( col > 7 ) col = 14 - col;
    if ( row > 7 ) row = 14 - row;
    index = (row*8) + col;
    if ( index >= 8*8 ) {
        return (XWBonusType)EM;
    } else {
        return (XWBonusType)scrabbleBoard[index];
    }
} /* frank_util_getSquareBonus */

static XP_UCHAR* 
frank_util_getUserString( XW_UtilCtxt* uc, XP_U16 stringCode )
{
    switch( stringCode ) {
    case STRD_REMAINING_TILES_ADD:
        return (XP_UCHAR*)"+ %d [all remaining tiles]";
    case STRD_UNUSED_TILES_SUB:
        return (XP_UCHAR*)"- %d [unused tiles]";
    case STR_BONUS_ALL:
        return (XP_UCHAR*)"Bonus for using all tiles: 50\n";
    case STRD_TURN_SCORE:
        return (XP_UCHAR*)"Score for turn: %d\n";
    case STR_COMMIT_CONFIRM:
        return (XP_UCHAR*)"Commit the current move?\n";
    case STR_NONLOCAL_NAME:
        return (XP_UCHAR*)"%s (remote)";
    case STR_LOCAL_NAME:
        return (XP_UCHAR*)"%s";
    case STRD_TIME_PENALTY_SUB:
        return (XP_UCHAR*)" - %d [time]";

    case STRD_CUMULATIVE_SCORE:
        return (XP_UCHAR*)"Cumulative score: %d\n";
    case STRS_MOVE_ACROSS:
        return (XP_UCHAR*)"move (from %s across)\n";
    case STRS_MOVE_DOWN:
        return (XP_UCHAR*)"move (from %s down)\n";
    case STRS_TRAY_AT_START:
        return (XP_UCHAR*)"Tray at start: %s\n";

    case STRS_NEW_TILES:
        return (XP_UCHAR*)"New tiles: %s\n";
    case STRSS_TRADED_FOR:
        return (XP_UCHAR*)"Traded %s for %s.";
    case STR_PASS:
        return (XP_UCHAR*)"pass\n";
    case STR_PHONY_REJECTED:
        return (XP_UCHAR*)"Illegal word in move; turn lost!\n";
    case STRD_ROBOT_TRADED:
        return (XP_UCHAR*)"Robot traded %d tiles this turn.";
    case STR_ROBOT_MOVED:
        return (XP_UCHAR*)"The robot made this move:\n";

    case STR_PASSED: 
        return (XP_UCHAR*)"Passed";
    case STRSD_SUMMARYSCORED: 
        return (XP_UCHAR*)"%s:%d";
    case STRD_TRADED: 
        return (XP_UCHAR*)"Traded %d";
    case STR_LOSTTURN:
        return (XP_UCHAR*)"Lost turn";

    case STRS_VALUES_HEADER:
        return (XP_UCHAR*)"%s counts/values:\n";

    default:
        return (XP_UCHAR*)"unknown code ";
    }
} /* frank_util_getUserString */

static void
formatBadWords( BadWordInfo* bwi, char buf[] )
{
    XP_U16 i;

    for ( i = 0, buf[0] = '\0'; ; ) {
        char wordBuf[18];
        sprintf( wordBuf, "\"%s\"", bwi->words[i] );
        strcat( buf, wordBuf );
        if ( ++i == bwi->nWords ) {
            break;
        }
        strcat( buf, ", " );
    }
} /* formatBadWords */

static XP_Bool
frank_util_warnIllegalWord( XW_UtilCtxt* uc, BadWordInfo* bwi, 
			    XP_U16 turn, XP_Bool turnLost )
{
    char buf[200];
    char wordsBuf[150];
    XP_Bool result;
    CXWordsWindow* self = (CXWordsWindow*)uc->closure;

    formatBadWords( bwi, wordsBuf );

    if ( turnLost ) {
        XP_UCHAR* name = self->fGameInfo.players[turn].name;
        XP_ASSERT( !!name );
        sprintf( buf, "Player %d (%s) played illegal word[s] "
                 "%s; loses turn",
                 turn+1, name, wordsBuf );
        (void)GUI_Alert( ALERT_ERROR, buf ); 
        result = XP_TRUE;
    } else {
        sprintf( buf, "Word %s not in the current dictionary. "
                 "Use it anyway?", wordsBuf );
        result = GUI_Alert( ALERT_OK, buf ); 
    }
    return result;
} /* frank_util_warnIllegalWord */

#ifdef SHOW_PROGRESS
static void
frank_util_engineStarting( XW_UtilCtxt* uc, XP_U16 nBlanks )
{
    CXWordsWindow* self = (CXWordsWindow*)uc->closure;
    self->startProgressBar();
} /* frank_util_engineStarting */

static void
frank_util_engineStopping( XW_UtilCtxt* uc )
{
    CXWordsWindow* self = (CXWordsWindow*)uc->closure;
    self->finishProgressBar();
} /* frank_util_engineStopping */
#endif /* SHOW_PROGRESS */
