 /* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 1997 - 2000 by Eric House (fixin@peak.org).  All rights reserved.
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
#include "mempool.h"
#include "vtabmgr.h"

#define LETTER_NONE '\0'

typedef enum {
    ERR_TILES_NOT_IN_LINE,   /* scoring a move where tiles aren't in line */
    ERR_NO_EMPTIES_IN_TURN,
    ERR_TWO_TILES_FIRST_MOVE,
    ERR_TILES_MUST_CONTACT,
/*     ERR_NO_HINT_MID_TURN, */
    ERR_TOO_FEW_TILES_LEFT_TO_TRADE,
    ERR_NOT_YOUR_TURN,
    ERR_NO_PEEK_ROBOT_TILES,
#ifndef XWFEATURE_STANDALONE_ONLY
    ERR_SERVER_DICT_WINS,
    ERR_NO_PEEK_REMOTE_TILES,
    ERR_REG_UNEXPECTED_USER,	/* server asked to register too many remote
				   users */
#endif
    ERR_CANT_TRADE_MID_MOVE,
    ERR_CANT_ENGINE_MID_MOVE,
/*     ERR_NOT_YOUR_TURN_TO_TRADE, */
/*     ERR_NOT_YOUR_TURN_TO_MOVE, */
    ERR_CANT_UNDO_TILEASSIGN
} UtilErrID;

typedef enum {
    QUERY_COMMIT_TURN,		/* 0 means cancel; 1 means commit */
    QUERY_COMMIT_TRADE,
    QUERY_ROBOT_MOVE,
    QUERY_ROBOT_TRADE
} UtilQueryID;

typedef enum {
    PICK_FOR_BLANK
    , PICK_FOR_CHEAT
} PICK_WHY;

#define PICKER_PICKALL -1
#define PICKER_BACKUP -2

typedef struct PickInfo {
    XP_UCHAR4* curTiles;
    XP_U16 nCurTiles;
    XP_U16 nTotal;
    XP_U16 thisPick;            /* <= nTotal */
    PICK_WHY why;
} PickInfo;

typedef struct BadWordInfo {
    XP_U16 nWords;
    XP_UCHAR* words[MAX_TRAY_TILES+1]; /* can form in both directions */
} BadWordInfo;

/* Platform-specific utility functions that need to be
 */
typedef struct UtilVtable {
    
    VTableMgr* (*m_util_getVTManager)(XW_UtilCtxt* uc);

    XWStreamCtxt* (*m_util_makeStreamFromAddr )(XW_UtilCtxt* uc,
                                                XP_U16 channelNo );
    
    XWBonusType (*m_util_getSquareBonus)( XW_UtilCtxt* uc, ModelCtxt* model,
                                          XP_U16 col, XP_U16 row );
    void (*m_util_userError)( XW_UtilCtxt* uc, UtilErrID id );

    XP_Bool (*m_util_userQuery)( XW_UtilCtxt* uc, UtilQueryID id,
                                 XWStreamCtxt* stream );

    /* return of < 0 means computer should pick */
    XP_S16 (*m_util_userPickTile)( XW_UtilCtxt* uc, PickInfo* pi, 
                                   XP_U16 playerNum,
                                   XP_UCHAR4* texts, XP_U16 nTiles );

    XP_Bool (*m_util_askPassword)( XW_UtilCtxt* uc, const XP_UCHAR* name,
                                   XP_UCHAR* buf, XP_U16* len );

    void (*m_util_trayHiddenChange)(XW_UtilCtxt* uc, 
                                    XW_TrayVisState newState );
    void (*m_util_yOffsetChange)(XW_UtilCtxt* uc, XP_U16 oldOffset,
                                 XP_U16 newOffset );

    void (*m_util_notifyGameOver)( XW_UtilCtxt* uc );

    XP_Bool (*m_util_hiliteCell)( XW_UtilCtxt* uc, XP_U16 col, XP_U16 row );

    XP_Bool (*m_util_engineProgressCallback)( XW_UtilCtxt* uc );

    void (*m_util_setTimer)( XW_UtilCtxt* uc, XWTimerReason why );

    void (*m_util_requestTime)( XW_UtilCtxt* uc );

    XP_U32 (*m_util_getCurSeconds)( XW_UtilCtxt* uc );

    DictionaryCtxt* (*m_util_makeEmptyDict)( XW_UtilCtxt* uc );

    XP_UCHAR* (*m_util_getUserString)( XW_UtilCtxt* uc, XP_U16 stringCode );

    XP_Bool (*m_util_warnIllegalWord)( XW_UtilCtxt* uc, BadWordInfo* bwi, 
                                       XP_U16 turn, XP_Bool turnLost );
#ifdef BEYOND_IR
    void (*m_util_listenPortChange)( XW_UtilCtxt* uc, XP_U16 listenPort );
#endif

#ifdef SHOW_PROGRESS
    void (*m_util_engineStarting)( XW_UtilCtxt* uc );
    void (*m_util_engineStopping)( XW_UtilCtxt* uc );
#endif
} UtilVtable;


