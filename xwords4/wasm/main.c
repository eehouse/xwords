/* -*- compile-command: "cd ../wasm && make MEMDEBUG=TRUE install -j3"; -*- */
/*
 * Copyright 2021 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#include <sys/time.h>
#include <stdio.h>
#include <stdarg.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <emscripten.h>
#include <unistd.h>
#include <stdlib.h>

#include "game.h"
#include "device.h"
#include "mempool.h"
#include "nli.h"
#include "strutils.h"
#include "movestak.h"

#include "main.h"
#include "wasmdraw.h"
#include "wasmutil.h"
#include "wasmdutil.h"
#include "wasmdict.h"
#include "wasmasm.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#define WASM_BOARD_LEFT 0
#define WASM_HOR_SCORE_TOP 0

#define WINDOW_WIDTH 400
#define WINDOW_HEIGHT 600
#define BDWIDTH WINDOW_WIDTH
#define BDHEIGHT WINDOW_HEIGHT

#define KEY_LAST_GID "cur_game"
#define KEY_PLAYER_NAME "player_name"
#define KEY_GAME_PREFIX "key_data_"
#define KEY_NAME_PREFIX "key_name_"

#define DICTNAME "assets_dir/CollegeEng_2to8.xwd"


#define BUTTON_OK "OK"
#define BUTTON_CANCEL "Cancel"

#define BUTTONS_ID_GAME "game_buttons"
#define BUTTONS_ID_DEVICE "device_buttons"

#define BUTTON_HINTDOWN "Prev Hint"
#define BUTTON_HINTUP "Next Hint"
#define BUTTON_TRADE "Trade"
#define BUTTON_STOPTRADE "Cancel Trade"
#define BUTTON_COMMIT "Commit"
#define BUTTON_FLIP "Flip"
#define BUTTON_REDO "Redo"
#define BUTTON_VALS "Vals"
#define BUTTON_INVITE "Invite"

#define BUTTON_GAME_NEW "New Game"
#define BUTTON_GAME_OPEN "Open Game"
#define BUTTON_GAME_RENAME "Rename Game"
#define BUTTON_GAME_DELETE "Delete Game"
#define MAX_BUTTONS 20          /* not sure what's safe here */

typedef struct _NewGameParams {
    bool isRobotNotRemote;
    bool hintsNotAllowed;
} NewGameParams;

static void updateScreen( GameState* gs, bool doSave );
static void clearScreen( Globals* globals );
static GameState* newGameState( Globals* globals );
static GameState* getSavedGame( Globals* globals, int gameID );
static void loadAndDraw( Globals* globals, const NetLaunchInfo* invite,
                         const char* gameID, NewGameParams* params );
static GameState* getCurGame( Globals* globals );
static void nameGame( GameState* gs );
static void ensureName( GameState* gs );
static void loadName( GameState* gs );
static void saveName( GameState* gs );


typedef void (*StringProc)(void* closure, const char* str);

EM_JS(void, show_name, (const char* name), {
        let jsname = UTF8ToString(name);
        document.getElementById('gamename').textContent = jsname;
    });

EM_JS(void, call_dialog, (const char* str, const char** but_strs,
                          StringProc proc, void* closure), {
          var buttons = [];
          for ( let ii = 0; ; ++ii ) {
              const mem = HEAP32[(but_strs + (ii * 4)) >> 2];
              if ( 0 == mem ) {
                  break;
              }
              const str = UTF8ToString(mem);
              buttons.push(str);
          }
          nbDialog(UTF8ToString(str), buttons, proc, closure);
      } );

EM_JS(void, call_pickBlank, (const char* msg, const char** strs, int nStrs,
                             StringProc proc, void* closure), {
          var buttons = [];
          for ( let ii = 0; ii < nStrs; ++ii ) {
              const mem = HEAP32[(strs + (ii * 4)) >> 2];
              const str = UTF8ToString(mem);
              buttons.push(str);
          }
          nbBlankPick(UTF8ToString(msg), buttons, proc, closure);
      } );

EM_JS(void, call_pickGame, (const char* msg, StringProc proc, void* closure), {
        var map = {};
        for (var ii = 0; ii < localStorage.length; ++ii ) {
            var key = localStorage.key(ii);
            if ( key.startsWith('key_data_') ) { // KEY_GAME_PREFIX
                /* This is a legit stored game. Get the gameID part as key,
                   mapped to name if known. */
                let arr = key.split('_');
                let id = arr[arr.length - 1];

                let name = localStorage.getItem('key_name_' + id);
                if (! name ) {
                    name = key;
                }
                map[id] = name;
                console.log('added ' + id + ' -> ' + name);
            }
        }

        nbGamePick(UTF8ToString(msg), map, proc, closure);
    } );

EM_JS(void, call_get_string, (const char* msg, const char* dflt,
                              StringProc proc, void* closure), {
          let jsMgs = UTF8ToString(msg);
          let jsDflt = UTF8ToString(dflt);
          nbGetString( jsMgs, jsDflt, proc, closure );
      } );

EM_JS(void, call_haveDevID, (void* closure, const char* devid,
                             const char* gitrev, int now, StringProc proc), {
          let jsgr = UTF8ToString(gitrev);
          onHaveDevID(closure, UTF8ToString(devid), jsgr, now, proc);
      });

EM_JS(bool, call_mqttSend, (const char* topic, const uint8_t* ptr, int len), {
        let topStr = UTF8ToString(topic);
        let buffer = new Uint8Array(Module.HEAPU8.buffer, ptr, len);
        return mqttSend(topStr, buffer);
});

EM_JS(int, getNextGameNo, (void), {
        let jskey = UTF8ToString('next_gameno_2');
        let val = localStorage.getItem(jskey);
        if (val) {
            val = parseInt(val);
        } else {
            val = 0;
        }
        ++val;
        localStorage.setItem(jskey, val);
        return val;
    });

