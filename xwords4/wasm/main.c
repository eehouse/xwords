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

#define KEY_LAST "cur_game"
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

static void loadAndDraw( Globals* globals, const NetLaunchInfo* invite,
                         const char* key, NewGameParams* params );
static void nameGame( Globals* globals );
static void ensureName( Globals* globals );
static void loadName( Globals* globals );
static void saveName( Globals* globals );


typedef void (*StringProc)(void* closure, const char* str);

/* typedef struct _Buttons { */
/*     int nButtons; */
/*     const char** buttons; */
/* } Buttons; */

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

static void updateScreen( Globals* globals, bool doSave );

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
        Globals* globals = (Globals*)closure;
        CommsAddrRec myAddr = {0};
        makeSelfAddr( globals, &myAddr );

        NetLaunchInfo nli = {0};    /* include everything!!! */
        nli_init( &nli, &globals->gs.gi, &myAddr, 1, 1 );

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
        Globals* globals = (Globals*)closure;

        XP_Bool draw = XP_FALSE;
        BoardCtxt* board = globals->gs.game.board;
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
            globals->cp.tvType = (globals->cp.tvType + 1) % TVT_N_ENTRIES;
            draw = board_prefsChanged( board, &globals->cp );
        } else if ( 0 == strcmp(button, BUTTON_INVITE) ) {
            call_get_string( "Invitee's MQTT Device ID?", "",
                             onGotInviteeID, globals );
        }

        if ( draw ) {
            updateScreen( globals, true );
        }
    }
}

