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

#include "util.h"
#include "comtypes.h"
#include "strutils.h"
#include "main.h"
#include "dbgutil.h"
#include "wasmdict.h"
#include "wasmdutil.h"

typedef struct _WasmUtilCtx {
    XW_UtilCtxt super;

    XW_DUtilCtxt* dctxt;
    GameState* closure;
} WasmUtilCtx;

static XWStreamCtxt*
wasm_util_makeStreamFromAddr( XW_UtilCtxt* uc, XWEnv xwe, XP_PlayerAddr channelNo )
{
    LOG_FUNC();

    WasmUtilCtx* wuctxt = (WasmUtilCtx*)uc;
    GameState* gs = wuctxt->closure;
    XWStreamCtxt* stream = mem_stream_make( MPPARM(uc->mpool)
                                            gs->globals->vtMgr, gs,
                                            channelNo, main_sendOnClose );
    return stream;
}

static XWBonusType
wasm_util_getSquareBonus( XW_UtilCtxt* uc, XWEnv xwe, XP_U16 boardSize,
                          XP_U16 col, XP_U16 row )
{
#define BONUS_DIM 8
    static const int s_buttsBoard[BONUS_DIM][BONUS_DIM] = {
        { BONUS_TRIPLE_WORD,  BONUS_NONE,         BONUS_NONE,BONUS_DOUBLE_LETTER,BONUS_NONE,BONUS_NONE,BONUS_NONE,BONUS_TRIPLE_WORD },
        { BONUS_NONE,         BONUS_DOUBLE_WORD,  BONUS_NONE,BONUS_NONE,BONUS_NONE,BONUS_TRIPLE_LETTER,BONUS_NONE,BONUS_NONE },

        { BONUS_NONE,         BONUS_NONE,         BONUS_DOUBLE_WORD,BONUS_NONE,BONUS_NONE,BONUS_NONE,BONUS_DOUBLE_LETTER,BONUS_NONE },
        { BONUS_DOUBLE_LETTER,BONUS_NONE,         BONUS_NONE,BONUS_DOUBLE_WORD,BONUS_NONE,BONUS_NONE,BONUS_NONE,BONUS_DOUBLE_LETTER },
                            
        { BONUS_NONE,         BONUS_NONE,         BONUS_NONE,BONUS_NONE,BONUS_DOUBLE_WORD,BONUS_NONE,BONUS_NONE,BONUS_NONE },
        { BONUS_NONE,         BONUS_TRIPLE_LETTER,BONUS_NONE,BONUS_NONE,BONUS_NONE,BONUS_TRIPLE_LETTER,BONUS_NONE,BONUS_NONE },
                            
        { BONUS_NONE,         BONUS_NONE,         BONUS_DOUBLE_LETTER,BONUS_NONE,BONUS_NONE,BONUS_NONE,BONUS_DOUBLE_LETTER,BONUS_NONE },
        { BONUS_TRIPLE_WORD,  BONUS_NONE,         BONUS_NONE,BONUS_DOUBLE_LETTER,BONUS_NONE,BONUS_NONE,BONUS_NONE,BONUS_DOUBLE_WORD },
    }; /* buttsBoard */

    int half = boardSize / 2;
    if ( col > half ) { col = (half*2) - col; }
    if ( row > half ) { row = (half*2) - row; }
    XP_ASSERT( col < BONUS_DIM && row < BONUS_DIM );
    return s_buttsBoard[row][col];
}