struct XW_UtilCtxt {
    UtilVtable* vtable;

    struct CurGameInfo* gameInfo;

    void* closure;
    MPSLOT
};

#define util_getVTManager(uc) \
         (uc)->vtable->m_util_getVTManager((uc))

#define util_makeStreamFromAddr(uc,a) \
         (uc)->vtable->m_util_makeStreamFromAddr((uc),(a))

#define util_getSquareBonus(uc,m,c,r) \
         (uc)->vtable->m_util_getSquareBonus((uc),(m),(c),(r))

#define util_userError(uc,err) \
         (uc)->vtable->m_util_userError((uc),(err))

#define util_userQuery(uc,qcode,str) \
         (uc)->vtable->m_util_userQuery((uc),(qcode),(str))

#define util_userPickTile( uc, w, n, tx, nt ) \
         (uc)->vtable->m_util_userPickTile( (uc), (w), (n), (tx), (nt) )
#define util_askPassword( uc, n, b, lp ) \
         (uc)->vtable->m_util_askPassword( (uc), (n), (b), (lp) )

#define util_trayHiddenChange( uc, b ) \
         (uc)->vtable->m_util_trayHiddenChange((uc), (b))

#define util_yOffsetChange( uc, o, n ) \
         (uc)->vtable->m_util_yOffsetChange((uc), (o), (n) )

#define util_notifyGameOver( uc ) \
         (uc)->vtable->m_util_notifyGameOver((uc))

#define util_hiliteCell( uc, c, r ) \
         (uc)->vtable->m_util_hiliteCell((uc), (c), (r))

#define util_engineProgressCallback( uc ) \
         (uc)->vtable->m_util_engineProgressCallback((uc))

#define util_setTimer( uc, why ) \
         (uc)->vtable->m_util_setTimer((uc),(why))

#define util_requestTime( uc ) \
         (uc)->vtable->m_util_requestTime((uc))

#define util_getCurSeconds(uc) \
         (uc)->vtable->m_util_getCurSeconds((uc))

#define util_makeEmptyDict( uc ) \
         (uc)->vtable->m_util_makeEmptyDict((uc))

#define util_getUserString( uc, c ) \
         (uc)->vtable->m_util_getUserString((uc),(c))

#define util_warnIllegalWord( uc, w, p, b ) \
         (uc)->vtable->m_util_warnIllegalWord((uc),(w),(p),(b))

#ifdef BEYOND_IR
#define util_listenPortChange( uc, port ) \
         (uc)->vtable->m_util_listenPortChange((uc), (port))
#endif

# ifdef SHOW_PROGRESS
# define util_engineStarting( uc ) \
         (uc)->vtable->m_util_engineStarting((uc))
# define util_engineStopping( uc ) \
         (uc)->vtable->m_util_engineStopping((uc))
# else
# define util_engineStarting( uc )
# define util_engineStopping( uc )
# endif

#endif
