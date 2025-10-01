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
#include "gamemgr.h"
#include "curseschat.h"

typedef struct CursesBoardState {
    LaunchParams* params;
    CursesMenuState* menuState;
    OnGameSaved onGameSaved;
} CursesBoardState;

struct CursesBoardGlobals {
    CommonGlobals cGlobals;
    CursesBoardState* cbState;
    CursesMenuState* menuState; /* null if we're not using menus */

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
                                       GameRef gr,
                                       const CommsAddrRec* addrP );
static void enableDraw( CursesBoardGlobals* bGlobals, const cb_dims* dims );
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
static bool openChat( void* closure, int key );

static XP_Bool rematch_and_save( CursesBoardGlobals* bGlobals, RematchOrder ro,
                                 XP_U32* newGameIDP );
static void disposeBoard( CursesBoardGlobals* bGlobals );
static void initCP( CommonGlobals* cGlobals );
static CursesBoardGlobals* commonInit( CursesBoardState* cbState, GameRef gr );

CursesBoardState*
cb_init( LaunchParams* params, CursesMenuState* menuState,
         OnGameSaved onGameSaved )
{
    CursesBoardState* result = g_malloc0( sizeof(*result) );
    result->params = params;
    result->menuState = menuState;
    result->onGameSaved = onGameSaved;
    return result;
}

void
cb_resized( CursesBoardState* cbState, const cb_dims* dims )
{
    CommonAppGlobals* cag = cbState->params->cag;
    for ( GSList* iter = cag->globalsList; !!iter; iter = iter->next ) {
        CursesBoardGlobals* one = (CursesBoardGlobals*)iter->data;
        disposeDraw( one );
        enableDraw( one, dims );
    }
}

/* static gint */
/* inviteIdle( gpointer data ) */
/* { */
/*     CursesBoardGlobals* bGlobals = (CursesBoardGlobals*)data; */
/*     LaunchParams* params = bGlobals->cGlobals.params; */
/*     if ( !!params->connInfo.sms.inviteePhones */
/* #ifdef XWFEATURE_RELAY */
/*          || !!params->connInfo.relay.inviteeRelayIDs */
/* #endif */
/*          || !!params->connInfo.mqtt.inviteeDevIDs ) { */
/*         sendInvite( bGlobals, 0 ); */
/*     } */
/*     return FALSE; */
/* } */

void
cb_open( CursesBoardState* cbState, GameRef gr, const cb_dims* dims )
{
    LOG_FUNC();
    CursesBoardGlobals* bGlobals = findOrOpen( cbState, gr, NULL );
    initMenus( bGlobals );
    enableDraw( bGlobals, dims );
    setupBoard( bGlobals );

    CommonGlobals* cGlobals = &bGlobals->cGlobals;
    LaunchParams* params = cGlobals->params;
    /* if ( gr_haveComms(params->dutil, cGlobals->gameRef, NULL_XWE ) ) { */
    /*     gr_resendAll( params->dutil, cGlobals->gameRef, NULL_XWE, */
    /*                   COMMS_CONN_NONE, XP_FALSE ); */
    /* } */
    /* if ( params->forceInvite ) { */
    /*     (void)ADD_ONETIME_IDLE( inviteIdle, bGlobals ); */
    /* } */
    DrawCtx* draw = cGlobals->draw;
    XP_ASSERT( !!draw );
    XP_ASSERT( !cGlobals->util );

    XW_DUtilCtxt* dutil = params->dutil;
    const CurGameInfo* gi = gr_getGI( dutil, cGlobals->gr, NULL_XWE );
    cGlobals->util = linux_util_make( dutil, gi, cGlobals->gr );
    setupLinuxUtilCallbacks( cGlobals->util, XP_TRUE );

    gr_setDraw( dutil, cGlobals->gr, NULL_XWE, draw,
                cGlobals->util );
}

bool
cb_newGame( CursesBoardState* cbState, const CurGameInfo* gi,
            XP_U32* newGameIDP )
{
    LOG_FUNC();
    XW_DUtilCtxt* dutil = cbState->params->dutil;
    GameRef gr = gmgr_newFor( dutil, NULL_XWE, GROUP_DEFAULT, gi, NULL );
    bool success = !!gr;
    if ( success && !!newGameIDP ) {
        const CurGameInfo* gi = gr_getGI( dutil, gr, NULL_XWE );
        *newGameIDP = gi->gameID;
    }
    return success;
}

/* Close the board, but don't dispose of its globals, which belong to the util
   instance that belongs to common/gameref */
static void
closeBoard( CursesBoardGlobals* bGlobals, XP_Bool rmFromList )
{
    CommonGlobals* cGlobals = &bGlobals->cGlobals;
    LaunchParams* params = cGlobals->params;
    gr_setDraw( params->dutil, cGlobals->gr, NULL_XWE, NULL, NULL );
    disposeDraw( bGlobals );

    CursesBoardState* cbState = bGlobals->cbState;
    if ( !!cbState && !!cbState->menuState ) {
        cmenu_pop( cbState->menuState );
    }

    if ( rmFromList && !!cbState ) {
        CommonAppGlobals* cag = cbState->params->cag;
        forgetGameGlobals( cag, &bGlobals->cGlobals );
    }

    util_unref( cGlobals->util, NULL_XWE );
}