static const XP_UCHAR*
wasm_getErrString( UtilErrID id, XP_Bool* silent )
{
    *silent = XP_FALSE;
    const char* message = NULL;

    switch( (int)id ) {
    case ERR_TILES_NOT_IN_LINE:
        message = "All tiles played must be in a line.";
        break;
    case ERR_NO_EMPTIES_IN_TURN:
        message = "Empty squares cannot separate tiles played.";
        break;

    case ERR_TOO_FEW_TILES_LEFT_TO_TRADE:
        message = "Too few tiles left to trade.";
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

#ifndef XWFEATURE_STANDALONE_ONLY
    case ERR_NO_PEEK_REMOTE_TILES:
        message = "No peeking at remote players' tiles!";
        break;
    case ERR_REG_UNEXPECTED_USER:
        message = "Refused attempt to register unexpected user[s].";
        break;
    case ERR_SERVER_DICT_WINS:
        message = "Conflict between Host and Guest dictionaries; Host wins.";
        XP_WARNF( "GTK may have problems here." );
        break;
    case ERR_REG_SERVER_SANS_REMOTE:
        message = "At least one player must be marked remote for a game "
            "started as Host.";
        break;
#endif

    case ERR_NO_EMPTY_TRADE:
        message = "No tiles selected; trade cancelled.";
        break;

    case ERR_TOO_MANY_TRADE:
        message = "More tiles selected than remain in pool.";
        break;

    case ERR_NO_HINT_FOUND:
        message = "Unable to suggest any moves.";
        break;

    case ERR_CANT_UNDO_TILEASSIGN:
        message = "Tile assignment can't be undone.";
        break;

    case ERR_CANT_HINT_WHILE_DISABLED:
        message = "The hint feature is disabled for this game.  Enable "
            "it for a new game using the Preferences dialog.";
        break;

/*     case INFO_REMOTE_CONNECTED: */
/*         message = "Another device has joined the game"; */
/*         break; */

    case ERR_RELAY_BASE + XWRELAY_ERROR_LOST_OTHER:
        *silent = XP_TRUE;
        message = "XWRELAY_ERROR_LOST_OTHER";
        break;

    case ERR_RELAY_BASE + XWRELAY_ERROR_TIMEOUT:
        message = "The relay timed you out; other players "
            "have left or never showed up.";
        break;

    case ERR_RELAY_BASE + XWRELAY_ERROR_HEART_YOU:
        message = "You were disconnected from relay because it didn't "
            "hear from you in too long.";
        break;
    case ERR_RELAY_BASE + XWRELAY_ERROR_HEART_OTHER:
/*         *silent = XP_TRUE; */
        message = "The relay has lost contact with a device in this game.";
        break;

    case ERR_RELAY_BASE + XWRELAY_ERROR_OLDFLAGS:
        message = "You need to upgrade your copy of Crosswords.";
        break;
        
    case ERR_RELAY_BASE + XWRELAY_ERROR_SHUTDOWN:
        message = "Relay disconnected you to shut down (and probably reboot).";
        break;

    case ERR_RELAY_BASE + XWRELAY_ERROR_BADPROTO:
        message = "XWRELAY_ERROR_BADPROTO";
        break;
    case ERR_RELAY_BASE + XWRELAY_ERROR_RELAYBUSY:
        message = "XWRELAY_ERROR_RELAYBUSY";
        break;

    case ERR_RELAY_BASE + XWRELAY_ERROR_OTHER_DISCON:
        *silent = XP_TRUE;      /* happens all the time, and shouldn't matter */
        message = "XWRELAY_ERROR_OTHER_DISCON";
        break;

    case ERR_RELAY_BASE + XWRELAY_ERROR_NO_ROOM:
        message = "No such room.  Has the host connected yet to reserve it?";
        break;
    case ERR_RELAY_BASE + XWRELAY_ERROR_DUP_ROOM:
        message = "That room is reserved by another host.  Rename your room, "
            "become a guest, or try again in a few minutes.";
        break;

    case ERR_RELAY_BASE + XWRELAY_ERROR_TOO_MANY:
        message = "You tried to supply more players than the host expected.";
        break;

    case ERR_RELAY_BASE + XWRELAY_ERROR_DELETED:
        message = "Game deleted .";
        break;
    case ERR_RELAY_BASE + XWRELAY_ERROR_NORECONN:
        message = "Cannot reconnect.";
        break;
    case ERR_RELAY_BASE + XWRELAY_ERROR_DEADGAME:
        message = "Game is listed as dead on relay.";
        break;

    default:
        XP_LOGF( "no code for error: %d", id );
        message = "<unrecognized error code reported>";
    }

    return (XP_UCHAR*)message;
}