typedef void (*JSCallback)(void* closure);

EM_JS(void, jscallback_set, (JSCallback proc, void* closure, int inMS), {
        let timerproc = function(closure) {
            ccall('cbckVoid', null, ['number', 'number'], [proc, closure]);
        };
        setTimeout( timerproc, inMS, closure );
    });

EM_JS(void, setButtonText, (const char* id, const char* text), {
        let jsid = UTF8ToString(id);
        let jstext = UTF8ToString(text);
        document.getElementById(jsid).textContent = jstext;
    });

EM_JS(void, setButtons, (const char* id, const char** bstrs,
                         StringProc proc, void* closure), {
          var buttons = [];
          for ( let ii = 0; ; ++ii ) {
              const mem = HEAP32[(bstrs + (ii * 4)) >> 2];
              if ( 0 == mem ) {
                  break;
              }
              const str = UTF8ToString(mem);
              buttons.push(str);
          }
          setDivButtons(UTF8ToString(id), buttons, proc, closure);
    });

EM_JS(void, callNewGame, (const char* msg, void* closure), {
        let jsmsg = UTF8ToString(msg);
        nbGetNewGame(closure, jsmsg);
    });

typedef void (*ConfirmProc)( void* closure, bool confirmed );

typedef struct _ConfirmState {
    Globals* globals;
    ConfirmProc proc;
    void* closure;
} ConfirmState;

static void
onConfirmed( void* closure, const char* button )
{
    bool confirmed = 0 == strcmp( button, BUTTON_OK );
    ConfirmState* cs = (ConfirmState*)closure;
    (*cs->proc)( cs->closure, confirmed );
    XP_FREE( cs->globals->mpool, cs );
}

static void
call_confirm( Globals* globals, const char* msg,
              ConfirmProc proc, void* closure )
{
    const char* buttons[] = { BUTTON_CANCEL, BUTTON_OK, NULL };
    ConfirmState* cs = XP_MALLOC( globals->mpool, sizeof(*cs) );
    cs->globals = globals;
    cs->proc = proc;
    cs->closure = closure;
    call_dialog( msg, buttons, onConfirmed, cs );
}

static void
call_alert( const char* msg )
{
    const char* buttons[] = { BUTTON_OK, NULL };
    call_dialog( msg, buttons, NULL, NULL );
}

static bool
sendStreamToDev( XWStreamCtxt* stream, const MQTTDevID* devID )
{
    XP_S16 nSent = -1;
    XP_UCHAR topic[64];
    formatMQTTTopic( devID, topic, sizeof(topic) );

    XP_U16 streamLen = stream_getSize( stream );
    bool success = call_mqttSend( topic, stream_getPtr( stream ), streamLen );
    stream_destroy( stream, NULL );
    LOG_RETURNF("%d", nSent);
    return success;
}

static XP_S16
send_msg( XWEnv xwe, const XP_U8* buf, XP_U16 len,
          const XP_UCHAR* msgNo, const CommsAddrRec* addr,
          CommsConnType conType, XP_U32 gameID, void* closure )
{
    XP_S16 nSent = -1;
    Globals* globals = (Globals*)closure;

    if ( addr_hasType( addr, COMMS_CONN_MQTT ) ) {
        // MQTTDevID devID = addr->u.mqtt.devID;
        XWStreamCtxt* stream = mem_stream_make_raw( MPPARM(globals->mpool)
                                                    globals->vtMgr );
        dvc_makeMQTTMessage( globals->dutil, NULL, stream,
                             gameID, buf, len );
        if ( sendStreamToDev( stream, &addr->u.mqtt.devID ) ) {
            nSent = len;
        }
    }

    LOG_RETURNF( "%d", nSent );
    return nSent;
}

static void
formatGameID( char* buf, size_t len, int gameID )
{
    snprintf( buf, len, "%X", gameID );
}

static void
unformatGameID( int* gameID, const char* gameIDStr )
{
    sscanf( gameIDStr, "%X", gameID );
    XP_LOGFF( "unformatGameID(%s) => %X", gameIDStr, *gameID );
}

static void
formatGameKeyStr( char* buf, size_t len, const char* gameID )
{
    snprintf( buf, len, KEY_GAME_PREFIX "%s", gameID );
}

static void
formatGameKeyInt( char* buf, size_t len, int gameID )
{
    snprintf( buf, len, KEY_GAME_PREFIX "%X", gameID );
}

static void
formatNameKey( char* buf, size_t len, int gameID )
{
    snprintf( buf, len, KEY_NAME_PREFIX "%X", gameID );
}

static void
makeSelfAddr( Globals* globals, CommsAddrRec* addr )
{
    addr_setType( addr, COMMS_CONN_MQTT );
    dvc_getMQTTDevID( globals->dutil, NULL, &addr->u.mqtt.devID );
}

static void
onGotInviteeID( void* closure, const char* mqttid )
{
    MQTTDevID remoteDevID;
    if ( strToMQTTCDevID( mqttid, &remoteDevID ) ) {
        CAST_GS(GameState*, gs, closure);
        Globals* globals = gs->globals;
        CommsAddrRec myAddr = {0};
        makeSelfAddr( globals, &myAddr );

        NetLaunchInfo nli = {0};    /* include everything!!! */
        nli_init( &nli, &gs->gi, &myAddr, 1, 1 );

        XWStreamCtxt* stream = mem_stream_make_raw( MPPARM(globals->mpool)
                                                    globals->vtMgr );
        dvc_makeMQTTInvite( globals->dutil, NULL, stream, &nli );

        sendStreamToDev( stream, &remoteDevID );
    } else {
        call_alert( "MQTT id looks badly formed" );
    }
}