void
cb_newFor( CursesBoardState* cbState, const NetLaunchInfo* nli,
           const cb_dims* XP_UNUSED(dims) )
{
    LaunchParams* params = cbState->params;

    GameRef gr = gmgr_addForInvite( params->dutil, NULL_XWE,
                                    GROUP_DEFAULT, nli );
    CursesBoardGlobals* bGlobals = commonInit( cbState, gr );
    CommonGlobals* cGlobals = &bGlobals->cGlobals;
    initCP( cGlobals );

    linuxSaveGame( cGlobals );

    // disposeBoard( bGlobals, XP_TRUE );
    // cg_unref( cGlobals );
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
    { openChat, "chAt", "A", 'A' },

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

static void
initMenus( CursesBoardGlobals* bGlobals )
{
    if ( !bGlobals->menuState ) {
        bGlobals->menuState = bGlobals->cbState->menuState;
        cmenu_push( bGlobals->menuState, bGlobals, g_allMenuList, NULL );
    }
}

static void
onGameSaved( void* XP_UNUSED(closure), GameRef XP_UNUSED(gr), XP_Bool XP_UNUSED(firstTime) )
{
    XP_ASSERT(0);
    /* CursesBoardGlobals* bGlobals = (CursesBoardGlobals*)closure; */
    /* CommonGlobals* cGlobals = &bGlobals->cGlobals; */
    /* LaunchParams* params = cGlobals->params; */
    /* /\* onCursesGameSaved( bGlobals->aGlobals, rowid ); *\/ */

    /* // BoardCtxt* board = gr_getGame(cGlobals->gr)->board; */
    /* gr_invalAll( params->dutil, cGlobals->gr, NULL_XWE ); */
    /* gr_draw( params->dutil, cGlobals->gr, NULL_XWE ); */
    /* /\* May not be recorded *\/ */
    /* // XP_ASSERT( cGlobals->rowid == rowid ); */
    /* // cGlobals->rowid = rowid; */

    /* CursesBoardState* cbState = bGlobals->cbState; */
    /* (*cbState->onGameSaved)( cbState->aGlobals, rowid, firstTime ); */
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
freeCursesBoardGlobals( CommonGlobals* cGlobals )
{
    CursesBoardGlobals* bGlobals = (CursesBoardGlobals*)cGlobals;
    disposeBoard( bGlobals );
}

CommonGlobals*
allocCursesBoardGlobals()
{
    CursesBoardGlobals* bGlobals = g_malloc0( sizeof(*bGlobals) );
    CommonGlobals* cGlobals = &bGlobals->cGlobals;
    cg_init( cGlobals, freeCursesBoardGlobals );
    return cGlobals;
}

static CursesBoardGlobals*
commonInit( CursesBoardState* cbState, GameRef gr )
{
    LaunchParams* params = cbState->params;
    CommonAppGlobals* cag = params->cag;
    CommonGlobals* cGlobals = globalsForGameRef( cag, gr, XP_TRUE );
    CursesBoardGlobals* bGlobals = (CursesBoardGlobals*)cGlobals;

    XP_LOGFF( "found bGlobals %p", bGlobals );
    XP_ASSERT( !!bGlobals );

    XP_ASSERT( cGlobals->gr == gr );
    bGlobals->cbState = cbState;
    XP_ASSERT( cGlobals->params == params );

    cGlobals->gi = gr_getGI( params->dutil, gr, NULL_XWE );
    XP_ASSERT( !!cGlobals->gi );

    cGlobals->socketAddedClosure = bGlobals;
    cGlobals->onSave = onGameSaved;
    cGlobals->onSaveClosure = bGlobals;
    cGlobals->addAcceptor = curses_socket_acceptor;

    makeSelfAddress( &cGlobals->selfAddr, params );

    return bGlobals;
} /* commonInit */

static void
disposeDraw( CursesBoardGlobals* bGlobals )
{
    if ( !!bGlobals->boardWin ) {
        draw_unref( bGlobals->cGlobals.draw, NULL_XWE );
        wclear( bGlobals->boardWin );
        wrefresh( bGlobals->boardWin );
        delwin( bGlobals->boardWin );
    }
}

static void
disposeBoard( CursesBoardGlobals* bGlobals )
{
    CommonGlobals* cGlobals = &bGlobals->cGlobals;
    XP_ASSERT( 0 == cGlobals->refCount );
    XP_LOGFF( "passed bGlobals %p", bGlobals );

    XP_LOGFF( "freeing globals: %p", bGlobals );
    forgetGameGlobals( cGlobals->params->cag, cGlobals );
    g_free( bGlobals );
}

static int
utf8_len( const char* face )
{
    const int max = strlen(face);
    int count = 0;
    mbstate_t ps = {};
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
    // BoardCtxt* board = gr_getGame(cGlobals->gr)->board;
    const int width = bGlobals->winWidth;
    const int height = bGlobals->winHeight;

    XP_U16 fontWidth, fontHt;
    XW_DUtilCtxt* dutil = cGlobals->params->dutil;
    const DictionaryCtxt* dict =
        gr_getDictionary( dutil, cGlobals->gr, NULL_XWE );
    if ( !!dict ) {
        getFromDict( dict, &fontWidth, &fontHt );
        BoardDims dims;
        gr_figureLayout( dutil, cGlobals->gr, NULL_XWE, 
                         0, 0, width, height, 100,
                         150, 200, /* percents */
                         width*75/100,
                         fontWidth, fontHt,
                         XP_FALSE, &dims );
        gr_applyLayout( dutil, cGlobals->gr, NULL_XWE, &dims );
        XP_LOGFF( "calling board_draw()" );
        /* gr_invalAll( dutil, cGlobals->gr, NULL_XWE ); */
        /* gr_draw( dutil, cGlobals->gr, NULL_XWE ); */
    }
}

static void
initCP( CommonGlobals* cGlobals )
{
    const LaunchParams* params = cGlobals->params;
    cpFromLP( &cGlobals->cp, params );
}

static CursesBoardGlobals*
initNoDraw( CursesBoardState* cbState, GameRef gr,
            const CommsAddrRec* returnAddr )
{
    LOG_FUNC();
    CursesBoardGlobals* result = commonInit( cbState, gr );
    CommonGlobals* cGlobals = &result->cGlobals;

    if ( !!returnAddr ) {
        cGlobals->hostAddr = *returnAddr;
    }

    initCP( cGlobals );

    if ( !linuxOpenGame( cGlobals ) ) {
        disposeBoard( result );
        result = NULL;
    }
    return result;
}

static void
enableDraw( CursesBoardGlobals* bGlobals, const cb_dims* dims )
{
    LOG_FUNC();
    XP_ASSERT( !!dims );
    XP_ASSERT( !bGlobals->boardWin );
    bGlobals->boardWin = newwin( dims->height, dims->width, dims->top, 0 );
    getmaxyx( bGlobals->boardWin, bGlobals->winHeight, bGlobals->winWidth );

    CommonGlobals* cGlobals = &bGlobals->cGlobals;
    if( !!bGlobals->boardWin ) {
        cGlobals->draw =
            cursesDrawCtxtMake( bGlobals->cGlobals.params,
                                bGlobals->boardWin, cGlobals->gr,
                                DT_SCREEN );
    }

    setupBoard( bGlobals );
}

static CursesBoardGlobals*
findOrOpenForGameID( CursesBoardState* cbState, XP_U32 gameID )
{
    GameRef grs[1];
    XP_U16 nRefs = VSIZE(grs);
    gmgr_getForGID( cbState->params->dutil, NULL_XWE, gameID, grs, &nRefs );
    return nRefs == 1 ? findOrOpen( cbState, grs[0], NULL ) : NULL;
}

const CommonGlobals*
cb_getForGameID( CursesBoardState* cbState, XP_U32 gameID )
{
    CursesBoardGlobals* cbg = findOrOpenForGameID( cbState, gameID );
    CommonGlobals* result = &cbg->cGlobals;
    // XP_LOGFF( "(%X) => %p", gameID, result );
    return result;
}

static CursesBoardGlobals*
findOrOpen( CursesBoardState* cbState, GameRef gr,
            const CommsAddrRec* returnAddr )
{
    CommonGlobals* cGlobals = globalsForGameRef( cbState->params->cag,
                                                 gr, XP_FALSE );
    CursesBoardGlobals* result = (CursesBoardGlobals*)cGlobals;
    if ( !result ) {
        result = initNoDraw( cbState, gr, returnAddr ); /* adds to list */
        XP_ASSERT( !!result );
        setupBoard( result );
    }
    return result;
}

/* bool */
/* cb_feedRow( CursesBoardState* cbState, sqlite3_int64 rowid, XP_U16 expectSeed, */
/*             const XP_U8* buf, XP_U16 len, const CommsAddrRec* from ) */
/* { */
    /* LOG_FUNC(); */
    /* CursesBoardGlobals* bGlobals = findOrOpen( cbState, rowid, NULL ); */
    /* CommonGlobals* cGlobals = &bGlobals->cGlobals; */
    /* XP_U16 seed = gr_getChannelSeed( cGlobals->params->dutil, cGlobals->gr ); */
    /* bool success = 0 == expectSeed || seed == expectSeed; */
    /* if ( success ) { */
    /*     gameGotBuf( cGlobals, XP_TRUE, buf, len, from ); */
    /* } else { */
    /*     XP_LOGFF( "msg for seed %d but I opened %d", expectSeed, seed ); */
    /* } */
    /* return success; */
/* } */

void
cb_addInvites( CursesBoardState* cbState, XP_U32 gameID, XP_U16 nRemotes,
               const CommsAddrRec destAddrs[] )
{
    CursesBoardGlobals* bGlobals = findOrOpenForGameID( cbState, gameID );
    CommonGlobals* cGlobals = &bGlobals->cGlobals;
    linux_addInvites( cGlobals, nRemotes, destAddrs );
}

XP_Bool
cb_makeRematch( CursesBoardState* cbState, XP_U32 gameID, RematchOrder ro,
                XP_U32* newGameIDP )
{
    CursesBoardGlobals* bGlobals = findOrOpenForGameID( cbState, gameID );
    XP_Bool success = rematch_and_save( bGlobals, ro, newGameIDP );
    return success;
}

XP_Bool
cb_makeMoveIf( CursesBoardState* cbState, XP_U32 gameID, XP_Bool tryTrade )
{
    XP_LOGFF( "(tryTrade: %s)", boolToStr(tryTrade));
    CursesBoardGlobals* bGlobals = findOrOpenForGameID( cbState, gameID );
    XP_LOGFF( "bGlobals: %p", bGlobals );
    XP_Bool success = !!bGlobals
        && linux_makeMoveIf( &bGlobals->cGlobals, tryTrade );

    LOG_RETURNF( "%s", boolToStr(success) );
    return success;
}

XP_Bool
cb_sendChat( CursesBoardState* cbState, XP_U32 gameID, const char* msg )
{
    CursesBoardGlobals* bGlobals = findOrOpenForGameID( cbState, gameID );
    XP_Bool success = !!bGlobals;
    if ( success ) {
        CommonGlobals* cGlobals = &bGlobals->cGlobals;
        gr_sendChat( cGlobals->params->dutil, cGlobals->gr, NULL_XWE, msg );
    }
    return success;
}

XP_Bool
cb_undoMove( CursesBoardState* cbState, XP_U32 gameID )
{
    CursesBoardGlobals* bGlobals = findOrOpenForGameID( cbState, gameID );
    XP_Bool success = !!bGlobals;
    if ( success ) {
        XP_U16 limit = 0;
        CommonGlobals* cGlobals = &bGlobals->cGlobals;
        success = gr_handleUndo( cGlobals->params->dutil, cGlobals->gr,
                                 NULL_XWE, limit );
    }
    return success;
}

XP_Bool
cb_resign( CursesBoardState* cbState, XP_U32 gameID )
{
    CursesBoardGlobals* bGlobals = findOrOpenForGameID( cbState, gameID );
    XP_Bool success = !!bGlobals;
    if ( success ) {
        CommonGlobals* cGlobals = &bGlobals->cGlobals;
        gr_endGame( cGlobals->params->dutil, cGlobals->gr, NULL_XWE );
    }
    return success;
}

/* static void */
/* kill_board( gpointer data ) */
/* { */
/*     CursesBoardGlobals* bGlobals = (CursesBoardGlobals*)data; */
/*     CommonGlobals* cGlobals = &bGlobals->cGlobals; */
/*     linuxSaveGame( cGlobals ); */
/*     cg_unref( cGlobals ); */
/* } */

void
cb_closeAll( CursesBoardState* XP_UNUSED(cbState) )
{
    /* LOG_FUNC(); */
    /* GSList** listLoc = gamesListLocFromCBS( cbState ); */
    /* if ( !!*listLoc ) { */
    /*     g_slist_free_full( *listLoc, kill_board ); */
    /*     *listLoc = NULL; */
    /* } */
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
    CommonAppGlobals* cag = getCag( uc );
    CursesBoardGlobals* bGlobals = (CursesBoardGlobals*)
        globalsForGameRef( cag, uc->gr, XP_FALSE );
    char query[128];
    const char* playerName = bGlobals->cGlobals.gi->players[playerNum].name;

    snprintf( query, sizeof(query), 
              "Pick tile for %s! (Tab or type letter to select "
              "then hit <cr>.)", playerName );

    /*index = */curses_askLetter( bGlobals->boardWin, query, texts, nTiles );
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
    CommonGlobals* cGlobals = globalsForUtil( uc, XP_FALSE );
    CursesBoardGlobals* globals = (CursesBoardGlobals*)cGlobals;
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

    if ( 0 == cursesask( bGlobals->boardWin,  VSIZE(answers)-1, answers,
                         cGlobals->question ) ) {
        // BoardCtxt* board = gr_getGame(cGlobals->gr)->board;
        PhoniesConf pc = { .confirmed = XP_TRUE };
        XW_DUtilCtxt* dutil = cGlobals->params->dutil;
        gr_commitTurn( dutil, cGlobals->gr, NULL_XWE, &pc, XP_TRUE, NULL );//  ) {
            /* gr_draw( dutil, cGlobals->gr, NULL_XWE ); */
        /*     linuxSaveGame( &bGlobals->cGlobals ); */
        /* } */
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
        CommonGlobals* cGlobals = globalsForUtil( uc, XP_FALSE );
        if ( !!cGlobals ) {
            CursesBoardGlobals* bGlobals = (CursesBoardGlobals*)cGlobals;
            if ( !!bGlobals->boardWin ) {
                ca_informf( bGlobals->boardWin, "%s(oldOffset=%d, newOffset=%d)",
                            __func__, oldOffset, newOffset );
            }
        }
    }
} /* curses_util_yOffsetChange */

static void
curses_util_dictGone( XW_UtilCtxt* XP_UNUSED(uc), XWEnv XP_UNUSED(xwe),
                      const XP_UCHAR* XP_UNUSED_DBG(dictName) )
{
    XP_LOGFF( "(dictName: %s)", dictName );
}

#ifdef XWFEATURE_TURNCHANGENOTIFY
static void
curses_util_turnChanged( XW_UtilCtxt* XP_UNUSED(uc), XWEnv XP_UNUSED(xwe),
                         XP_S16 XP_UNUSED_DBG(newTurn) )
{
    XP_LOGFF( "(newTurn=%d)", newTurn );
}
#endif

static void
curses_util_notifyIllegalWords( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe),
                                const BadWordInfo* bwi,
                                const XP_UCHAR* XP_UNUSED(dictName),
                                XP_U16 player, XP_Bool turnLost,
                                XP_U32 bwKey )
{
    gchar* strs = g_strjoinv( "\", \"", (gchar**)bwi->words );
    gchar* msg = g_strdup_printf( "Player %d played bad word[s]: \"%s\". "
                                  "Turn lost: %s; key=%d", player, strs,
                                  boolToStr(turnLost), bwKey );

    CursesBoardGlobals* bGlobals = (CursesBoardGlobals*)
        globalsForUtil( uc, XP_FALSE );

    if ( !!bGlobals->boardWin ) {
        ca_inform( bGlobals->boardWin, msg );
    } else {
        XP_LOGFF( "msg: %s", msg );
    }
    g_free( strs );
    g_free( msg );
} /* curses_util_notifyIllegalWord */

static void
curses_util_countChanged( XW_UtilCtxt* XP_UNUSED_DBG(uc), XWEnv xwe,
                          XP_U16 XP_UNUSED_DBG(count),
                          XP_Bool XP_UNUSED_DBG(quashed) )
{
    XP_LOGFF( "(gr: %lX, count: %d, quashed: %s)", uc->gr, count,
              boolToStr(quashed) );
    XP_USE(xwe);
}

/* this needs to change!!! */
static void
curses_util_notifyMove( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe), XWStreamCtxt* stream )
{
    CursesBoardGlobals* globals = (CursesBoardGlobals*)
        globalsForUtil( uc, XP_FALSE );
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
    if (0 == cursesask( bGlobals->boardWin, VSIZE(buttons), buttons,
                        cGlobals->question ) ) {
        // BoardCtxt* board = gr_getGame(cGlobals->gr)->board;
        PhoniesConf pc = { .confirmed = XP_TRUE };
        XW_DUtilCtxt* dutil = cGlobals->params->dutil;
        gr_commitTurn( dutil, cGlobals->gr, NULL_XWE, &pc, XP_TRUE, NULL );// ) {
        /*     gr_draw( dutil, cGlobals->gr, NULL_XWE ); */
        /*     linuxSaveGame( cGlobals ); */
        /* } */
    }
    return FALSE;
}