static void
wasm_util_userError( XW_UtilCtxt* uc, XWEnv xwe, UtilErrID id )
{
    WasmUtilCtx* wuctxt = (WasmUtilCtx*)uc;
    GameState* gs = wuctxt->closure;
    XP_Bool silent;
    const XP_UCHAR* str = wasm_getErrString( id, &silent );
    if ( !silent ) {
        main_alert( gs, str );
    }
}

static void
query_proc_notifyMove( void* closure, XP_Bool confirmed )
{
    if ( confirmed ) {
        WasmUtilCtx* wuctxt = (WasmUtilCtx*)closure;
        GameState* gs = wuctxt->closure;
        if ( board_commitTurn( gs->game.board, NULL,
                               XP_TRUE, XP_TRUE, NULL ) ) {
            main_updateScreen( gs );
        }
    }
}

static void
wasm_util_notifyMove( XW_UtilCtxt* uc, XWEnv xwe, XWStreamCtxt* stream )
{
    WasmUtilCtx* wuctxt = (WasmUtilCtx*)uc;
    GameState* gs = wuctxt->closure;

    XP_U16 len = stream_getSize( stream );
    XP_UCHAR buf[len+1];
    stream_getBytes( stream, buf, len );
    buf[len] = '\0';
    main_query( gs, buf, query_proc_notifyMove, uc );
}

static void
wasm_util_notifyTrade( XW_UtilCtxt* uc, XWEnv xwe, const XP_UCHAR** tiles,
                       XP_U16 nTiles )
{
    WasmUtilCtx* wuctxt = (WasmUtilCtx*)uc;
    GameState* gs = wuctxt->closure;
    XP_UCHAR buf[128];
    XP_SNPRINTF( buf, sizeof(buf),
                 "Are you sure you want to trade the %d selected tiles?", nTiles );
    main_query( gs, buf, query_proc_notifyMove, uc );
}

static void
wasm_util_notifyPickTileBlank( XW_UtilCtxt* uc, XWEnv xwe, XP_U16 playerNum,
                               XP_U16 col, XP_U16 row,
                               const XP_UCHAR** tileFaces, XP_U16 nTiles )
{
    WasmUtilCtx* wuctxt = (WasmUtilCtx*)uc;
    GameState* gs = wuctxt->closure;
    main_pickBlank( gs, playerNum, col, row, tileFaces, nTiles );
}

static void
wasm_util_informNeedPickTiles( XW_UtilCtxt* uc, XWEnv xwe, XP_Bool isInitial,
                               XP_U16 player, XP_U16 nToPick,
                               XP_U16 nFaces, const XP_UCHAR** faces,
                               const XP_U16* counts )
{
    LOG_FUNC();
}

static void
wasm_util_informNeedPassword( XW_UtilCtxt* uc, XWEnv xwe, XP_U16 playerNum,
                              const XP_UCHAR* name )
{
    LOG_FUNC();
}

static void
wasm_util_trayHiddenChange(XW_UtilCtxt* uc, XWEnv xwe, 
                           XW_TrayVisState newState,
                           XP_U16 nVisibleRows )
{
    LOG_FUNC();
}

static void
wasm_util_yOffsetChange( XW_UtilCtxt* uc, XWEnv xwe, XP_U16 maxOffset,
                         XP_U16 oldOffset, XP_U16 newOffset )
{
    // XP_LOGFF( "(oldOffset=%d; newOffset=%d)", oldOffset, newOffset );
}

static void
wasm_util_turnChanged(XW_UtilCtxt* uc, XWEnv xwe, XP_S16 newTurn)
{
    WasmUtilCtx* wuctxt = (WasmUtilCtx*)uc;
    GameState* gs = wuctxt->closure;
    main_turnChanged(gs, newTurn);
}

