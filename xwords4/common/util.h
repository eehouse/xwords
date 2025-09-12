/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 1997 - 2022 by Eric House (xwords@eehouse.org).  All rights
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

#ifndef _UTIL_H_
#define _UTIL_H_

#include "comtypes.h"

#include "dawg.h"
#include "model.h"
#include "board.h"
#include "comms.h"
#include "dutil.h"
#include "gameinfo.h"

#include "xwrelay.h"

#define LETTER_NONE '\0'

typedef enum {
    ERR_NONE,                   /* 0 is special case */
    ERR_TILES_NOT_IN_LINE,   /* scoring a move where tiles aren't in line */
    ERR_NO_EMPTIES_IN_TURN,
    ERR_TWO_TILES_FIRST_MOVE,
    ERR_TILES_MUST_CONTACT,
/*     ERR_NO_HINT_MID_TURN, */
    ERR_TOO_FEW_TILES_LEFT_TO_TRADE,
    ERR_NOT_YOUR_TURN,
    ERR_NO_PEEK_ROBOT_TILES,
    ERR_SERVER_DICT_WINS,
    ERR_NO_PEEK_REMOTE_TILES,
    ERR_REG_UNEXPECTED_USER, /* server asked to register too many remote
                                users */
    ERR_REG_SERVER_SANS_REMOTE,
    STR_NEED_BT_HOST_ADDR,
    ERR_NO_EMPTY_TRADE,
    ERR_TOO_MANY_TRADE,
/*     ERR_CANT_ENGINE_MID_MOVE, */
/*     ERR_NOT_YOUR_TURN_TO_TRADE, */
/*     ERR_NOT_YOUR_TURN_TO_MOVE, */
    ERR_CANT_UNDO_TILEASSIGN,
    ERR_CANT_HINT_WHILE_DISABLED,
    ERR_NO_HINT_FOUND,          /* not really an error... */

    ERR_RELAY_BASE,
} UtilErrID;

#define PICKER_PICKALL -1
#define PICKER_BACKUP -2

typedef struct PickInfo {
    const XP_UCHAR** curTiles;
    XP_U16 nCurTiles;
    XP_U16 nTotal;              /* count to fetch for turn, <= MAX_TRAY_TILES */
    XP_U16 thisPick;            /* <= nTotal */
} PickInfo;

typedef struct _BadWordInfo {
    XP_U16 nWords;
    /* Null-terminated array of ptrs */
    const XP_UCHAR* words[MAX_TRAY_TILES+2]; /* can form in both directions */
} BadWordInfo;

/* UtilTimerProc returns true if redraw was necessitated by what the proc did */
typedef XP_Bool (*UtilTimerProc)( void* closure, XWEnv xwe, XWTimerReason why );

/* Platform-specific utility functions that need to be
 */