static void
curses_util_notifyTrade( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe),
                         const XP_UCHAR** tiles, XP_U16 nTiles )
{
    CursesBoardGlobals* globals = (CursesBoardGlobals*)
        globalsForUtil(uc, XP_FALSE);
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

void
cursesShowFinalScores( CursesBoardGlobals* bGlobals )
{
    if ( !!bGlobals->boardWin ) {
        XWStreamCtxt* stream;
        XP_UCHAR* text;

        CommonGlobals* cGlobals = &bGlobals->cGlobals;
        stream = mem_stream_make_raw( MPPARM(cGlobals->params->mpool)
                                      cGlobals->params->vtMgr );
        XW_DUtilCtxt* dutil = cGlobals->params->dutil;
        gr_writeFinalScores( dutil, cGlobals->gr, NULL_XWE, stream );

        text = strFromStream( stream );

        (void)ca_inform( bGlobals->boardWin, text );

        free( text );
        stream_destroy( stream );
    }
} /* cursesShowFinalScores */

void
curses_util_notifyDupStatus( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe),
                             XP_Bool XP_UNUSED(amHost),
                             const XP_UCHAR* msg )
{
    CursesBoardGlobals* bGlobals = (CursesBoardGlobals*)
        globalsForUtil( uc, XP_FALSE );
    if ( !!bGlobals->boardWin ) {
        ca_inform( bGlobals->boardWin, msg );
    }
}

