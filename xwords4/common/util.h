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
    const XP_UCHAR* dictName;
    /* Null-terminated array of ptrs */
    const XP_UCHAR* words[MAX_TRAY_TILES+2]; /* can form in both directions */
} BadWordInfo;

/* XWTimerProc returns true if redraw was necessitated by what the proc did */
typedef XP_Bool (*XWTimerProc)( void* closure, XWEnv xwe, XWTimerReason why );

/* Platform-specific utility functions that need to be
 */
typedef struct UtilVtable {
    XWStreamCtxt* (*m_util_makeStreamFromAddr)( XW_UtilCtxt* uc, XWEnv xwe,
                                                XP_PlayerAddr channelNo );
    void (*m_util_userError)( XW_UtilCtxt* uc, XWEnv xwe, UtilErrID id );

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
    void (*m_util_informMove)( XW_UtilCtxt* uc, XWEnv xwe, XP_S16 turn, 
                               XWStreamCtxt* expl, XWStreamCtxt* words );
    void (*m_util_informUndo)( XW_UtilCtxt* uc, XWEnv xwe );
    void (*m_util_informNetDict)( XW_UtilCtxt* uc, XWEnv xwe, const XP_UCHAR* isoCode,
                                  const XP_UCHAR* oldName, const XP_UCHAR* newName,
                                  const XP_UCHAR* newSum,
                                  XWPhoniesChoice phoniesAction );
    const DictionaryCtxt* (*m_util_getDict)( XW_UtilCtxt* uc, XWEnv xwe,
                                             const XP_UCHAR* isoCode,
                                             const XP_UCHAR* dictName );
    void (*m_util_notifyGameOver)( XW_UtilCtxt* uc, XWEnv xwe, XP_S16 quitter );
#ifdef XWFEATURE_HILITECELL
    XP_Bool (*m_util_hiliteCell)( XW_UtilCtxt* uc, XWEnv xwe, XP_U16 col, XP_U16 row );
#endif

    XP_Bool (*m_util_engineProgressCallback)( XW_UtilCtxt* uc, XWEnv xwe );

    void (*m_util_setTimer)( XW_UtilCtxt* uc, XWEnv xwe, XWTimerReason why, XP_U16 when,
                             XWTimerProc proc, void* closure );
    void (*m_util_clearTimer)( XW_UtilCtxt* uc, XWEnv xwe, XWTimerReason why );

    void (*m_util_requestTime)( XW_UtilCtxt* uc, XWEnv xwe );

    XP_Bool (*m_util_altKeyDown)( XW_UtilCtxt* uc, XWEnv xwe );
    DictionaryCtxt* (*m_util_makeEmptyDict)( XW_UtilCtxt* uc, XWEnv xwe );

    void (*m_util_notifyIllegalWords)( XW_UtilCtxt* uc, XWEnv xwe, BadWordInfo* bwi,
                                       XP_U16 turn, XP_Bool turnLost );

    void (*m_util_remSelected)(XW_UtilCtxt* uc, XWEnv xwe);

    /* Solving a time-limited problem of games that know how to connect via
       relay but not MQTT. Once this method succeeds and the platform
       implementation calls comms_addMQTTDevID() we never need it again for
       that game. */
    void (*m_util_getMQTTIDsFor)( XW_UtilCtxt* uc, XWEnv xwe, XP_U16 nRelayIDs,
                                  const XP_UCHAR* relayIDs[] );

    void (*m_util_timerSelected)(XW_UtilCtxt* uc, XWEnv xwe, XP_Bool inDuplicateMode,
                                 XP_Bool canPause);

    void (*m_util_formatPauseHistory)( XW_UtilCtxt* uc, XWEnv xwe, XWStreamCtxt* stream,
                                       DupPauseType typ, XP_S16 turn,
                                       XP_U32 secsPrev, XP_U32 secsCur,
                                       const XP_UCHAR* msg );

#ifndef XWFEATURE_MINIWIN
    void (*m_util_bonusSquareHeld)( XW_UtilCtxt* uc, XWEnv xwe, XWBonusType bonus );
    void (*m_util_playerScoreHeld)( XW_UtilCtxt* uc, XWEnv xwe, XP_U16 player );
#endif
#ifdef XWFEATURE_BOARDWORDS
    void (*m_util_cellSquareHeld)( XW_UtilCtxt* uc, XWEnv xwe, XWStreamCtxt* words );
#endif

    void (*m_util_informMissing)( XW_UtilCtxt* uc, XWEnv xwe, XP_Bool isHost,
                                  const CommsAddrRec* hostAddr,
                                  const CommsAddrRec* selfAddr, XP_U16 nDevs,
                                  XP_U16 nMissing, XP_U16 nInvited );

    void (*m_util_informWordsBlocked)( XW_UtilCtxt* uc, XWEnv xwe, XP_U16 nBadWords,
                                       XWStreamCtxt* words, const XP_UCHAR* dictName );

    void (*m_util_getInviteeName)( XW_UtilCtxt* uc, XWEnv xwe, XP_U16 plyrNum,
                                   XP_UCHAR* buf, XP_U16* bufLen );

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

    XW_DUtilCtxt* (*m_util_getDevUtilCtxt)( XW_UtilCtxt* uc, XWEnv xwe );

} UtilVtable;


