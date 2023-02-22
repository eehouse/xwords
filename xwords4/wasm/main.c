/* -*- compile-command: "cd ../wasm && make MEMDEBUG=TRUE install -j3"; -*- */
/*
 * Copyright 2021 - 2023 by Eric House (xwords@eehouse.org).  All rights
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

/* TODO
 *
 * Downloading wordlists. Will likely need to fetch and store them as base64
 * data, meaning I'll need a working btoa to use them. Here's one way to download:
 * https://emscripten.org/docs/api_reference/fetch.html
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
#include "knownplyr.h"
#include "dbgutil.h"

#include "main.h"
#include "wasmdraw.h"
#include "wasmutil.h"
#include "wasmdutil.h"
#include "wasmdict.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#define WASM_BOARD_LEFT 0
#define WASM_HOR_SCORE_TOP 0

#define KEY_LAST_GID "cur_game"
#define KEY_PLAYER_NAME "player_name"
#define KEY_GAMES "games"
#define KEY_SUMMARY "summary"
#define KEY_GAME "game_data"
#define KEY_NAME "game_name"
#define KEY_NEXT_GAME "next_game"
#define KEY_LANG_NAME "lang_name"
#define KEY_NEWGAME_DFLTS "ng_defaults"
#define KEY_PREFS "prefs"
#define KEY_COMMON "common"

#define BUTTON_OK "OK"
#define BUTTON_CANCEL "Cancel"
#define BUTTON_REPLY "Reply"

#define BUTTONS_ID_GAME "game_buttons"
#define BUTTONS_ID_DEVICE "device_buttons"

#define BUTTON_HINTDOWN "Prev Hint"
#define BUTTON_HINTUP "Next Hint"
#define BUTTON_TRADE "Trade"
#define BUTTON_STOPTRADE "Cancel Trade"
#define BUTTON_COMMIT "Commit"
// #define BUTTON_FLIP "Flip"
#define BUTTON_REDO "Redo"
#define BUTTON_VALS "Vals"
#define BUTTON_CHAT "Chat"
#define BUTTON_INVITE "Invite"

#define BUTTON_GAME_GAMES "Games"
#define BUTTON_GAME_NEW "New Game"
#define BUTTON_GAME_RENAME "Rename Game"
#define BUTTON_GAME_DELETE "Delete Game"
#define BUTTON_NAME "My Name"
#define MAX_BUTTONS 20          /* not sure what's safe here */

// I get a JS exception if I do this... So don't
// #define GLOBALS_ON_STACK

typedef struct _NewGameParams {
    Globals* globals;
    bool isRobot;
    bool allowHints;
    char langName[32];
} NewGameParams;

static void updateScreen( GameState* gs, bool doSave );
static void updateDeviceButtons( Globals* globals );
static void clearScreen( Globals* globals );
static GameState* newGameState( Globals* globals );
static GameState* getSavedGame( Globals* globals, int gameID );
static void loadAndDraw( Globals* globals, const NetLaunchInfo* invite,
                         int gameID, NewGameParams* params );
static GameState* getCurGame( Globals* globals );
static void nameGame( GameState* gs, const char* name );
static void ensureName( GameState* gs );
static void loadName( GameState* gs );
static void saveName( GameState* gs );
static bool isVisible( GameState* gs );
static int countLangs( Globals* globals );

typedef struct _GotDictData {
    const char* lc;
    const char* langName;
    const char* dictName;
    uint8_t* data;
    int len;
} GotDictData;

typedef void (*GotDictProc)(void* closure, GotDictData* data);

EM_JS(void, call_get_dict, (const char* lc, GotDictProc proc,
                            void* closure), {
        let langs;
        if (lc) {
            langs = [UTF8ToString(lc)];
        } else {
            langs = [navigator.language.split('-')[0]];
            if (langs[0] != 'en') {
                langs.push('en');
            }
        }
        getDict(langs, proc, closure);
    });

EM_JS(void, show_name, (const char* name), {
        let jsname = name ? UTF8ToString(name) : "";
        document.getElementById('gamename').textContent = jsname;
    });