static void
onGameButton( void* closure, const char* button )
{
    if ( !!button ) {
        CAST_GS(GameState*, gs, closure);

        XP_Bool draw = XP_FALSE;
        BoardCtxt* board = gs->game.board;
        XP_Bool redo;

        if ( 0 == strcmp(button, BUTTON_HINTDOWN ) ) {
            draw = board_requestHint( board, NULL, XP_TRUE, &redo );
        } else if ( 0 == strcmp(button, BUTTON_HINTUP) ) {
            draw = board_requestHint( board, NULL, XP_FALSE, &redo );
        } else if ( 0 == strcmp(button, BUTTON_TRADE ) ) {
            draw = board_beginTrade( board, NULL );
        } else if ( 0 == strcmp(button, BUTTON_STOPTRADE ) ) {
            draw = board_endTrade( board );
        } else if ( 0 == strcmp(button, BUTTON_COMMIT) ) {
            draw = board_commitTurn( board, NULL, XP_FALSE, XP_FALSE, NULL );
        } else if ( 0 == strcmp(button, BUTTON_FLIP) ) {
            draw = board_flip( board );
        } else if ( 0 == strcmp(button, BUTTON_REDO) ) {
            draw = board_redoReplacedTiles( board, NULL )
                || board_replaceTiles( board, NULL );
        } else if ( 0 == strcmp(button, BUTTON_VALS) ) {
            Globals* globals = gs->globals;
            globals->cp.tvType = (globals->cp.tvType + 1) % TVT_N_ENTRIES;
            draw = board_prefsChanged( board, &globals->cp );
        } else if ( 0 == strcmp(button, BUTTON_INVITE) ) {
            call_get_string( "Invitee's MQTT Device ID?", "",
                             onGotInviteeID, gs );
        }

        if ( draw ) {
            updateScreen( gs, true );
        }
    }
}

static void
updateGameButtons( GameState* gs )
{
    const char* buttons[MAX_BUTTONS];
    int cur = 0;

    if ( !!gs->util ) {
        XP_U16 nPending = server_getPendingRegs( gs->game.server );
        if ( 0 < nPending ) {
            buttons[cur++] = BUTTON_INVITE;
        } else {
            GameStateInfo gsi;
            game_getState( &gs->game, NULL, &gsi );

            if ( gsi.canHint ) {
                buttons[cur++] = BUTTON_HINTDOWN;
                buttons[cur++] = BUTTON_HINTUP;
            }

            if ( gsi.inTrade ) {
                buttons[cur++] = BUTTON_STOPTRADE;
            } else if ( gsi.canTrade ) {
                buttons[cur++] = BUTTON_TRADE;
            }
            buttons[cur++] = BUTTON_COMMIT;
            buttons[cur++] = BUTTON_FLIP;

            if ( gsi.canRedo ) {
                buttons[cur++] = BUTTON_REDO;
            }

            buttons[cur++] = BUTTON_VALS;
        }
    }
    buttons[cur++] = NULL;

    setButtons( BUTTONS_ID_GAME, buttons, onGameButton, gs );
}

static void
onGameChosen( void* closure, const char* key )
{
    CAST_GLOB(Globals*, globals, closure);
    loadAndDraw( globals, NULL, key, NULL );
}

static void
onGameRanamed( void* closure, const char* newName )
{
    CAST_GS(GameState*, gs, closure);
    if ( !!newName ) {
        snprintf( gs->gameName, sizeof(gs->gameName) - 1,
                  "%s", newName );
        // be safe
        gs->gameName[sizeof(gs->gameName)-1] = '\0';
        saveName( gs );
    }
}

static void
cleanupGame( GameState* gs )
{
    if ( !!gs->util ) {
        game_dispose( &gs->game, NULL );
        gi_disposePlayerInfo( MPPARM(gs->globals->mpool) &gs->gi );
        wasm_util_destroy( gs->util );
        gs->util = NULL;
        // XP_MEMSET( &gs, 0, sizeof(globals->gs) );
    }
}

static void
deleteGame( GameState* gs )
{
    int gameID = gs->gi.gameID; /* remember it */
    cleanupGame( gs );

    char key[32];
    formatNameKey( key, sizeof(key), gameID );
    remove_stored_value( key );

    formatGameKeyInt( key, sizeof(key), gameID );
    remove_stored_value( key );
}

static void
onDeleteConfirmed( void* closure, bool confirmed )
{
    if ( confirmed ) {
        CAST_GS(GameState*, gs, closure);
        Globals* globals = gs->globals;
        deleteGame( gs );
        clearScreen( globals );
    }
}

static void
onDeviceButton( void* closure, const char* button )
{
    CAST_GLOB(Globals*, globals, closure);
    XP_LOGFF( "(button=%s)", button );
    if ( 0 == strcmp(button, BUTTON_GAME_NEW) ) {
        callNewGame("Configure your new game", globals);
    } else if ( 0 == strcmp(button, BUTTON_GAME_OPEN) ) {
        const char* msg = "Choose game to open";
        call_pickGame( msg, onGameChosen, globals );
    } else if ( 0 == strcmp(button, BUTTON_GAME_RENAME ) ) {
        GameState* curGS = getCurGame( globals );
        ensureName( curGS );
        call_get_string( "Rename your game", curGS->gameName,
                         onGameRanamed, curGS );
    } else if ( 0 == strcmp(button, BUTTON_GAME_DELETE) ) {
        GameState* curGS = getCurGame( globals );
        char msg[256];
        snprintf( msg, sizeof(msg), "Are you sure you want to delete the game \"%s\"?"
                  "\nThis action cannot be undone.",
                  curGS->gameName );
        call_confirm( globals, msg, onDeleteConfirmed, curGS );
    }
}

static void
updateDeviceButtons( Globals* globals )
{
    const char* buttons[MAX_BUTTONS];
    int cur = 0;
    buttons[cur++] = BUTTON_GAME_NEW;
    buttons[cur++] = BUTTON_GAME_OPEN;
    buttons[cur++] = BUTTON_GAME_RENAME;
    buttons[cur++] = BUTTON_GAME_DELETE;
    buttons[cur++] = NULL;

    setButtons( BUTTONS_ID_DEVICE, buttons, onDeviceButton, globals );
}

