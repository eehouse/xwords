/* -*- compile-command: "make MEMDEBUG=TRUE -j5"; -*- */
/* 
 * Copyright 2000 - 2020 by Eric House (xwords@eehouse.org).  All rights
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

#include <ncurses.h>
#include <wchar.h>

#include "cursesboard.h"
#include "linuxmain.h"
#include "linuxutl.h"
#include "relaycon.h"
#include "mqttcon.h"
#include "cursesask.h"
#include "cursesmenu.h"
#include "cursesletterask.h"
#include "linuxdict.h"
#include "gamesdb.h"
#include "game.h"
#include "gsrcwrap.h"
#include "linuxsms.h"
#include "strutils.h"
#include "dbgutil.h"

typedef struct CursesBoardState {
    CursesAppGlobals* aGlobals;
    LaunchParams* params;
    CursesMenuState* menuState;
    OnGameSaved onGameSaved;

    GSList* games;
} CursesBoardState;

struct CursesBoardGlobals {
    CommonGlobals cGlobals;
    CursesBoardState* cbState;
    CursesMenuState* menuState; /* null if we're not using menus */
    int refCount;

    union {
        struct {
            XWStreamCtxt* stream; /* how we can reach the server */
        } client;
        struct {
            int serverSocket;
            XP_Bool socketOpen;
        } server;
    } csInfo;
    XWGameState state;
    XP_U16 nChatsSent;
    XP_U16 nextQueryTimeSecs;

    WINDOW* boardWin;
    int winWidth, winHeight;

    const MenuList* lastSubMenu;

    XP_Bool amServer;	/* this process acting as server */
};

static CursesBoardGlobals* findOrOpen( CursesBoardState* cbState,
                                       sqlite3_int64 rowid,
                                       const CurGameInfo* gi,
                                       const CommsAddrRec* addrP );
static void enableDraw( CursesBoardGlobals* bGlobals, const cb_dims* dims );
static CursesBoardGlobals* ref( CursesBoardGlobals* bGlobals );
static void unref( CursesBoardGlobals* bGlobals );
static void setupBoard( CursesBoardGlobals* bGlobals );
static void initMenus( CursesBoardGlobals* bGlobals );
static void disposeDraw( CursesBoardGlobals* bGlobals );

#ifdef KEYBOARD_NAV
static bool handleLeft( void* closure, int key );
static bool handleRight( void* closure, int key );
static bool handleUp( void* closure, int key );
static bool handleDown( void* closure, int key );
static bool handleClose( void* closure, int key );
#endif
#ifdef ALT_KEYS_WORKING
static bool handleAltLeft( void* closure, int key );
static bool handleAltRight( void* closure, int key );
static bool handleAltUp( void* closure, int key );
static bool handleAltDown( void* closure, int key );
#endif
static bool handleJuggle( void* closure, int key );
static bool handleHide( void* closure, int key );
/* static bool handleResend( void* closure, int key ); */
static bool handleSpace( void* closure, int key );
static bool handleRet( void* closure, int key );
static bool handleShowVals( void* closure, int key );
static bool handleHint( void* closure, int key );
static bool handleCommit( void* closure, int key );
static bool handleFlip( void* closure, int key );
static bool handleToggleValues( void* closure, int key );
static bool handleBackspace( void* closure, int key );
static bool handleUndo( void* closure, int key );
static bool handleReplace( void* closure, int key );
#ifdef CURSES_SMALL_SCREEN
static bool handleRootKeyShow( void* closure, int key );
static bool handleRootKeyHide( void* closure, int key );
#endif
static bool sendInvite( void* closure, int key );

#ifdef XWFEATURE_RELAY
static void relay_connd_curses( XWEnv xwe, void* closure, XP_UCHAR* const room,
                                XP_Bool reconnect, XP_U16 devOrder,
                                XP_Bool allHere, XP_U16 nMissing );
static void relay_status_curses( XWEnv xwe, void* closure, CommsRelayState state );
static void relay_error_curses( XWEnv xwe, void* closure, XWREASON relayErr );
static XP_Bool relay_sendNoConn_curses( XWEnv xwe, const XP_U8* msg, XP_U16 len,
                                        const XP_UCHAR* msgNo,
                                        const XP_UCHAR* relayID, void* closure );
#endif
static void curses_countChanged( XWEnv xwe, void* closure, XP_U16 newCount,
                                 XP_Bool quashed );
static XP_U32 curses_getFlags( XWEnv xwe, void* closure );
#ifdef RELAY_VIA_HTTP
static void relay_requestJoin_curses( void* closure, const XP_UCHAR* devID,
                                      const XP_UCHAR* room, XP_U16 nPlayersHere,
                                      XP_U16 nPlayersTotal, XP_U16 seed, XP_U16 lang );
#endif

static XP_Bool rematch_and_save( CursesBoardGlobals* bGlobals, RematchOrder ro,
                                 XP_U32* newGameIDP );
static void disposeBoard( CursesBoardGlobals* bGlobals, XP_Bool rmFromList );
static void initCP( CommonGlobals* cGlobals );
static CursesBoardGlobals* commonInit( CursesBoardState* cbState,
                                       sqlite3_int64 rowid,
                                       const CurGameInfo* gip );

CursesBoardState*
cb_init( CursesAppGlobals* aGlobals, LaunchParams* params,
         CursesMenuState* menuState, OnGameSaved onGameSaved )
{
    CursesBoardState* result = g_malloc0( sizeof(*result) );
    result->aGlobals = aGlobals;
    result->params = params;
    result->menuState = menuState;
    result->onGameSaved = onGameSaved;
    return result;
}

void
cb_resized( CursesBoardState* cbState, const cb_dims* dims )
{
    for ( GSList* iter = cbState->games; !!iter; iter = iter->next ) {
        CursesBoardGlobals* one = (CursesBoardGlobals*)iter->data;
        disposeDraw( one );
        enableDraw( one, dims );
    }
}

static gint
inviteIdle( gpointer data )
{
    CursesBoardGlobals* bGlobals = (CursesBoardGlobals*)data;
    LaunchParams* params = bGlobals->cGlobals.params;
    if ( !!params->connInfo.sms.inviteePhones
#ifdef XWFEATURE_RELAY
         || !!params->connInfo.relay.inviteeRelayIDs
#endif
         || !!params->connInfo.mqtt.inviteeDevIDs ) {
        sendInvite( bGlobals, 0 );
    }
    return FALSE;
}

void
cb_open( CursesBoardState* cbState, sqlite3_int64 rowid, const cb_dims* dims )
{
    LOG_FUNC();
    CursesBoardGlobals* bGlobals = findOrOpen( cbState, rowid, NULL, NULL );
    initMenus( bGlobals );
    enableDraw( bGlobals, dims );
    setupBoard( bGlobals );

    CommonGlobals* cGlobals = &bGlobals->cGlobals;
    if ( !!cGlobals->game.comms ) {
        comms_resendAll( cGlobals->game.comms, NULL_XWE, COMMS_CONN_NONE, XP_FALSE );
    }
    if ( bGlobals->cGlobals.params->forceInvite ) {
        (void)ADD_ONETIME_IDLE( inviteIdle, bGlobals );
    }
}

bool
cb_newGame( CursesBoardState* cbState, const cb_dims* dims,
            const CurGameInfo* gi, XP_U32* newGameIDP )
{
    CursesBoardGlobals* bGlobals = findOrOpen( cbState, -1, gi, NULL );
    if ( !!bGlobals ) {
        initMenus( bGlobals );
        enableDraw( bGlobals, dims );
        setupBoard( bGlobals );
    }
    XP_Bool success = NULL != bGlobals;

    if ( success && !!newGameIDP ) {
        *newGameIDP = bGlobals->cGlobals.gi->gameID;
    }

    return success;
}

void
cb_newFor( CursesBoardState* cbState, const NetLaunchInfo* nli,
           const cb_dims* XP_UNUSED(dims) )
{
    LaunchParams* params = cbState->params;

    CommsAddrRec selfAddr;
    makeSelfAddress( &selfAddr, params );

    CursesBoardGlobals* bGlobals = commonInit( cbState, -1, NULL );
    CommonGlobals* cGlobals = &bGlobals->cGlobals;
    initCP( cGlobals );
    if ( game_makeFromInvite( &cGlobals->game, NULL_XWE, nli, &selfAddr,
                              cGlobals->util, (DrawCtx*)NULL,
                              &cGlobals->cp, &cGlobals->procs ) ) {
        linuxSaveGame( cGlobals );
    } else {
        XP_ASSERT( 0 );
    }

    disposeBoard( bGlobals, XP_TRUE );
}

