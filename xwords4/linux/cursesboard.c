/* -*- compile-command: "make MEMDEBUG=TRUE -j3"; -*- */
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
#include "cursesask.h"
#include "cursesmenu.h"
#include "cursesletterask.h"
#include "linuxdict.h"
#include "gamesdb.h"
#include "game.h"
#include "gsrcwrap.h"

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

    TransportProcs procs;

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
                                       sqlite3_int64 rowid );
static void enableDraw( CursesBoardGlobals* bGlobals,
                        int width, int top, int height );
static CursesBoardGlobals* ref( CursesBoardGlobals* bGlobals );
static void unref( CursesBoardGlobals* bGlobals );
static void setupBoard( CursesBoardGlobals* bGlobals );

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
cb_open( CursesBoardState* cbState, sqlite3_int64 rowid,
         int width, int top, int height )
{
    LOG_FUNC();
    CursesBoardGlobals* bGlobals = findOrOpen( cbState, rowid );
    enableDraw( bGlobals, width, top, height );
    setupBoard( bGlobals );

    CommonGlobals* cGlobals = &bGlobals->cGlobals;
    if ( !!cGlobals->game.comms ) {
        comms_resendAll( cGlobals->game.comms, COMMS_CONN_NONE, XP_FALSE );
    }
}

void
cb_new( CursesBoardState* cbState, int width, int top, int height )
{
    CursesBoardGlobals* bGlobals = findOrOpen( cbState, -1 );
    enableDraw( bGlobals, width, top, height );
    setupBoard( bGlobals );
}

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

static void relay_connd_curses( void* closure, XP_UCHAR* const room,
                                XP_Bool reconnect, XP_U16 devOrder,
                                XP_Bool allHere, XP_U16 nMissing );
static void relay_status_curses( void* closure, CommsRelayState state );
static void relay_error_curses( void* closure, XWREASON relayErr );
static XP_Bool relay_sendNoConn_curses( const XP_U8* msg, XP_U16 len,
                                        const XP_UCHAR* msgNo,
                                        const XP_UCHAR* relayID, void* closure );
static void curses_countChanged( void* closure, XP_U16 newCount );
static XP_U32 curses_getFlags( void* closure );
#ifdef RELAY_VIA_HTTP
static void relay_requestJoin_curses( void* closure, const XP_UCHAR* devID,
                                      const XP_UCHAR* room, XP_U16 nPlayersHere,
                                      XP_U16 nPlayersTotal, XP_U16 seed, XP_U16 lang );
#endif

const MenuList g_allMenuList[] = {
    { handleLeft, "Left", "H", 'H' },
    { handleRight, "Right", "L", 'L' },
    { handleUp, "Up", "J", 'J' },
    { handleDown, "Down", "K", 'K' },
    { handleClose, "Close", "W", 'W' },
    { handleSpace, "Raise focus", "<spc>", ' ' },
    { handleRet, "Click/tap", "<ret>", '\r' },
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

static void initMenus( CursesBoardGlobals* bGlobals )
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
    board_draw( board );
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
        XP_LOGF( "%s: removal of listener not implemented!!!!!", __func__ );
    } else {
        CursesBoardGlobals* globals = (CursesBoardGlobals*)cGlobals;
        XP_ASSERT( !cGlobals->acceptor || (func == cGlobals->acceptor) );
        cGlobals->acceptor = func;
        globals->csInfo.server.serverSocket = listener;
        ADD_SOCKET( globals, listener, fire_acceptor );
    }
}