struct XW_UtilCtxt {
    UtilVtable* vtable;

    struct CurGameInfo* gameInfo;

    void* closure;
    MPSLOT
};

#define util_makeStreamFromAddr(uc,e,a) \
         (uc)->vtable->m_util_makeStreamFromAddr((uc), (e),(a))

#define util_userError(uc,e,err)                    \
         (uc)->vtable->m_util_userError((uc),(e),(err))

#define util_notifyMove(uc,e, str)                         \
         (uc)->vtable->m_util_notifyMove((uc), (e), (str))

#define util_notifyTrade(uc,e, tx, nt)                            \
        (uc)->vtable->m_util_notifyTrade((uc), (e), (tx), (nt))

#define util_notifyPickTileBlank( uc,e, c, r, n, tx, nt )                  \
        (uc)->vtable->m_util_notifyPickTileBlank( (uc), (e), (c), (r), (n), (tx), (nt) )

#define util_informNeedPickTiles( uc,e, ii, pl, np, nt, fc, cn )           \
        (uc)->vtable->m_util_informNeedPickTiles( (uc), (e), (ii), (pl), (np), (nt), (fc), (cn) )

#define util_informNeedPassword( uc,e, pn, n ) \
         (uc)->vtable->m_util_informNeedPassword( (uc), (e), (pn), (n) )

#define util_trayHiddenChange( uc,e, b, n ) \
         (uc)->vtable->m_util_trayHiddenChange((uc), (e), (b), (n))

#define util_yOffsetChange( uc,e, m, o, n )                        \
         (uc)->vtable->m_util_yOffsetChange((uc), (e), (m), (o), (n) )

#ifdef XWFEATURE_TURNCHANGENOTIFY
# define util_turnChanged( uc,e, t )                    \
        (uc)->vtable->m_util_turnChanged( (uc), (e), (t) )
#else
# define util_turnChanged( uc,e, t )
#endif

#define util_notifyDupStatus(uc,e, h, m)                      \
    (uc)->vtable->m_util_notifyDupStatus( (uc), (e), (h), (m) )
#define util_informMove(uc,e,t,ex,w)                               \
         (uc)->vtable->m_util_informMove( (uc), (e), (t), (ex), (w))
#define util_informUndo(uc,e) \
         (uc)->vtable->m_util_informUndo( (uc), (e))
#define util_informNetDict(uc,e, cd, on, nn, ns, pa )                      \
         (uc)->vtable->m_util_informNetDict( (uc), (e), (cd), (on), (nn), (ns), \
                                             (pa) )
#define util_getDict( uc, xwe, isoCode, dictName )                     \
         (uc)->vtable->m_util_getDict((uc), (xwe), (isoCode), (dictName))

#define util_notifyGameOver( uc,e, q )                  \
         (uc)->vtable->m_util_notifyGameOver((uc), (e), (q))