const MenuList g_allMenuList[] = {
    { handleLeft, "Left", "H", 'H' },
    { handleRight, "Right", "L", 'L' },
    { handleUp, "Up", "J", 'J' },
    { handleDown, "Down", "K", 'K' },
    { handleClose, "Close", "W", 'W' },
    { handleSpace, "Raise focus", "<spc>", ' ' },
    { handleRet, "Tap", "<[alt-]ret>", '\r' },
    { handleShowVals, "Tile values", "T", 'T' },
};

const MenuList g_boardMenuList[] = {
#ifdef ALT_KEYS_WORKING
    { handleAltLeft,  "Force left", "{", '{' },
    { handleAltRight, "Force right", "}", '}' },
    { handleAltUp,    "Force up", "_", '_' },
    { handleAltDown,  "Force down", "+", '+' },
#endif
    { handleHint, "Hint", "?", '?' },

    { handleCommit, "Commit move", "C", 'C' },
    { handleFlip, "Flip", "F", 'F' },
    { handleToggleValues, "Show values", "V", 'V' },

    { handleBackspace, "Remove from board", "<del>", 8 },
    { handleUndo, "Undo prev", "U", 'U' },
    { handleReplace, "uNdo cur", "N", 'N' },

    { sendInvite, "invitE", "E", 'E' },

    { NULL, NULL, NULL, '\0'}
};

const MenuList g_trayMenuList[] = {
    { handleJuggle, "Juggle", "G", 'G' },
    { handleHide, "[un]hIde", "I", 'I' },
#ifdef ALT_KEYS_WORKING
    { handleAltLeft, "Divider left", "{", '{' },
    { handleAltRight, "Divider right", "}", '}' },
#endif
    { NULL, NULL, NULL, '\0'}
};

const MenuList g_scoreMenuList[] = {
#ifdef KEYBOARD_NAV
#endif
    { NULL, NULL, NULL, '\0'}
};

#ifdef KEYBOARD_NAV
static void
changeMenuForFocus( CursesBoardGlobals* bGlobals, BoardObjectType focussed )
{
    const MenuList* subMenu = NULL;
    if ( focussed == OBJ_TRAY ) {
        subMenu = g_trayMenuList;
    } else if ( focussed == OBJ_BOARD ) {
        subMenu = g_boardMenuList;
    } else if ( focussed == OBJ_SCORE ) {
        subMenu = g_scoreMenuList;
    }

    CursesMenuState* menuState = bGlobals->menuState;
    if ( !!menuState ) {
        cmenu_removeMenus( menuState, bGlobals->lastSubMenu, NULL );
        bGlobals->lastSubMenu = subMenu;

        cmenu_addMenus( menuState, bGlobals, subMenu, NULL );
    }
    // drawMenuLargeOrSmall( globals->apg, menuList, globals );
} /* changeMenuForFocus */
#endif

static void setupCursesUtilCallbacks( CursesBoardGlobals* bGlobals, XW_UtilCtxt* util );

static void
initMenus( CursesBoardGlobals* bGlobals )
{
    if ( !bGlobals->menuState ) {
        bGlobals->menuState = bGlobals->cbState->menuState;
        cmenu_push( bGlobals->menuState, bGlobals, g_allMenuList, NULL );
    }
}

static void
onGameSaved( void* closure, sqlite3_int64 rowid, XP_Bool firstTime )
{
    CursesBoardGlobals* bGlobals = (CursesBoardGlobals*)closure;
    CommonGlobals* cGlobals = &bGlobals->cGlobals;
    /* onCursesGameSaved( bGlobals->aGlobals, rowid ); */

    BoardCtxt* board = cGlobals->game.board;
    board_invalAll( board );
    board_draw( board, NULL_XWE );
    /* May not be recorded */
    XP_ASSERT( cGlobals->rowid == rowid );
    // cGlobals->rowid = rowid;

    CursesBoardState* cbState = bGlobals->cbState;
    (*cbState->onGameSaved)( cbState->aGlobals, rowid, firstTime );
}

static gboolean
fire_acceptor( GIOChannel* source, GIOCondition condition, gpointer data )
{
    if ( 0 != (G_IO_IN & condition) ) {
        CursesBoardGlobals* globals = (CursesBoardGlobals*)data;

        int fd = g_io_channel_unix_get_fd( source );
        XP_ASSERT( fd == globals->csInfo.server.serverSocket );
        (*globals->cGlobals.acceptor)( fd, globals );
    }
    return TRUE;
}

static void
curses_socket_acceptor( int listener, Acceptor func, CommonGlobals* cGlobals,
                        void** XP_UNUSED(storage) )
{
    if ( -1 == listener ) {
        XP_LOGFF( "removal of listener not implemented!!!!!" );
    } else {
        CursesBoardGlobals* globals = (CursesBoardGlobals*)cGlobals;
        XP_ASSERT( !cGlobals->acceptor || (func == cGlobals->acceptor) );
        cGlobals->acceptor = func;
        globals->csInfo.server.serverSocket = listener;
        ADD_SOCKET( globals, listener, fire_acceptor );
    }
}

static void
initTProcsCurses( CommonGlobals* cGlobals )
{
    cGlobals->procs.closure = cGlobals;
    cGlobals->procs.sendMsgs = linux_send;
#ifdef XWFEATURE_COMMS_INVITE
    cGlobals->procs.sendInvt = linux_send_invt;
#endif
#ifdef COMMS_HEARTBEAT
    cGlobals->procs.reset = linux_reset;
#endif
#ifdef XWFEATURE_RELAY
    cGlobals->procs.rstatus = relay_status_curses;
    cGlobals->procs.rconnd = relay_connd_curses;
    cGlobals->procs.rerror = relay_error_curses;
    cGlobals->procs.sendNoConn = relay_sendNoConn_curses;
#endif
    cGlobals->procs.countChanged = curses_countChanged;
    cGlobals->procs.getFlags = curses_getFlags;
# ifdef RELAY_VIA_HTTP
    cGlobals->procs.requestJoin = relay_requestJoin_curses;
#endif
}

static CursesBoardGlobals*
commonInit( CursesBoardState* cbState, sqlite3_int64 rowid,
            const CurGameInfo* gip )
{
    CursesBoardGlobals* bGlobals = g_malloc0( sizeof(*bGlobals) );
    XP_LOGFF( "alloc'd bGlobals %p", bGlobals );
    CommonGlobals* cGlobals = &bGlobals->cGlobals;
    LaunchParams* params = cbState->params;

    cGlobals->gi = &cGlobals->_gi;
    gi_copy( MPPARM(params->mpool) cGlobals->gi, !!gip? gip : &params->pgi );

    cGlobals->rowid = rowid;
    bGlobals->cbState = cbState;
    cGlobals->params = params;
    setupUtil( cGlobals );
    setupCursesUtilCallbacks( bGlobals, cGlobals->util );

    cGlobals->socketAddedClosure = bGlobals;
    cGlobals->onSave = onGameSaved;
    cGlobals->onSaveClosure = bGlobals;
    cGlobals->addAcceptor = curses_socket_acceptor;

    initTProcsCurses( cGlobals );
    makeSelfAddress( &cGlobals->selfAddr, params );

    setOneSecondTimer( cGlobals );
    return bGlobals;
} /* commonInit */

static void
disposeDraw( CursesBoardGlobals* bGlobals )
{
    if ( !!bGlobals->boardWin ) {
        cursesDrawCtxtFree( bGlobals->cGlobals.draw );
        wclear( bGlobals->boardWin );
        wrefresh( bGlobals->boardWin );
        delwin( bGlobals->boardWin );
    }
}