typedef void (*UtilDestroy)( XW_UtilCtxt* uc, XWEnv xwe );
typedef struct UtilVtable {
    UtilDestroy m_util_destroy;
    void (*m_util_userError)( XW_UtilCtxt* uc, XWEnv xwe, UtilErrID id );
    void (*m_util_countChanged)( XW_UtilCtxt* uc, XWEnv xwe,
                                 XP_U16 count, XP_Bool quashed );
    void (*m_util_notifyMove)( XW_UtilCtxt* uc, XWEnv xwe, XWStreamCtxt* stream );
    void (*m_util_notifyTrade)( XW_UtilCtxt* uc, XWEnv xwe, const XP_UCHAR** tiles,
                                XP_U16 nTiles );
    void (*m_util_notifyPickTileBlank)( XW_UtilCtxt* uc, XWEnv xwe, XP_U16 playerNum,
                                        XP_U16 col, XP_U16 row,
                                        const XP_UCHAR** tileFaces, 
                                        XP_U16 nTiles );
    void (*m_util_informNeedPickTiles)( XW_UtilCtxt* uc, XWEnv xwe, XP_Bool isInitial,
                                        XP_U16 player, XP_U16 nToPick,
                                        XP_U16 nFaces, const XP_UCHAR** faces,
                                        const XP_U16* counts );

    void (*m_util_informNeedPassword)( XW_UtilCtxt* uc, XWEnv xwe, XP_U16 playerNum,
                                       const XP_UCHAR* name );

    void (*m_util_trayHiddenChange)(XW_UtilCtxt* uc, XWEnv xwe, 
                                    XW_TrayVisState newState,
                                    XP_U16 nVisibleRows );
    void (*m_util_yOffsetChange)(XW_UtilCtxt* uc, XWEnv xwe, XP_U16 maxOffset,
                                 XP_U16 oldOffset, XP_U16 newOffset );
#ifdef XWFEATURE_TURNCHANGENOTIFY
    void (*m_util_turnChanged)(XW_UtilCtxt* uc, XWEnv xwe, XP_S16 newTurn);
#endif
    void (*m_util_notifyDupStatus)( XW_UtilCtxt* uc, XWEnv xwe, XP_Bool amHost,
                                    const XP_UCHAR* msg );
    void (*m_util_informUndo)( XW_UtilCtxt* uc, XWEnv xwe );
    void (*m_util_informNetDict)( XW_UtilCtxt* uc, XWEnv xwe, const XP_UCHAR* isoCode,
                                  const XP_UCHAR* oldName, const XP_UCHAR* newName,
                                  const XP_UCHAR* newSum,
                                  XWPhoniesChoice phoniesAction );
#ifdef XWFEATURE_HILITECELL
    XP_Bool (*m_util_hiliteCell)( XW_UtilCtxt* uc, XWEnv xwe, XP_U16 col, XP_U16 row );
#endif

    XP_Bool (*m_util_engineProgressCallback)( XW_UtilCtxt* uc, XWEnv xwe );

    XP_Bool (*m_util_altKeyDown)( XW_UtilCtxt* uc, XWEnv xwe );

    void (*m_util_notifyIllegalWords)( XW_UtilCtxt* uc, XWEnv xwe,
                                       const BadWordInfo* bwi,
                                       const XP_UCHAR* dictName,
                                       XP_U16 turn, XP_Bool turnLost,
                                       XP_U32 badWordsKey );

    void (*m_util_remSelected)(XW_UtilCtxt* uc, XWEnv xwe);

    void (*m_util_timerSelected)(XW_UtilCtxt* uc, XWEnv xwe, XP_Bool inDuplicateMode,
                                 XP_Bool canPause);

    void (*m_util_formatPauseHistory)( XW_UtilCtxt* uc, XWEnv xwe, XWStreamCtxt* stream,
                                       DupPauseType typ, XP_S16 turn,
                                       XP_U32 secsPrev, XP_U32 secsCur,
                                       const XP_UCHAR* msg );
    void (*m_util_dictGone)( XW_UtilCtxt* uc, XWEnv xwe, const XP_UCHAR* dictName );


#ifndef XWFEATURE_MINIWIN
    void (*m_util_bonusSquareHeld)( XW_UtilCtxt* uc, XWEnv xwe, XWBonusType bonus );
    void (*m_util_playerScoreHeld)( XW_UtilCtxt* uc, XWEnv xwe, XP_U16 player );
#endif
#ifdef XWFEATURE_BOARDWORDS
    void (*m_util_cellSquareHeld)( XW_UtilCtxt* uc, XWEnv xwe, XWStreamCtxt* words );
#endif

    /* void (*m_util_informMissing)( XW_UtilCtxt* uc, XWEnv xwe, XP_Bool isHost, */
    /*                               const CommsAddrRec* hostAddr, */
    /*                               const CommsAddrRec* selfAddr, XP_U16 nDevs, */
    /*                               XP_U16 nMissing, XP_U16 nInvited, */
    /*                               XP_Bool fromRematch ); */

    void (*m_util_informWordsBlocked)( XW_UtilCtxt* uc, XWEnv xwe, XP_U16 nBadWords,
                                       XWStreamCtxt* words, const XP_UCHAR* dictName );
#ifdef XWFEATURE_SEARCHLIMIT
    XP_Bool (*m_util_getTraySearchLimits)(XW_UtilCtxt* uc, XWEnv xwe, 
                                          XP_U16* min, XP_U16* max );
#endif

#ifdef XWFEATURE_CHAT
    void (*m_util_showChat)( XW_UtilCtxt* uc, XWEnv xwe, const XP_UCHAR* const msg, 
                             XP_S16 from, XP_U32 timestamp );
#endif

#ifdef SHOW_PROGRESS
    void (*m_util_engineStarting)( XW_UtilCtxt* uc, XWEnv xwe, XP_U16 nBlanks );
    void (*m_util_engineStopping)( XW_UtilCtxt* uc, XWEnv xwe );
#endif

    // XW_DUtilCtxt* (*m_util_getDevUtilCtxt)( XW_UtilCtxt* uc, XWEnv xwe );

} UtilVtable;