static void
copyParmsAddr( CommonGlobals* cGlobals )
{
    LaunchParams* params = cGlobals->params;
    CommsAddrRec* addr = &cGlobals->addr;

    CommsConnType typ;
    for ( XP_U32 st = 0; addr_iter( &params->addr, &typ, &st ); ) {
        addr_addType( addr, typ );
        switch( typ ) {
#ifdef XWFEATURE_RELAY
        case COMMS_CONN_RELAY:
            addr->u.ip_relay.ipAddr = 0;       /* ??? */
            addr->u.ip_relay.port = params->connInfo.relay.defaultSendPort;
            addr->u.ip_relay.seeksPublicRoom =
                params->connInfo.relay.seeksPublicRoom;
            addr->u.ip_relay.advertiseRoom = params->connInfo.relay.advertiseRoom;
            XP_STRNCPY( addr->u.ip_relay.hostName,
                        params->connInfo.relay.relayName,
                        sizeof(addr->u.ip_relay.hostName) - 1 );
            XP_STRNCPY( addr->u.ip_relay.invite, params->connInfo.relay.invite,
                        sizeof(addr->u.ip_relay.invite) - 1 );
            break;
#endif
#ifdef XWFEATURE_SMS
        case COMMS_CONN_SMS:
            XP_STRNCPY( addr->u.sms.phone, params->connInfo.sms.myPhone,
                        sizeof(addr->u.sms.phone) - 1 );
            addr->u.sms.port = params->connInfo.sms.port;
            break;
#endif
#ifdef XWFEATURE_BLUETOOTH
        case COMMS_CONN_BT:
            XP_ASSERT( sizeof(addr->u.bt.btAddr)
                       >= sizeof(params->connInfo.bt.hostAddr));
            XP_MEMCPY( &addr->u.bt.btAddr, &params->connInfo.bt.hostAddr,
                       sizeof(params->connInfo.bt.hostAddr) );
            break;
#endif
        default:
            break;
        }
    }
}

static CursesBoardGlobals*
commonInit( CursesBoardState* cbState, sqlite3_int64 rowid )
{
    CursesBoardGlobals* bGlobals = g_malloc0( sizeof(*bGlobals) );
    XP_LOGF( "%s(): alloc'd bGlobals %p", __func__, bGlobals );
    CommonGlobals* cGlobals = &bGlobals->cGlobals;
    LaunchParams* params = cbState->params;

    cGlobals->gi = &cGlobals->_gi;
    gi_copy( MPPARM(params->mpool) cGlobals->gi, &params->pgi );

    cGlobals->rowid = rowid;
    bGlobals->cbState = cbState;
    cGlobals->params = params;
    setupUtil( cGlobals );
    setupCursesUtilCallbacks( bGlobals, cGlobals->util );

    cGlobals->socketAddedClosure = bGlobals;
    cGlobals->onSave = onGameSaved;
    cGlobals->onSaveClosure = bGlobals;
    cGlobals->addAcceptor = curses_socket_acceptor;

    bGlobals->procs.closure = cGlobals;
    bGlobals->procs.send = LINUX_SEND;
#ifdef COMMS_HEARTBEAT
    bGlobals->procs.reset = linux_reset;
#endif
    bGlobals->procs.rstatus = relay_status_curses;
    bGlobals->procs.rconnd = relay_connd_curses;
    bGlobals->procs.rerror = relay_error_curses;
    bGlobals->procs.sendNoConn = relay_sendNoConn_curses;
    bGlobals->procs.countChanged = curses_countChanged;
    bGlobals->procs.getFlags = curses_getFlags;
# ifdef RELAY_VIA_HTTP
    bGlobals->procs.requestJoin = relay_requestJoin_curses;
#endif

    copyParmsAddr( cGlobals );

    CurGameInfo* gi = cGlobals->gi;
    if ( !!gi ) {
        XP_ASSERT( !cGlobals->dict );
        cGlobals->dict = linux_dictionary_make( MPPARM(cGlobals->util->mpool) params,
                                                gi->dictName, XP_TRUE );
        gi->dictLang = dict_getLangCode( cGlobals->dict );
    }

    setOneSecondTimer( cGlobals );
    return bGlobals;
} /* commonInit */

static void
disposeBoard( CursesBoardGlobals* bGlobals )
{
    XP_LOGF( "%s(): passed bGlobals %p", __func__, bGlobals );
    /* XP_ASSERT( 0 == bGlobals->refCount ); */
    CommonGlobals* cGlobals = &bGlobals->cGlobals;
    if ( !!bGlobals->boardWin ) {
        cursesDrawCtxtFree( cGlobals->draw );
        wclear( bGlobals->boardWin );
        wrefresh( bGlobals->boardWin );
        delwin( bGlobals->boardWin );
    }

    if ( !!bGlobals->cbState->menuState ) {
        cmenu_pop( bGlobals->cbState->menuState );
    }

    clearOneSecondTimer( cGlobals );

    gi_disposePlayerInfo( MPPARM(cGlobals->util->mpool) cGlobals->gi );
    game_dispose( &cGlobals->game );

    if ( !!cGlobals->dict ) {
        dict_unref( cGlobals->dict );
    }

    disposeUtil( cGlobals );

    CursesBoardState* cbState = bGlobals->cbState;
    cbState->games = g_slist_remove( cbState->games, bGlobals );
    
    /* onCursesBoardClosing( bGlobals->aGlobals, bGlobals ); */
    g_free( bGlobals );
}