static void
disposeBoard( CursesBoardGlobals* bGlobals, XP_Bool rmFromList )
{
    XP_LOGFF( "passed bGlobals %p", bGlobals );
    /* XP_ASSERT( 0 == bGlobals->refCount ); */
    CommonGlobals* cGlobals = &bGlobals->cGlobals;

    disposeDraw( bGlobals );

    if ( !!bGlobals->cbState->menuState ) {
        cmenu_pop( bGlobals->cbState->menuState );
    }

    clearOneSecondTimer( cGlobals );

    gi_disposePlayerInfo( MPPARM(cGlobals->util->mpool) cGlobals->gi );
    game_dispose( &cGlobals->game, NULL_XWE );

    disposeUtil( cGlobals );

    if ( rmFromList ) {
        CursesBoardState* cbState = bGlobals->cbState;
        cbState->games = g_slist_remove( cbState->games, bGlobals ); /* no!!! */
    }
    
    /* onCursesBoardClosing( bGlobals->aGlobals, bGlobals ); */
    XP_LOGFF( "freeing globals: %p", bGlobals );
    g_free( bGlobals );
}

static CursesBoardGlobals*
ref( CursesBoardGlobals* bGlobals )
{
    ++bGlobals->refCount;
    XP_LOGFF( "refCount now %d", bGlobals->refCount );
    return bGlobals;
}

static void
unref( CursesBoardGlobals* bGlobals )
{
    --bGlobals->refCount;
    XP_LOGFF( "refCount now %d", bGlobals->refCount );
    if ( 0 == bGlobals->refCount ) {
        disposeBoard( bGlobals, XP_TRUE );
    }
}

static int
utf8_len( const char* face )
{
    const int max = strlen(face);
    int count = 0;
    mbstate_t ps = {0};
    for ( int offset = 0; offset < max; ) {
        size_t nBytes = mbrlen( &face[offset], max - offset, &ps );
        if ( 0 < nBytes ) {
            ++count;
            offset += nBytes;
        } else {
            break;
        }
    }
    return count;
}

static void
getFromDict( const DictionaryCtxt* dict, XP_U16* fontWidthP,
             XP_U16* fontHtP )
{
    int maxSide = 1;

    for ( Tile tile = 0; tile < dict->nFaces; ++tile ) {
        const XP_UCHAR* face = dict_getTileString( dict, tile );
        int thisLen = utf8_len( face );
        while ( thisLen > maxSide * maxSide ) {
            ++maxSide;
        }
    }

    /* XP_LOGF( "%s(): width = %d", __func__, maxLen ); */
    *fontWidthP = *fontHtP = maxSide;
}

static void
setupBoard( CursesBoardGlobals* bGlobals )
{
    LOG_FUNC();
    /* positionSizeStuff( bGlobals ); */
    CommonGlobals* cGlobals = &bGlobals->cGlobals;
    BoardCtxt* board = cGlobals->game.board;
    const int width = bGlobals->winWidth;
    const int height = bGlobals->winHeight;

    XP_U16 fontWidth, fontHt;
    const DictionaryCtxt* dict = model_getDictionary( cGlobals->game.model );
    getFromDict( dict, &fontWidth, &fontHt );
    BoardDims dims;
    board_figureLayout( board, NULL_XWE, cGlobals->gi,
                        0, 0, width, height, 100,
                        150, 200, /* percents */
                        width*75/100,
                        fontWidth, fontHt,
                        XP_FALSE, &dims );
    board_applyLayout( board, NULL_XWE, &dims );
    XP_LOGFF( "calling board_draw()" );
    board_invalAll( board );
    board_draw( board, NULL_XWE );
}

static void
initCP( CommonGlobals* cGlobals )
{
    const LaunchParams* params = cGlobals->params;
    cGlobals->cp.showBoardArrow = XP_TRUE;
    cGlobals->cp.showRobotScores = params->showRobotScores;
    cGlobals->cp.hideTileValues = params->hideValues;
    cGlobals->cp.skipMQTTAdd = params->skipMQTTAdd;
    cGlobals->cp.skipCommitConfirm = params->skipCommitConfirm;
    cGlobals->cp.sortNewTiles = params->sortNewTiles;
    cGlobals->cp.showColors = params->showColors;
    cGlobals->cp.allowPeek = params->allowPeek;
#ifdef XWFEATURE_SLOW_ROBOT
    cGlobals->cp.robotThinkMin = params->robotThinkMin;
    cGlobals->cp.robotThinkMax = params->robotThinkMax;
    cGlobals->cp.robotTradePct = params->robotTradePct;
#endif
#ifdef XWFEATURE_ROBOTPHONIES
    cGlobals->cp.makePhonyPct = params->makePhonyPct;
#endif
}

static CursesBoardGlobals*
initNoDraw( CursesBoardState* cbState, sqlite3_int64 rowid,
            const CurGameInfo* gi, const CommsAddrRec* returnAddr )
{
    LOG_FUNC();
    CursesBoardGlobals* result = commonInit( cbState, rowid, gi );
    CommonGlobals* cGlobals = &result->cGlobals;

    if ( !!returnAddr ) {
        cGlobals->hostAddr = *returnAddr;
    }

    initCP( cGlobals );

    if ( linuxOpenGame( cGlobals ) ) {
         result = ref( result );
    } else {
        disposeBoard( result, XP_TRUE );
        result = NULL;
    }
    return result;
}

static void
enableDraw( CursesBoardGlobals* bGlobals, const cb_dims* dims )
{
    LOG_FUNC();
    XP_ASSERT( !!dims );
    bGlobals->boardWin = newwin( dims->height, dims->width, dims->top, 0 );
    getmaxyx( bGlobals->boardWin, bGlobals->winHeight, bGlobals->winWidth );

    CommonGlobals* cGlobals = &bGlobals->cGlobals;
    if( !!bGlobals->boardWin ) {
        cGlobals->draw = cursesDrawCtxtMake( bGlobals->boardWin );
        board_setDraw( cGlobals->game.board, NULL_XWE, cGlobals->draw );
    }

    setupBoard( bGlobals );
}

CursesBoardGlobals*
findOrOpenForGameID( CursesBoardState* cbState, XP_U32 gameID,
                     const CurGameInfo* gi, const CommsAddrRec* returnAddr )
{
    CursesBoardGlobals* result = NULL;
    sqlite3_int64 rowids[1];
    int nRowIDs = VSIZE(rowids);
    gdb_getRowsForGameID( cbState->params->pDb, gameID, rowids, &nRowIDs );
    if ( 1 == nRowIDs ) {
        result = findOrOpen( cbState, rowids[0], gi, returnAddr );
    }
    return result;
}

const CommonGlobals*
cb_getForGameID( CursesBoardState* cbState, XP_U32 gameID )
{
    CursesBoardGlobals* cbg = findOrOpenForGameID( cbState, gameID, NULL, NULL );
    CommonGlobals* result = &cbg->cGlobals;
    // XP_LOGFF( "(%X) => %p", gameID, result );
    return result;
}

static CursesBoardGlobals*
findOrOpen( CursesBoardState* cbState, sqlite3_int64 rowid,
            const CurGameInfo* gi, const CommsAddrRec* returnAddr )
{
    CursesBoardGlobals* result = NULL;
    for ( GSList* iter = cbState->games;
          rowid >= 0 && !!iter && !result; iter = iter->next ) {
        CursesBoardGlobals* one = (CursesBoardGlobals*)iter->data;
        if ( one->cGlobals.rowid == rowid ) {
            result = one;
        }
    }

    if ( !result ) {
        result = initNoDraw( cbState, rowid, gi, returnAddr );
        if ( !!result ) {
            setupBoard( result );
            cbState->games = g_slist_append( cbState->games, result );
        }
    }
    return result;
}

bool
cb_feedRow( CursesBoardState* cbState, sqlite3_int64 rowid, XP_U16 expectSeed,
            const XP_U8* buf, XP_U16 len, const CommsAddrRec* from )
{
    LOG_FUNC();
    CursesBoardGlobals* bGlobals = findOrOpen( cbState, rowid, NULL, NULL );
    CommonGlobals* cGlobals = &bGlobals->cGlobals;
    XP_U16 seed = comms_getChannelSeed( cGlobals->game.comms );
    bool success = 0 == expectSeed || seed == expectSeed;
    if ( success ) {
        gameGotBuf( cGlobals, XP_TRUE, buf, len, from );
    } else {
        XP_LOGFF( "msg for seed %d but I opened %d", expectSeed, seed );
    }
    return success;
}