typedef struct UtilTimerState UtilTimerState;

struct XW_UtilCtxt {
    UtilVtable* vtable;
    XW_DUtilCtxt* dutil;
    UtilTimerState* uts;
    const CurGameInfo* gi;
    GameRef gr;
    int refCount;
#ifdef DEBUG
    XP_Bool _inited;
#endif
#ifdef MEM_DEBUG
    MemPoolCtx* _mpool;         /* remove this eventually */
    // MPSLOT
#endif
};

#define util_userError(uc,...)                          \
    (uc)->vtable->m_util_userError((uc), __VA_ARGS__)
#define util_countChanged(uc,...)                          \
    (uc)->vtable->m_util_countChanged((uc), __VA_ARGS__)
#define util_notifyMove(uc,...)                         \
    (uc)->vtable->m_util_notifyMove((uc), __VA_ARGS__)
#define util_notifyTrade(uc,...)                                \
    (uc)->vtable->m_util_notifyTrade((uc), __VA_ARGS__)
#define util_notifyPickTileBlank( uc,...)                               \
    (uc)->vtable->m_util_notifyPickTileBlank( (uc), __VA_ARGS__ )

#ifdef DEBUG
XW_UtilCtxt* check_uc(XW_UtilCtxt* uc);
# define CHECK_UC(UC) check_uc(UC)
#else
# define CHECK_UC(UC) UC
#endif

#define util_destroy(uc, ...)                             \
    (uc)->vtable->m_util_destroy( (uc), __VA_ARGS__ )

#define util_informNeedPickTiles( uc, ...) \
    (uc)->vtable->m_util_informNeedPickTiles( (uc), __VA_ARGS__ )
#define util_makeStreamFromAddr(uc,...)                         \
    (uc)->vtable->m_util_makeStreamFromAddr((uc), __VA_ARGS__)
#define util_informNeedPassword( uc, ... )                              \
    (uc)->vtable->m_util_informNeedPassword( (uc), __VA_ARGS__ )

#define util_trayHiddenChange( uc, ...)                         \
    (uc)->vtable->m_util_trayHiddenChange((uc), __VA_ARGS__)

#define util_yOffsetChange( uc, ...)                        \
    (uc)->vtable->m_util_yOffsetChange((uc), __VA_ARGS__)

#ifdef XWFEATURE_TURNCHANGENOTIFY
# define util_turnChanged( uc, ... )                    \
    (uc)->vtable->m_util_turnChanged( (uc), __VA_ARGS__)
#else
# define util_turnChanged( uc, ... )
#endif

#define util_notifyDupStatus(uc,...)                            \
    (uc)->vtable->m_util_notifyDupStatus( (uc), __VA_ARGS__ )
#define util_informUndo(uc,...) \
    (uc)->vtable->m_util_informUndo( (uc), __VA_ARGS__)
#define util_informNetDict(uc, ... )                      \
    (uc)->vtable->m_util_informNetDict( (uc), __VA_ARGS__)

#ifdef XWFEATURE_HILITECELL
# define util_hiliteCell( uc, ... ) \
    (uc)->vtable->m_util_hiliteCell((uc), __VA_ARGS__)
#endif

#define util_engineProgressCallback( uc, ... ) \
    (uc)->vtable->m_util_engineProgressCallback((uc), __VA_ARGS__)

#define util_altKeyDown( uc, ... )                 \
    (uc)->vtable->m_util_altKeyDown((uc), __VA_ARGS__)