static void
onConflict( void* closure, const char* ignored )
{
    Globals* globals = (Globals*)closure;
    call_alert( "Control passed to another tab" );
    XP_MEMSET( globals, 0, sizeof(*globals) ); /* stop everything :-) */
}

static void
initDeviceGlobals( Globals* globals )
{
    globals->cp.showBoardArrow = XP_TRUE;
    globals->cp.allowPeek = XP_TRUE;
    globals->cp.showRobotScores = XP_TRUE;
    globals->cp.sortNewTiles = XP_TRUE;
    globals->cp.showColors = XP_TRUE;

    globals->procs.send = send_msg;
    globals->procs.closure = globals;

#ifdef MEMDEBUG
    globals->mpool = mpool_make( "wasm" );
#endif
    globals->vtMgr = make_vtablemgr( MPPARM_NOCOMMA(globals->mpool) );
    globals->dutil = wasm_dutil_make( MPPARM(globals->mpool) globals->vtMgr, globals );
    globals->dictMgr = dmgr_make( MPPARM_NOCOMMA(globals->mpool) );
    globals->dict = wasm_dictionary_make( MPPARM(globals->mpool) NULL,
                                          globals, DICTNAME, true );

    dict_ref( globals->dict, NULL );

    globals->draw = wasm_draw_make( MPPARM(globals->mpool)
                                    WINDOW_WIDTH, WINDOW_HEIGHT );

    MQTTDevID devID;
    dvc_getMQTTDevID( globals->dutil, NULL, &devID );
    XP_UCHAR buf[32];
    XP_SNPRINTF( buf, VSIZE(buf), MQTTDevID_FMT, devID );
    XP_LOGFF( "got mqtt devID: %s", buf );
    int now = dutil_getCurSeconds( globals->dutil, NULL );
    call_haveDevID( globals, buf, GITREV, now, onConflict );
}

static void
storeCurOpen( GameState* gs )
{
    char gidBuf[16];
    formatGameID( gidBuf, sizeof(gidBuf), gs->gi.gameID );
    set_stored_value( KEY_LAST_GID, gidBuf );
    // XP_LOGFF( "saved KEY_LAST_GID: %s", gidBuf );
}

static void
startGame( GameState* gs, const char* name )
{
    gs->globals->curGame = gs;
    ensureName( gs );
    XP_LOGFF( "changed curGame to %s", gs->gameName );
    show_name( gs->gameName );
    storeCurOpen( gs );

    BoardDims dims;
    board_figureLayout( gs->game.board, NULL, &gs->gi,
                        WASM_BOARD_LEFT, WASM_HOR_SCORE_TOP, BDWIDTH, BDHEIGHT,
                        110, 150, 200, BDWIDTH-25, BDWIDTH/15, BDHEIGHT/15,
                        XP_FALSE, &dims );
    XP_LOGFF( "calling board_applyLayout" );
    board_applyLayout( gs->game.board, NULL, &dims );
    XP_LOGFF( "calling model_setDictionary" );
    model_setDictionary( gs->game.model, NULL, gs->globals->dict );

    board_invalAll( gs->game.board ); /* redraw screen on loading new game */

    if ( SERVER_ISCLIENT == gs->gi.serverRole ) {
        if ( !!name ) {
            replaceStringIfDifferent( gs->globals->mpool,
                                      &gs->gi.players[0].name,
                                      name );
        }
        server_initClientConnection( gs->game.server, NULL );
    }
    
    (void)server_do( gs->game.server, NULL ); /* assign tiles, etc. */
    if ( !!gs->game.comms ) {
        comms_resendAll( gs->game.comms, NULL, COMMS_CONN_MQTT, XP_TRUE );
    }

    updateScreen( gs, true );
    LOG_RETURN_VOID();
}

typedef struct _AskReplaceState {
    Globals* globals;
    NetLaunchInfo invite;
} AskReplaceState;

static void
onPlayerNamed( void* closure, const char* name )
{
    CAST_GS(GameState*, gs, closure);
    if ( !!name ) {
        set_stored_value( KEY_PLAYER_NAME, name );
        startGame( gs, name );
    }
}

static GameState*
newFromInvite( Globals* globals, const NetLaunchInfo* invite )
{
    GameState* gs = newGameState(globals);
    gs->util = wasm_util_make( MPPARM(globals->mpool) &gs->gi,
                               globals->dutil, gs );

    game_makeFromInvite( MPPARM(globals->mpool) NULL, invite,
                         &gs->game, &gs->gi,
                         globals->dict, NULL,
                         gs->util, globals->draw,
                         &globals->cp, &globals->procs );
    if ( invite->gameName[0] ) {
        strcpy( gs->gameName, invite->gameName );
    }
    ensureName( gs );
    return gs;
}

/* If you launch a URL that encodes an invitation, you'll get here. If it's
 * the first time (the game hasn't been created yet) you'll get a new game
 * that connects to the host. If you've already created the game, you'll be
 * taken to it in whatever state it's in. If you've deleted the game, bad
 * situation: you'll get a new game that will be unable to connect to any
 * host.
 */
static GameState*
gameFromInvite( Globals* globals, const NetLaunchInfo* invite )
{
    bool needsLoad = true;
    GameState* gs = getSavedGame( globals, invite->gameID );
    if ( !gs ) {
        if ( invite->lang == 1 ) {
            gs = newFromInvite( globals, invite );
        } else {
            call_alert( "Invitations are only supported for play in English right now." );
        }
    }
    return gs;
}