static void
wasm_util_notifyDupStatus( XW_UtilCtxt* uc, XWEnv xwe, XP_Bool amHost,
                                    const XP_UCHAR* msg )
{
    LOG_FUNC();
}

static void
wasm_util_informMove( XW_UtilCtxt* uc, XWEnv xwe, XP_S16 turn, 
                      XWStreamCtxt* expl, XWStreamCtxt* words )
{
    XWStreamCtxt* useMe = expl; /*!!words ? words : expl;*/
    XP_U16 len = stream_getSize( useMe );
    XP_UCHAR buf[len+1];
    stream_getBytes( useMe, buf, len );
    buf[len] = '\0';

    WasmUtilCtx* wuctxt = (WasmUtilCtx*)uc;
    GameState* gs = wuctxt->closure;
    main_alert( gs, buf );
}

static void
wasm_util_informUndo( XW_UtilCtxt* uc, XWEnv xwe )
{
    LOG_FUNC();
}

static void
wasm_util_informNetDict( XW_UtilCtxt* uc, XWEnv xwe, XP_LangCode lang,
                                  const XP_UCHAR* oldName,
                                  const XP_UCHAR* newName,
                                  const XP_UCHAR* newSum,
                                  XWPhoniesChoice phoniesAction )
{
    LOG_FUNC();
}

static void
wasm_util_notifyGameOver( XW_UtilCtxt* uc, XWEnv xwe, XP_S16 quitter )
{
    WasmUtilCtx* wuctxt = (WasmUtilCtx*)uc;
    GameState* gs = wuctxt->closure;
    main_showGameOver( gs );
}

static XP_Bool
wasm_util_engineProgressCallback( XW_UtilCtxt* uc, XWEnv xwe )
{
    // LOG_RETURN_VOID();
    return XP_TRUE;
}

static void
wasm_util_setTimer( XW_UtilCtxt* uc, XWEnv xwe, XWTimerReason why, XP_U16 when,
                    XWTimerProc proc, void* closure )
{
    XP_LOGFF( "(why: %d)", why );
    WasmUtilCtx* wuctxt = (WasmUtilCtx*)uc;
    GameState* gs = wuctxt->closure;
    main_set_timer( gs, why, when, proc, closure );
    LOG_RETURN_VOID();
}

static void
wasm_util_clearTimer( XW_UtilCtxt* uc, XWEnv xwe, XWTimerReason why )
{
    LOG_FUNC();
    WasmUtilCtx* wuctxt = (WasmUtilCtx*)uc;
    GameState* gs = wuctxt->closure;
    main_clear_timer( gs, why );
}

static XP_Bool
on_idle( void* closure )
{
    WasmUtilCtx* wuctxt = (WasmUtilCtx*)closure;
    GameState* gs = wuctxt->closure;
    return server_do( gs->game.server, NULL );
}

static void
wasm_util_requestTime( XW_UtilCtxt* uc, XWEnv xwe )
{
    WasmUtilCtx* wuctxt = (WasmUtilCtx*)uc;
    GameState* gs = wuctxt->closure;
    main_set_idle( gs, on_idle, wuctxt );
}

static XP_Bool
wasm_util_altKeyDown( XW_UtilCtxt* uc, XWEnv xwe )
{
    LOG_FUNC();
    return XP_FALSE;
}

static DictionaryCtxt*
wasm_util_makeEmptyDict( XW_UtilCtxt* uc, XWEnv xwe )
{
    WasmUtilCtx* wuctxt = (WasmUtilCtx*)uc;
    GameState* gs = wuctxt->closure;
    return wasm_dictionary_make_empty( gs->globals );
}