#define util_notifyIllegalWords( uc, ... )                      \
    (uc)->vtable->m_util_notifyIllegalWords((uc), __VA_ARGS__)
#define util_remSelected( uc,... )                        \
    (uc)->vtable->m_util_remSelected((uc), __VA_ARGS__)

#define util_timerSelected( uc, ... )                        \
    (uc)->vtable->m_util_timerSelected((uc), __VA_ARGS__)

#define util_formatPauseHistory( uc, ...)                               \
    (uc)->vtable->m_util_formatPauseHistory( (uc), __VA_ARGS__)
#define util_dictGone( uc, ...)                         \
    (uc)->vtable->m_util_dictGone( (uc), __VA_ARGS__)

#ifndef XWFEATURE_MINIWIN
# define util_bonusSquareHeld( uc, ... )                                  \
    (uc)->vtable->m_util_bonusSquareHeld( (uc), __VA_ARGS__ )
# define util_playerScoreHeld( uc, ... )                                \
    (uc)->vtable->m_util_playerScoreHeld( (uc), __VA_ARGS__ )
#endif
#ifdef XWFEATURE_BOARDWORDS
#define util_cellSquareHeld(uc, ...)                      \
    (uc)->vtable->m_util_cellSquareHeld( (uc), __VA_ARGS__)
#endif

/* #define util_informMissing( uc, ...)                    \ */
/*     (uc)->vtable->m_util_informMissing((uc), __VA_ARGS__) */
#define util_informWordsBlocked(uc, ...)                        \
    (uc)->vtable->m_util_informWordsBlocked( (uc), __VA_ARGS__)

#ifdef XWFEATURE_SEARCHLIMIT
#define util_getTraySearchLimits(uc, ...) \
    (uc)->vtable->m_util_getTraySearchLimits((uc), __VA_ARGS__)
#endif

#ifdef XWFEATURE_CHAT
# define util_showChat( uc, ... ) (uc)->vtable->m_util_showChat((uc), __VA_ARGS__)
#endif

# ifdef SHOW_PROGRESS
# define util_engineStarting( uc, ... ) \
    (uc)->vtable->m_util_engineStarting((uc), __VA_ARGS__)
# define util_engineStopping( uc, ... ) \
    (uc)->vtable->m_util_engineStopping((uc), __VA_ARGS__)
# else
# define util_engineStarting( uc, ... )
# define util_engineStopping( uc, ... )
# endif

# define util_getDevUtilCtxt(uc) ((uc)->dutil)

void util_super_init( MPFORMAL XW_UtilCtxt* util, const CurGameInfo* gi,
                      XW_DUtilCtxt* dutil, GameRef gr,
                      UtilDestroy destProc );
void util_super_cleanup( XW_UtilCtxt* util, XWEnv xwe );
const CurGameInfo* util_getGI(XW_UtilCtxt* util);
void util_clearTimer( XW_UtilCtxt* uc, XWEnv xwe, XWTimerReason why );
void util_setTimer( XW_UtilCtxt* uc, XWEnv xwe, XWTimerReason why,
                    XP_U16 inSecs, UtilTimerProc proc, void* closure );
/* Replace me with something updating CurGameInfo with invitee names pending
   their registration */
void util_getInviteeName( XW_UtilCtxt* uc, XWEnv xwe, XP_U16 plyrNum,
                          XP_UCHAR* buf, XP_U16* bufLen );

XW_UtilCtxt* _util_ref( XW_UtilCtxt* util
#ifdef DEBUG
                    ,const char* proc, int line
#endif
                    );
void _util_unref( XW_UtilCtxt* util, XWEnv xwe
#ifdef DEBUG
                    ,const char* proc, int line
#endif
                  );

#ifdef DEBUG
# define util_ref(UC) _util_ref((UC), __func__, __LINE__)
# define util_unref(UC, XWE) _util_unref((UC), XWE, __func__, __LINE__)
#else
# define util_ref(UC) _util_ref((UC))
# define util_unref(UC, XWE) _util_unref((UC), XWE)
#endif

# ifdef MEM_DEBUG
MemPoolCtx* util_getMemPool( const XW_UtilCtxt* util, XWEnv xwe );
# endif

#endif