#ifdef XWFEATURE_HILITECELL
# define util_hiliteCell( uc,e, c, r ) \
         (uc)->vtable->m_util_hiliteCell((uc), (e), (c), (r))
#endif

#define util_engineProgressCallback( uc,e ) \
         (uc)->vtable->m_util_engineProgressCallback((uc), (e))

#define util_setTimer( uc,e, why, when, proc, clos ) \
         (uc)->vtable->m_util_setTimer((uc), (e),(why),(when),(proc),(clos))
#define util_clearTimer( uc,e, why ) \
         (uc)->vtable->m_util_clearTimer((uc), (e),(why))

#define util_requestTime( uc,e ) \
         (uc)->vtable->m_util_requestTime((uc), (e))

#define util_altKeyDown( uc,e )                 \
    (uc)->vtable->m_util_altKeyDown((uc),(e))

#define util_makeEmptyDict( uc, e )                     \
    (uc)->vtable->m_util_makeEmptyDict((uc), (e))

#define util_notifyIllegalWords( uc,e, w, p, b ) \
         (uc)->vtable->m_util_notifyIllegalWords((uc), (e),(w),(p),(b))

#define util_remSelected( uc,e )                        \
         (uc)->vtable->m_util_remSelected((uc), (e))

#define util_getMQTTIDsFor( uc, e, cnt, rids )               \
    (uc)->vtable->m_util_getMQTTIDsFor((uc), (e), (cnt), (rids))

#define util_timerSelected( uc,e, dm, cp )                        \
         (uc)->vtable->m_util_timerSelected((uc), (e), (dm), (cp))

#define util_formatPauseHistory( uc,e, s, typ, turn, secsPrev, secsCur, msg ) \
    (uc)->vtable->m_util_formatPauseHistory( (uc), (e), (s), (typ), (turn),  \
                                             (secsPrev), (secsCur), (msg) )

#ifndef XWFEATURE_MINIWIN
# define util_bonusSquareHeld( uc,e, b )                                  \
         (uc)->vtable->m_util_bonusSquareHeld( (uc), (e), (b) )
# define util_playerScoreHeld( uc,e, player )                                \
         (uc)->vtable->m_util_playerScoreHeld( (uc), (e), (player) )
#endif
#ifdef XWFEATURE_BOARDWORDS
#define util_cellSquareHeld(uc,e, s)                      \
    (uc)->vtable->m_util_cellSquareHeld( (uc), (e), (s) )
#endif

#define util_informMissing( uc, e, is, ha, sa, nd, nm, ni )             \
    (uc)->vtable->m_util_informMissing((uc), (e), (is), (ha), (sa), (nd), (nm), (ni) )

#define util_informWordsBlocked(uc,e, c, w, d)                        \
    (uc)->vtable->m_util_informWordsBlocked( (uc), (e), (c), (w), (d) )
#define util_getInviteeName(uc, xwe, plyrNum, buf, len )              \
    (uc)->vtable->m_util_getInviteeName( (uc), (xwe), (plyrNum), (buf), (len) )

#ifdef XWFEATURE_SEARCHLIMIT
#define util_getTraySearchLimits(uc,e,min,max) \
         (uc)->vtable->m_util_getTraySearchLimits((uc), (e), (min), (max))
#endif

#ifdef XWFEATURE_CHAT
# define util_showChat( uc,e, m, f, ts ) (uc)->vtable->m_util_showChat((uc), (e),(m),(f), (ts))
#endif

# ifdef SHOW_PROGRESS
# define util_engineStarting( uc,e, nb ) \
         (uc)->vtable->m_util_engineStarting((uc), (e),(nb))
# define util_engineStopping( uc,e ) \
         (uc)->vtable->m_util_engineStopping((uc), (e))
# else
# define util_engineStarting( uc,e, nb )
# define util_engineStopping( uc,e )
# endif

# define util_getDevUtilCtxt(uc,e) \
    (uc)->vtable->m_util_getDevUtilCtxt( (uc), (e) )

#endif