void
cb_feedGame( CursesBoardState* cbState, XP_U32 gameID,
             const XP_U8* buf, XP_U16 len, const CommsAddrRec* from )
{
    sqlite3_int64 rowids[4];
    int nRows = VSIZE( rowids );
    LaunchParams* params = cbState->params;
    gdb_getRowsForGameID( params->pDb, gameID, rowids, &nRows );
    XP_LOGFF( "found %d rows for gameID %X", nRows, gameID );
    for ( int ii = 0; ii < nRows; ++ii ) {
#ifdef DEBUG
        bool success =
#endif
            cb_feedRow( cbState, rowids[ii], 0, buf, len, from );
        XP_ASSERT( success );
    }
}

void
cb_addInvites( CursesBoardState* cbState, XP_U32 gameID, XP_U16 nRemotes,
               XP_U16 forceChannels[], const CommsAddrRec destAddrs[] )
{
    CursesBoardGlobals* bGlobals = findOrOpenForGameID( cbState, gameID,
                                                        NULL, NULL );
    CommonGlobals* cGlobals = &bGlobals->cGlobals;
    linux_addInvites( cGlobals, nRemotes, forceChannels, destAddrs );
}

XP_Bool
cb_makeRematch( CursesBoardState* cbState, XP_U32 gameID, RematchOrder ro,
                XP_U32* newGameIDP )
{
    CursesBoardGlobals* bGlobals = findOrOpenForGameID( cbState, gameID,
                                                        NULL, NULL );
    XP_Bool success = rematch_and_save( bGlobals, ro, newGameIDP );
    return success;
}

XP_Bool
cb_makeMoveIf( CursesBoardState* cbState, XP_U32 gameID, XP_Bool tryTrade )
{
    XP_LOGFF( "(tryTrade: %s)", boolToStr(tryTrade));
    CursesBoardGlobals* bGlobals =
        findOrOpenForGameID( cbState, gameID, NULL, NULL );
    XP_Bool success = !!bGlobals
        && linux_makeMoveIf( &bGlobals->cGlobals, tryTrade );

    LOG_RETURNF( "%s", boolToStr(success) );
    return success;
}

XP_Bool
cb_sendChat( CursesBoardState* cbState, XP_U32 gameID, const char* msg )
{
    CursesBoardGlobals* bGlobals =
        findOrOpenForGameID( cbState, gameID, NULL, NULL );
    XP_Bool success = !!bGlobals;
    if ( success ) {
        board_sendChat( bGlobals->cGlobals.game.board, NULL_XWE, msg );
    }
    return success;
}

static void
kill_board( gpointer data )
{
    CursesBoardGlobals* bGlobals = (CursesBoardGlobals*)data;
    linuxSaveGame( &bGlobals->cGlobals );
    disposeBoard( bGlobals, XP_FALSE );
}

void
cb_closeAll( CursesBoardState* cbState )
{
    g_slist_free_full( cbState->games, kill_board );
}

static void
cursesUserError( CursesBoardGlobals* bGlobals, const char* format, ... )
{
    char buf[512];
    va_list ap;
    va_start( ap, format );
    vsprintf( buf, format, ap );
    va_end(ap);

    if ( !!bGlobals->boardWin ) {
        (void)ca_inform( bGlobals->boardWin, buf );
    } else {
        XP_LOGFF( "(msg=%s)", buf );
    }
} /* cursesUserError */

static void
curses_util_notifyPickTileBlank( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe), XP_U16 playerNum,
                                 XP_U16 XP_UNUSED(col), XP_U16 XP_UNUSED(row),
                                 const XP_UCHAR** texts, XP_U16 nTiles )
{
    CursesBoardGlobals* globals = (CursesBoardGlobals*)uc->closure;
    char query[128];
    char* playerName = globals->cGlobals.gi->players[playerNum].name;

    snprintf( query, sizeof(query), 
              "Pick tile for %s! (Tab or type letter to select "
              "then hit <cr>.)", playerName );

    /*index = */curses_askLetter( globals->boardWin, query, texts, nTiles );
    // return index;
} /* util_userPickTile */

static void
curses_util_informNeedPickTiles( XW_UtilCtxt* XP_UNUSED(uc),
                                 XWEnv XP_UNUSED(xwe),
                                 XP_Bool XP_UNUSED(isInitial),
                                 XP_U16 XP_UNUSED(player),
                                 XP_U16 XP_UNUSED(nToPick),
                                 XP_U16 XP_UNUSED(nFaces),
                                 const XP_UCHAR** XP_UNUSED(faces),
                                 const XP_U16* XP_UNUSED(counts) )
{
    XP_ASSERT(0);
    /* CursesBoardGlobals* globals = (CursesBoardGlobals*)uc->closure; */
    /* char query[128]; */
    /* XP_S16 index; */
    /* char* playerName = globals->cGlobals.gi->players[playerNum].name; */

    /* snprintf( query, sizeof(query),  */
    /*           "Pick tile for %s! (Tab or type letter to select " */
    /*           "then hit <cr>.)", playerName ); */

    /* index = curses_askLetter( globals, query, texts, nTiles ); */
    /* return index; */
}

static void
curses_util_userError( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe), UtilErrID id )
{
    CursesBoardGlobals* globals = (CursesBoardGlobals*)uc->closure;
    XP_Bool silent;
    const XP_UCHAR* message = linux_getErrString( id, &silent );

    if ( silent ) {
        XP_LOGFF( "silent userError: %s", message );
    } else {
        cursesUserError( globals, message );
    }
} /* curses_util_userError */

static gint
ask_move( gpointer data )
{
    CursesBoardGlobals* bGlobals = (CursesBoardGlobals*)data;
    CommonGlobals* cGlobals = &bGlobals->cGlobals;
    const char* answers[] = {"Ok", "Cancel", NULL};

    if (0 == cursesask(bGlobals->boardWin, cGlobals->question,
                       VSIZE(answers)-1, answers) ) {
        BoardCtxt* board = cGlobals->game.board;
        if ( board_commitTurn( board, NULL_XWE, XP_TRUE, XP_TRUE, NULL ) ) {
            board_draw( board, NULL_XWE );
            linuxSaveGame( &bGlobals->cGlobals );
        }
    }

    return FALSE;
}

static void
curses_util_informNeedPassword( XW_UtilCtxt* XP_UNUSED(uc),
                                XWEnv XP_UNUSED(xwe),
                                XP_U16 XP_UNUSED_DBG(playerNum),
                                const XP_UCHAR* XP_UNUSED_DBG(name) )
{
    XP_WARNF( "curses_util_informNeedPassword(num=%d, name=%s", playerNum, name );
    XP_ASSERT(0);
} /* curses_util_askPassword */

static void
curses_util_yOffsetChange( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe),
                           XP_U16 XP_UNUSED(maxOffset),
                           XP_U16 oldOffset, XP_U16 newOffset )
{
    if ( 0 != newOffset ) {
        CursesBoardGlobals* bGlobals = (CursesBoardGlobals*)uc->closure;
        if ( !!bGlobals->boardWin ) {
            ca_informf( bGlobals->boardWin, "%s(oldOffset=%d, newOffset=%d)", __func__,
                        oldOffset, newOffset );
        }
    }
} /* curses_util_yOffsetChange */

#ifdef XWFEATURE_TURNCHANGENOTIFY
static void
curses_util_turnChanged( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe),
                         XP_S16 XP_UNUSED_DBG(newTurn) )
{
    XP_LOGFF( "(newTurn=%d)", newTurn );
    CursesBoardGlobals* bGlobals = (CursesBoardGlobals*)uc->closure;
    linuxSaveGame( &bGlobals->cGlobals );
}
#endif

static void
curses_util_notifyIllegalWords( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe), BadWordInfo* bwi,
                                XP_U16 player, XP_Bool turnLost )
{
    gchar* strs = g_strjoinv( "\", \"", (gchar**)bwi->words );
    gchar* msg = g_strdup_printf( "Player %d played bad word[s]: \"%s\". "
                                  "Turn lost: %s", player, strs, boolToStr(turnLost) );

    CursesBoardGlobals* bGlobals = (CursesBoardGlobals*)uc->closure;
    if ( !!bGlobals->boardWin ) {
        ca_inform( bGlobals->boardWin, msg );
    } else {
        XP_LOGFF( "msg: %s", msg );
    }
    g_free( strs );
    g_free( msg );
} /* curses_util_notifyIllegalWord */