static void
curses_util_informUndo( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe) )
{
    CursesBoardGlobals* bGlobals = (CursesBoardGlobals*)
        globalsForUtil( uc, XP_FALSE );
    if ( !!bGlobals->boardWin ) {
        ca_inform( bGlobals->boardWin, "informUndo(): undo was done" );
    }
}

/* static void */
/* rematch_and_save_once( CursesBoardGlobals* bGlobals, RematchOrder ro ) */
/* { */
/*     LOG_FUNC(); */
/*     CommonGlobals* cGlobals = &bGlobals->cGlobals; */

/*     int32_t alreadyDone; */
/*     gchar key[128]; */
/*     snprintf( key, sizeof(key), "%X/rematch_done", cGlobals->gi->gameID ); */
/*     if ( gdb_fetchInt( cGlobals->params->pDb, key, &alreadyDone ) */
/*          && 0 != alreadyDone ) { */
/*         XP_LOGFF( "already rematched game %X", cGlobals->gi->gameID ); */
/*     } else { */
/*         if ( rematch_and_save( bGlobals, ro, NULL ) ) { */
/*             gdb_storeInt( cGlobals->params->pDb, key, 1 ); */
/*         } */
/*     } */
/*     LOG_RETURN_VOID(); */
/* } */

static XP_Bool
rematch_and_save( CursesBoardGlobals* bGlobals, RematchOrder ro,
                  XP_U32* newGameIDP )
{
    LOG_FUNC();
    CommonGlobals* cGlobals = &bGlobals->cGlobals;
    XW_DUtilCtxt* dutil = cGlobals->params->dutil;

    XP_UCHAR* newName = "newName";
    GameRef newGR = gr_makeRematch( dutil, cGlobals->gr, NULL_XWE, newName,
                                    ro, XP_TRUE, XP_FALSE );

    XP_Bool success = !!newGR;
    if ( success ) {
        if ( !!newGameIDP ) {
            const CurGameInfo* gi = gr_getGI( dutil, newGR, NULL_XWE );
            *newGameIDP = gi->gameID;
        }
    }
    LOG_RETURNF( "%s", boolToStr(success) );
    return success;
}