static void
wasm_util_notifyIllegalWords( XW_UtilCtxt* uc, XWEnv xwe, BadWordInfo* bwi,
                              XP_U16 turn, XP_Bool turnLost )
{
    XP_UCHAR words[256];
    int offset = 0;

    for ( int ii = 0; ;  ) {
        offset += XP_SNPRINTF( &words[offset], VSIZE(words) - offset, "%s",
                               bwi->words[ii] );
        if ( ++ii >= bwi->nWords ) {
            break;
        }
        offset += XP_SNPRINTF( &words[offset], VSIZE(words) - offset, ", " );
    }
    
    XP_UCHAR buf[256];
    XP_SNPRINTF( buf, VSIZE(buf), "Word[s] \"%s\" not in the current "
                 "dictionary (%s). Use anyway?", words, bwi->dictName );

    WasmUtilCtx* wuctxt = (WasmUtilCtx*)uc;
    GameState* gs = wuctxt->closure;
    main_query( gs, buf, query_proc_notifyMove, uc );
}

static void
wasm_util_remSelected( XW_UtilCtxt* uc, XWEnv xwe )
{
    WasmUtilCtx* wuctxt = (WasmUtilCtx*)uc;
    GameState* gs = wuctxt->closure;
    main_showRemaining( gs );
}

static void
wasm_util_timerSelected( XW_UtilCtxt* uc, XWEnv xwe, XP_Bool inDuplicateMode,
                         XP_Bool canPause )
{
    LOG_FUNC();
}

static void
wasm_util_formatPauseHistory( XW_UtilCtxt* uc, XWEnv xwe, XWStreamCtxt* stream,
                              DupPauseType typ, XP_S16 turn,
                              XP_U32 secsPrev, XP_U32 secsCur,
                              const XP_UCHAR* msg )
{
    LOG_FUNC();
}

static void
wasm_util_bonusSquareHeld( XW_UtilCtxt* uc, XWEnv xwe, XWBonusType bonus )
{
    LOG_FUNC();
}

static void
wasm_util_playerScoreHeld( XW_UtilCtxt* uc, XWEnv xwe, XP_U16 player )
{
    WasmUtilCtx* wuctxt = (WasmUtilCtx*)uc;
    GameState* gs = wuctxt->closure;
    main_playerScoreHeld( gs, player );
}

typedef struct _ForLangState {
    XW_DUtilCtxt* duc;
    XWEnv xwe;
    uint8_t* ptr;
    XP_U32 len;
} ForLangState;

static XP_Bool
gotForLang( void* closure, const XP_UCHAR* keys[] )
{
    XP_LOGFF("name: %s", keys[2]);
    ForLangState* fls = (ForLangState*)closure;
    fls->ptr = wasm_dutil_mallocAndLoad( fls->duc, keys, &fls->len );
    if ( !fls->ptr ) {
        XP_LOGFF( "nothing for %s/%s", keys[1], keys[2] );
    }
    return NULL == fls->ptr;
}

static const DictionaryCtxt*
wasm_util_getDict( XW_UtilCtxt* uc, XWEnv xwe,
                   XP_LangCode lang, const XP_UCHAR* dictName )
{
    XP_LOGFF( "(dictName: %s)", dictName );
    WasmUtilCtx* wuctxt = (WasmUtilCtx*)uc;
    GameState* gs = wuctxt->closure;
    Globals* globals = gs->globals;
    XW_DUtilCtxt* duc = util_getDevUtilCtxt(uc, xwe);

    const char* lc = lcToLocale( lang );

    const DictionaryCtxt* result = dmgr_get( globals->dictMgr, xwe, dictName );
    if ( !result ) {
        XP_U32 len = 0;
        const char* keys[] = {KEY_DICTS, lc, KEY_DICTS, dictName, NULL };

        uint8_t* ptr = wasm_dutil_mallocAndLoad( duc, keys, &len );
        if ( !ptr ) {
            XP_LOGFF( "trying for another %s dict", lc );
            ForLangState fls = { .duc = duc,
                                 .xwe = xwe,
            };
            const char* langKeys[] = {KEY_DICTS, lc, KEY_DICTS, KEY_WILDCARD, NULL};
            dutil_forEach( duc, xwe, langKeys, gotForLang, &fls );
            if ( !!fls.ptr ) {
                ptr = fls.ptr;
                len = fls.len;
            }
        }

        if ( !!ptr ) {
            result = wasm_dictionary_make( globals, xwe, dictName, ptr, len );
            XP_FREE( globals->mpool, ptr );
            dmgr_put( globals->dictMgr, xwe, dictName, result );
        }
    }

    if ( !result ) {
        main_needDictForGame( gs, lang, dictName );
    }

    XP_LOGFF("(%s, %s)=>%p", lc, dictName, result );
    return result;
}