static GameState*
newGameState( Globals* globals )
{
    GameState* gs = XP_CALLOC( globals->mpool, sizeof(*gs) );
    gs->globals = globals;
#ifdef DEBUG
    gs->_GUARD = GUARD_GS;
#endif
    gs->next = globals->games;
    globals->games = gs;

    return gs;
}

static GameState*
getCurGame( Globals* globals )
{
    return globals->curGame;
}

static void
removeGameState( GameState* gs )
{
    XP_ASSERT(0);
}

static GameState*
getSavedGame( Globals* globals, int gameID )
{
    GameState* gs;

    for ( gs = globals->games; !!gs; gs = gs->next ) {
        if ( gameID == gs->gi.gameID ) {
            break;
        }
    }

    if ( !gs ) {
        gs = newGameState( globals );

        XP_LOGFF( "gameID: %X", gameID );
        bool loaded = false;
        XWStreamCtxt* stream = mem_stream_make_raw( MPPARM(globals->mpool)
                                                    globals->vtMgr );
        char key[32];
        formatGameKeyInt( key, sizeof(key), gameID );
        dutil_loadStream( globals->dutil, NULL, key, NULL, stream );
        if ( 0 < stream_getSize( stream ) ) {
            XP_ASSERT( !gs->util );
            gs->util = wasm_util_make( MPPARM(globals->mpool) &gs->gi,
                                       globals->dutil, gs );

            XP_LOGFF( "there's a saved game!!" );
            loaded = game_makeFromStream( MPPARM(globals->mpool) NULL, stream,
                                          &gs->game, &gs->gi,
                                          globals->dict, NULL,
                                          gs->util, globals->draw,
                                          &globals->cp, &globals->procs );

            if ( loaded ) {
                loadName( gs );
                ensureName( gs );
                updateScreen( gs, false );
            } else {
                removeGameState( gs );
            }
        } else {
            XP_LOGFF( "ERROR: no saved data for key %s", key );
            XP_ASSERT( globals->games == gs );
            globals->games = gs->next;
            XP_FREE( globals->mpool, gs );
            gs = NULL;
        }
        stream_destroy( stream, NULL );
    }
    XP_LOGFF( "(%X) => %p", gameID, gs );
    return gs;
}

static void
saveName( GameState* gs )
{
    char key[32];
    formatNameKey( key, sizeof(key), gs->gi.gameID );
    set_stored_value( key, gs->gameName );
    XP_LOGFF( "wrote %s => %s", key, gs->gameName );
}

static void
loadName( GameState* gs )
{
    char key[32];
    formatNameKey( key, sizeof(key), gs->gi.gameID );
    const char* ptr = get_stored_value( key );
    if ( !!ptr ) {
        snprintf( gs->gameName, sizeof(gs->gameName),
                  "%s", ptr );
        free( (void*)ptr );
    }
}

static void
ensureName( GameState* gs )
{
    if ( '\0' == gs->gameName[0] ) {
        nameGame( gs );
        saveName( gs );
    }
}

static void
nameGame( GameState* gs )
{
    snprintf( gs->gameName, sizeof(gs->gameName),
              "Game %d", getNextGameNo() );
    XP_LOGFF( "named game: %s", gs->gameName );
}

static void
loadAndDraw( Globals* globals, const NetLaunchInfo* invite,
             const char* gameIDStr, NewGameParams* params )
{
    XP_LOGFF( "(gameIDStr: %s)", gameIDStr );
    GameState* gs = NULL;

    bool haveGame;
    if ( !params ) {
        /* First, load any saved game. We need it e.g. to confirm that an incoming
           invite is a dup and should be dropped. */
        if ( !!gameIDStr ) {
            int gameID;
            unformatGameID( &gameID, gameIDStr );
            gs = getSavedGame( globals, gameID );
        }
        if ( !!invite ) {   /* overwrite gs is ok: we'll likely want both games  */
            gs = gameFromInvite( globals, invite );
        }
    }

    if ( !gs ) {
        gs = newGameState( globals );
        gs->gi.serverRole = !!params && !params->isRobotNotRemote
            ? SERVER_ISSERVER : SERVER_STANDALONE;

        gs->gi.phoniesAction = PHONIES_WARN;
        gs->gi.hintsNotAllowed = !!params && params->hintsNotAllowed || false;
        gs->gi.gameID = 0;
        gs->gi.dictLang = 1; /* English only for now */
        replaceStringIfDifferent( globals->mpool, &gs->gi.dictName,
                                  "CollegeEng_2to8" );
        gs->gi.nPlayers = 2;
        gs->gi.boardSize = 15;
        gs->gi.players[0].name = copyString( globals->mpool, "Player 1" ); /* FIXME */
        gs->gi.players[0].isLocal = XP_TRUE;
        gs->gi.players[0].robotIQ = 0;

        gs->gi.players[1].name = copyString( globals->mpool, "Player 2" );
        gs->gi.players[1].isLocal = !!params ? params->isRobotNotRemote : true;
        XP_LOGFF( "set isLocal[1]: %d", gs->gi.players[1].isLocal );
        gs->gi.players[1].robotIQ = 99; /* doesn't matter if remote */

        gs->util = wasm_util_make( MPPARM(globals->mpool) &gs->gi,
                                           globals->dutil, gs );

        XP_LOGFF( "calling game_makeNewGame()" );
        game_makeNewGame( MPPARM(globals->mpool) NULL,
                          &gs->game, &gs->gi,
                          gs->util, globals->draw,
                          &globals->cp, &globals->procs );

        ensureName( gs );
        if ( !!gs->game.comms ) {
            CommsAddrRec addr = {0};
            makeSelfAddr( globals, &addr );
            comms_augmentHostAddr( gs->game.comms, NULL, &addr );
        }
    }
    startGame( gs, NULL );
    LOG_RETURN_VOID();
}

static bool
isVisible( GameState* gs )
{
    return getCurGame(gs->globals) == gs;
}