/* static void */
/* curses_util_notifyGameOver( XW_UtilCtxt* uc, XWEnv xwe, XP_S16 quitter ) */
/* { */
/*     LOG_FUNC(); */
/*     CursesBoardGlobals* bGlobals = (CursesBoardGlobals*) */
/*         globalsForUtil( uc, XP_FALSE ); */
/*     if ( !!bGlobals ) { */
/*         CommonGlobals* cGlobals = &bGlobals->cGlobals; */
/*         LaunchParams* params = cGlobals->params; */
/*         XW_DUtilCtxt* dutil = params->dutil; */
/*         // XWGame* game = gr_getGame( cGlobals->gr ); */
/*         gr_draw( dutil, cGlobals->gr, xwe ); */

/*         /\* game belongs in cGlobals... *\/ */
/*         if ( params->printHistory ) { */
/*             catGameHistory( cGlobals ); */
/*         } */

/*         catFinalScores( cGlobals, quitter ); */

/*         if ( params->quitAfter >= 0 ) { */
/*             sleep( params->quitAfter ); */
/*             handleQuit( cGlobals->params->cag, 0 ); */
/*         } else if ( params->undoWhenDone ) { */
/*             gr_handleUndo( dutil, cGlobals->gr, xwe, 0 ); */
/*         } else if ( !params->skipGameOver && !!bGlobals->boardWin ) { */
/*             /\* This is modal.  Don't show if quitting *\/ */
/*             cursesShowFinalScores( bGlobals ); */
/*         } */