#ifdef XWFEATURE_BOARDWORDS
static void
wasm_util_cellSquareHeld( XW_UtilCtxt* uc, XWEnv xwe, XWStreamCtxt* words )
{
    char* ptr = (char*)stream_getPtr(words);
    const char* end = ptr + stream_getSize(words);
    while ( ptr < end ) {
        XP_LOGFF( "word: %s", (char*)ptr );
        ptr += 1 + strlen(ptr);
    }
}
#endif

static void
wasm_util_informMissing(XW_UtilCtxt* uc, XWEnv xwe, XP_Bool isServer, 
                        const CommsAddrRec* addr, XP_U16 nDevs,
                        XP_U16 nMissing )
{
    LOG_FUNC();
}

static void
wasm_util_addrChange( XW_UtilCtxt* uc, XWEnv xwe, const CommsAddrRec* oldAddr,
                      const CommsAddrRec* newAddr )
{
    LOG_FUNC();
}

static void
wasm_util_informWordsBlocked( XW_UtilCtxt* uc, XWEnv xwe, XP_U16 nBadWords,
                               XWStreamCtxt* words, const XP_UCHAR* dictName )
{
    LOG_FUNC();
}

#ifdef XWFEATURE_SEARCHLIMIT
static XP_Bool
wasm_util_getTraySearchLimits( XW_UtilCtxt* uc, XWEnv xwe, 
                               XP_U16* min, XP_U16* max )
{
    LOG_FUNC();
}
#endif

static void
wasm_util_showChat( XW_UtilCtxt* uc, XWEnv xwe, const XP_UCHAR* const msg, 
                    XP_S16 from, XP_U32 timestamp )
{
    LOG_FUNC();
}

static XW_DUtilCtxt*
wasm_util_getDevUtilCtxt( XW_UtilCtxt* uc, XWEnv xwe )
{
    WasmUtilCtx* wuctxt = (WasmUtilCtx*)uc;
    return wuctxt->dctxt;
}