void
main_gameFromInvite( Globals* globals, const NetLaunchInfo* invite )
{
    GameState* gs = gameFromInvite( globals, invite );
    if ( gs ) {
        startGame( gs, NULL );
    }
}

typedef struct _OpenForMessageState {
    Globals* globals;
    int gameID;
    CommsAddrRec from;
    XWStreamCtxt* stream;
} OpenForMessageState;

void
main_onGameMessage( Globals* globals, XP_U32 gameID,
                    const CommsAddrRec* from, XWStreamCtxt* stream )
{
    GameState* gs = getSavedGame( globals, gameID );
    if ( !!gs ) {
        if ( game_receiveMessage( &gs->game, NULL, stream, from ) ) {
            updateScreen( gs, true );
        }
        if ( gs != getCurGame(gs->globals) ) {
        }
    } else {
        char msg[128];
        snprintf( msg, sizeof(msg), "Dropping move for deleted game (id: %X)", gameID );
        call_alert( msg );
    }
}

void
main_onGameGone( Globals* globals, XP_U32 gameID )
{
    GameState* gs = getSavedGame( globals, gameID );
    if ( !!gs ) {
        const char* msg = "This game has been deleted on the remote device. "
            "Delete here too?";
        call_confirm( globals, msg, onDeleteConfirmed, gs );
    }
}

void
main_sendOnClose( XWStreamCtxt* stream, XWEnv env, void* closure )
{
    CAST_GS(GameState*, gs, closure );
    XP_LOGFF( "called with msg of len %d", stream_getSize(stream) );
    (void)comms_send( gs->game.comms, NULL, stream );
}

void
main_playerScoreHeld( GameState* gs, XP_U16 player )
{
    LastMoveInfo lmi;
    XP_UCHAR buf[128];
    if ( model_getPlayersLastScore( gs->game.model, NULL, player, &lmi ) ) {
        switch ( lmi.moveType ) {
        case ASSIGN_TYPE:
            XP_SNPRINTF( buf, sizeof(buf), "Tiles assigned to %s", lmi.names[0] );
            break;
        case MOVE_TYPE:
            XP_SNPRINTF( buf, sizeof(buf), "%s formed %s for %d points", lmi.names[0],
                         lmi.word, lmi.score );
            break;
        case TRADE_TYPE:
            XP_SNPRINTF( buf, sizeof(buf), "%s traded %d tiles", lmi.names[0],
                         lmi.nTiles );
            break;
        default:
            buf[0] = '\0';
        }
    }

    if ( buf[0] ) {
        call_alert( buf );
    }
}

void
main_showRemaining( GameState* gs )
{
    XWStreamCtxt* stream = mem_stream_make_raw( MPPARM(gs->globals->mpool)
                                                gs->globals->vtMgr );
    board_formatRemainingTiles( gs->game.board, NULL, stream );
    stream_putU8( stream, 0 );
    call_alert( (const XP_UCHAR*)stream_getPtr( stream ) );
    stream_destroy( stream, NULL );
}

static void
openConfirmed(void* closure, bool confirmed)
{
    if ( confirmed ) {
        CAST_GS(GameState*, gs, closure);
        char key[16];
        formatGameID( key, sizeof(key), gs->gi.gameID );
        loadAndDraw( gs->globals, NULL, key, NULL );
    }
}

void
main_turnChanged( GameState* gs, int newTurn )
{
    if ( 0 <= newTurn && !isVisible(gs) && gs->gi.players[newTurn].isLocal ) {
        char msg[128];
        snprintf( msg, sizeof(msg),
                  "It's your turn in background game \"%s\". Would you like to open it now?",
                  gs->gameName );
        call_confirm( gs->globals, msg, openConfirmed, gs );
    }
}

typedef struct _BlankPickState {
    GameState* gs;
    int col, row;
    int nTiles;
    int playerNum;
    char** faces;
} BlankPickState;

static void
onBlankPicked( void* closure, const char* face )
{
    XP_LOGFF( "face: %s", face );
    BlankPickState* bps = (BlankPickState*)closure;
    GameState* gs = bps->gs;
    Globals* globals = gs->globals;

    int indx = -1;
    for ( int ii = 0; ii < bps->nTiles; ++ii ) {
        char* oneFace = bps->faces[ii];
        if ( indx < 0 && 0 == strcmp( face, oneFace ) ) {
            indx = ii;
        }
        XP_FREE( globals->mpool, oneFace );
    }
    XP_FREE( globals->mpool, bps->faces );

    if ( board_setBlankValue( gs->game.board, bps->playerNum,
                              bps->col, bps->row, indx ) ) {
        updateScreen( gs, true );
    }

    XP_FREE( globals->mpool, bps );
}

void
main_pickBlank( GameState* gs, int playerNum, int col, int row,
                const char** tileFaces, int nTiles )
{
    BlankPickState* bps = XP_MALLOC( gs->globals->mpool, sizeof(*bps) );
    bps->gs = gs;
    bps->row = row;
    bps->col = col;
    bps->playerNum = playerNum;
    bps->nTiles = nTiles;
    bps->faces = XP_CALLOC( gs->globals->mpool, nTiles * sizeof(bps->faces[0]) );
    for ( int ii = 0; ii < nTiles; ++ii ) {
        replaceStringIfDifferent( gs->globals->mpool, &bps->faces[ii], tileFaces[ii] );
    }

    call_pickBlank( "Pick for your blank", tileFaces, nTiles,
                    onBlankPicked, bps );
}

void
main_showGameOver( GameState* gs )
{
    if ( isVisible(gs) ) {
        XWStreamCtxt* stream = mem_stream_make_raw( MPPARM(gs->globals->mpool)
                                                    gs->globals->vtMgr );
        server_writeFinalScores( gs->game.server, NULL, stream );
        stream_putU8( stream, 0 );
        call_alert( (const XP_UCHAR*)stream_getPtr( stream ) );
        stream_destroy( stream, NULL );
    }
}