/*         if ( params->rematchOnDone ) { */
/*             RematchOrder ro = !!params->rematchOrder */
/*                 ? roFromStr(params->rematchOrder) : RO_NONE; */
/*             rematch_and_save_once( bGlobals, ro ); */
/*         } */
/*     } */
/* } /\* curses_util_notifyGameOver *\/ */

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
    CursesBoardGlobals* globals = (CursesBoardGlobals*)
        globalsForUtil( uc, XP_FALSE );
    if ( !!globals ) {
        if ( globals->cGlobals.params->sleepOnAnchor ) {
            usleep( 10000 );
        }
    }
    return XP_TRUE;
} /* curses_util_hiliteCell */
#endif

#ifdef XWFEATURE_STOP_ENGINE
static XP_Bool
curses_util_stopEngineProgress( XW_UtilCtxt* XP_UNUSED(uc), XWEnv XP_UNUSED(xwe) )
{
    return XP_FALSE;
} /* curses_util_engineProgressCallback */
#endif

static XP_Bool
curses_util_altKeyDown( XW_UtilCtxt* XP_UNUSED(uc), XWEnv XP_UNUSED(xwe) )
{
    return XP_FALSE;
}

static void
curses_util_remSelected( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe) )
{
    CommonGlobals* cGlobals = (CommonGlobals*)
        globalsForUtil( uc, XP_FALSE );
    CursesBoardGlobals* bGlobals = (CursesBoardGlobals*)cGlobals;
    XWStreamCtxt* stream;
    XP_UCHAR* text;

    stream = mem_stream_make_raw( MPPARM(cGlobals->params->mpool)
                                  cGlobals->params->vtMgr );
    XW_DUtilCtxt* dutil = cGlobals->params->dutil;
    gr_formatRemainingTiles( dutil, cGlobals->gr, NULL_XWE, stream );

    text = strFromStream( stream );

    (void)ca_inform( bGlobals->boardWin, text );

    free( text );
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
    CursesBoardGlobals* bGlobals = (CursesBoardGlobals*)
        globalsForUtil( uc, XP_FALSE );
    LastMoveInfo lmi;
    CommonGlobals* cGlobals = &bGlobals->cGlobals;
    XW_DUtilCtxt* dutil = cGlobals->params->dutil;
    if ( gr_getPlayersLastScore( dutil, cGlobals->gr,
                                 NULL_XWE, player, &lmi ) ) {
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
    catAndClose( words );
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

static XP_Bool
curses_util_showChat( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe),
                      const XP_UCHAR* const XP_UNUSED_DBG(msg),
                      XP_S16 XP_UNUSED_DBG(from),
                      XP_U32 XP_UNUSED(timestamp) )
{
    CursesBoardGlobals* bGlobals = (CursesBoardGlobals*)
        globalsForUtil( uc, XP_FALSE );
    bGlobals->nChatsSent = 0;
    XP_LOGFF( "got \"%s\" from player[%d]", msg, from );

    CommonGlobals* cGlobals = &bGlobals->cGlobals;
    WINDOW* win = bGlobals->boardWin;
    XP_Bool shown = !!win;
    if ( shown ) {
        XW_DUtilCtxt* dutil = cGlobals->params->dutil;
        curses_openChat( win, dutil, cGlobals->gr );
    }
    return shown;
}

void
cb_setupUtilCallbacks( XW_UtilCtxt* util )
{
    // util->closure = bGlobals;
#define SET_PROC(NAM) util->vtable->m_util_##NAM = curses_util_##NAM
    SET_PROC(userError);
    SET_PROC(countChanged);
    SET_PROC(notifyMove);
    SET_PROC(notifyTrade);
    SET_PROC(notifyPickTileBlank);
    SET_PROC(informNeedPickTiles);
    SET_PROC(informNeedPassword);
    SET_PROC(trayHiddenChange);
    SET_PROC(yOffsetChange);
    SET_PROC(dictGone);
#ifdef XWFEATURE_TURNCHANGENOTIFY
    SET_PROC(turnChanged);
#endif
    SET_PROC(notifyDupStatus);
    SET_PROC(informUndo);
    SET_PROC(informNetDict);
    // SET_PROC(notifyGameOver);
#ifdef XWFEATURE_HILITECELL
    SET_PROC(hiliteCell);
#endif
#ifdef XWFEATURE_STOP_ENGINE
    SET_PROC(stopEngineProgress);
#endif
    SET_PROC(altKeyDown);       /* ?? */
    SET_PROC(notifyIllegalWords);
    SET_PROC(remSelected);
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
    SET_PROC(showChat);
#undef SET_PROC

    assertTableFull( util->vtable, sizeof(*util->vtable), "curses util" );
} /* setupCursesUtilCallbacks */

static bool
handleFlip( void* closure, int XP_UNUSED(key) )
{
    CursesBoardGlobals* bGlobals = (CursesBoardGlobals*)closure;
    CommonGlobals* cGlobals = &bGlobals->cGlobals;
    // BoardCtxt* board = gr_getGame(cGlobals->gr)->board;
    XW_DUtilCtxt* dutil = cGlobals->params->dutil;
    gr_flip( dutil, cGlobals->gr, NULL_XWE );

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
    XW_DUtilCtxt* dutil = cGlobals->params->dutil;
    gr_handleKey( dutil, cGlobals->gr, NULL_XWE,
                  XP_CURSOR_KEY_DEL, &handled );
    return XP_TRUE;
} /* handleBackspace */

static bool
handleUndo( void* closure, int XP_UNUSED(key) )
{
    CursesBoardGlobals* bGlobals = (CursesBoardGlobals*)closure;
    CommonGlobals* cGlobals = &bGlobals->cGlobals;
    XW_DUtilCtxt* dutil = cGlobals->params->dutil;
    if ( gr_handleUndo( dutil, cGlobals->gr, NULL_XWE, 0 ) ) {
        gr_draw( dutil, cGlobals->gr, NULL_XWE );
    }
    return XP_TRUE;
} /* handleUndo */

static bool
handleReplace( void* closure, int XP_UNUSED(key) )
{
    CommonGlobals* cGlobals = &((CursesBoardGlobals*)closure)->cGlobals;
    // BoardCtxt* board = gr_getGame(cGlobals->gr)->board;
    XW_DUtilCtxt* dutil = cGlobals->params->dutil;
    gr_replaceTiles( dutil, cGlobals->gr, NULL_XWE );
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
        CommsAddrRec destAddr = {};
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
            XW_DUtilCtxt* dutil = cGlobals->params->dutil;
            gr_invite( dutil, cGlobals->gr, NULL_XWE, &nli, &destAddr, XP_TRUE );
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
    // CommsCtxt* comms = _getGame(cGlobals->gr)->comms;
    XW_DUtilCtxt* dutil = cGlobals->params->dutil;
    gr_getSelfAddr( dutil, cGlobals->gr, NULL_XWE, &selfAddr );

    gint forceChannel = 1;
    const XP_U16 nPlayers = params->connInfo.inviteeCounts[forceChannel-1];
    NetLaunchInfo nli;
    nli_init( &nli, cGlobals->gi, &selfAddr, nPlayers, forceChannel );

    if ( ROLE_ISHOST != cGlobals->gi->deviceRole ) {
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
        // linux_sms_invite( params, &nli, selfAddr.u.sms.phone, selfAddr.u.sms.port );
        XP_ASSERT(0);
    } else if ( addr_hasType( &selfAddr, COMMS_CONN_MQTT ) ) {
        XP_ASSERT(0);
        // mqttc_invite( params, &nli, mqttc_getDevID( params ) );
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
openChat( void* closure, int XP_UNUSED(key) )
{
    LOG_FUNC();
    CursesBoardGlobals* bGlobals = (CursesBoardGlobals*)closure;
    CommonGlobals* cGlobals = &bGlobals->cGlobals;
    XW_DUtilCtxt* dutil = cGlobals->params->dutil;
    curses_openChat( bGlobals->boardWin, dutil, cGlobals->gr );
    return XP_TRUE;
}

static bool
handleCommit( void* closure, int XP_UNUSED(key) )
{
    CursesBoardGlobals* bGlobals = (CursesBoardGlobals*)closure;
    CommonGlobals* cGlobals = &bGlobals->cGlobals;
    // BoardCtxt* board = gr_getGame(cGlobals->gr)->board;
    XW_DUtilCtxt* dutil = cGlobals->params->dutil;
    gr_commitTurn( dutil, cGlobals->gr, NULL_XWE, NULL, XP_FALSE, NULL );
    /*     gr_draw( dutil, cGlobals->gr, NULL_XWE ); */
    /* } */
    return XP_TRUE;
} /* handleCommit */

static bool
handleJuggle( void* closure, int XP_UNUSED(key) )
{
    CommonGlobals* cGlobals = &((CursesBoardGlobals*)closure)->cGlobals;
    // BoardCtxt* board = gr_getGame(cGlobals->gr)->board;
    XW_DUtilCtxt* dutil = cGlobals->params->dutil;
    gr_juggleTray( dutil, cGlobals->gr, NULL_XWE );
    return XP_TRUE;
} /* handleJuggle */

static bool
handleHide( void* closure, int XP_UNUSED(key) )
{
    CommonGlobals* cGlobals = &((CursesBoardGlobals*)closure)->cGlobals;
    // BoardCtxt* board = gr_getGame(cGlobals->gr)->board;
    XW_DUtilCtxt* dutil = cGlobals->params->dutil;
    XW_TrayVisState curState = gr_getTrayVisState( dutil, cGlobals->gr, NULL_XWE );

    bool draw = curState == TRAY_REVEALED
        ? gr_hideTray( dutil, cGlobals->gr, NULL_XWE )
        : gr_showTray( dutil, cGlobals->gr, NULL_XWE );
    if ( draw ) {
        gr_draw( dutil, cGlobals->gr, NULL_XWE );
    }

    return XP_TRUE;
} /* handleJuggle */

#ifdef KEYBOARD_NAV
static void
checkAssignFocus( XW_DUtilCtxt* dutil, GameRef gr )
{
    if ( OBJ_NONE == gr_getFocusOwner(dutil, gr, NULL_XWE) ) {
        gr_focusChanged( dutil, gr, NULL_XWE, OBJ_BOARD, XP_TRUE );
    }
}

static XP_Bool
handleFocusKey( CursesBoardGlobals* bGlobals, XP_Key key )
{
    CommonGlobals* cGlobals = &bGlobals->cGlobals;
    // BoardCtxt* board = gr_getGame(cGlobals->gr)->board;
    XW_DUtilCtxt* dutil = cGlobals->params->dutil;
    checkAssignFocus( dutil, cGlobals->gr );

    XP_Bool handled;
    gr_handleKey( dutil, cGlobals->gr, NULL_XWE, key, &handled );
    if ( !handled ) {
        BoardObjectType nxt;
        BoardObjectType order[] = { OBJ_BOARD, OBJ_SCORE, OBJ_TRAY };
        linShiftFocus( cGlobals, key, order, &nxt );
        if ( nxt != OBJ_NONE ) {
            changeMenuForFocus( bGlobals, nxt );
        }
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
    // BoardCtxt* board = gr_getGame(cGlobals->gr)->board;
    XW_DUtilCtxt* dutil = cGlobals->params->dutil;
    XP_Bool redo;
    gr_requestHint( dutil, cGlobals->gr, NULL_XWE,
#ifdef XWFEATURE_SEARCHLIMIT
                    XP_FALSE,
#endif
                    XP_FALSE, &redo );
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
    closeBoard( bGlobals, XP_TRUE );
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
    // BoardCtxt* board = gr_getGame(bGlobals->cGlobals.gameRef)->board;
    CommonGlobals* cGlobals = &bGlobals->cGlobals;
    XW_DUtilCtxt* dutil = cGlobals->params->dutil;
    GameRef gr = cGlobals->gr;
    checkAssignFocus( dutil, gr );
    XP_Bool handled;
    (void)gr_handleKey( dutil, gr, NULL_XWE, XP_RAISEFOCUS_KEY, &handled );
    gr_draw( dutil, gr, NULL_XWE );
    return XP_TRUE;
} /* handleSpace */

static bool
handleRet( void* closure, int key )
{
    CursesBoardGlobals* bGlobals = (CursesBoardGlobals*)closure;
    CommonGlobals* cGlobals = &bGlobals->cGlobals;
    GameRef gr = cGlobals->gr;
    XW_DUtilCtxt* dutil = cGlobals->params->dutil;
    // BoardCtxt* board = gr_getGame(bGlobals->cGlobals.gameRef)->board;
    XP_Bool handled;
    XP_Key xpKey = (key & ALT_BIT) == 0 ? XP_RETURN_KEY : XP_ALTRETURN_KEY;
    gr_handleKey( dutil, gr, NULL_XWE, xpKey, &handled );
    return XP_TRUE;
} /* handleRet */

static bool
handleShowVals( void* closure, int XP_UNUSED(key) )
{
    CursesBoardGlobals* bGlobals = (CursesBoardGlobals*)closure;
    CommonGlobals* cGlobals = &bGlobals->cGlobals;

    XWStreamCtxt* stream = mem_stream_make_raw( MPPARM(cGlobals->params->mpool)
                                                cGlobals->params->vtMgr );
    XW_DUtilCtxt* dutil = cGlobals->params->dutil;
    gr_formatDictCounts( dutil, cGlobals->gr, NULL_XWE,
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