XW_UtilCtxt*
wasm_util_make( MPFORMAL CurGameInfo* gi, XW_DUtilCtxt* dctxt, GameState* closure )
{
    LOG_FUNC();
    WasmUtilCtx* wuctxt = XP_MALLOC( mpool, sizeof(*wuctxt) );
    wuctxt->super.vtable = XP_MALLOC( mpool, sizeof(*wuctxt->super.vtable) );
    MPASSIGN( wuctxt->super.mpool, mpool );
    wuctxt->super.gameInfo = gi;

    wuctxt->dctxt = dctxt;
    wuctxt->closure = closure;

    SET_VTABLE_ENTRY( wuctxt->super.vtable, util_userError, wasm );
    SET_VTABLE_ENTRY( wuctxt->super.vtable, util_makeStreamFromAddr, wasm );
    SET_VTABLE_ENTRY( wuctxt->super.vtable, util_getSquareBonus, wasm );

    SET_VTABLE_ENTRY( wuctxt->super.vtable, util_notifyMove, wasm );
    SET_VTABLE_ENTRY( wuctxt->super.vtable, util_notifyTrade, wasm );
    SET_VTABLE_ENTRY( wuctxt->super.vtable, util_notifyPickTileBlank, wasm );
    SET_VTABLE_ENTRY( wuctxt->super.vtable, util_informNeedPickTiles, wasm );
    SET_VTABLE_ENTRY( wuctxt->super.vtable, util_informNeedPassword, wasm );
    SET_VTABLE_ENTRY( wuctxt->super.vtable, util_trayHiddenChange, wasm );
    SET_VTABLE_ENTRY( wuctxt->super.vtable, util_yOffsetChange, wasm );
#ifdef XWFEATURE_TURNCHANGENOTIFY
    SET_VTABLE_ENTRY( wuctxt->super.vtable, util_turnChanged, wasm );
#endif
    SET_VTABLE_ENTRY( wuctxt->super.vtable, util_notifyDupStatus, wasm );
    SET_VTABLE_ENTRY( wuctxt->super.vtable, util_informMove, wasm );
    SET_VTABLE_ENTRY( wuctxt->super.vtable, util_informUndo, wasm );
    SET_VTABLE_ENTRY( wuctxt->super.vtable, util_informNetDict, wasm );
    SET_VTABLE_ENTRY( wuctxt->super.vtable, util_notifyGameOver, wasm );
    SET_VTABLE_ENTRY( wuctxt->super.vtable, util_engineProgressCallback, wasm );
#ifdef XWFEATURE_HILITECELL
    SET_VTABLE_ENTRY( wuctxt->super.vtable, util_hiliteCell, wasm );
#endif

    SET_VTABLE_ENTRY( wuctxt->super.vtable, util_setTimer, wasm );
    SET_VTABLE_ENTRY( wuctxt->super.vtable, util_clearTimer, wasm );
    SET_VTABLE_ENTRY( wuctxt->super.vtable, util_requestTime, wasm );
    SET_VTABLE_ENTRY( wuctxt->super.vtable, util_altKeyDown, wasm );
    SET_VTABLE_ENTRY( wuctxt->super.vtable, util_makeEmptyDict, wasm );
    SET_VTABLE_ENTRY( wuctxt->super.vtable, util_notifyIllegalWords, wasm );
    SET_VTABLE_ENTRY( wuctxt->super.vtable, util_remSelected, wasm );
    SET_VTABLE_ENTRY( wuctxt->super.vtable, util_timerSelected, wasm );
    SET_VTABLE_ENTRY( wuctxt->super.vtable, util_formatPauseHistory, wasm );
    SET_VTABLE_ENTRY( wuctxt->super.vtable, util_bonusSquareHeld, wasm );
    SET_VTABLE_ENTRY( wuctxt->super.vtable, util_playerScoreHeld, wasm );

    SET_VTABLE_ENTRY( wuctxt->super.vtable, util_getDict, wasm );

#ifdef XWFEATURE_BOARDWORDS
    SET_VTABLE_ENTRY( wuctxt->super.vtable, util_cellSquareHeld, wasm );
#endif
    SET_VTABLE_ENTRY( wuctxt->super.vtable, util_informMissing, wasm );
    SET_VTABLE_ENTRY( wuctxt->super.vtable, util_addrChange, wasm );
    SET_VTABLE_ENTRY( wuctxt->super.vtable, util_informWordsBlocked, wasm );
#ifdef XWFEATURE_CHAT
    SET_VTABLE_ENTRY( wuctxt->super.vtable, util_showChat, wasm );
#endif
    SET_VTABLE_ENTRY( wuctxt->super.vtable, util_getDevUtilCtxt, wasm );

    size_t sizeInBytes = sizeof(*wuctxt->super.vtable);
    assertTableFull( wuctxt->super.vtable, sizeInBytes, "wasmutilctx" );

    LOG_RETURNF( "%p", wuctxt );
    return (XW_UtilCtxt*)wuctxt;
}

void
wasm_util_destroy( XW_UtilCtxt* util )
{
    LOG_FUNC();
    WasmUtilCtx* wuctxt = (WasmUtilCtx*)util;
    XP_FREEP( wuctxt->super.mpool, &wuctxt->super.vtable );
    XP_FREEP( wuctxt->super.mpool, &wuctxt );
}