EM_JS(void, show_pool, (int cur, int max), {
        const msg = 'cur: ' + cur + 'b; max: ' + max + 'b';
        const div = document.getElementById('mempool');
        div.textContent = msg;
        div.parentNode.hidden = 0;
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

EM_JS(void, call_pickGame, (const char* msg, char** ids, char** names,
                            int nEntries, StringProc proc, void* closure), {
        let map = {};
        for (let ii = 0; ii < nEntries; ++ii ) {
            const idsMem = HEAP32[(ids + (ii * 4)) >> 2];
            const namesMem = HEAP32[(names + (ii * 4)) >> 2];
            let id = UTF8ToString(idsMem);
            map[id] = UTF8ToString(namesMem);
        }

        nbGamePick(UTF8ToString(msg), map, proc, closure);
    } );

EM_JS(void, call_get_string, (const char* msg, const char* dflt,
                              StringProc proc, void* closure), {
          let jsMgs = UTF8ToString(msg);
          let jsDflt = UTF8ToString(dflt);
          nbGetString( jsMgs, jsDflt, proc, closure );
      } );

EM_JS(void, call_setup, (void* closure, bool dbg, const char* devid,
                         const char* gitrev, int now,
                         StringProc conflictProc,
                         StringProc focussedProc,
                         MsgProc msgProc), {
          let jsgr = UTF8ToString(gitrev);
          jssetup(closure, dbg, UTF8ToString(devid), jsgr, now,
                  conflictProc, focussedProc, msgProc);
      });

EM_JS(bool, call_mqttSend, (const char* topic, const uint8_t* ptr, int len), {
        let topStr = UTF8ToString(topic);
        let buffer = new Uint8Array(Module.HEAPU8.buffer, ptr, len);
        return mqttSend(topStr, buffer);
});

typedef void (*JSCallback)(void* closure);

EM_JS(void, jscallback_set, (JSCallback proc, void* closure, int inMS), {
        let timerproc = function(closure) {
            ccall('cbckVoid', null, ['number', 'number'], [proc, closure]);
        };
        setTimeout( timerproc, inMS, closure );
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

EM_JS(void, js_callNewGame, (const char* msg, void* closure,
                             bool allowHints, bool isRobot,
                             const char* langName, char** langs, int nLangs), {
          let jslangName = UTF8ToString(langName);
          let jsmsg = UTF8ToString(msg);

          let jlangs = [];
          for ( let ii = 0; ii < nLangs; ++ii ) {
              const mem = HEAP32[(langs + (ii * 4)) >> 2];
              let str = UTF8ToString(mem);
              jlangs.push(str);
          }
          nbGetNewGame(closure, jsmsg, allowHints, isRobot, jlangs, jslangName );
      });

/* Keep these in sync with ints used in js_getHaveNotifyPerm!!! */
typedef enum {UNSUPPORTED = 0,
              UNREQUESTED = 1,
              BLOCKED = 2,
              GRANTED = 3,
} NOTIFY_STATE;


EM_JS(int, js_getHaveNotifyPerm, (), {
        let state = 1; // UNREQUESTED;
        try {
            let asStr = Notification.permission;
            // console.error('permission: ', asStr);
            if ( asStr == 'granted' ) {
                state = 3; // GRANTED;
            } else if ( asStr == 'denied' ) {
                state = 2; // BLOCKED;
            } else {
                state = 1; // UNREQUESTED;
            }
        } catch (ex) {
            state = 0; // UNSUPPORTED
        }
        return state;
    });

EM_JS(void, js_requestNotify, (), {
        Notification.requestPermission();
    });

EM_JS( void, js_notify, (const char* msg), {
        const jsmsg = UTF8ToString(msg);
        new Notification('WASM CrossWords',
                         { body: jsmsg, renotify: true, }
                         );
    });

static void
loadPrefs( Globals* globals )
{
    const XP_UCHAR* keys[] = { KEY_PREFS, KEY_COMMON, NULL };
    XP_U32 len = sizeof(globals->cp);
    dutil_loadPtr( globals->dutil, NULL_XWE, keys, &globals->cp, &len );
}

static void
savePrefs( Globals* globals )
{
    const XP_UCHAR* keys[] = { KEY_PREFS, KEY_COMMON, NULL };
    dutil_storePtr( globals->dutil, NULL_XWE, keys, &globals->cp, sizeof(globals->cp) );
}

typedef struct _ConfirmState {
    Globals* globals;
    BoolProc proc;
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
              BoolProc proc, void* closure )
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

static void
onMsgAndTopic( void* closure, const XP_UCHAR* topic,
               const XP_U8* msgBuf, XP_U16 msgLen )
{
    /*bool success = */
    call_mqttSend( topic, msgBuf, msgLen );
}

static XP_S16
send_msg( XWEnv xwe, const XP_U8* buf, XP_U16 len,
          XP_U16 streamVersion, const XP_UCHAR* msgNo,
          XP_U32 createdStamp, const CommsAddrRec* addr,
          CommsConnType conType, XP_U32 gameID,
          void* closure )
{
    XP_S16 nSent = -1;
    Globals* globals = (Globals*)closure;

    if ( addr_hasType( addr, COMMS_CONN_MQTT ) ) {
        dvc_makeMQTTMessages( globals->dutil, NULL_XWE,
                              onMsgAndTopic, NULL,
                              &addr->u.mqtt.devID,
                              gameID, buf, len,
                              streamVersion );
    }
    LOG_RETURNF( "%d", nSent );
    return nSent;
}

static XP_S16
send_invite( XWEnv xwe, const NetLaunchInfo* nli,
             XP_U32 createdStamp, const CommsAddrRec* addr,
             CommsConnType conType, void* closure )
{
    Globals* globals = (Globals*)closure;
    dvc_makeMQTTInvites( globals->dutil, NULL_XWE,
                         onMsgAndTopic, NULL,
                         &addr->u.mqtt.devID, nli );
    return -1;
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
makeSelfAddr( Globals* globals, CommsAddrRec* addr )
{
    addr_setType( addr, COMMS_CONN_MQTT );
    dvc_getMQTTDevID( globals->dutil, NULL_XWE, &addr->u.mqtt.devID );
}

static void
sendInviteTo(GameState* gs, const MQTTDevID* remoteDevID)
{
    Globals* globals = gs->globals;
    CommsAddrRec myAddr = {0};
    makeSelfAddr( globals, &myAddr );

    NetLaunchInfo nli = {0};    /* include everything!!! */
    nli_init( &nli, &gs->gi, &myAddr, 1, 1 );
    nli_setGameName( &nli, gs->gameName );

    dvc_makeMQTTInvites( globals->dutil, NULL_XWE,
                         onMsgAndTopic, NULL,
                         remoteDevID, &nli );
}

static void
onGotInviteeID( void* closure, const char* mqttid )
{
    MQTTDevID remoteDevID;
    if ( strToMQTTCDevID( mqttid, &remoteDevID ) ) {
        CAST_GS(GameState*, gs, closure);
        sendInviteTo( gs, &remoteDevID );
    } else if (!!mqttid) {
        call_alert( "MQTT id looks badly formed" );
    }
}

typedef struct _KnownSelState {
    GameState* gs;
    const char** names;
    XP_U16 nNames;
} KnownSelState;

static void
onKnownSelected( void* closure, const char* name )
{
    XP_LOGFF( "(name=%s)", name );
    KnownSelState* kss = (KnownSelState*)closure;
    GameState* gs = kss->gs;
    if ( 0 == strcmp( BUTTON_CANCEL, name ) ) {
        call_get_string( "Invitee's MQTT Device ID?", "",
                         onGotInviteeID, gs );
    } else {
        CommsAddrRec addr;
        if ( kplr_getAddr( gs->globals->dutil, NULL_XWE, name, &addr, NULL ) ) {
            sendInviteTo(gs, &addr.u.mqtt.devID);
        }
    }

    XP_FREE( gs->globals->mpool, kss->names );
    XP_FREE( gs->globals->mpool, kss );
}

static void
handleInvite( GameState* gs )
{
    if ( isVisible(gs) ) {
        Globals* globals = gs->globals;
        XW_DUtilCtxt* dutil = globals->dutil;
        if ( kplr_havePlayers( dutil, NULL_XWE ) ) {
            KnownSelState* kss = XP_MALLOC( globals->mpool, sizeof(*kss) );
            kss->gs = gs;
            kss->nNames = 0;
            kplr_getNames( dutil, NULL_XWE, NULL, &kss->nNames );
            kss->names = XP_CALLOC( globals->mpool,
                                    (kss->nNames + 2) * sizeof(kss->names[0]) );
            kplr_getNames( dutil, NULL_XWE, kss->names, &kss->nNames );
            kss->names[kss->nNames] = BUTTON_CANCEL;
            XP_ASSERT( NULL == kss->names[kss->nNames+1] );
            const char* msg = "Pick an invitee you've played before, "
                "or cancel to enter manually.";
            call_dialog( msg, kss->names, onKnownSelected, kss );
        } else {
            call_get_string( "Invitee's MQTT Device ID?", "",
                             onGotInviteeID, gs );
        }
    }
}

static void
onChatComposed( void* closure, const char* msg )
{
    if ( !!msg ) {
        CAST_GS(GameState*, gs, closure);
        board_sendChat( gs->game.board, NULL_XWE, msg );
    }
}

static void
onGameButton( void* closure, const char* button )
{
    if ( !!button ) {
        CAST_GS(GameState*, gs, closure);
        Globals* globals = gs->globals;

        XP_Bool draw = XP_FALSE;
        BoardCtxt* board = gs->game.board;
        XP_Bool redo;

        if ( 0 == strcmp(button, BUTTON_HINTDOWN ) ) {
            draw = board_requestHint( board, NULL_XWE, XP_TRUE, &redo );
        } else if ( 0 == strcmp(button, BUTTON_HINTUP) ) {
            draw = board_requestHint( board, NULL_XWE, XP_FALSE, &redo );
        } else if ( 0 == strcmp(button, BUTTON_CHAT) ) {
            call_get_string( "Your message?", "", onChatComposed, gs );
        } else if ( 0 == strcmp(button, BUTTON_TRADE ) ) {
            wasm_draw_setInTrade( globals->draw, true );
            board_invalAll( gs->game.board );
            draw = board_beginTrade( board, NULL_XWE );
        } else if ( 0 == strcmp(button, BUTTON_STOPTRADE ) ) {
            wasm_draw_setInTrade( globals->draw, false );
            board_invalAll( gs->game.board );
            draw = board_endTrade( board );
        } else if ( 0 == strcmp(button, BUTTON_COMMIT) ) {
            draw = board_commitTurn( board, NULL_XWE, XP_FALSE, XP_FALSE, NULL );
        /* } else if ( 0 == strcmp(button, BUTTON_FLIP) ) { */
        /*     draw = board_flip( board ); */
        } else if ( 0 == strcmp(button, BUTTON_REDO) ) {
            draw = board_redoReplacedTiles( board, NULL_XWE )
                || board_replaceTiles( board, NULL_XWE );
        } else if ( 0 == strcmp(button, BUTTON_VALS) ) {
            globals->cp.tvType = (globals->cp.tvType + 1) % TVT_N_ENTRIES;
            savePrefs( globals );
            draw = board_prefsChanged( board, &globals->cp );
        } else if ( 0 == strcmp(button, BUTTON_INVITE) ) {
            handleInvite(gs);
        }

        if ( draw ) {
            updateScreen( gs, true );
        }
    }
}

static void
updateGameButtons( Globals* globals )
{
    GameState* gs = getCurGame(globals);
    const char* buttons[MAX_BUTTONS];
    int cur = 0;

    if ( gs && !!gs->util ) {
        XP_U16 nPending = server_getPendingRegs( gs->game.server );
        if ( 0 < nPending ) {
            buttons[cur++] = BUTTON_INVITE;
        } else {
            GameStateInfo gsi;
            game_getState( &gs->game, NULL_XWE, &gsi );

            if ( gsi.canHint ) {
                buttons[cur++] = BUTTON_HINTDOWN;
                buttons[cur++] = BUTTON_HINTUP;
            }

            if ( gsi.canChat ) {
                buttons[cur++] = BUTTON_CHAT;
            }

            if ( gsi.inTrade ) {
                buttons[cur++] = BUTTON_STOPTRADE;
            } else if ( gsi.canTrade ) {
                buttons[cur++] = BUTTON_TRADE;
            }
            if ( gsi.curTurnSelected ) {
                buttons[cur++] = BUTTON_COMMIT;
            }
            /* buttons[cur++] = BUTTON_FLIP; */

            if ( gsi.canRedo ) {
                buttons[cur++] = BUTTON_REDO;
            }

            buttons[cur++] = BUTTON_VALS;
        }
    }

    buttons[cur++] = NULL;
    setButtons( BUTTONS_ID_GAME, buttons, onGameButton, gs );
}

static bool
langNameFor( Globals* globals, const char* lc, char buf[], size_t buflen )
{
    /* const char* lc = lcToLocale( code ); */
    const XP_UCHAR* keys[] = { KEY_DICTS, lc, KEY_LANG_NAME, NULL };
    XP_U32 len = buflen;
    dutil_loadPtr( globals->dutil, NULL_XWE, keys, buf, &len );
    return 0 < len;
}

static void
showName( GameState* gs )
{
    const char* title = NULL;
    if ( !!gs ) {
        Globals* globals = gs->globals;
        title = gs->gameName;
        char buf[64];
        if ( 1 < countLangs( globals ) ) {
            char langName[32];
            if ( !langNameFor( globals, gs->gi.isoCodeStr, langName, sizeof(langName) ) ) {
                strcpy( langName, "??" );
            }
            sprintf( buf, "%s (%s)", title, langName );
            title = buf;
        }
    }
    show_name( title );
}

static void
onGameRanamed( void* closure, const char* newName )
{
    if ( !!newName ) {
        CAST_GS(GameState*, gs, closure);
        nameGame( gs, newName );
        showName( gs );
    }
}

typedef struct _NameIterState {
    Globals* globals;
    int count;
    char** names;
    char** ids;
} NameIterState;

static void
onGameChosen( void* closure, const char* key )
{
    CAST_GLOB(Globals*, globals, closure);

    /* To be safe, let's make sure the game exists. We don't want to create
     * another if somehow it doesn't */
    int gameID;
    unformatGameID( &gameID, key );
    if ( !!getSavedGame(globals, gameID) ) {
        loadAndDraw( globals, NULL, gameID, NULL );
    }

    updateDeviceButtons( globals );
}

static char*
formatForGame(Globals* globals, bool multiLangs, const XP_UCHAR* gameKey )
{
    const XP_UCHAR* keys[] = {KEY_GAMES, gameKey, KEY_NAME, NULL};
    char gameName[32];
    XP_U32 len = sizeof(gameName);
    dutil_loadPtr( globals->dutil, NULL_XWE, keys, gameName, &len );

    char buf[256];
    int offset = snprintf( buf, sizeof(buf), "%s", gameName );

    GameSummary summary;
    len = sizeof(summary);
    keys[2] = KEY_SUMMARY;
    dutil_loadPtr( globals->dutil, NULL_XWE, keys, &summary, &len );
    if ( len == sizeof(summary) ) {
        if ( multiLangs ) {
            char langName[32];
            if ( langNameFor( globals, summary.isoCodeStr, langName, sizeof(langName) ) ) {
                offset += snprintf( buf+offset, sizeof(buf)-offset, " (in %s)", langName );
            }
        }
        bool inPlay = 0 <= summary.turn;
        if ( inPlay ) {
            offset += snprintf( buf+offset, sizeof(buf)-offset, " Opponent: %s",
                                summary.opponents );
        }

        if ( summary.gameOver ) {
            offset += snprintf( buf+offset, sizeof(buf)-offset, " Game over" );
        } else if ( !inPlay ) {
            offset += snprintf( buf+offset, sizeof(buf)-offset, " Game NOT in play" );
        } else {
            offset += snprintf( buf+offset, sizeof(buf)-offset, " My turn: %s",
                                0 <= summary.turn && summary.turnIsLocal ? "YES" : "NO" );
        }
    }
    char* result = NULL;
    replaceStringIfDifferent( globals->mpool, &result, buf );
    return result;
}

static XP_Bool
onOneGameName( void* closure, const XP_UCHAR* keys[] )
{
    const char* gameIDStr = keys[1];
    if ( 0 != strcmp( gameIDStr, "0" ) ) { /* temporary */
        NameIterState* nis = (NameIterState*)closure;
        Globals* globals = nis->globals;
        bool multiLangs = 1 < countLangs(globals);

        /* Make sure game exists. This may be unnecessary later */
        const XP_UCHAR* dataKeys[] = { keys[0], keys[1], KEY_GAME, NULL };
        XP_U32 dataLen;
        dutil_loadPtr( globals->dutil, NULL_XWE, dataKeys, NULL, &dataLen );
        if ( 0 < dataLen ) {
            int cur = nis->count++;
            nis->names = XP_REALLOC( globals->mpool, nis->names,
                                     nis->count * sizeof(nis->names[0]) );
            nis->names[cur] = formatForGame( globals, multiLangs, keys[1] );
            nis->ids = XP_REALLOC( globals->mpool, nis->ids,
                                   nis->count * sizeof(nis->ids[0]) );
            nis->ids[cur] = XP_MALLOC( globals->mpool, 1 + strlen(gameIDStr) );
            strcpy( nis->ids[cur], gameIDStr );
        }
    }
    return true;                /* keep going */
}

static void
pickGame( Globals* globals )
{
    XW_DUtilCtxt* dutil = globals->dutil;
    NameIterState nis = { .globals = globals, };

    const XP_UCHAR* keys[] = {KEY_GAMES, KEY_WILDCARD, KEY_NAME, NULL};
    dutil_forEach( dutil, NULL_XWE, keys, onOneGameName, &nis );

    const char* msg = "Choose game to open:";
    call_pickGame(msg, nis.ids, nis.names, nis.count, onGameChosen, globals);

    for ( int ii = 0; ii < nis.count; ++ii ) {
        XP_FREE( globals->mpool, nis.names[ii] );
        XP_FREE( globals->mpool, nis.ids[ii] );
    }
    XP_FREE( globals->mpool, nis.names );
    XP_FREE( globals->mpool, nis.ids );
}

static void
cleanupGame( GameState* gs )
{
    if ( !!gs->util ) {
        game_dispose( &gs->game, NULL_XWE );
        gi_disposePlayerInfo( MPPARM(gs->globals->mpool) &gs->gi );
        wasm_util_destroy( gs->util );
        gs->util = NULL;
        // XP_MEMSET( &gs, 0, sizeof(globals->gs) );
    }
}

static void
removeGameState( GameState* gs )
{
    Globals* globals = gs->globals;
    GameState** prev = &globals->games;
    for ( GameState* cur = globals->games; !!cur; cur = cur->next ) {
        if ( gs == cur ) {
            *prev = cur->next;
            break;
        }
        prev = &cur->next;
    }
}

static void
deleteGame( GameState* gs )
{
    Globals* globals = gs->globals;
    if ( globals->curGame == gs ) {
        globals->curGame = NULL;
    }
    updateGameButtons( globals );

    int gameID = gs->gi.gameID; /* remember it */
    cleanupGame( gs );

    // Remove from linked list, but don't actually free because could live in
    // a js-side object still. Needs refcounting or somesuch
    removeGameState( gs );

    char gameIDStr[16];
    formatGameID( gameIDStr, sizeof(gameIDStr), gameID );
    const XP_UCHAR* keys[] = {KEY_GAMES, gameIDStr, NULL};
    dutil_remove( globals->dutil, keys );
    showName( NULL );
}

static void
onDeleteConfirmed( void* closure, bool confirmed )
{
    if ( confirmed ) {
        CAST_GS(GameState*, gs, closure);
        Globals* globals = gs->globals;
        bool wasVisible = gs == getCurGame(globals);

        deleteGame( gs );
        if ( wasVisible ) {
            clearScreen( globals );
        }
        updateDeviceButtons( globals );
    }
}

static bool
getLocalName( Globals* globals, char* playerName, size_t buflen )
{
    XP_U32 len = buflen;
    const XP_UCHAR* keys[] = {KEY_PLAYER_NAME, NULL};
    dutil_loadPtr( globals->dutil, NULL_XWE, keys, playerName, &len );
    bool haveName = 0 != len;
    if ( !haveName ) {        /* not found? */
        strcpy( playerName, "Player 1" );
    }
    return haveName;
}

static void
onPlayerNamed( void* closure, const char* name )
{
    CAST_GLOB(Globals*, globals, closure);
    if ( !!name ) {
        const XP_UCHAR* keys[] = {KEY_PLAYER_NAME, NULL};
        dutil_storePtr( globals->dutil, NULL_XWE, keys, (void*)name, 1 + strlen(name) );
    }
}

typedef struct _NewGameState {
    Globals* globals;
    char* langs[16];
    int nLangs;
} NewGameState;

static XP_Bool
onOneLang( void* closure, const XP_UCHAR* keys[] )
{
    NewGameState* ngs = (NewGameState*)closure;

    char langName[32];
    XP_U32 len = sizeof(langName);
    dutil_loadPtr( ngs->globals->dutil, NULL_XWE, keys, langName, &len );

    replaceStringIfDifferent( ngs->globals->mpool, &ngs->langs[ngs->nLangs], langName );
    XP_LOGFF( "set langs[%d] %s", ngs->nLangs, ngs->langs[ngs->nLangs] );
    ++ngs->nLangs;
    XP_ASSERT( ngs->nLangs < VSIZE(ngs->langs));
    return true;
}

static void
setNewGameDefaults( Globals* globals, NewGameParams* params )
{
    XP_MEMSET( params, 0, sizeof(*params) );
    params->allowHints = true;
    params->isRobot = true;
}

static void
callNewGame( Globals* globals )
{
    NewGameState ngs = {.globals = globals};
    const XP_UCHAR* keys1[] = {KEY_DICTS, KEY_WILDCARD, KEY_LANG_NAME, NULL};
    dutil_forEach( globals->dutil, NULL_XWE, keys1, onOneLang, &ngs );

    NewGameParams params;
    XP_U32 len = sizeof(params);
    const XP_UCHAR* keys2[] = {KEY_NEWGAME_DFLTS, NULL};
    dutil_loadPtr( globals->dutil, NULL_XWE, keys2, &params, &len );
    if ( len != sizeof(params) ) {
        setNewGameDefaults( globals, &params );
    }

    js_callNewGame("Configure your new game", globals, params.allowHints,
                   params.isRobot, params.langName, ngs.langs, ngs.nLangs );
}

static void
onDeviceButton( void* closure, const char* button )
{
    CAST_GLOB(Globals*, globals, closure);
    XP_LOGFF( "(button=%s)", button );
    if ( 0 == strcmp(button, BUTTON_GAME_NEW) ) {
        callNewGame(globals);
    } else if ( 0 == strcmp(button, BUTTON_GAME_GAMES) ) {
        pickGame( globals );
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
    } else if ( 0 == strcmp(button, BUTTON_NAME ) ) {
        char playerName[32];
        getLocalName( globals, playerName, sizeof(playerName)-1 );
        call_get_string( "Set your (local) player name", playerName,
                         onPlayerNamed, globals );
    }
}

static XP_Bool
upCounter( void* closure, const XP_UCHAR* keys[] )
{
    int* intp = (int*)closure;
    ++*intp;
    return true;
}

static int
countGames( Globals* globals )
{
    int nFound = 0;
    const XP_UCHAR* keys[] = {KEY_GAMES, KEY_WILDCARD, KEY_GAME, NULL};
    dutil_forEach( globals->dutil, NULL_XWE, keys, upCounter, &nFound );
    return nFound;
}

static int
countLangs( Globals* globals )
{
    int count = 0;
    const XP_UCHAR* keys[] = {KEY_DICTS, KEY_WILDCARD, KEY_DICTS, NULL};
    dutil_forEach( globals->dutil, NULL_XWE, keys, upCounter, &count );
    LOG_RETURNF( "%d", count );
    return count;
}

static bool
haveDictFor(Globals* globals, const char* lc)
{
    int count = 0;
    const XP_UCHAR* keys[] = {KEY_DICTS, lc, KEY_DICTS, KEY_WILDCARD, NULL};
    dutil_forEach( globals->dutil, NULL_XWE, keys, upCounter, &count );
    return 0 < count;
}

static void
updateDeviceButtons( Globals* globals )
{
    const char* buttons[MAX_BUTTONS];
    int cur = 0;
    if ( 0 < countGames(globals) ) {
        buttons[cur++] = BUTTON_GAME_GAMES;
    }
    if ( 0 < countLangs( globals ) ) {
        buttons[cur++] = BUTTON_GAME_NEW;
    }
    if ( !!getCurGame( globals ) ) {
        buttons[cur++] = BUTTON_GAME_RENAME;
        buttons[cur++] = BUTTON_GAME_DELETE;
    }
    buttons[cur++] = BUTTON_NAME;
    buttons[cur++] = NULL;
    XP_ASSERT( cur <= VSIZE(buttons) );

    setButtons( BUTTONS_ID_DEVICE, buttons, onDeviceButton, globals );
}

static void
onConflict( void* closure, const char* ignored )
{
    CAST_GLOB(Globals*, globals, closure);
    call_alert( "Control passed to another tab" );
    XP_MEMSET( globals, 0, sizeof(*globals) ); /* stop everything :-) */
}

static void
onFocussed( void* closure, const char* newState )
{
    CAST_GLOB(Globals*, globals, closure);
    globals->focussed = 0 == strcmp("focus", newState);
    XP_LOGFF( "focussed now: %d", globals->focussed );
    /* This hasn't worked.... */
    GameState* gs = getCurGame( globals );
    if ( !!gs ) {
        board_invalAll( gs->game.board );
        updateScreen( gs, false );
    }
}

static void
onMqttMsg(void* closure, const char* topic, const uint8_t* data, int len )
{
    CAST_GLOB(Globals*, globals, closure);
    dvc_parseMQTTPacket( globals->dutil, NULL_XWE, topic, data, len );
}

static bool
storeAsDict( Globals* globals, GotDictData* gdd )
{
    char shortName[32];
    sprintf( shortName, "%s", gdd->dictName );
    char* dot = strstr(shortName, ".xwd");
    if ( !!dot ) {
        *dot = '\0';
    }
    XP_LOGFF("shortName: %s", shortName);

    /* First make a dict of it. If it doesn't work out, don't store the
       data! */
    DictionaryCtxt* dict =
        wasm_dictionary_make( globals, shortName, gdd->data, gdd->len );
    bool success = !!dict;
    if ( success ) {
        dict_unref( dict, NULL_XWE );

        const XP_UCHAR* keys1[] = {KEY_DICTS, gdd->lc, KEY_DICTS, shortName, NULL};
        dutil_storePtr( globals->dutil, NULL_XWE, keys1, gdd->data, gdd->len );
        const XP_UCHAR* keys2[] = {KEY_DICTS, gdd->lc, KEY_LANG_NAME, NULL};
        dutil_storePtr( globals->dutil, NULL_XWE, keys2, gdd->langName,
                        strlen(gdd->langName) + 1 );

        char msg[128];
        sprintf( msg, "Successfull installed wordlist for play in %s.",
                 gdd->langName );
        call_alert( msg );
    }
    LOG_RETURNF( "%d", success );
    return success;
}

static void
resizeBoard( Globals* globals, GameState* gs )
{
    BoardDims dims;
    int useWidth = globals->useWidth;
    int useHeight = globals->useHeight;
    board_figureLayout( gs->game.board, NULL_XWE, &gs->gi,
                        WASM_BOARD_LEFT, WASM_HOR_SCORE_TOP,
                        useWidth, useHeight,
                        110, 150, 200, useWidth-25, useWidth/15,
                        useHeight/15, XP_FALSE, &dims );
    board_applyLayout( gs->game.board, NULL_XWE, &dims );

    board_invalAll( gs->game.board ); /* redraw screen on loading new game */
}

static void
resizeBoards( Globals* globals )
{
    for ( GameState* cur = globals->games; !!cur; cur = cur->next ) {
        resizeBoard( globals, cur );
    }
}

/* We need window dimensions to draw, but other stuff too. So when we get
 * dimensions, set them.
 */
static void
initWindow( Globals* globals, int winWidth, int winHeight )
{
    // we're ready to go IFF we are inited.
    if ( !!globals->vtMgr ) {   /* inited? */
        globals->useWidth = winWidth;
        globals->useHeight = winHeight;

        if ( !!globals->window ) {
            SDL_DestroyRenderer(globals->renderer);
            globals->renderer = NULL;
            SDL_DestroyWindow(globals->window);
            globals->window = NULL;
        }

        SDL_CreateWindowAndRenderer( winWidth, winHeight, 0,
                                     &globals->window, &globals->renderer );

        /* wipe the canvas to background */
        SDL_SetRenderDrawColor( globals->renderer, 155, 155, 155, 255 );
        SDL_RenderClear( globals->renderer );

        if ( !!globals->draw ) {
            wasm_draw_resize( globals->draw, winWidth, winHeight );
            resizeBoards( globals );
        } else {
            globals->draw = wasm_draw_make( MPPARM(globals->mpool)
                                            winWidth, winHeight );
        }

        GameState* gs = getCurGame( globals );
        if ( !!gs ) {
            board_invalAll( gs->game.board );
            updateScreen( gs, true );
        }
    } else {
        XP_ASSERT(0);
    }
}

static void
initDeviceGlobals( Globals* globals )
{
    globals->cp.showBoardArrow = XP_TRUE;
    globals->cp.allowPeek = XP_TRUE;
    globals->cp.showRobotScores = XP_TRUE;
    globals->cp.sortNewTiles = XP_TRUE;
    globals->cp.showColors = XP_TRUE;

    globals->transportProcs.sendMsg = send_msg;
    globals->transportProcs.sendInvt = send_invite;
    globals->transportProcs.closure = globals;

#ifdef MEM_DEBUG
    globals->mpool = mpool_make( "wasm" );
#endif
    globals->vtMgr = make_vtablemgr( MPPARM_NOCOMMA(globals->mpool) );
    globals->dutil = wasm_dutil_make( MPPARM(globals->mpool) globals->vtMgr, globals );
    globals->dictMgr = dmgr_make( MPPARM_NOCOMMA(globals->mpool) );

    loadPrefs( globals );

    MQTTDevID devID;
    dvc_getMQTTDevID( globals->dutil, NULL_XWE, &devID );
    XP_UCHAR buf[32];
    XP_SNPRINTF( buf, VSIZE(buf), MQTTDevID_FMT, devID );
    XP_LOGFF( "got mqtt devID: %s", buf );
    int now = dutil_getCurSeconds( globals->dutil, NULL_XWE );
    bool dbg =
#ifdef DEBUG
        true
#else
        false
#endif
        ;
    call_setup( globals, dbg, buf, GITREV, now, onConflict,
                onFocussed, onMqttMsg );
}

static void
storeCurOpen( GameState* gs )
{
    const XP_UCHAR* keys[] = {KEY_LAST_GID, NULL};
    dutil_storePtr( gs->globals->dutil, NULL_XWE, keys,
                    &gs->gi.gameID, sizeof(gs->gi.gameID) );
}

static void
startGame( GameState* gs, const char* name )
{
    Globals* globals = gs->globals;
    globals->curGame = gs;
    ensureName( gs );
    XP_LOGFF( "changed curGame to %s", gs->gameName );
    showName( gs );
    storeCurOpen( gs );

    resizeBoard( globals, gs );

    if ( SERVER_ISCLIENT == gs->gi.serverRole ) {
        if ( !!name ) {
            replaceStringIfDifferent( globals->mpool,
                                      &gs->gi.players[0].name,
                                      name );
        }
        server_initClientConnection( gs->game.server, NULL_XWE );
    }
    
    (void)server_do( gs->game.server, NULL_XWE ); /* assign tiles, etc. */
    if ( !!gs->game.comms ) {
        comms_resendAll( gs->game.comms, NULL_XWE, COMMS_CONN_MQTT, XP_TRUE );
    }

    updateScreen( gs, true );
    LOG_RETURN_VOID();
}

static GameState*
newFromInvite( Globals* globals, const NetLaunchInfo* nli )
{
    GameState* gs = newGameState(globals);
    gs->util = wasm_util_make( MPPARM(globals->mpool) &gs->gi,
                               globals->dutil, gs );

    char playerName[32];
    getLocalName( globals, playerName, sizeof(playerName) );

    const CommsAddrRec* selfAddr = NULL;
    game_makeFromInvite( &gs->game, NULL_XWE, nli,
                         selfAddr,
                         gs->util, globals->draw,
                         &globals->cp, &globals->transportProcs );
    if ( nli->gameName[0] ) {
        nameGame( gs, nli->gameName );
    }
    ensureName( gs );
    return gs;
}

typedef struct _DictDownState {
    Globals* globals;
    NetLaunchInfo invite;
    const char* lc;
} DictDownState;


static void
onDictForInvite( void* closure, GotDictData* gdd )
{
    DictDownState* dds = (DictDownState*)closure;
    if ( !!gdd->data
         && 0 < gdd->len
         && storeAsDict( dds->globals, gdd ) ) {
        loadAndDraw( dds->globals, &dds->invite, 0, NULL );
    } else {
        char msg[128];
        sprintf( msg, "Unable to download %s worldlist for invitation", dds->lc );
        call_alert( msg );
    }
    XP_FREE( dds->globals->mpool, dds );
}

static void
onDictConfirmed( void* closure, bool confirmed )
{
    DictDownState* dds = (DictDownState*)closure;
    if ( confirmed ) {
        call_get_dict( dds->lc, onDictForInvite, dds );
    } else {
        XP_FREE( dds->globals->mpool, dds );
    }
 }

/* If you launch a URL that encodes an invitation, you'll get here. If it's
 * the first time (the game hasn't been created yet) you'll get a new game
 * that connects to the host. If you've already created the game, you'll be
 * taken to it in whatever state it's in. If you've deleted the game, bad
 * situation: you'll get a new game that will be unable to connect to any
 * host.
 */
static GameState*
gameFromInvite( Globals* globals, const NetLaunchInfo* nli )
{
    bool needsLoad = true;
    GameState* gs = getSavedGame( globals, nli->gameID );
    if ( !gs ) {
        const char* lc = nli->isoCodeStr;
        if ( haveDictFor(globals, lc) ) {
            gs = newFromInvite( globals, nli );
        } else {
            char msg[128];
            sprintf( msg, "Invitation requires a wordlist %s for "
                     "locale %s; download now?", nli->dict, lc );

            DictDownState* dds = XP_MALLOC( globals->mpool, sizeof(*dds) );
            dds->globals = globals;
            dds->invite = *nli;
            dds->lc = lc;
            call_confirm(globals, msg, onDictConfirmed, dds);
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

static GameState*
getSavedGame( Globals* globals, int gameID )
{
    GameState* gs;

    /* Is it already open? */
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
        char gameIDStr[16];
        formatGameID( gameIDStr, sizeof(gameIDStr), gameID );
        const XP_UCHAR* keys[] = {KEY_GAMES, gameIDStr, KEY_GAME, NULL};
        dutil_loadStream( globals->dutil, NULL_XWE, keys, stream );
        if ( 0 < stream_getSize( stream ) ) {
            XP_ASSERT( !gs->util );
            gs->util = wasm_util_make( MPPARM(globals->mpool) &gs->gi,
                                       globals->dutil, gs );

            XP_LOGFF( "there's a saved game!!" );
            loaded = game_makeFromStream( MPPARM(globals->mpool) NULL_XWE, stream,
                                          &gs->game, &gs->gi,
                                          gs->util, globals->draw,
                                          &globals->cp, &globals->transportProcs );

            if ( loaded ) {
                loadName( gs );
                ensureName( gs );
                updateScreen( gs, false );
            } else {
                removeGameState( gs );
                gs = NULL;
            }
        } else {
            XP_LOGFF( "ERROR: no saved data for game %s", gameIDStr );
            XP_ASSERT( globals->games == gs );
            globals->games = gs->next;
            XP_FREE( globals->mpool, gs );
            gs = NULL;
        }
        stream_destroy( stream );
    }
    XP_LOGFF( "(%X) => %p", gameID, gs );
    return gs;
}

static void
saveName( GameState* gs )
{
    char gameIDStr[32];
    formatGameID( gameIDStr, sizeof(gameIDStr), gs->gi.gameID );
    const XP_UCHAR* keys[] = {KEY_GAMES, gameIDStr, KEY_NAME, NULL};
    dutil_storePtr( gs->globals->dutil, NULL_XWE, keys,
                    (XP_U8*)gs->gameName, 1 + strlen(gs->gameName) );
}

static void
loadName( GameState* gs )
{
    char gameIDStr[32];
    formatGameID( gameIDStr, sizeof(gameIDStr), gs->gi.gameID );

    XP_U32 len = sizeof(gs->gameName);
    const XP_UCHAR* keys[] = {KEY_GAMES, gameIDStr, KEY_NAME, NULL};
    dutil_loadPtr( gs->globals->dutil, NULL_XWE, keys, (XP_U8*)gs->gameName, &len );
}

static void
ensureName( GameState* gs )
{
    if ( '\0' == gs->gameName[0] ) {
        nameGame( gs, NULL );
    }
}

static int
getNextGameNo( Globals* globals )
{
    int val = 0;
    const XP_UCHAR* keys[] = { KEY_NEXT_GAME, NULL };
    XP_U32 len = sizeof(val);
    dutil_loadPtr( globals->dutil, NULL_XWE, keys, (XP_U8*)&val, &len );
    ++val;
    dutil_storePtr( globals->dutil, NULL_XWE, keys, (XP_U8*)&val, sizeof(val) );
    XP_LOGFF( "getNextGameNo() => %d", val );
    return val;
}

static void
nameGame( GameState* gs, const char* name )
{
    if ( !!name ) {
        snprintf( gs->gameName, sizeof(gs->gameName), "%s", name );
    } else {
        snprintf( gs->gameName, sizeof(gs->gameName),
                  "Game %d", getNextGameNo(gs->globals) );
    }
    saveName( gs );
    XP_LOGFF( "named game: %s", gs->gameName );
}

typedef struct _FindOneState {
    Globals* globals;
    const char* langName;
    DictionaryCtxt* dict;
} FindOneState;

static XP_Bool
onOneDict( void* closure, const XP_UCHAR* keysIn[] )
{
    // XP_LOGFF( "(%s/%s/%s/%s)", keysIn[0], keysIn[1], keysIn[2], keysIn[3] );
    FindOneState* fos = (FindOneState*)closure;
    Globals* globals = fos->globals;
    XW_DUtilCtxt* dutil = globals->dutil;
    /* I've got a dict directory with its language name. IF the name's a
       match, or if I'm not looking for a match, load and make a dict from
       it */
    char langName[32];
    XP_U32 len = sizeof(langName);
    const XP_UCHAR* keys[] = { keysIn[0], keysIn[1], KEY_LANG_NAME, NULL };
    dutil_loadPtr( dutil, NULL_XWE, keys, langName, &len );
    XP_ASSERT( 0 < len );
    // XP_LOGFF( "langName: %s", langName );

    bool useIt = NULL == fos->langName || 0 == strcmp( fos->langName, langName );
    if ( useIt ) {
        const XP_UCHAR* dictName = keysIn[3];
        // XP_LOGFF( "dictName: %s", dictName );
        XP_ASSERT( !fos->dict );

        XP_U32 len;
        uint8_t* ptr = wasm_dutil_mallocAndLoad( dutil, keysIn, &len );
        if ( !!ptr ) {
            fos->dict = wasm_dictionary_make( globals, dictName, ptr, len );
            XP_FREE( globals->mpool, ptr );
        }
    }
    return NULL == fos->dict;
}

static DictionaryCtxt*
loadAnyDict( Globals* globals, const char* langName )
{
    FindOneState fos = {.globals = globals,
                        .langName = langName,
    };
    const XP_UCHAR* keys[] = {KEY_DICTS, KEY_WILDCARD, KEY_DICTS, KEY_WILDCARD, NULL};
    dutil_forEach( globals->dutil, NULL_XWE, keys, onOneDict, &fos );
    LOG_RETURNF( "%p", fos.dict );
    return fos.dict;
}

typedef struct _FindLCState {
    Globals* globals;
    const char* langName;
    char* lc;
} FindLCState;

static XP_Bool
onOneLangName( void* closure, const XP_UCHAR* keys[] )
{
    bool found = false;
    FindLCState* lcs = (FindLCState*)closure;

    char langName[32];
    XP_U32 len = sizeof(langName);
    dutil_loadPtr( lcs->globals->dutil, NULL_XWE, keys, langName, &len );

    if ( 0 == strcmp( lcs->langName, langName ) ) {
        strcpy( lcs->lc, keys[1] );
        found = true;
    }
    return !found;
}

static void
loadAndDraw( Globals* globals, const NetLaunchInfo* invite,
             int gameID, NewGameParams* params )
{
    XP_LOGFF( "(gameID: %X)", gameID );
    GameState* gs = NULL;

    bool haveGame;
    if ( !params ) {
        /* First, load any saved game. We need it e.g. to confirm that an incoming
           invite is a dup and should be dropped. */
        if ( 0 != gameID ) {
            gs = getSavedGame( globals, gameID );
        }
        if ( !!invite ) {   /* overwrite gs is ok: we'll likely want both games  */
            gs = gameFromInvite( globals, invite );
        }
    }

    if ( !gs ) {
        const char* langName = NULL == params ? NULL : params->langName;
        DictionaryCtxt* dict = loadAnyDict( globals, langName );
        if ( !!dict ) {
            char playerName[32];
            getLocalName( globals, playerName, sizeof(playerName) );

            gs = newGameState( globals );
            gs->gi.serverRole = !!params && !params->isRobot
                ? SERVER_ISSERVER : SERVER_STANDALONE;

            gs->gi.phoniesAction = PHONIES_WARN;
            gs->gi.hintsNotAllowed = !!params && !params->allowHints || false;
            gs->gi.gameID = 0;
            XP_STRNCPY( gs->gi.isoCodeStr, dict_getISOCode(dict), VSIZE(gs->gi.isoCodeStr) );
            replaceStringIfDifferent( globals->mpool, &gs->gi.dictName,
                                      dict_getShortName(dict) );
            gs->gi.nPlayers = 2;
            gs->gi.boardSize = 15;
            gs->gi.traySize = gs->gi.bingoMin = 7;
            gs->gi.players[0].name = copyString( globals->mpool, playerName );
            gs->gi.players[0].isLocal = XP_TRUE;
            gs->gi.players[0].robotIQ = 0;

            bool otherLocal = !!params ? params->isRobot : true;
            gs->gi.players[1].name = copyString( globals->mpool,
                                                 otherLocal ? "Robot" : "(remote)" );
            gs->gi.players[1].isLocal = otherLocal;
            XP_LOGFF( "set isLocal[1]: %d", gs->gi.players[1].isLocal );
            gs->gi.players[1].robotIQ = 99; /* doesn't matter if remote */

            gs->util = wasm_util_make( MPPARM(globals->mpool) &gs->gi,
                                       globals->dutil, gs );

            XP_LOGFF( "calling game_makeNewGame()" );
            const CommsAddrRec* selfAddr = NULL;
            CommsAddrRec _selfAddr;
            if ( SERVER_STANDALONE != gs->gi.serverRole ) {
                makeSelfAddr( globals, &_selfAddr );
                selfAddr = &_selfAddr;
            }

            const CommsAddrRec* hostAddr = NULL;
            game_makeNewGame( MPPARM(globals->mpool) NULL_XWE,
                              &gs->game, &gs->gi,
                              selfAddr, hostAddr,
                              gs->util, globals->draw,
                              &globals->cp, &globals->transportProcs );

            ensureName( gs );
            dict_unref( dict, NULL_XWE );
        }
    }
    if ( !!gs ) {
        startGame( gs, NULL );
    }
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
                    const CommsAddrRec* from, const XP_U8* buf,
                    XP_U16 len )
{
    GameState* gs = getSavedGame( globals, gameID );
    if ( !!gs ) {
        XWStreamCtxt* stream = mem_stream_make_raw( MPPARM(gs->globals->mpool)
                                                    gs->globals->vtMgr );
        stream_putBytes( stream, buf, len );
        if ( game_receiveMessage( &gs->game, NULL_XWE, stream, from ) ) {
            updateScreen( gs, true );
        }
        stream_destroy( stream );
        if ( !globals->focussed && GRANTED == js_getHaveNotifyPerm() ) {
            GameSummary summary;
            game_summarize( &gs->game, &gs->gi, &summary );
            if ( summary.turnIsLocal && 0 <= summary.turn ) {
                char buf[128];
                sprintf( buf, "Your turn in game %s", gs->gameName );
                js_notify( buf );
            }
        }
    } else {
        dvc_makeMQTTNoSuchGames( globals->dutil, NULL_XWE,
                                 onMsgAndTopic, NULL,
                                 &from->u.mqtt.devID, gameID );
#ifdef DEBUG
        char msg[128];
        snprintf( msg, sizeof(msg), "Dropping move for deleted game (id: %X/%d)",
                  gameID, gameID );
        call_alert( msg );
#endif
    }
}

void
main_onCtrlReceived( Globals* globals, const XP_U8* buf, XP_U16 len )
{
    XP_LOGFF("(len=%d)", len );
    XP_ASSERT(0);
}

void
main_onGameGone( Globals* globals, XP_U32 gameID )
{
    GameState* gs = getSavedGame( globals, gameID );
    if ( !!gs ) {
        char msg[128];
        sprintf( msg, "The game %s has been deleted on the remote "
                 "device. Delete here too?", gs->gameName );
        call_confirm( globals, msg, onDeleteConfirmed, gs );
    }
}

void
main_sendOnClose( XWStreamCtxt* stream, XWEnv env, void* closure )
{
    CAST_GS(GameState*, gs, closure );
    XP_LOGFF( "called with msg of len %d", stream_getSize(stream) );
    (void)comms_send( gs->game.comms, NULL_XWE, stream );
}

void
main_playerScoreHeld( GameState* gs, XP_U16 player )
{
    LastMoveInfo lmi;
    XP_UCHAR buf[128];
    if ( model_getPlayersLastScore( gs->game.model, NULL_XWE, player, &lmi ) ) {
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
    board_formatRemainingTiles( gs->game.board, NULL_XWE, stream );
    stream_putU8( stream, 0 );
    call_alert( (const XP_UCHAR*)stream_getPtr( stream ) );
    stream_destroy( stream );
}

static void
openConfirmed(void* closure, bool confirmed)
{
    if ( confirmed ) {
        CAST_GS(GameState*, gs, closure);
        loadAndDraw( gs->globals, NULL, gs->gi.gameID, NULL );
    }
}

void
main_turnChanged( GameState* gs, int newTurn )
{
    if ( 0 <= newTurn && !isVisible(gs) && gs->gi.players[newTurn].isLocal ) {
        char msg[128];
        snprintf( msg, sizeof(msg),
                  "It's your turn in game \"%s\". Would you like to open it now?",
                  gs->gameName );
        call_confirm( gs->globals, msg, openConfirmed, gs );
    }
}

static void
onChatReceiptDone( void* closure, const char* button )
{
    if ( 0 == strcmp( BUTTON_REPLY, button ) ) {
        call_get_string( "Your message?", "", onChatComposed, closure );
    }
}

void
main_chatReceived( GameState* gs, const char* msg )
{
    char buf[256];
    size_t len = snprintf( buf, sizeof(buf), "Chat message received:\n%s", msg );
    if ( len < sizeof(buf)) {
        const char* buttons[] = { BUTTON_REPLY, BUTTON_OK, NULL };
        call_dialog( buf, buttons, onChatReceiptDone, gs );
    }
}

typedef struct _BlankPickState {
    GameState* gs;
    int col, row;
    int nTiles;
    int playerNum;
} BlankPickState;

static void
onBlankPicked( void* closure, const char* str )
{
    XP_LOGFF( "index: %s", str );
    BlankPickState* bps = (BlankPickState*)closure;
    GameState* gs = bps->gs;
    Globals* globals = gs->globals;

    int indx = atoi(str);
    if ( 0 <= indx && indx < bps->nTiles
         && board_setBlankValue( gs->game.board, bps->playerNum,
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
    call_pickBlank( "Pick a tile for your blank", tileFaces, nTiles,
                    onBlankPicked, bps );
}

void
main_showGameOver( GameState* gs )
{
    if ( isVisible(gs) ) {
        XWStreamCtxt* stream = mem_stream_make_raw( MPPARM(gs->globals->mpool)
                                                    gs->globals->vtMgr );
        server_writeFinalScores( gs->game.server, NULL_XWE, stream );
        stream_putU8( stream, 0 );
        call_alert( (const XP_UCHAR*)stream_getPtr( stream ) );
        stream_destroy( stream );
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
                && board_handlePenDown( board, NULL_XWE,
                                        event.button.x, event.button.y,
                                        &handled );
            break;
        case SDL_MOUSEBUTTONUP:
            draw = event.button.button == SDL_BUTTON_LEFT
                && board_handlePenUp( board, NULL_XWE,
                                      event.button.x, event.button.y );
            break;
        case SDL_MOUSEMOTION:
            draw = board_handlePenMove( board, NULL_XWE,
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
            board_draw( gs->game.board, NULL_XWE );
            wasm_draw_render( globals->draw, globals->renderer );
        }
        SDL_RenderPresent( globals->renderer );

    } else {
        XP_LOGFF( "not drawing %s; not visible", gs->gameName );
    }

    updateGameButtons( globals );

    /* Let's save state here too, though likely too often */
    if ( doSave ) {
        XWStreamCtxt* stream = mem_stream_make_raw( MPPARM(globals->mpool)
                                                    globals->vtMgr );
        game_saveToStream( &gs->game, &gs->gi, stream, ++gs->saveToken );

        GameSummary summary;
        game_summarize( &gs->game, &gs->gi, &summary );

        char gameIDStr[32];
        formatGameID( gameIDStr, sizeof(gameIDStr), gs->gi.gameID );
        const XP_UCHAR* keys[] = { KEY_GAMES, gameIDStr, KEY_GAME, NULL };
        dutil_storeStream( globals->dutil, NULL_XWE, keys, stream );
        stream_destroy( stream );
        game_saveSucceeded( &gs->game, NULL_XWE, gs->saveToken );

        keys[2] = KEY_SUMMARY;
        dutil_storePtr( globals->dutil, NULL_XWE, keys, &summary, sizeof(summary) );
    }
}

void
main_updateScreen( GameState* gs )
{
    updateScreen( gs, true );
}

static void
onGotMissingDict( void* closure, GotDictData* gdd )
{
    CAST_GLOB(Globals*, globals, closure);
    if ( !!gdd->data
         && 0 < gdd->len ) {
        storeAsDict( globals, gdd );
    }
}

void
main_needDictForGame( GameState* gs, const char* lc,
                      const XP_UCHAR* dictName )
{
    call_get_dict( lc, onGotMissingDict, gs->globals );
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
    }
    wasm_dutil_syncIf( globals->dutil );

#ifdef MEM_DEBUG
    if ( mpool_getStats( globals->mpool, &globals->mpstats ) ) {
        show_pool(globals->mpstats.curBytes, globals->mpstats.maxBytes);
        /* XP_LOGFF( "mempool: cur: %d; max: %d", globals->mpstats.curBytes, */
        /*           globals->mpstats.maxBytes ); */
    }
#endif
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
            XP_STRNCPY( gi.isoCodeStr, param, VSIZE(gi.isoCodeStr) );
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

void
MQTTConnectedChanged( void* closure, bool connected )
{
    if ( connected ) {
        CAST_GLOB(Globals*, globals, closure);
        GameState* gs = getCurGame( globals );
        if ( !!gs && !!gs->game.comms ) {
            comms_resendAll( gs->game.comms, NULL_XWE, COMMS_CONN_MQTT, XP_TRUE );
        }
    }
}

void
cbckBinary( BinProc proc, void* closure, int len, const uint8_t* msg )
{
    (*proc)(closure, msg, len );
}

static void
onAllowNotify(void* closure, bool confirmed)
{
    NewGameParams* ngp = (NewGameParams*)closure;
    Globals* globals = ngp->globals;
    if ( confirmed ) {
        js_requestNotify();
    }
    loadAndDraw( globals, NULL, 0, ngp );
    updateDeviceButtons( globals );
    XP_FREE( globals->mpool, ngp );
}

void
onNewGame( void* closure, bool isRobot, const char* langName,
           bool allowHints)
{
    CAST_GLOB(Globals*, globals, closure);

    NewGameParams* ngp = XP_CALLOC( globals->mpool, sizeof(*ngp) );
    ngp->allowHints = allowHints;
    ngp->isRobot = isRobot;
    strcpy( ngp->langName, langName );
    const XP_UCHAR* keys[] = {KEY_NEWGAME_DFLTS, NULL};
    dutil_storePtr( globals->dutil, NULL_XWE, keys, ngp, sizeof(*ngp) );
    ngp->globals = globals;

    if ( !isRobot && UNREQUESTED == js_getHaveNotifyPerm() ) {
        const char* msg = "You are creating a networked game. Would you like "
            "notifications when a move arrives and it becomes your turn?";
        call_confirm( globals, msg, onAllowNotify, ngp );
    } else {
        onAllowNotify(ngp, false);
    }
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

void
gotDictBinary( GotDictProc proc, void* closure, const char* xwd,
               const char* lc, const char* langName,
               uint8_t* data, int len )
{
    XP_LOGFF( "lc: %s, langName: %s, xwd: %s; len: %d",
              lc, langName, xwd, len );
    GotDictData gdd = { .lc = lc,
                        .dictName = xwd,
                        .langName = langName,
                        .data = data,
                        .len = len,
    };
    (*proc)(closure, &gdd);
}

void
onResize( void* closure, int width, int height )
{
    CAST_GLOB( Globals*, globals, closure );
    XP_LOGFF( "width=%d, height=%d)", width, height );
    initWindow( globals, width, height );
    updateGameButtons( globals );
    updateDeviceButtons( globals );
}

/* On first launch, we may have an invitation. Or not. We want to ask for a
 * local name (but only once), then download any wordlist we need, then open
 * any pre-existing game or else a new one.
 */

typedef struct _LaunchState {
    Globals* globals;
    NetLaunchInfo nli;
    char playerName[32];
    bool hadName;
} LaunchState;

static void
storeOrAlert( Globals* globals, GotDictData* gdd )
{
    if ( !!gdd ) {
        if ( 0 == gdd->len || !storeAsDict( globals, gdd ) ) {
            call_alert( "Unable to download wordlist. Reload the page to try again?" );
        }
    }
}

static void
onGotInviteDictAtLaunch( void* closure, GotDictData* gdd )
{
    // XP_LOGFF("(gdd: %p)", gdd );
    LaunchState* ls = (LaunchState*)closure;
    Globals* globals = ls->globals;

    storeOrAlert( globals, gdd );

    /* We're ready to start. If we had an invitation, launch for it. Otherwise
       launch the last game that was open */
    NetLaunchInfo* nlip = '\0' == ls->nli.isoCodeStr[0] ? NULL : &ls->nli;
    int gameID = 0;
    XP_U32 len = sizeof(gameID);
    const XP_UCHAR* keys[] = {KEY_LAST_GID, NULL};
    dutil_loadPtr( globals->dutil, NULL_XWE, keys, (XP_U8*)&gameID, &len );
    XP_LOGFF( "loaded KEY_LAST_GID: %X", gameID );
    loadAndDraw( globals, nlip, gameID, NULL );

    updateDeviceButtons( globals );

    XP_FREE( globals->mpool, ls );
}

static void
onGotNativeDictAtLaunch( void* closure, GotDictData* gdd )
{
    LaunchState* ls = (LaunchState*)closure;
    Globals* globals = ls->globals;
    // XP_LOGFF("(gdd: %p)", gdd );
    storeOrAlert( globals, gdd );

    /* Now download a wordlist if we need one */
    const char* neededLC = NULL;
    if ( '\0' != ls->nli.isoCodeStr[0] ) {   /* 0 means unset: no invite */
        if ( !haveDictFor(ls->globals, ls->nli.isoCodeStr) ) {
            neededLC = ls->nli.isoCodeStr;
        }
    }
    if ( !!neededLC ) {
        call_get_dict( neededLC, onGotInviteDictAtLaunch, ls );
    } else {
        onGotInviteDictAtLaunch( ls, NULL);
    }
}

static void
onPlayerNamedAtLaunch( void* closure, const char* responseName )
{
    LaunchState* ls = (LaunchState*)closure;
    Globals* globals = ls->globals;
    // XP_LOGFF("(name: %s)", responseName );

    /* Did user change name? Save it */
    if ( !!responseName && 0 != strcmp( responseName, ls->playerName ) ) {
        onPlayerNamed( globals, responseName );
    } else if ( !ls->hadName ) {
        onPlayerNamed( globals, ls->playerName );

        char buf[128];
        sprintf( buf, "Ok. Using default name %s. You can change it anytime "
                 "using the \"%s\" button.", ls->playerName, BUTTON_NAME );
        call_alert( buf );
    }

    if ( 0 == countLangs( globals ) ) {
        call_get_dict( NULL, onGotNativeDictAtLaunch, ls );
    } else {
        onGotNativeDictAtLaunch( ls, NULL );
    }
 }

static void
startLaunchSequence( Globals* globals, NetLaunchInfo* nli )
{
    LaunchState* ls = XP_CALLOC( globals->mpool, sizeof(*ls) );
    ls->globals = globals;
    if ( NULL != nli ) {
        ls->nli = *nli;
    }

    /* No saved name? Ask. Politely */
    if ( getLocalName( globals, ls->playerName, sizeof(ls->playerName) ) ) {
        ls->hadName = true;
        onPlayerNamedAtLaunch( ls, NULL );
    } else {
        call_get_string( "Please choose a name for your player. It's what you "
                         "and others will see in the scoreboard", ls->playerName,
                         onPlayerNamedAtLaunch, ls );
    }
}

void
mainPostSync( int argc, const char** argv )
{
    XP_LOGFF( "(argc=%d)", argc );
    time_t now = getCurMS();
    srandom( now );
    XP_LOGFF( "called(srandom( %x )", now );

    Globals* globals;
#ifdef GLOBALS_ON_STACK
    Globals _globals = {0};
    globals = &_globals;
#else
    globals = calloc(1, sizeof(*globals));
#endif
#ifdef DEBUG
    globals->_GUARD = GUARD_GLOB;
#endif
    initDeviceGlobals( globals ); /* takes care of getting mqtt devid */

    NetLaunchInfo nli = {0};
    NetLaunchInfo* nlip = NULL;
    if ( inviteFromArgv( globals, &nli, argc, argv ) ) {
        nlip = &nli;
    }

    SDL_Init( SDL_INIT_EVENTS );
    TTF_Init();

    startLaunchSequence( globals, nlip );

#ifdef GLOBALS_ON_STACK
    emscripten_set_main_loop_arg( looper, globals, -1, 1 );
#else
    emscripten_set_main_loop_arg( looper, globals, -1, 0 );
#endif
}

EM_JS( void, loadDBThen, (const char* root, int argc, const char** argv), {
        let jsroot = UTF8ToString(root);
        FS.mkdir(jsroot);
        FS.mount(IDBFS, {}, jsroot);
        FS.syncfs(true, function (err) {
                if ( err ) {
                    console.log('fsSyncIn: err: ' + err);
                }
                ccall('mainPostSync', null, ['number', 'number'], [argc, argv]);
            });
    });

int
main( int argc, const char** argv )
{
    XP_LOGFF( "MAIN ENTRY(argc=%d)", argc );
    loadDBThen( ROOT_PATH, argc, argv );
    return 0;
}