static time_t
getCurMS()
{
    struct timeval tv;
    gettimeofday( &tv, NULL );
    time_t result = tv.tv_sec * 1000; /* convert to millis */
    result += tv.tv_usec / 1000;         /* convert to millis too */
    // LOG_RETURNF( "%x", result );
    return result;
}

void
main_clear_timer( GameState* gs, XWTimerReason why )
{
    XP_LOGFF( "why: %d" );
    // XP_ASSERT(0); fires when start new game
}

typedef struct _TimerClosure {
    GameState* gs;
    XWTimerReason why;
    XWTimerProc proc;
    void* closure;
} TimerClosure;

static void
onTimerFired( void* closure )
{
    LOG_FUNC();
    TimerClosure* tc = (TimerClosure*)closure;
    XP_Bool draw = (*tc->proc)( tc->closure, NULL, tc->why );
    if ( draw ) {
        updateScreen( tc->gs, true );
    }
    XP_FREE( tc->gs->globals->mpool, tc );
}

void
main_set_timer( GameState* gs, XWTimerReason why, XP_U16 when,
                XWTimerProc proc, void* closure )
{
    XP_LOGFF( "why: %d", why );
    TimerClosure* tc = XP_MALLOC( gs->globals->mpool, sizeof(*tc) );
    tc->gs = gs;
    tc->proc = proc;
    tc->closure = closure;
    tc->why = why;

    if ( 0 == when ) {
        when = 1;
    }
    when *= 1000;               /* convert to ms */

    jscallback_set( onTimerFired, tc, when );
}

typedef struct _QueryState {
    GameState* gs;
    QueryProc proc;
    void* closure;
} QueryState;

static void
onQueryCalled( void* closure, const char* button )
{
    QueryState* qs = (QueryState*)closure;
    bool ok = 0 == strcmp( button, BUTTON_OK );
    (*qs->proc)( qs->closure, ok );
    XP_FREE( qs->gs->globals->mpool, qs );
}

void
main_query( GameState* gs, const XP_UCHAR* query, QueryProc proc, void* closure )
{
    if ( isVisible(gs) ) {
        QueryState* qs = XP_MALLOC( gs->globals->mpool, sizeof(*qs) );
        qs->proc = proc;
        qs->closure = closure;
        qs->gs = gs;

        const char* buttons[] = { BUTTON_CANCEL, BUTTON_OK, NULL };
        call_dialog( query, buttons, onQueryCalled, qs );
    }
}

void
main_alert( GameState* gs, const XP_UCHAR* msg )
{
    if ( isVisible(gs) ) {
        call_alert( msg );
    }
}

typedef struct _IdleClosure {
    GameState* gs;
    IdleProc proc;
    void* closure;
} IdleClosure;

static void
onIdleFired( void* closure )
{
    LOG_FUNC();
    IdleClosure* ic = (IdleClosure*)closure;
    XP_Bool draw = (*ic->proc)(ic->closure);
    if ( draw ) {
        updateScreen( ic->gs, true );
    }
    XP_FREE( ic->gs->globals->mpool, ic );
}

void
main_set_idle( GameState* gs, IdleProc proc, void* closure )
{
    LOG_FUNC();
    IdleClosure* ic = XP_MALLOC( gs->globals->mpool, sizeof(*ic) );
    ic->gs = gs;
    ic->proc = proc;
    ic->closure = closure;

    jscallback_set( onIdleFired, ic, 0 );
}

static XP_Bool
checkForEvent( GameState* gs )
{
    XP_Bool handled;
    XP_Bool draw = XP_FALSE;
    BoardCtxt* board = gs->game.board;

    SDL_Event event;
    if ( SDL_PollEvent(&event) ) {
        switch ( event.type ) {
        case SDL_MOUSEBUTTONDOWN:
            draw = event.button.button == SDL_BUTTON_LEFT
                && board_handlePenDown( board, NULL,
                                        event.button.x, event.button.y,
                                        &handled );
            break;
        case SDL_MOUSEBUTTONUP:
            draw = event.button.button == SDL_BUTTON_LEFT
                && board_handlePenUp( board, NULL,
                                      event.button.x, event.button.y );
            break;
        case SDL_MOUSEMOTION:
            draw = board_handlePenMove( board, NULL,
                                        event.motion.x, event.motion.y );
            break;
        default:
            break;
        }
    }

    // XP_LOGFF( "draw: %d", draw );
    return draw;
}

static void
clearScreen( Globals* globals )
{
    SDL_RenderClear( globals->renderer );
    SDL_RenderPresent( globals->renderer );
}

static void
updateScreen( GameState* gs, bool doSave )
{
    Globals* globals = gs->globals;
    if ( gs == getCurGame(globals) ) {
        SDL_RenderClear( globals->renderer );
        if ( !!gs->game.board ) {
            board_draw( gs->game.board, NULL );
            wasm_draw_render( globals->draw, globals->renderer );
        }
        SDL_RenderPresent( globals->renderer );

        updateGameButtons( gs );
    } else {
        XP_LOGFF( "not drawing %s; not visible", gs->gameName );
    }

    /* Let's save state here too, though likely too often */
    if ( doSave ) {
        XWStreamCtxt* stream = mem_stream_make_raw( MPPARM(globals->mpool)
                                                    globals->vtMgr );
        game_saveToStream( &gs->game, NULL, &gs->gi,
                           stream, ++gs->saveToken );

        char buf[32];
        formatGameKeyInt( buf, sizeof(buf), gs->gi.gameID );
        dutil_storeStream( globals->dutil, NULL, buf, stream );
        stream_destroy( stream, NULL );
        game_saveSucceeded( &gs->game, NULL, gs->saveToken );
    }
}

void
main_updateScreen( GameState* gs )
{
    updateScreen( gs, true );
}