static void
updateGameButtons( Globals* globals )
{
    const char* buttons[MAX_BUTTONS];
    int cur = 0;

    if ( !!globals->gs.util ) {
        XP_U16 nPending = server_getPendingRegs( globals->gs.game.server );
        if ( 0 < nPending ) {
            buttons[cur++] = BUTTON_INVITE;
        } else {
            GameStateInfo gsi;
            game_getState( &globals->gs.game, NULL, &gsi );

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

    setButtons( BUTTONS_ID_GAME, buttons, onGameButton, globals );
}

static void
onGameChosen( void* closure, const char* key )
{
    Globals* globals = (Globals*)closure;
    loadAndDraw( globals, NULL, key, NULL );
}

static void
onGameRanamed( void* closure, const char* newName )
{
    if ( !!newName ) {
        Globals* globals = (Globals*)closure;
        snprintf( globals->gs.gameName, sizeof(globals->gs.gameName) - 1,
                  "%s", newName );
        // be safe
        globals->gs.gameName[sizeof(globals->gs.gameName)-1] = '\0';
        saveName( globals );
    }
}

static void
cleanupGame( Globals* globals )
{
    if ( !!globals->gs.util ) {
        game_dispose( &globals->gs.game, NULL );
        gi_disposePlayerInfo( MPPARM(globals->mpool) &globals->gs.gi );
        wasm_util_destroy( globals->gs.util );
        XP_MEMSET( &globals->gs, 0, sizeof(globals->gs) );
    }
}

static void
deleteCurGame( Globals* globals )
{
    int gameID = globals->gs.gi.gameID; /* remember it */
    cleanupGame( globals );

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
        Globals* globals = (Globals*)closure;
        deleteCurGame( globals );
        updateScreen( globals, false );
    }
}

static void
onDeviceButton( void* closure, const char* button )
{
    Globals* globals = (Globals*)closure;
    XP_LOGFF( "(button=%s)", button );
    if ( 0 == strcmp(button, BUTTON_GAME_NEW) ) {
        callNewGame("Configure your new game", globals);
    } else if ( 0 == strcmp(button, BUTTON_GAME_OPEN) ) {
        const char* msg = "Choose game to open";
        call_pickGame( msg, onGameChosen, globals);
    } else if ( 0 == strcmp(button, BUTTON_GAME_RENAME ) ) {
        ensureName( globals );
        call_get_string( "Rename your game", globals->gs.gameName,
                         onGameRanamed, globals );
    } else if ( 0 == strcmp(button, BUTTON_GAME_DELETE) ) {
        char msg[256];
        snprintf( msg, sizeof(msg), "Are you sure you want to delete the game \"%s\"?"
                  "\nThis action cannot be undone.",
                  globals->gs.gameName );
        call_confirm( globals, msg, onDeleteConfirmed, globals );
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
startGame( Globals* globals, const char* name )
{
    LOG_FUNC();
    BoardDims dims;
    board_figureLayout( globals->gs.game.board, NULL, &globals->gs.gi,
                        WASM_BOARD_LEFT, WASM_HOR_SCORE_TOP, BDWIDTH, BDHEIGHT,
                        110, 150, 200, BDWIDTH-25, BDWIDTH/15, BDHEIGHT/15,
                        XP_FALSE, &dims );
    XP_LOGFF( "calling board_applyLayout" );
    board_applyLayout( globals->gs.game.board, NULL, &dims );
    XP_LOGFF( "calling model_setDictionary" );
    model_setDictionary( globals->gs.game.model, NULL, globals->dict );

    if ( SERVER_ISCLIENT == globals->gs.gi.serverRole ) {
        if ( !!name ) {
            replaceStringIfDifferent( globals->mpool,
                                      &globals->gs.gi.players[0].name,
                                      name );
        }
        server_initClientConnection( globals->gs.game.server, NULL );
    }
    
    (void)server_do( globals->gs.game.server, NULL ); /* assign tiles, etc. */
    if ( !!globals->gs.game.comms ) {
        comms_resendAll( globals->gs.game.comms, NULL, COMMS_CONN_MQTT, XP_TRUE );
    }

    updateScreen( globals, true );
    LOG_RETURN_VOID();
}

typedef struct _AskReplaceState {
    Globals* globals;
    NetLaunchInfo invite;
} AskReplaceState;

static void
onPlayerNamed( void* closure, const char* name )
{
    Globals* globals = (Globals*)closure;
    if ( !!name ) {
        set_stored_value( KEY_PLAYER_NAME, name );
        startGame( globals, name );
    }
}

static void
doReplace( Globals* globals, const NetLaunchInfo* invite )
{
    cleanupGame( globals );

    gi_disposePlayerInfo( MPPARM(globals->mpool) &globals->gs.gi );
    XP_MEMSET( &globals->gs.gi, 0, sizeof(globals->gs.gi) );

    globals->gs.util = wasm_util_make( MPPARM(globals->mpool) &globals->gs.gi,
                                       globals->dutil, globals );

    game_makeFromInvite( MPPARM(globals->mpool) NULL, invite,
                         &globals->gs.game, &globals->gs.gi,
                         globals->dict, NULL,
                         globals->gs.util, globals->draw,
                         &globals->cp, &globals->procs );
    ensureName( globals );

    const char* name = get_stored_value( KEY_PLAYER_NAME );
    if ( NULL != name ) {
        startGame( globals, name );
        free( (void*)name );
    } else {
        const char* msg = "Please enter your name so you opponent knows it's you";
        call_get_string( msg, "Player 1", onPlayerNamed, globals );
    }
}

static void
onReplaceConfirmed( void* closure, bool confirmed )
{
    AskReplaceState* ars = (AskReplaceState*)closure;
    Globals* globals = ars->globals;

    if ( confirmed ) {
        doReplace( globals, &ars->invite );
    }

    XP_FREE( globals->mpool, ars );
}

static bool
gameFromInvite( Globals* globals, const NetLaunchInfo* invite )
{
    bool needsLoad = true;

    if ( NULL != globals->gs.game.model ) {
        /* there's a current game. Ignore the invitation if it has the same
           gameID. Otherwise ask to replace */
        if ( globals->gs.gi.gameID == invite->gameID ) {
            call_alert( "Duplicate invitation: game already open" );
            needsLoad = false;
        } else {
            AskReplaceState* ars = XP_MALLOC( globals->mpool, sizeof(*ars) );
            ars->globals = globals;
            ars->invite = *invite;
            call_confirm( globals, "Invitation received; replace current game?",
                          onReplaceConfirmed, ars );
            needsLoad = false;
        }
    } else if ( invite->lang != 1 ) {
        call_alert( "Invitations are only supported for play in English right now." );
        needsLoad = false;
    } else {
        // No game open. Just do it
        doReplace( globals, invite );
    }

    bool loaded = !needsLoad;
    LOG_RETURNF( "%d", loaded );
    return loaded;
}

static bool
loadSavedGame( Globals* globals, const char* key )
{
    XP_LOGFF( "key: %s", key );
    bool loaded = false;
    XWStreamCtxt* stream = mem_stream_make_raw( MPPARM(globals->mpool)
                                                globals->vtMgr );
    char buf[32];
    formatGameKeyStr( buf, sizeof(buf), key );
    dutil_loadStream( globals->dutil, NULL, buf, NULL, stream );
    if ( 0 < stream_getSize( stream ) ) {
        XP_ASSERT( !globals->gs.util );
        globals->gs.util = wasm_util_make( MPPARM(globals->mpool) &globals->gs.gi,
                                           globals->dutil, globals );

        XP_LOGFF( "there's a saved game!!" );
        loaded = game_makeFromStream( MPPARM(globals->mpool) NULL, stream,
                                      &globals->gs.game, &globals->gs.gi,
                                      globals->dict, NULL,
                                      globals->gs.util, globals->draw,
                                      &globals->cp, &globals->procs );

        if ( loaded ) {
            loadName( globals );
            ensureName( globals );
            updateScreen( globals, false );
        }
    } else {
        XP_LOGFF( "ERROR: no saved data for key %s", key );
    }
    stream_destroy( stream, NULL );
    return loaded;
}

static void
saveName( Globals* globals )
{
    char key[32];
    formatNameKey( key, sizeof(key), globals->gs.gi.gameID );
    set_stored_value( key, globals->gs.gameName );
    XP_LOGFF( "wrote %s => %s", key, globals->gs.gameName );
}

static void
loadName( Globals* globals )
{
    char key[32];
    formatNameKey( key, sizeof(key), globals->gs.gi.gameID );
    const char* ptr = get_stored_value( key );
    if ( !!ptr ) {
        snprintf( globals->gs.gameName, sizeof(globals->gs.gameName),
                  "%s", ptr );
        free( (void*)ptr );
    }
}

static void
ensureName( Globals* globals )
{
    if ( '\0' == globals->gs.gameName[0] ) {
        nameGame( globals );
        saveName( globals );
    }
}

static void
nameGame( Globals* globals )
{
    snprintf( globals->gs.gameName, sizeof(globals->gs.gameName),
              "Game %d", getNextGameNo() );
    XP_LOGFF( "named game: %s", globals->gs.gameName );
}

static void
loadAndDraw( Globals* globals, const NetLaunchInfo* invite,
             const char* key, NewGameParams* params )
{
    cleanupGame( globals );

    bool haveGame;
    if ( !!params ) {
        haveGame = false;
    } else {
        /* First, load any saved game. We need it e.g. to confirm that an incoming
           invite is a dup and should be dropped. */
        if ( !!key ) {
            haveGame = loadSavedGame( globals, key );
        }
        if ( !!invite ) {
            haveGame = gameFromInvite( globals, invite );
        }
    }

    if ( !haveGame ) {
        globals->gs.gi.serverRole = !!params && !params->isRobotNotRemote
            ? SERVER_ISSERVER : SERVER_STANDALONE;

        globals->gs.gi.phoniesAction = PHONIES_WARN;
        globals->gs.gi.hintsNotAllowed = !!params && params->hintsNotAllowed || false;
        globals->gs.gi.gameID = 0;
        globals->gs.gi.dictLang = 1; /* English only for now */
        replaceStringIfDifferent( globals->mpool, &globals->gs.gi.dictName,
                                  "CollegeEng_2to8" );
        globals->gs.gi.nPlayers = 2;
        globals->gs.gi.boardSize = 15;
        globals->gs.gi.players[0].name = copyString( globals->mpool, "Player 1" ); /* FIXME */
        globals->gs.gi.players[0].isLocal = XP_TRUE;
        globals->gs.gi.players[0].robotIQ = 0;

        globals->gs.gi.players[1].name = copyString( globals->mpool, "Player 2" );
        globals->gs.gi.players[1].isLocal = !!params ? params->isRobotNotRemote : true;
        XP_LOGFF( "set isLocal[1]: %d", globals->gs.gi.players[1].isLocal );
        globals->gs.gi.players[1].robotIQ = 99; /* doesn't matter if remote */

        globals->gs.util = wasm_util_make( MPPARM(globals->mpool) &globals->gs.gi,
                                           globals->dutil, globals );

        XP_LOGFF( "calling game_makeNewGame()" );
        game_makeNewGame( MPPARM(globals->mpool) NULL,
                          &globals->gs.game, &globals->gs.gi,
                          globals->gs.util, globals->draw,
                          &globals->cp, &globals->procs );
        ensureName( globals );
        if ( !!globals->gs.game.comms ) {
            CommsAddrRec addr = {0};
            makeSelfAddr( globals, &addr );
            comms_augmentHostAddr( globals->gs.game.comms, NULL, &addr );
        }
    }
    startGame( globals, NULL );
}

void
main_gameFromInvite( Globals* globals, const NetLaunchInfo* invite )
{
    if ( gameFromInvite( globals, invite ) ) {
        startGame( globals, NULL );
    }
}

typedef struct _OpenForMessageState {
    Globals* globals;
    int gameID;
    CommsAddrRec from;
    XWStreamCtxt* stream;
} OpenForMessageState;

static void
onOpenForMsgConfirmed( void* closure, bool confirmed )
{
    OpenForMessageState* ofm = (OpenForMessageState*)closure;
    Globals* globals = ofm->globals;
    if ( confirmed ) {

        char gameID[16];
        formatGameID( gameID, sizeof(gameID), ofm->gameID );
        loadAndDraw( globals, NULL, gameID, NULL );
        XP_ASSERT( globals->gs.gi.gameID == ofm->gameID );

        if ( game_receiveMessage( &globals->gs.game, NULL, ofm->stream, &ofm->from ) ) {
            updateScreen( globals, true );
        }
    }
    stream_destroy( ofm->stream, NULL );
    XP_FREE( globals->mpool, ofm );
}

void
main_onGameMessage( Globals* globals, XP_U32 gameID,
                    const CommsAddrRec* from, XWStreamCtxt* stream )
{
    if ( gameID == globals->gs.gi.gameID ) { /* current game open */
        if ( game_receiveMessage( &globals->gs.game, NULL, stream, from ) ) {
            updateScreen( globals, true );
        }
    } else {
        char key[32];
        formatGameKeyInt( key, sizeof(key), gameID );
        if ( have_stored_value( key ) ) {
            formatNameKey( key, sizeof(key), gameID );
            const char* name = get_stored_value( key );
            XP_ASSERT( !!name );
            char msg[128];
            snprintf( msg, sizeof(msg), "Move arrived for closed game \"%s\"; "
                      "Shall I open it?", name );
            free( (void*)name);

            OpenForMessageState* ofm = XP_MALLOC( globals->mpool, sizeof(*ofm) );
            ofm->globals = globals;
            ofm->gameID = gameID;
            ofm->from = *from;
            ofm->stream = stream_ref( stream );

            call_confirm( globals, msg, onOpenForMsgConfirmed, ofm );
        } else {
            XWStreamCtxt* stream = mem_stream_make_raw( MPPARM(globals->mpool)
                                                        globals->vtMgr );
            dvc_makeMQTTNoSuchGame( globals->dutil, NULL, stream, gameID );
            sendStreamToDev( stream, &from->u.mqtt.devID );

            call_alert( "Dropping move for deleted game" );
        }
    }
}

void
main_onGameGone( Globals* globals, XP_U32 gameID )
{
    LOG_FUNC();
    const char* msg = "This game has been deleted on the remote device. "
        "Delete here too?";
    call_confirm( globals, msg, onDeleteConfirmed, globals );
}

void
main_sendOnClose( XWStreamCtxt* stream, XWEnv env, void* closure )
{
    Globals* globals = (Globals*)closure;
    XP_LOGFF( "called with msg of len %d", stream_getSize(stream) );
    (void)comms_send( globals->gs.game.comms, NULL, stream );
}

void
main_playerScoreHeld( Globals* globals, XP_U16 player )
{
    LastMoveInfo lmi;
    XP_UCHAR buf[128];
    if ( model_getPlayersLastScore( globals->gs.game.model, NULL, player, &lmi ) ) {
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
main_showRemaining( Globals* globals )
{
    XWStreamCtxt* stream = mem_stream_make_raw( MPPARM(globals->mpool)
                                                globals->vtMgr );
    board_formatRemainingTiles( globals->gs.game.board, NULL, stream );
    stream_putU8( stream, 0 );
    call_alert( (const XP_UCHAR*)stream_getPtr( stream ) );
    stream_destroy( stream, NULL );
}

typedef struct _BlankPickState {
    Globals* globals;
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
    Globals* globals = bps->globals;

    int indx = -1;
    for ( int ii = 0; ii < bps->nTiles; ++ii ) {
        char* oneFace = bps->faces[ii];
        if ( indx < 0 && 0 == strcmp( face, oneFace ) ) {
            indx = ii;
        }
        XP_FREE( globals->mpool, oneFace );
    }
    XP_FREE( globals->mpool, bps->faces );

    if ( board_setBlankValue( globals->gs.game.board, bps->playerNum,
                              bps->col, bps->row, indx ) ) {
        updateScreen( globals, true );
    }

    XP_FREE( bps->globals->mpool, bps );
}

void
main_pickBlank( Globals* globals, int playerNum, int col, int row,
                const char** tileFaces, int nTiles )
{
    BlankPickState* bps = XP_MALLOC( globals->mpool, sizeof(*bps) );
    bps->globals = globals;
    bps->row = row;
    bps->col = col;
    bps->playerNum = playerNum;
    bps->nTiles = nTiles;
    bps->faces = XP_CALLOC( globals->mpool, nTiles * sizeof(bps->faces[0]) );
    for ( int ii = 0; ii < nTiles; ++ii ) {
        replaceStringIfDifferent( globals->mpool, &bps->faces[ii], tileFaces[ii] );
    }

    call_pickBlank( "Pick for your blank", tileFaces, nTiles,
                    onBlankPicked, bps );
}

void
main_showGameOver( Globals* globals )
{
    XWStreamCtxt* stream = mem_stream_make_raw( MPPARM(globals->mpool)
                                                globals->vtMgr );
    server_writeFinalScores( globals->gs.game.server, NULL, stream );
    stream_putU8( stream, 0 );
    call_alert( (const XP_UCHAR*)stream_getPtr( stream ) );
    stream_destroy( stream, NULL );
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
main_clear_timer( Globals* globals, XWTimerReason why )
{
    XP_LOGFF( "why: %d" );
    // XP_ASSERT(0); fires when start new game
}

typedef struct _TimerClosure {
    Globals* globals;
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
        updateScreen( tc->globals, true );
    }
    XP_FREE( tc->globals->mpool, tc );
}

void
main_set_timer( Globals* globals, XWTimerReason why, XP_U16 when,
                XWTimerProc proc, void* closure )
{
    XP_LOGFF( "why: %d", why );
    TimerClosure* tc = XP_MALLOC( globals->mpool, sizeof(*tc) );
    tc->globals = globals;
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
    Globals* globals;
    QueryProc proc;
    void* closure;
} QueryState;

static void
onQueryCalled( void* closure, const char* button )
{
    QueryState* qs = (QueryState*)closure;
    bool ok = 0 == strcmp( button, BUTTON_OK );
    (*qs->proc)( qs->closure, ok );
    XP_FREE( qs->globals->mpool, qs );
}

void
main_query( Globals* globals, const XP_UCHAR* query, QueryProc proc, void* closure )
{
    QueryState* qs = XP_MALLOC( globals->mpool, sizeof(*qs) );
    qs->proc = proc;
    qs->closure = closure;
    qs->globals = globals;

    const char* buttons[] = { BUTTON_CANCEL, BUTTON_OK, NULL };
    call_dialog( query, buttons, onQueryCalled, qs );
}

void
main_alert( Globals* globals, const XP_UCHAR* msg )
{
    call_alert( msg );
}

typedef struct _IdleClosure {
    Globals* globals;
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
        updateScreen( ic->globals, true );
    }
    XP_FREE( ic->globals->mpool, ic );
}

void
main_set_idle( Globals* globals, IdleProc proc, void* closure )
{
    LOG_FUNC();
    IdleClosure* ic = XP_MALLOC( globals->mpool, sizeof(*ic) );
    ic->globals = globals;
    ic->proc = proc;
    ic->closure = closure;

    jscallback_set( onIdleFired, ic, 0 );
}

static XP_Bool
checkForEvent( Globals* globals )
{
    XP_Bool handled;
    XP_Bool draw = XP_FALSE;
    BoardCtxt* board = globals->gs.game.board;

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
updateScreen( Globals* globals, bool doSave )
{
    SDL_RenderClear( globals->renderer );
    if ( !!globals->gs.game.board ) {
        board_draw( globals->gs.game.board, NULL );
        wasm_draw_render( globals->draw, globals->renderer );
    }
    SDL_RenderPresent( globals->renderer );

    updateGameButtons( globals );

    /* Let's save state here too, though likely too often */
    if ( doSave ) {
        XWStreamCtxt* stream = mem_stream_make_raw( MPPARM(globals->mpool)
                                                    globals->vtMgr );
        game_saveToStream( &globals->gs.game, NULL, &globals->gs.gi,
                           stream, ++globals->gs.saveToken );

        char buf[32];
        formatGameKeyInt( buf, sizeof(buf), globals->gs.gi.gameID );
        dutil_storeStream( globals->dutil, NULL, buf, stream );
        stream_destroy( stream, NULL );
        game_saveSucceeded( &globals->gs.game, NULL, globals->gs.saveToken );

        char gidBuf[16];
        formatGameID( gidBuf, sizeof(gidBuf), globals->gs.gi.gameID );
        set_stored_value( KEY_LAST, gidBuf );
        XP_LOGFF( "saved KEY_LAST: %s", gidBuf );
    }
}

void
main_updateScreen( Globals* globals )
{
    updateScreen( globals, true );
}

static void
looper( void* closure )
{
    Globals* globals = (Globals*)closure;
    if ( checkForEvent( globals ) ) {
        updateScreen( globals, true );
    }
}

static bool
loadInvite( Globals* globals, NetLaunchInfo* nlip,
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

    Globals* globals = calloc(1, sizeof(*globals));

    NetLaunchInfo nli = {0};
    NetLaunchInfo* nlip = NULL;
    if ( loadInvite( globals, &nli, argc, argv ) ) {
        nlip = &nli;
    }

    SDL_Init( SDL_INIT_EVENTS );
    TTF_Init();

    SDL_CreateWindowAndRenderer( WINDOW_WIDTH, WINDOW_HEIGHT, 0,
                                 &globals->window, &globals->renderer );

    /* whip the canvas to background */
    SDL_SetRenderDrawColor( globals->renderer, 155, 155, 155, 255 );
    SDL_RenderClear( globals->renderer );

    initDeviceGlobals( globals );

    const char* lastKey = get_stored_value( KEY_LAST );
    XP_LOGFF( "loaded KEY_LAST: %s", lastKey );
    loadAndDraw( globals, nlip, lastKey, NULL );
    if ( !!lastKey ) {
        free( (void*)lastKey );
    }

    updateDeviceButtons( globals );

    emscripten_set_main_loop_arg( looper, globals, -1, 1 );
}

void
MQTTConnectedChanged( void* closure, bool connected )
{
    XP_LOGFF( "connected=%d", connected);
    Globals* globals = (Globals*)closure;
    if ( connected && !!globals->gs.game.comms ) {
        comms_resendAll( globals->gs.game.comms, NULL, COMMS_CONN_MQTT, XP_TRUE );
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