/* this needs to change!!! */
static void
curses_util_notifyMove( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe), XWStreamCtxt* stream )
{
    CursesBoardGlobals* globals = (CursesBoardGlobals*)uc->closure;
    CommonGlobals* cGlobals = &globals->cGlobals;
    XP_U16 len = stream_getSize( stream );
    XP_ASSERT( len <= VSIZE(cGlobals->question) );
    stream_getBytes( stream, cGlobals->question, len );
    cGlobals->question[len] = '\0';
    (void)ADD_ONETIME_IDLE( ask_move, globals );
} /* curses_util_not */

static gint
ask_trade( gpointer data )
{
    CursesBoardGlobals* bGlobals = (CursesBoardGlobals*)data;
    CommonGlobals* cGlobals = &bGlobals->cGlobals;

    const char* buttons[] = { "Ok", "Cancel" };
    if (0 == cursesask( bGlobals->boardWin, cGlobals->question,
                        VSIZE(buttons), buttons ) ) {
        BoardCtxt* board = cGlobals->game.board;
        if ( board_commitTurn( board, NULL_XWE, XP_TRUE, XP_TRUE, NULL ) ) {
            board_draw( board, NULL_XWE );
            linuxSaveGame( cGlobals );
        }
    }
    return FALSE;
}

static void
curses_util_notifyTrade( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe),
                         const XP_UCHAR** tiles, XP_U16 nTiles )
{
    CursesBoardGlobals* globals = (CursesBoardGlobals*)uc->closure;
    formatConfirmTrade( &globals->cGlobals, tiles, nTiles );
    (void)ADD_ONETIME_IDLE( ask_trade, globals );
}

static void
curses_util_trayHiddenChange( XW_UtilCtxt* XP_UNUSED(uc), XWEnv XP_UNUSED(xwe),
                              XW_TrayVisState XP_UNUSED(state),
                              XP_U16 XP_UNUSED(nVisibleRows) )
{
    /* nothing to do if we don't have a scrollbar */
} /* curses_util_trayHiddenChange */

static void
cursesShowFinalScores( CursesBoardGlobals* bGlobals )
{
    XWStreamCtxt* stream;
    XP_UCHAR* text;

    CommonGlobals* cGlobals = &bGlobals->cGlobals;
    stream = mem_stream_make_raw( MPPARM(cGlobals->util->mpool)
                                  cGlobals->params->vtMgr );
    server_writeFinalScores( cGlobals->game.server, NULL_XWE, stream );

    text = strFromStream( stream );

    (void)ca_inform( bGlobals->boardWin, text );

    free( text );
    stream_destroy( stream );
} /* cursesShowFinalScores */

static void
curses_util_informMove( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe),
                        XP_S16 XP_UNUSED(turn), XWStreamCtxt* expl,
                        XWStreamCtxt* XP_UNUSED(words))
{
    CursesBoardGlobals* bGlobals = (CursesBoardGlobals*)uc->closure;
    if ( !!bGlobals->boardWin ) {
        char* question = strFromStream( expl );
        (void)ca_inform( bGlobals->boardWin, question );
        free( question );
    }
}

static void
curses_util_notifyDupStatus( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe),
                             XP_Bool XP_UNUSED(amHost),
                             const XP_UCHAR* msg )
{
    CursesBoardGlobals* bGlobals = (CursesBoardGlobals*)uc->closure;
    if ( !!bGlobals->boardWin ) {
        ca_inform( bGlobals->boardWin, msg );
    }
}

static void
curses_util_informUndo( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe) )
{
    CursesBoardGlobals* bGlobals = (CursesBoardGlobals*)uc->closure;
    if ( !!bGlobals->boardWin ) {
        ca_inform( bGlobals->boardWin, "informUndo(): undo was done" );
    }
}

static void
rematch_and_save_once( CursesBoardGlobals* bGlobals, RematchOrder ro )
{
    LOG_FUNC();
    CommonGlobals* cGlobals = &bGlobals->cGlobals;

    int32_t alreadyDone;
    gchar key[128];
    snprintf( key, sizeof(key), "%X/rematch_done", cGlobals->gi->gameID );
    if ( gdb_fetchInt( cGlobals->params->pDb, key, &alreadyDone )
         && 0 != alreadyDone ) {
        XP_LOGFF( "already rematched game %X", cGlobals->gi->gameID );
    } else {
        if ( rematch_and_save( bGlobals, ro, NULL ) ) {
            gdb_storeInt( cGlobals->params->pDb, key, 1 );
        }
    }
    LOG_RETURN_VOID();
}

static XP_Bool
rematch_and_save( CursesBoardGlobals* bGlobals, RematchOrder ro,
                  XP_U32* newGameIDP )
{
    LOG_FUNC();
    CommonGlobals* cGlobals = &bGlobals->cGlobals;
    CursesBoardState* cbState = bGlobals->cbState;

    CursesBoardGlobals* bGlobalsNew = commonInit( cbState, -1, NULL );

    NewOrder no;
    server_figureOrder( cGlobals->game.server, ro, &no );

    XP_Bool success = game_makeRematch( &cGlobals->game, NULL_XWE,
                                        bGlobalsNew->cGlobals.util,
                                        &cGlobals->cp, &bGlobalsNew->cGlobals.procs,
                                        &bGlobalsNew->cGlobals.game, "newName", &no );
    if ( success ) {
        if ( !!newGameIDP ) {
            *newGameIDP = bGlobalsNew->cGlobals.gi->gameID;
        }
        linuxSaveGame( &bGlobalsNew->cGlobals );
    }
    disposeBoard( bGlobalsNew, XP_TRUE );
    LOG_RETURNF( "%s", boolToStr(success) );
    return success;
}

static void
curses_util_notifyGameOver( XW_UtilCtxt* uc, XWEnv xwe, XP_S16 quitter )
{
    LOG_FUNC();
    CursesBoardGlobals* bGlobals = (CursesBoardGlobals*)uc->closure;
    CommonGlobals* cGlobals = &bGlobals->cGlobals;
    LaunchParams* params = cGlobals->params;
    board_draw( cGlobals->game.board, xwe );

    /* game belongs in cGlobals... */
    if ( params->printHistory ) {
        catGameHistory( cGlobals );
    }

    catFinalScores( cGlobals, quitter );

    if ( params->quitAfter >= 0 ) {
        sleep( params->quitAfter );
        handleQuit( bGlobals->cbState->aGlobals, 0 );
    } else if ( params->undoWhenDone ) {
        server_handleUndo( cGlobals->game.server, xwe, 0 );
    } else if ( !params->skipGameOver && !!bGlobals->boardWin ) {
        /* This is modal.  Don't show if quitting */
        cursesShowFinalScores( bGlobals );
    }

    if ( params->rematchOnDone ) {
        RematchOrder ro = !!params->rematchOrder
            ? roFromStr(params->rematchOrder) : RO_NONE;
        rematch_and_save_once( bGlobals, ro );
    }
} /* curses_util_notifyGameOver */

static void
curses_util_informNetDict( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe),
                           const XP_UCHAR* XP_UNUSED(isoCode),
                           const XP_UCHAR* XP_UNUSED_DBG(oldName),
                           const XP_UCHAR* XP_UNUSED_DBG(newName), 
                           const XP_UCHAR* XP_UNUSED_DBG(newSum),
                           XWPhoniesChoice phoniesAction )
{
    XP_USE(uc);
    XP_USE(phoniesAction);
    XP_LOGFF( "%s => %s (cksum: %s)", oldName, newName, newSum );
}

#ifdef XWFEATURE_SEARCHLIMIT
static XP_Bool
curses_util_getTraySearchLimits( XW_UtilCtxt* XP_UNUSED(uc),XWEnv XP_UNUSED(xwe),
                                 XP_U16* XP_UNUSED(min), XP_U16* XP_UNUSED(max) )
{
    XP_ASSERT(0);
    return XP_TRUE;
}
#endif


#ifdef XWFEATURE_HILITECELL
static XP_Bool
curses_util_hiliteCell( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe),
                        XP_U16 XP_UNUSED(col), XP_U16 XP_UNUSED(row) )
{
    CursesBoardGlobals* globals = (CursesBoardGlobals*)uc->closure;
    if ( globals->cGlobals.params->sleepOnAnchor ) {
        usleep( 10000 );
    }
    return XP_TRUE;
} /* curses_util_hiliteCell */
#endif