static void
looper( void* closure )
{
    Globals* globals = (Globals*)closure;
    GameState* gs = getCurGame( globals );
    if ( !!gs ) {
        if ( checkForEvent( gs ) ) {
            updateScreen( gs, true );
        }
    } else {
        XP_LOGFF( "no visible game" );
    }
}

static bool
inviteFromArgv( Globals* globals, NetLaunchInfo* nlip,
                int argc, const char** argv )
{
    LOG_FUNC();
    CurGameInfo gi = {0};
    CommsAddrRec addr = {0};
    MQTTDevID mqttDevID = 0;
    XP_U16 nPlayersH = 0;
    XP_U16 forceChannel = 0;
    const XP_UCHAR* gameName = NULL;
    const XP_UCHAR* inviteID = NULL;

    for ( int ii = 0; ii < argc; ++ii ) {
        const char* argp = argv[ii];
        char* param = strchr(argp, '=');
        if ( !param ) {         /* no '='? */
            continue;
        }
        char arg[8];
        int argLen = param - argp;
        XP_MEMCPY( arg, argp, argLen );
        arg[argLen] = '\0';
        ++param;                /* skip the '=' */

        if ( 0 == strcmp( "lang", arg ) ) {
            gi.dictLang = atoi(param);
        } else if ( 0 == strcmp( "np", arg ) ) {
            gi.nPlayers = atoi(param);
        } else if ( 0 == strcmp( "nh", arg ) ) {
            nPlayersH = atoi(param);
        } else if ( 0 == strcmp( "gid", arg ) ) {
            gi.gameID = atoi(param);
        } else if ( 0 == strcmp( "fc", arg ) ) {
            gi.forceChannel = atoi(param);
        } else if ( 0 == strcmp( "nm", arg ) ) {
            gameName = param;
        } else if ( 0 == strcmp( "id", arg ) ) {
            inviteID = param;
        } else if ( 0 == strcmp( "wl", arg ) ) {
            replaceStringIfDifferent( globals->mpool, &gi.dictName, param );
        } else if ( 0 == strcmp( "r2id", arg ) ) {
            if ( strToMQTTCDevID( param, &addr.u.mqtt.devID ) ) {
                addr_addType( &addr, COMMS_CONN_MQTT );
            } else {
                XP_LOGFF( "bad devid %s", param );
            }
        } else {
            XP_LOGFF( "dropping arg %s, param %s", arg, param );
        }
    }

    bool success = 0 < nPlayersH &&
        addr_hasType( &addr, COMMS_CONN_MQTT );

    if ( success ) {
        nli_init( nlip, &gi, &addr, nPlayersH, forceChannel );
        if ( !!gameName ) {
            nli_setGameName( nlip, gameName );
        }
        if ( !!inviteID ) {
            nli_setInviteID( nlip, inviteID );
        }
        LOGNLI( nlip );
    }
    gi_disposePlayerInfo( MPPARM(globals->mpool) &gi );
    LOG_RETURNF( "%d", success );
    return success;
}

static void
initNoReturn( int argc, const char** argv )
{
    time_t now = getCurMS();
    srandom( now );
    XP_LOGFF( "called(srandom( %x )", now );

    Globals globals = {0}; // calloc(1, sizeof(*globals));
#ifdef DEBUG
    globals._GUARD = GUARD_GLOB;
#endif

    NetLaunchInfo nli = {0};
    NetLaunchInfo* nlip = NULL;
    if ( inviteFromArgv( &globals, &nli, argc, argv ) ) {
        nlip = &nli;
    }

    SDL_Init( SDL_INIT_EVENTS );
    TTF_Init();

    SDL_CreateWindowAndRenderer( WINDOW_WIDTH, WINDOW_HEIGHT, 0,
                                 &globals.window, &globals.renderer );

    /* whip the canvas to background */
    SDL_SetRenderDrawColor( globals.renderer, 155, 155, 155, 255 );
    SDL_RenderClear( globals.renderer );

    initDeviceGlobals( &globals );

    const char* lastKey = get_stored_value( KEY_LAST_GID );
    XP_LOGFF( "loaded KEY_LAST_GID: %s", lastKey );
    loadAndDraw( &globals, nlip, lastKey, NULL );
    if ( !!lastKey ) {
        free( (void*)lastKey );
    }

    updateDeviceButtons( &globals );

    emscripten_set_main_loop_arg( looper, &globals, -1, 1 );
}

void
MQTTConnectedChanged( void* closure, bool connected )
{
    XP_LOGFF( "connected=%d", connected);
    if ( connected ) {
        CAST_GLOB(Globals*, globals, closure);
        GameState* gs = getCurGame( globals );
        if ( !!gs && !!gs->game.comms ) {
            comms_resendAll( gs->game.comms, NULL, COMMS_CONN_MQTT, XP_TRUE );
        }
    }
}

void
gotMQTTMsg( void* closure, int len, const uint8_t* msg )
{
    Globals* globals = (Globals*)closure;
    dvc_parseMQTTPacket( globals->dutil, NULL, msg, len );
}

void
onNewGame( void* closure, bool opponentIsRobot )
{
    Globals* globals = (Globals*)closure;
    XP_LOGFF( "isRobot: %d", opponentIsRobot );

    NewGameParams ngp = {0};
    ngp.isRobotNotRemote = opponentIsRobot;
    loadAndDraw( globals, NULL, NULL, &ngp );
}

/* Called from js with a proc and closure */
void
cbckVoid( JSCallback proc, void* closure )
{
    LOG_FUNC();
    (*proc)(closure);
}

/* Called from js with a proc, closure, and string */
void
cbckString( StringProc proc, void* closure, const char* str )
{
    if ( !!proc ) {
        (*proc)( closure, str );
    }
}

int
main( int argc, const char** argv )
{
    XP_LOGFF( "(argc=%d)", argc );
    initNoReturn( argc, argv );
    return 0;
}