static CursesBoardGlobals*
ref( CursesBoardGlobals* bGlobals )
{
    ++bGlobals->refCount;
    XP_LOGF( "%s(): refCount now %d", __func__, bGlobals->refCount );
    return bGlobals;
}

static void
unref( CursesBoardGlobals* bGlobals )
{
    --bGlobals->refCount;
    XP_LOGF( "%s(): refCount now %d", __func__, bGlobals->refCount );
    if ( 0 == bGlobals->refCount ) {
        disposeBoard( bGlobals );
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
getFromDict( const CommonGlobals* cGlobals, XP_U16* fontWidthP,
             XP_U16* fontHtP )
{
    int maxLen = 0;

    DictionaryCtxt* dict = cGlobals->dict;
    for ( Tile tile = 0; tile < dict->nFaces; ++tile ) {
        const XP_UCHAR* face = dict_getTileString( dict, tile );
        int thisLen = utf8_len( face );
        /* XP_LOGF( "%s(): looking at face '%s' with len %d", __func__, */
        /*          face, thisLen ); */
        if ( thisLen > maxLen ) {
            maxLen = thisLen;
        }
    }

    /* XP_LOGF( "%s(): width = %d", __func__, maxLen ); */
    *fontWidthP = maxLen;
    *fontHtP = 1;
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
    getFromDict( cGlobals, &fontWidth, &fontHt );
    BoardDims dims;
    board_figureLayout( board, cGlobals->gi,
                        0, 0, width, height, 100,
                        150, 200, /* percents */
                        width*75/100,
                        fontWidth, fontHt,
                        XP_FALSE, &dims );
    board_applyLayout( board, &dims );
    XP_LOGF( "%s(): calling board_draw()", __func__ );
    board_draw( board );
}

static CursesBoardGlobals*
initNoDraw( CursesBoardState* cbState, sqlite3_int64 rowid )
{
    LOG_FUNC();
    CursesBoardGlobals* bGlobals = commonInit( cbState, rowid );
    CommonGlobals* cGlobals = &bGlobals->cGlobals;
    if ( -1 == rowid ) {
        cGlobals->rowid = -1;
    }
    linuxOpenGame( cGlobals, &bGlobals->procs );
    return ref( bGlobals );
}

static void
enableDraw( CursesBoardGlobals* bGlobals, int width, int top, int height )
{
    LOG_FUNC();
    XP_ASSERT( 0 != width );
    bGlobals->boardWin = newwin( height, width, top, 0 );
    getmaxyx( bGlobals->boardWin, bGlobals->winHeight, bGlobals->winWidth );

    CommonGlobals* cGlobals = &bGlobals->cGlobals;
    cGlobals->draw = cursesDrawCtxtMake( bGlobals->boardWin );
    board_setDraw( cGlobals->game.board, cGlobals->draw );

    setupBoard( bGlobals );

    initMenus( bGlobals );
}

static CursesBoardGlobals*
findOrOpen( CursesBoardState* cbState, sqlite3_int64 rowid )
{
    CursesBoardGlobals* result = NULL;
    for ( GSList* iter = cbState->games;
          rowid != 0 && !!iter && !result; iter = iter->next ) {
        CursesBoardGlobals* one = (CursesBoardGlobals*)iter->data;
        if ( one->cGlobals.rowid == rowid ) {
            result = one;
        }
    }

    if ( !result ) {
        result = initNoDraw( cbState, rowid );
        setupBoard( result );
        cbState->games = g_slist_append( cbState->games, result );
    }
    return result;
}

XP_U16
cb_feedBuffer( CursesBoardState* cbState, sqlite3_int64 rowid,
               const XP_U8* buf, XP_U16 len, const CommsAddrRec* from )
{
    CursesBoardGlobals* bGlobals = findOrOpen( cbState, rowid );
    CommonGlobals* cGlobals = &bGlobals->cGlobals;
    gameGotBuf( cGlobals, XP_TRUE, buf, len, from );
    XP_U16 seed = comms_getChannelSeed( cGlobals->game.comms );
    return seed;
}

static void
kill_board( gpointer data )
{
    CursesBoardGlobals* bGlobals = (CursesBoardGlobals*)data;
    disposeBoard( bGlobals );
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

    const char* buttons[] = {"OK"};
    (void)cursesask( bGlobals->boardWin, buf, VSIZE(buttons), buttons );

    va_end(ap);
} /* cursesUserError */

static void
curses_util_notifyPickTileBlank( XW_UtilCtxt* uc, XP_U16 playerNum,
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
                                 XP_Bool XP_UNUSED(isInitial),
                                 XP_U16 XP_UNUSED(player),
                                 XP_U16 XP_UNUSED(nToPick),
                                 XP_U16 XP_UNUSED(nFaces),
                                 const XP_UCHAR** XP_UNUSED(faces),
                                 const XP_U16* XP_UNUSED(counts) )
{
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
curses_util_userError( XW_UtilCtxt* uc, UtilErrID id )
{
    CursesBoardGlobals* globals = (CursesBoardGlobals*)uc->closure;
    XP_Bool silent;
    const XP_UCHAR* message = linux_getErrString( id, &silent );

    if ( silent ) {
        XP_LOGF( "silent userError: %s", message );
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
        if ( board_commitTurn( board, XP_TRUE, XP_TRUE, NULL ) ) {
            board_draw( board );
            linuxSaveGame( &bGlobals->cGlobals );
        }
    }

    return FALSE;
}

static void
curses_util_informNeedPassword( XW_UtilCtxt* XP_UNUSED(uc),
                                XP_U16 XP_UNUSED_DBG(playerNum),
                                const XP_UCHAR* XP_UNUSED_DBG(name) )
{
    XP_WARNF( "curses_util_informNeedPassword(num=%d, name=%s", playerNum, name );
} /* curses_util_askPassword */

static void
curses_util_yOffsetChange( XW_UtilCtxt* XP_UNUSED(uc), 
                           XP_U16 XP_UNUSED(maxOffset),
                           XP_U16 XP_UNUSED(oldOffset), XP_U16 XP_UNUSED(newOffset) )
{
    /* if ( oldOffset != newOffset ) { */
    /*     XP_WARNF( "curses_util_yOffsetChange(%d,%d,%d) not implemented", */
    /*               maxOffset, oldOffset, newOffset ); */
    /* } */
} /* curses_util_yOffsetChange */

#ifdef XWFEATURE_TURNCHANGENOTIFY
static void
curses_util_turnChanged( XW_UtilCtxt* XP_UNUSED(uc), XP_S16 XP_UNUSED_DBG(newTurn) )
{
    XP_LOGF( "%s(turn=%d)", __func__, newTurn );
}
#endif

static void
curses_util_notifyIllegalWords( XW_UtilCtxt* XP_UNUSED(uc),
                                BadWordInfo* XP_UNUSED(bwi),
                                XP_U16 XP_UNUSED(player),
                                XP_Bool XP_UNUSED(turnLost) )
{
    XP_WARNF( "curses_util_notifyIllegalWords not implemented" );
} /* curses_util_notifyIllegalWord */

/* this needs to change!!! */
static void
curses_util_notifyMove( XW_UtilCtxt* uc, XWStreamCtxt* stream )
{
    CursesBoardGlobals* globals = (CursesBoardGlobals*)uc->closure;
    CommonGlobals* cGlobals = &globals->cGlobals;
    XP_U16 len = stream_getSize( stream );
    XP_ASSERT( len <= VSIZE(cGlobals->question) );
    stream_getBytes( stream, cGlobals->question, len );
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
        if ( board_commitTurn( board, XP_TRUE, XP_TRUE, NULL ) ) {
            board_draw( board );
            linuxSaveGame( cGlobals );
        }
    }
    return FALSE;
}

static void
curses_util_notifyTrade( XW_UtilCtxt* uc, const XP_UCHAR** tiles, XP_U16 nTiles )
{
    CursesBoardGlobals* globals = (CursesBoardGlobals*)uc->closure;
    formatConfirmTrade( &globals->cGlobals, tiles, nTiles );
    (void)ADD_ONETIME_IDLE( ask_trade, globals );
}

static void
curses_util_trayHiddenChange( XW_UtilCtxt* XP_UNUSED(uc), 
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
    server_writeFinalScores( cGlobals->game.server, stream );

    text = strFromStream( stream );

    const char* buttons[] = { "Ok" };
    (void)cursesask( bGlobals->boardWin, text, VSIZE(buttons), buttons );

    free( text );
    stream_destroy( stream );
} /* cursesShowFinalScores */

static void
curses_util_informMove( XW_UtilCtxt* uc, XP_S16 XP_UNUSED(turn),
                        XWStreamCtxt* expl, XWStreamCtxt* XP_UNUSED(words))
{
    CursesBoardGlobals* bGlobals = (CursesBoardGlobals*)uc->closure;
    char* question = strFromStream( expl );
    const char* buttons[] = { "Ok" };
    (void)cursesask( bGlobals->boardWin, question, VSIZE(buttons), buttons );
    free( question );
}

static void
curses_util_informUndo( XW_UtilCtxt* XP_UNUSED(uc))
{
    LOG_FUNC();
}

static void
curses_util_notifyGameOver( XW_UtilCtxt* uc, XP_S16 quitter )
{
    CursesBoardGlobals* bGlobals = (CursesBoardGlobals*)uc->closure;
    CommonGlobals* cGlobals = &bGlobals->cGlobals;
    LaunchParams* params = cGlobals->params;
    board_draw( cGlobals->game.board );

    /* game belongs in cGlobals... */
    if ( params->printHistory ) {
        catGameHistory( cGlobals );
    }

    catFinalScores( cGlobals, quitter );

    if ( params->quitAfter >= 0 ) {
        sleep( params->quitAfter );
        handleQuit( bGlobals->cbState->aGlobals, 0 );
    } else if ( params->undoWhenDone ) {
        server_handleUndo( cGlobals->game.server, 0 );
    } else if ( !params->skipGameOver && !!bGlobals->boardWin ) {
        /* This is modal.  Don't show if quitting */
        cursesShowFinalScores( bGlobals );
    }
} /* curses_util_notifyGameOver */

static void
curses_util_informNetDict( XW_UtilCtxt* uc, XP_LangCode XP_UNUSED(lang),
                           const XP_UCHAR* XP_UNUSED_DBG(oldName),
                           const XP_UCHAR* XP_UNUSED_DBG(newName), 
                           const XP_UCHAR* XP_UNUSED_DBG(newSum),
                           XWPhoniesChoice phoniesAction )
{
    XP_USE(uc);
    XP_USE(phoniesAction);
    XP_LOGF( "%s: %s => %s (cksum: %s)", __func__, oldName, newName, newSum );
}

#ifdef XWFEATURE_SEARCHLIMIT
static XP_Bool
curses_util_getTraySearchLimits( XW_UtilCtxt* XP_UNUSED(uc),
                                 XP_U16* XP_UNUSED(min), XP_U16* XP_UNUSED(max) )
{
    XP_ASSERT(0);
    return XP_TRUE;
}
#endif


#ifdef XWFEATURE_HILITECELL
static XP_Bool
curses_util_hiliteCell( XW_UtilCtxt* uc, 
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
curses_util_engineProgressCallback( XW_UtilCtxt* XP_UNUSED(uc) )
{
    return XP_TRUE;
} /* curses_util_engineProgressCallback */

#ifdef USE_GLIBLOOP
static gboolean
timerFired( gpointer data )
{
    TimerInfo* ti = (TimerInfo*)data;
    CommonGlobals* globals = ti->globals;
    XWTimerReason why = ti - globals->timerInfo;
    if ( linuxFireTimer( globals, why ) ) {
        board_draw( globals->game.board );
    }

    return FALSE;
}
#endif

static void
curses_util_setTimer( XW_UtilCtxt* uc, XWTimerReason why, XP_U16 when,
                      XWTimerProc proc, void* closure )
{
    CursesBoardGlobals* globals = (CursesBoardGlobals*)uc->closure;
    TimerInfo* ti = &globals->cGlobals.timerInfo[why];

    ti->proc = proc;
    ti->closure = closure;

#ifdef USE_GLIBLOOP
    ti->globals = &globals->cGlobals;
    (void)g_timeout_add_seconds( when, timerFired, ti );
#else
    ti->when = util_getCurSeconds(uc) + when;
#endif
} /* curses_util_setTimer */

static void
curses_util_clearTimer( XW_UtilCtxt* uc, XWTimerReason why )
{
    CursesBoardGlobals* globals = (CursesBoardGlobals*)uc->closure;
    globals->cGlobals.timerInfo[why].proc = NULL;
}


#ifdef USE_GLIBLOOP
static gboolean
onetime_idle( gpointer data )
{
    CursesBoardGlobals* bGlobals = (CursesBoardGlobals*)data;
    XP_LOGF( "%s(): passed bGlobals %p", __func__, bGlobals );
    XWGame* game = &bGlobals->cGlobals.game;
    if ( !!game->server && server_do( game->server ) ) {
        if ( !!game->board ) {
            board_draw( game->board );
        }
        linuxSaveGame( &bGlobals->cGlobals );
    }
    return FALSE;
}
#endif 

static void
curses_util_requestTime( XW_UtilCtxt* uc ) 
{
    CursesBoardGlobals* globals = (CursesBoardGlobals*)uc->closure;
#if 1
    (void)ADD_ONETIME_IDLE( onetime_idle, globals );
#else
    (void)g_timeout_add( 1,// interval,
                         onetime_idle, globals );
#endif
} /* curses_util_requestTime */

static XP_Bool
curses_util_altKeyDown( XW_UtilCtxt* XP_UNUSED(uc) )
{
    return XP_FALSE;
}

static void
curses_util_remSelected( XW_UtilCtxt* uc )
{
    CursesBoardGlobals* bGlobals = (CursesBoardGlobals*)uc->closure;
    CommonGlobals* cGlobals = (CommonGlobals*)uc->closure;
    XWStreamCtxt* stream;
    XP_UCHAR* text;

    stream = mem_stream_make_raw( MPPARM(cGlobals->util->mpool)
                                  cGlobals->params->vtMgr );
    board_formatRemainingTiles( cGlobals->game.board, stream );

    text = strFromStream( stream );

    const char* buttons[] = { "Ok" };
    (void)cursesask( bGlobals->boardWin, text, VSIZE(buttons), buttons );

    free( text );
}

static void
curses_util_bonusSquareHeld( XW_UtilCtxt* uc, XWBonusType bonus )
{
    LOG_FUNC();
    XP_USE( uc );
    XP_USE( bonus );
}

static void
curses_util_playerScoreHeld( XW_UtilCtxt* uc, XP_U16 player )
{
    CursesBoardGlobals* bGlobals = (CursesBoardGlobals*)uc->closure;
    LastMoveInfo lmi;
    if ( model_getPlayersLastScore( bGlobals->cGlobals.game.model,
                                    player, &lmi ) ) {
        XP_UCHAR buf[128];
        formatLMI( &lmi, buf, VSIZE(buf) );
        const char* buttons[] = { "Ok" };
        (void)cursesask( bGlobals->boardWin, buf, VSIZE(buttons), buttons );
    }
}

#ifdef XWFEATURE_BOARDWORDS
static void
curses_util_cellSquareHeld( XW_UtilCtxt* uc, XWStreamCtxt* words )
{
    XP_USE( uc );
    catOnClose( words, NULL );
    fprintf( stderr, "\n" );
}
#endif


#ifndef XWFEATURE_STANDALONE_ONLY
static XWStreamCtxt*
curses_util_makeStreamFromAddr(XW_UtilCtxt* uc, XP_PlayerAddr channelNo )
{
    CursesBoardGlobals* globals = (CursesBoardGlobals*)uc->closure;
    LaunchParams* params = globals->cGlobals.params;

    XWStreamCtxt* stream = mem_stream_make( MPPARM(uc->mpool) params->vtMgr,
                                            &globals->cGlobals, channelNo,
                                            sendOnClose );
    return stream;
} /* curses_util_makeStreamFromAddr */
#endif

#ifdef XWFEATURE_CHAT
static void
curses_util_showChat( XW_UtilCtxt* uc, 
                      const XP_UCHAR* const XP_UNUSED_DBG(msg),
                      XP_S16 XP_UNUSED_DBG(from), XP_U32 XP_UNUSED(timestamp) )
{
    CursesBoardGlobals* globals = (CursesBoardGlobals*)uc->closure;
    globals->nChatsSent = 0;
# ifdef DEBUG
    const XP_UCHAR* name = "<unknown>";
    if ( 0 <= from ) {
        CommonGlobals* cGlobals = &globals->cGlobals;
        name = cGlobals->gi->players[from].name;
    }
    XP_LOGF( "%s: got \"%s\" from %s", __func__, msg, name );
# endif
}
#endif

static void
setupCursesUtilCallbacks( CursesBoardGlobals* bGlobals, XW_UtilCtxt* util )
{
    util->closure = bGlobals;
#define SET_PROC(NAM) util->vtable->m_util_##NAM = curses_util_##NAM
    SET_PROC(makeStreamFromAddr);
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
    SET_PROC(informMove);
    SET_PROC(informUndo);
    SET_PROC(informNetDict);
    SET_PROC(notifyGameOver);
#ifdef XWFEATURE_HILITECELL
    SET_PROC(hiliteCell);
#endif
    SET_PROC(engineProgressCallback);
    SET_PROC(setTimer);
    SET_PROC(clearTimer);
    SET_PROC(requestTime);
    SET_PROC(altKeyDown);       /* ?? */
    SET_PROC(notifyIllegalWords);
    SET_PROC(remSelected);
#ifndef XWFEATURE_MINIWIN
    SET_PROC(bonusSquareHeld);
    SET_PROC(playerScoreHeld);
#endif
#ifdef XWFEATURE_BOARDWORDS
    SET_PROC(cellSquareHeld);
#endif

#ifdef XWFEATURE_SEARCHLIMIT
    SET_PROC(getTraySearchLimits);
#endif
#ifdef XWFEATURE_CHAT
    SET_PROC(showChat);
#endif
#undef SET_PROC

    assertAllCallbacksSet( util );
} /* setupCursesUtilCallbacks */

static bool
handleFlip( void* closure, int XP_UNUSED(key) )
{
    CursesBoardGlobals* bGlobals = (CursesBoardGlobals*)closure;
    CommonGlobals* cGlobals = &bGlobals->cGlobals;
    if ( board_flip( cGlobals->game.board ) ) {
        board_draw( cGlobals->game.board );
    }

    return XP_TRUE;
} /* handleFlip */

static bool
handleToggleValues( void* closure, int XP_UNUSED(key) )
{
    CursesBoardGlobals* bGlobals = (CursesBoardGlobals*)closure;
    CommonGlobals* cGlobals = &bGlobals->cGlobals;
    if ( board_toggle_showValues( cGlobals->game.board ) ) {
        board_draw( cGlobals->game.board );
    }
    return XP_TRUE;
} /* handleToggleValues */

static bool
handleBackspace( void* closure, int XP_UNUSED(key) )
{
    CommonGlobals* cGlobals = &((CursesBoardGlobals*)closure)->cGlobals;
    XP_Bool handled;
    if ( board_handleKey( cGlobals->game.board,
                          XP_CURSOR_KEY_DEL, &handled ) ) {
        board_draw( cGlobals->game.board );
    }
    return XP_TRUE;
} /* handleBackspace */

static bool
handleUndo( void* closure, int XP_UNUSED(key) )
{
    CursesBoardGlobals* bGlobals = (CursesBoardGlobals*)closure;
    CommonGlobals* cGlobals = &bGlobals->cGlobals;
    if ( server_handleUndo( cGlobals->game.server, 0 ) ) {
        board_draw( cGlobals->game.board );
    }
    return XP_TRUE;
} /* handleUndo */

static bool
handleReplace( void* closure, int XP_UNUSED(key) )
{
    CommonGlobals* cGlobals = &((CursesBoardGlobals*)closure)->cGlobals;
    if ( board_replaceTiles( cGlobals->game.board ) ) {
        board_draw( cGlobals->game.board );
    }
    return XP_TRUE;
} /* handleReplace */

static bool
handleCommit( void* closure, int XP_UNUSED(key) )
{
    CursesBoardGlobals* bGlobals = (CursesBoardGlobals*)closure;
    CommonGlobals* cGlobals = &bGlobals->cGlobals;
    if ( board_commitTurn( cGlobals->game.board, XP_FALSE, XP_FALSE, NULL ) ) {
        board_draw( cGlobals->game.board );
    }
    return XP_TRUE;
} /* handleCommit */

static bool
handleJuggle( void* closure, int XP_UNUSED(key) )
{
    CommonGlobals* cGlobals = &((CursesBoardGlobals*)closure)->cGlobals;
    if ( board_juggleTray( cGlobals->game.board ) ) {
        board_draw( cGlobals->game.board );
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
        ? board_hideTray( cGlobals->game.board )
        : board_showTray( cGlobals->game.board );
    if ( draw ) {
        board_draw( cGlobals->game.board );
    }

    return XP_TRUE;
} /* handleJuggle */

#ifdef KEYBOARD_NAV
static void
checkAssignFocus( BoardCtxt* board )
{
    if ( OBJ_NONE == board_getFocusOwner(board) ) {
        board_focusChanged( board, OBJ_BOARD, XP_TRUE );
    }
}

static XP_Bool
handleFocusKey( CursesBoardGlobals* bGlobals, XP_Key key )
{
    CommonGlobals* cGlobals = &bGlobals->cGlobals;
    checkAssignFocus( cGlobals->game.board );

    XP_Bool handled;
    XP_Bool draw = board_handleKey( cGlobals->game.board, key, &handled );
    if ( !handled ) {
        BoardObjectType nxt;
        BoardObjectType order[] = { OBJ_BOARD, OBJ_SCORE, OBJ_TRAY };
        draw = linShiftFocus( cGlobals, key, order, &nxt ) || draw;
        if ( nxt != OBJ_NONE ) {
            changeMenuForFocus( bGlobals, nxt );
        }
    }

    if ( draw ) {
        board_draw( cGlobals->game.board );
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
    XP_Bool draw = board_requestHint( cGlobals->game.board, 
 #ifdef XWFEATURE_SEARCHLIMIT
                                      XP_FALSE,
 #endif
                                      XP_FALSE, &redo );
    if ( draw ) {
        board_draw( cGlobals->game.board );
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
    (void)board_handleKey( board, XP_RAISEFOCUS_KEY, &handled );
    board_draw( board );
    return XP_TRUE;
} /* handleSpace */

static bool
handleRet( void* closure, int XP_UNUSED(key) )
{
    CursesBoardGlobals* bGlobals = (CursesBoardGlobals*)closure;
    BoardCtxt* board = bGlobals->cGlobals.game.board;
    XP_Bool handled;
    (void)board_handleKey( board, XP_RETURN_KEY, &handled );
    board_draw( board );
    return XP_TRUE;
} /* handleRet */

#endif

static void
relay_connd_curses( void* XP_UNUSED(closure), XP_UCHAR* const XP_UNUSED(room),
                    XP_Bool XP_UNUSED(reconnect), XP_U16 XP_UNUSED(devOrder),
                    XP_Bool XP_UNUSED_DBG(allHere),
                    XP_U16 XP_UNUSED_DBG(nMissing) )
{
    XP_LOGF( "%s got allHere: %s; nMissing: %d", __func__,
             allHere?"true":"false", nMissing );
}

static void
relay_error_curses( void* XP_UNUSED(closure), XWREASON XP_UNUSED_DBG(relayErr) )
{
#ifdef DEBUG
    XP_LOGF( "%s(%s)", __func__, XWREASON2Str( relayErr ) );
#endif
}

static XP_Bool
relay_sendNoConn_curses( const XP_U8* msg, XP_U16 len,
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
curses_countChanged( void* XP_UNUSED(closure), XP_U16 XP_UNUSED_DBG(newCount) )
{
    XP_LOGF( "%s(newCount=%d)", __func__, newCount );
}

#ifdef COMMS_XPORT_FLAGSPROC
static XP_U32
curses_getFlags( void* XP_UNUSED(closure) )
{
    return COMMS_XPORT_FLAGS_HASNOCONN;
}
#endif

static void
relay_status_curses( void* XP_UNUSED(closure), CommsRelayState XP_UNUSED_DBG(state) )
{
    /* CommonGlobals* cGlobals = (CommonGlobals*)closure; */
    // bGlobals->commsRelayState = state;
    XP_LOGF( "%s got status: %s", __func__, CommsRelayState2Str(state) );
}