static XP_Bool
curses_util_engineProgressCallback( XW_UtilCtxt* XP_UNUSED(uc), XWEnv XP_UNUSED(xwe) )
{
    return XP_TRUE;
} /* curses_util_engineProgressCallback */

static XP_Bool
curses_util_altKeyDown( XW_UtilCtxt* XP_UNUSED(uc), XWEnv XP_UNUSED(xwe) )
{
    return XP_FALSE;
}

static void
curses_util_remSelected( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe) )
{
    CursesBoardGlobals* bGlobals = (CursesBoardGlobals*)uc->closure;
    CommonGlobals* cGlobals = (CommonGlobals*)uc->closure;
    XWStreamCtxt* stream;
    XP_UCHAR* text;

    stream = mem_stream_make_raw( MPPARM(cGlobals->util->mpool)
                                  cGlobals->params->vtMgr );
    board_formatRemainingTiles( cGlobals->game.board, NULL_XWE, stream );

    text = strFromStream( stream );

    (void)ca_inform( bGlobals->boardWin, text );

    free( text );
}

static void
curses_util_getMQTTIDsFor( XW_UtilCtxt* uc, XWEnv xwe, XP_U16 nRelayIDs,
                           const XP_UCHAR* relayIDs[] )
{
    XP_ASSERT(0);               /* implement me */
    XP_USE( uc );
    XP_USE( xwe );
    XP_USE( nRelayIDs );
    XP_USE( relayIDs );
}

static void
curses_util_timerSelected( XW_UtilCtxt* XP_UNUSED(uc), XWEnv XP_UNUSED(xwe),
                           XP_Bool XP_UNUSED_DBG(inDuplicateMode),
                           XP_Bool XP_UNUSED_DBG(canPause) )
{
    XP_LOGFF( "(inDuplicateMode=%d, canPause=%d)", inDuplicateMode, canPause );
}

static void
curses_util_bonusSquareHeld( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe),
                             XWBonusType bonus )
{
    LOG_FUNC();
    XP_USE( uc );
    XP_USE( bonus );
}

static void
curses_util_playerScoreHeld( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe), XP_U16 player )
{
    CursesBoardGlobals* bGlobals = (CursesBoardGlobals*)uc->closure;
    LastMoveInfo lmi;
    if ( model_getPlayersLastScore( bGlobals->cGlobals.game.model, NULL_XWE,
                                    player, &lmi ) ) {
        XP_UCHAR buf[128];
        formatLMI( &lmi, buf, VSIZE(buf) );
        (void)ca_inform( bGlobals->boardWin, buf );
    }
}

#ifdef XWFEATURE_BOARDWORDS
static void
curses_util_cellSquareHeld( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe),
                            XWStreamCtxt* words )
{
    XP_USE( uc );
    catOnClose( words, NULL_XWE, NULL );
    fprintf( stderr, "\n" );
}
#endif

static void
curses_util_informWordsBlocked( XW_UtilCtxt* XP_UNUSED(uc), XWEnv XP_UNUSED(xwe),
                                XP_U16 XP_UNUSED_DBG(nBadWords),
                                XWStreamCtxt* XP_UNUSED(words),
                                const XP_UCHAR* XP_UNUSED_DBG(dictName) )
{
    XP_LOGFF( "(nBadWords=%d, dict=%s)", nBadWords, dictName );
}

#ifdef XWFEATURE_CHAT
static void
curses_util_showChat( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe),
                      const XP_UCHAR* const XP_UNUSED_DBG(msg),
                      XP_S16 XP_UNUSED_DBG(from),
                      XP_U32 XP_UNUSED(timestamp) )
{
    CursesBoardGlobals* globals = (CursesBoardGlobals*)uc->closure;
    globals->nChatsSent = 0;
# ifdef DEBUG
    XP_LOGFF( "got \"%s\" from player[%d]", msg, from );
# endif
}
#endif

static void
setupCursesUtilCallbacks( CursesBoardGlobals* bGlobals, XW_UtilCtxt* util )
{
    util->closure = bGlobals;
#define SET_PROC(NAM) util->vtable->m_util_##NAM = curses_util_##NAM
    SET_PROC(userError);
    SET_PROC(notifyMove);
    SET_PROC(notifyTrade);
    SET_PROC(notifyPickTileBlank);
    SET_PROC(informNeedPickTiles);
    SET_PROC(informNeedPassword);
    SET_PROC(trayHiddenChange);
    SET_PROC(yOffsetChange);
#ifdef XWFEATURE_TURNCHANGENOTIFY
    SET_PROC(turnChanged);
#endif
    SET_PROC(notifyDupStatus);
    SET_PROC(informMove);
    SET_PROC(informUndo);
    SET_PROC(informNetDict);
    SET_PROC(notifyGameOver);
#ifdef XWFEATURE_HILITECELL
    SET_PROC(hiliteCell);
#endif
    SET_PROC(engineProgressCallback);
    SET_PROC(altKeyDown);       /* ?? */
    SET_PROC(notifyIllegalWords);
    SET_PROC(remSelected);
    SET_PROC(getMQTTIDsFor);
    SET_PROC(timerSelected);
#ifndef XWFEATURE_MINIWIN
    SET_PROC(bonusSquareHeld);
    SET_PROC(playerScoreHeld);
#endif
#ifdef XWFEATURE_BOARDWORDS
    SET_PROC(cellSquareHeld);
#endif
    SET_PROC(informWordsBlocked);

#ifdef XWFEATURE_SEARCHLIMIT
    SET_PROC(getTraySearchLimits);
#endif
#ifdef XWFEATURE_CHAT
    SET_PROC(showChat);
#endif
#undef SET_PROC

    assertTableFull( util->vtable, sizeof(*util->vtable), "curses util" );
} /* setupCursesUtilCallbacks */

static bool
handleFlip( void* closure, int XP_UNUSED(key) )
{
    CursesBoardGlobals* bGlobals = (CursesBoardGlobals*)closure;
    CommonGlobals* cGlobals = &bGlobals->cGlobals;
    if ( board_flip( cGlobals->game.board ) ) {
        board_draw( cGlobals->game.board, NULL_XWE );
    }

    return XP_TRUE;
} /* handleFlip */

static bool
handleToggleValues( void* XP_UNUSED(closure), int XP_UNUSED(key) )
{
    XP_ASSERT( 0 );
    return XP_TRUE;
} /* handleToggleValues */

static bool
handleBackspace( void* closure, int XP_UNUSED(key) )
{
    CommonGlobals* cGlobals = &((CursesBoardGlobals*)closure)->cGlobals;
    XP_Bool handled;
    if ( board_handleKey( cGlobals->game.board, NULL_XWE,
                          XP_CURSOR_KEY_DEL, &handled ) ) {
        board_draw( cGlobals->game.board, NULL_XWE );
    }
    return XP_TRUE;
} /* handleBackspace */

static bool
handleUndo( void* closure, int XP_UNUSED(key) )
{
    CursesBoardGlobals* bGlobals = (CursesBoardGlobals*)closure;
    CommonGlobals* cGlobals = &bGlobals->cGlobals;
    if ( server_handleUndo( cGlobals->game.server, NULL_XWE, 0 ) ) {
        board_draw( cGlobals->game.board, NULL_XWE );
    }
    return XP_TRUE;
} /* handleUndo */

static bool
handleReplace( void* closure, int XP_UNUSED(key) )
{
    CommonGlobals* cGlobals = &((CursesBoardGlobals*)closure)->cGlobals;
    if ( board_replaceTiles( cGlobals->game.board, NULL_XWE ) ) {
        board_draw( cGlobals->game.board, NULL_XWE );
    }
    return XP_TRUE;
} /* handleReplace */

static bool
inviteList( CommonGlobals* cGlobals, CommsAddrRec* myAddr, GSList* invitees,
            CommsConnType typ )
{
    bool haveAddressees = !!invitees;
    if ( haveAddressees ) {
        LaunchParams* params = cGlobals->params;
#ifdef XWFEATURE_COMMS_INVITE
        CommsAddrRec destAddr = {0};
#endif
        for ( int ii = 0; ii < g_slist_length(invitees); ++ii ) {
            const XP_U16 nPlayersH = params->connInfo.inviteeCounts[ii];
            const gint forceChannel = ii + 1;
            XP_LOGFF( "using nPlayersH of %d, forceChannel of %d for guest device %d",
                      nPlayersH, forceChannel, ii );
            NetLaunchInfo nli;
            nli_init( &nli, cGlobals->gi, myAddr, nPlayersH, forceChannel );
            switch ( typ ) {
#ifdef XWFEATURE_RELAY
            case COMMS_CONN_RELAY: {
                uint64_t inviteeRelayID = *(uint64_t*)g_slist_nth_data( invitees, ii );
                relaycon_invite( params, (XP_U32)inviteeRelayID, NULL, &nli );
            }
                break;
#endif
            case COMMS_CONN_SMS: {
                const gchar* inviteePhone = (const gchar*)g_slist_nth_data( invitees, ii );
#ifdef XWFEATURE_COMMS_INVITE
                addr_addType( &destAddr, COMMS_CONN_SMS );
                destAddr.u.sms.port = params->connInfo.sms.port;
                XP_STRNCPY( destAddr.u.sms.phone, inviteePhone,
                            sizeof(destAddr.u.sms.phone) );
#else
                linux_sms_invite( params, &nli, inviteePhone,
                                  params->connInfo.sms.port );
#endif
            }
                break;
            case COMMS_CONN_MQTT: {
                MQTTDevID devID;
                const gchar* str = g_slist_nth_data( invitees, ii );
                if ( strToMQTTCDevID( str, &devID ) ) {
#ifdef XWFEATURE_COMMS_INVITE
                    addr_addType( &destAddr, COMMS_CONN_MQTT );
                    destAddr.u.mqtt.devID = devID;
#else
                    mqttc_invite( params, 0, &nli, &devID );
#endif
                } else {
                    XP_LOGFF( "unable to convert devid %s", str );
                }
            }
                break;
            default:
                XP_ASSERT(0);
            }
#ifdef XWFEATURE_COMMS_INVITE
            comms_invite( cGlobals->game.comms, NULL_XWE, &nli,
                          &destAddr, XP_TRUE );
#endif
        }
    }
    if ( haveAddressees ) {
        XP_LOGFF( "worked for %s", ConnType2Str( typ ) );
    }
    return haveAddressees;
}

static bool
sendInvite( void* closure, int XP_UNUSED(key) )
{
    LOG_FUNC();
    CursesBoardGlobals* bGlobals = (CursesBoardGlobals*)closure;
    CommonGlobals* cGlobals = &bGlobals->cGlobals;
    LaunchParams* params = cGlobals->params;
    CommsAddrRec selfAddr;
    CommsCtxt* comms = cGlobals->game.comms;
    XP_ASSERT( comms );
    comms_getSelfAddr( comms, &selfAddr );

    gint forceChannel = 1;
    const XP_U16 nPlayers = params->connInfo.inviteeCounts[forceChannel-1];
    NetLaunchInfo nli;
    nli_init( &nli, cGlobals->gi, &selfAddr, nPlayers, forceChannel );

    if ( SERVER_ISHOST != cGlobals->gi->serverRole ) {
        ca_inform( bGlobals->boardWin, "Only hosts can invite" );

        /* Invite first based on an invitee provided. Otherwise, fall back to
           doing a send-to-self. Let the recipient code reject a duplicate if
           the user so desires. */
    } else if ( inviteList( cGlobals, &selfAddr, params->connInfo.sms.inviteePhones,
                            COMMS_CONN_SMS ) ) {
        /* do nothing */
#ifdef XWFEATURE_RELAY
    } else if ( inviteList( cGlobals, &selfAddr, params->connInfo.relay.inviteeRelayIDs,
                            COMMS_CONN_RELAY ) ) {
        /* do nothing */
#endif
    } else if ( inviteList( cGlobals, &selfAddr, params->connInfo.mqtt.inviteeDevIDs,
                            COMMS_CONN_MQTT ) ) {
        /* do nothing */
    /* Try sending to self, using the phone number or relayID of this device */
/* <<<<<<< HEAD */
    } else if ( addr_hasType( &selfAddr, COMMS_CONN_SMS ) ) {
        linux_sms_invite( params, &nli, selfAddr.u.sms.phone, selfAddr.u.sms.port );
    } else if ( addr_hasType( &selfAddr, COMMS_CONN_MQTT ) ) {
        mqttc_invite( params, &nli, mqttc_getDevID( params ) );
#ifdef XWFEATURE_RELAY
    } else if ( addr_hasType( &selfAddr, COMMS_CONN_RELAY ) ) {
/* ======= */
/*     } else if ( addr_hasType( &addr, COMMS_CONN_SMS ) ) { */
/*         linux_sms_invite( params, &nli, addr.u.sms.phone, addr.u.sms.port ); */
/*     } else if ( addr_hasType( &addr, COMMS_CONN_MQTT ) ) { */
/*         mqttc_invite( params, 0, &nli, mqttc_getDevID( params ) ); */
/*     } else if ( addr_hasType( &addr, COMMS_CONN_RELAY ) ) { */
/* >>>>>>> d9781d21e (snapshot: mqtt invites for gtk work via comms) */
        XP_U32 relayID = linux_getDevIDRelay( params );
        if ( 0 != relayID ) {
            relaycon_invite( params, relayID, NULL, &nli );
        }
#endif
    } else {
        ca_inform( bGlobals->boardWin, "Cannot invite via relayID, MQTT or by \"sms phone\"." );
    }
    LOG_RETURNF( "%s", "TRUE" );
    return XP_TRUE;
}

static bool
handleCommit( void* closure, int XP_UNUSED(key) )
{
    CursesBoardGlobals* bGlobals = (CursesBoardGlobals*)closure;
    CommonGlobals* cGlobals = &bGlobals->cGlobals;
    if ( board_commitTurn( cGlobals->game.board, NULL_XWE, XP_FALSE,
                           XP_FALSE, NULL ) ) {
        board_draw( cGlobals->game.board, NULL_XWE );
    }
    return XP_TRUE;
} /* handleCommit */

static bool
handleJuggle( void* closure, int XP_UNUSED(key) )
{
    CommonGlobals* cGlobals = &((CursesBoardGlobals*)closure)->cGlobals;
    if ( board_juggleTray( cGlobals->game.board, NULL_XWE ) ) {
        board_draw( cGlobals->game.board, NULL_XWE );
    }
    return XP_TRUE;
} /* handleJuggle */

static bool
handleHide( void* closure, int XP_UNUSED(key) )
{
    CommonGlobals* cGlobals = &((CursesBoardGlobals*)closure)->cGlobals;
    XW_TrayVisState curState = 
        board_getTrayVisState( cGlobals->game.board );

    bool draw = curState == TRAY_REVEALED
        ? board_hideTray( cGlobals->game.board, NULL_XWE )
        : board_showTray( cGlobals->game.board, NULL_XWE );
    if ( draw ) {
        board_draw( cGlobals->game.board, NULL_XWE );
    }

    return XP_TRUE;
} /* handleJuggle */

#ifdef KEYBOARD_NAV
static void
checkAssignFocus( BoardCtxt* board )
{
    if ( OBJ_NONE == board_getFocusOwner(board) ) {
        board_focusChanged( board, NULL_XWE, OBJ_BOARD, XP_TRUE );
    }
}

static XP_Bool
handleFocusKey( CursesBoardGlobals* bGlobals, XP_Key key )
{
    CommonGlobals* cGlobals = &bGlobals->cGlobals;
    checkAssignFocus( cGlobals->game.board );

    XP_Bool handled;
    XP_Bool draw = board_handleKey( cGlobals->game.board, NULL_XWE,
                                    key, &handled );
    if ( !handled ) {
        BoardObjectType nxt;
        BoardObjectType order[] = { OBJ_BOARD, OBJ_SCORE, OBJ_TRAY };
        draw = linShiftFocus( cGlobals, key, order, &nxt ) || draw;
        if ( nxt != OBJ_NONE ) {
            changeMenuForFocus( bGlobals, nxt );
        }
    }

    if ( draw ) {
        board_draw( cGlobals->game.board, NULL_XWE );
    }
    return XP_TRUE;
} /* handleFocusKey */

#ifdef ALT_KEYS_WORKING
static bool
handleAltLeft( void* closure, int XP_UNUSED(key) )
{
    CursesBoardGlobals* bGlobals = (CursesBoardGlobals*)closure;
    return handleFocusKey( bGlobals, XP_CURSOR_KEY_ALTLEFT );
}

static bool
handleAltRight( void* closure, int XP_UNUSED(key) )
{
    CursesBoardGlobals* bGlobals = (CursesBoardGlobals*)closure;
    return handleFocusKey( bGlobals, XP_CURSOR_KEY_ALTRIGHT );
}

static bool
handleAltUp( void* closure, int XP_UNUSED(key) )
{
    CursesBoardGlobals* bGlobals = (CursesBoardGlobals*)closure;
    return handleFocusKey( bGlobals, XP_CURSOR_KEY_ALTUP );
}

static bool
handleAltDown( void* closure, int XP_UNUSED(key) )
{
    CursesBoardGlobals* bGlobals = (CursesBoardGlobals*)closure;
    return handleFocusKey( bGlobals, XP_CURSOR_KEY_ALTDOWN );
}
#endif

static bool
handleHint( void* closure, int XP_UNUSED(key) )
{
    CursesBoardGlobals* bGlobals = (CursesBoardGlobals*)closure;
    CommonGlobals* cGlobals = &bGlobals->cGlobals;
    XP_Bool redo;
    XP_Bool draw = board_requestHint( cGlobals->game.board, NULL_XWE,
 #ifdef XWFEATURE_SEARCHLIMIT
                                      XP_FALSE,
 #endif
                                      XP_FALSE, &redo );
    if ( draw ) {
        board_draw( cGlobals->game.board, NULL_XWE );
    }
    return XP_TRUE;
}

static bool
handleLeft( void* closure, int XP_UNUSED(key) )
{
    CursesBoardGlobals* bGlobals = (CursesBoardGlobals*)closure;
    return handleFocusKey( bGlobals, XP_CURSOR_KEY_LEFT );
} /* handleLeft */

static bool
handleRight( void* closure, int XP_UNUSED(key) )
{
    CursesBoardGlobals* bGlobals = (CursesBoardGlobals*)closure;
    return handleFocusKey( bGlobals, XP_CURSOR_KEY_RIGHT );
} /* handleRight */

static bool
handleUp( void* closure, int XP_UNUSED(key) )
{
    CursesBoardGlobals* bGlobals = (CursesBoardGlobals*)closure;
    return handleFocusKey( bGlobals, XP_CURSOR_KEY_UP );
} /* handleUp */

static bool
handleDown( void* closure, int XP_UNUSED(key) )
{
    CursesBoardGlobals* bGlobals = (CursesBoardGlobals*)closure;
    return handleFocusKey( bGlobals, XP_CURSOR_KEY_DOWN );
} /* handleDown */

static gboolean
idle_close_game( gpointer data )
{
    CursesBoardGlobals* bGlobals = (CursesBoardGlobals*)data;
    linuxSaveGame( &bGlobals->cGlobals );
    unref( bGlobals );
    return FALSE;
}

static bool
handleClose( void* closure, int XP_UNUSED(key) )
{
    CursesBoardGlobals* bGlobals = (CursesBoardGlobals*)closure;
    (void)ADD_ONETIME_IDLE( idle_close_game, bGlobals );
    return XP_TRUE;
} /* handleDown */

static bool
handleSpace( void* closure, int XP_UNUSED(key) )
{
    CursesBoardGlobals* bGlobals = (CursesBoardGlobals*)closure;
    BoardCtxt* board = bGlobals->cGlobals.game.board;
    checkAssignFocus( board );
    XP_Bool handled;
    (void)board_handleKey( board, NULL_XWE, XP_RAISEFOCUS_KEY, &handled );
    board_draw( board, NULL_XWE );
    return XP_TRUE;
} /* handleSpace */

static bool
handleRet( void* closure, int key )
{
    CursesBoardGlobals* bGlobals = (CursesBoardGlobals*)closure;
    BoardCtxt* board = bGlobals->cGlobals.game.board;
    XP_Bool handled;
    XP_Key xpKey = (key & ALT_BIT) == 0 ? XP_RETURN_KEY : XP_ALTRETURN_KEY;
    if ( board_handleKey( board, NULL_XWE, xpKey, &handled ) ) {
        board_draw( board, NULL_XWE );
    }
    return XP_TRUE;
} /* handleRet */

static bool
handleShowVals( void* closure, int XP_UNUSED(key) )
{
    CursesBoardGlobals* bGlobals = (CursesBoardGlobals*)closure;
    CommonGlobals* cGlobals = &bGlobals->cGlobals;

    XWStreamCtxt* stream = mem_stream_make_raw( MPPARM(cGlobals->util->mpool)
                                                cGlobals->params->vtMgr );
    server_formatDictCounts( bGlobals->cGlobals.game.server, NULL_XWE,
                             stream, 5, XP_FALSE );
    const XP_U8* data = stream_getPtr( stream );
    XP_U16 len = stream_getSize( stream );
    XP_UCHAR buf[len + 1];
    XP_MEMCPY( buf, data, len );
    buf[len] = '\0';

    (void)ca_inform( bGlobals->boardWin, buf );
    stream_destroy( stream );

    return XP_TRUE;
}
#endif

#ifdef XWFEATURE_RELAY
static void
relay_connd_curses( XWEnv XP_UNUSED(xwe), void* XP_UNUSED(closure), XP_UCHAR* const XP_UNUSED(room),
                    XP_Bool XP_UNUSED(reconnect), XP_U16 XP_UNUSED(devOrder),
                    XP_Bool XP_UNUSED_DBG(allHere),
                    XP_U16 XP_UNUSED_DBG(nMissing) )
{
    XP_LOGFF( "got allHere: %s; nMissing: %d", allHere?"true":"false", nMissing );
}

static void
relay_error_curses( XWEnv XP_UNUSED(xwe), void* XP_UNUSED(closure),
                    XWREASON XP_UNUSED_DBG(relayErr) )
{
    XP_LOGFF( "(%s)", XWREASON2Str( relayErr ) );
}

static XP_Bool
relay_sendNoConn_curses( XWEnv XP_UNUSED(xwe), const XP_U8* msg, XP_U16 len,
                         const XP_UCHAR* XP_UNUSED(msgNo),
                         const XP_UCHAR* relayID, void* closure )
{
    XP_Bool success = XP_FALSE;
    CommonGlobals* cGlobals = (CommonGlobals*)closure;
    LaunchParams* params = cGlobals->params;
    if ( params->useUdp /*&& !cGlobals->draw*/ ) {
        XP_U16 seed = comms_getChannelSeed( cGlobals->game.comms );
        XP_U32 clientToken = makeClientToken( cGlobals->rowid, seed );
        XP_S16 nSent = relaycon_sendnoconn( params, msg, len, relayID, 
                                            clientToken );
        success = nSent == len;
    }
    return success;
} /* relay_sendNoConn_curses */
#endif

#ifdef RELAY_VIA_HTTP
static void
relay_requestJoin_curses( void* closure, const XP_UCHAR* devID, const XP_UCHAR* room,
                          XP_U16 nPlayersHere, XP_U16 nPlayersTotal,
                          XP_U16 seed, XP_U16 lang )
{
    CommonGlobals* cGlobals = (CommonGlobals*)closure;
    relaycon_join( cGlobals->params, devID, room, nPlayersHere, nPlayersTotal,
                   seed, lang, onJoined, globals );
}
#endif

static void
curses_countChanged( XWEnv XP_UNUSED(xwe), void* XP_UNUSED(closure),
                     XP_U16 XP_UNUSED_DBG(newCount),
                     XP_Bool XP_UNUSED_DBG(quashed) )
{
    /* discon_ok2.py depends on this log entry */
    XP_LOGFF( "(newCount=%d, quashed=%s)", newCount, boolToStr(quashed) );
}

#ifdef COMMS_XPORT_FLAGSPROC
static XP_U32
curses_getFlags( XWEnv XP_UNUSED(xwe), void* XP_UNUSED(closure) )
{
    return COMMS_XPORT_FLAGS_HASNOCONN;
}
#endif

#ifdef XWFEATURE_RELAY
static void
relay_status_curses( XWEnv XP_UNUSED(xwe), void* XP_UNUSED(closure),
                     CommsRelayState XP_UNUSED_DBG(state) )
{
    /* CommonGlobals* cGlobals = (CommonGlobals*)closure; */
    // bGlobals->commsRelayState = state;
    XP_LOGFF( "got status: %s", CommsRelayState2Str(state) );
}
#endif
