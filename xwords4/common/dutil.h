/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 2018 - 2022 by Eric House (xwords@eehouse.org).  All rights
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

#ifndef _DEVUTIL_H_
#define _DEVUTIL_H_

#include <pthread.h>

#include "comtypes.h"
#include "xwrelay.h"
#include "vtabmgr.h"
#include "commstyp.h"
#include "nlityp.h"
#include "cJSON.h"
#include "gameinfo.h"
#include "msgchnkp.h"
#include "xwarray.h"

typedef XP_Bool (*OnOneProc)(void* closure, const XP_UCHAR* keys[]);
typedef void (*OnGotKey)( const XP_UCHAR* key, void* closure, XWEnv xwe );

typedef struct _Md5SumBuf {
    XP_UCHAR buf[33];
} Md5SumBuf;

#define KEY_WILDCARD "*"

/* Keep these in sync with consts in DUtilCtxt.kt */
typedef enum {
    GCE_PLAYER_JOINED = 0x01,
    GCE_CONFIG_CHANGED = 0x02,
    GCE_SUMMARY_CHANGED = 0x04,
    GCE_TURN_CHANGED = 0x08,
    GCE_BOARD_CHANGED = 0x10,
    GCE_CHAT_ARRIVED = 0x20,
    GCE_MSGCOUNT_CHANGED = 0x40,
} GameChangeEvent;
typedef XP_U32 GameChangeEvents; /* bit vector of above */

typedef enum {
    GRCE_ADDED = 0x01,
    GRCE_DELETED = 0x02,
    GRCE_MOVED = 0x04,
    GRCE_RENAMED = 0x08,
    GRCE_COLLAPSED = 0x10,
    GRCE_EXPANDED = 0x20,
    GRCE_GAMES_REORDERED = 0x40,
    GRCE_GAME_ADDED = 0x80,
    GRCE_GAME_REMOVED = 0x100,
} GroupChangeEvent;
typedef XP_U32 GroupChangeEvents; /* bit vector of above */

typedef XP_U32 TimerKey;
typedef struct _DUtilVtable {
    /* 0 */
    DrawCtx* (*m_dutil_getThumbDraw)( XW_DUtilCtxt* duc, XWEnv xwe, GameRef gr );

    /* 1 */
    XP_U32 (*m_dutil_getCurSeconds)( XW_DUtilCtxt* duc, XWEnv xwe );
    /* 2 */
    const XP_UCHAR* (*m_dutil_getUserString)( XW_DUtilCtxt* duc, XWEnv xwe,
                                              XP_U16 stringCode );
    /* 3 */
    const XP_UCHAR* (*m_dutil_getUserQuantityString)( XW_DUtilCtxt* duc,
                                                      XWEnv xwe,
                                                      XP_U16 stringCode,
                                                      XP_U16 quantity );
    /* 4 */
    void (*m_dutil_storeStream)( XW_DUtilCtxt* duc, XWEnv xwe,
                                 const XP_UCHAR* key,
                                 XWStreamCtxt* stream );
    /* 5 */
    void (*m_dutil_loadStream)( XW_DUtilCtxt* duc, XWEnv xwe,
                                const XP_UCHAR* key,
                                XWStreamCtxt* inOut );
    void (*m_dutil_storePtr)( XW_DUtilCtxt* duc, XWEnv xwe,
                              const XP_UCHAR* key,
                              const void* data, XP_U32 len);
    /* 7 */
    void (*m_dutil_loadPtr)( XW_DUtilCtxt* duc, XWEnv xwe,
                             const XP_UCHAR* key,
                             void* data, XP_U32* lenp );
    /* 8 */
    void (*m_dutil_removeStored)( XW_DUtilCtxt* duc, XWEnv xwe,
                                  const XP_UCHAR* key );
    /* 9 */
    void (*m_dutil_getKeysLike)( XW_DUtilCtxt* duc, XWEnv xwe,
                                 const XP_UCHAR* prefix, OnGotKey proc,
                                 void* closure );
# ifdef XWFEATURE_DEVICE
    /* 10 */
    void (*m_dutil_forEach)( XW_DUtilCtxt* duc, XWEnv xwe,
                             const XP_UCHAR* keys[],
                             OnOneProc proc, void* closure );
#endif
#ifdef XWFEATURE_SMS
    /* 11 */
    XP_Bool (*m_dutil_phoneNumbersSame)( XW_DUtilCtxt* uc, XWEnv xwe, const XP_UCHAR* p1,
                                         const XP_UCHAR* p2 );
#endif

    /* 12 */
    void (*m_dutil_setTimer)( XW_DUtilCtxt* duc, XWEnv xwe, XP_U32 when, TimerKey key );
    /* 13 */
    void (*m_dutil_clearTimer)( XW_DUtilCtxt* duc, XWEnv xwe, TimerKey key );
    /* 14 */
    void (*m_dutil_md5sum)( XW_DUtilCtxt* duc, XWEnv xwe, const XP_U8* ptr,
                            XP_U32 len, Md5SumBuf* sb );
    /* 15 */
    void (*m_dutil_getUsername)( XW_DUtilCtxt* duc, XWEnv xwe, XP_U16 num,
                                 XP_Bool isLocal, XP_Bool isRobot,
                                 XP_UCHAR* buf, XP_U16* len );
    /* 16 */
    void (*m_dutil_getSelfAddr)( XW_DUtilCtxt* duc, XWEnv xwe, CommsAddrRec* addr );
    /* 17 */
    void (*m_dutil_notifyPause)( XW_DUtilCtxt* duc, XWEnv xwe, GameRef gr,
                                 DupPauseType pauseTyp, XP_U16 pauser,
                                 const XP_UCHAR* name, const XP_UCHAR* msg );
    void (*m_dutil_informMove)( XW_DUtilCtxt* duc, XWEnv xwe, GameRef gr, XP_S16 turn, 
                                XWStreamCtxt* expl, XWStreamCtxt* words );
    void (*m_dutil_notifyGameOver)( XW_DUtilCtxt* duc, XWEnv xwe, GameRef gr,
                                    XP_S16 quitter );
    /* XP_Bool (*m_dutil_haveGame)( XW_DUtilCtxt* duc, XWEnv xwe, XP_U32 gameID, */
    /*                              XP_U8 channel ); */
    /* 18 */
    void (*m_dutil_onDupTimerChanged)( XW_DUtilCtxt* duc, XWEnv xwe, GameRef gr,
                                       XP_U32 oldVal, XP_U32 newVal );
    void (*m_dutil_onGameChanged)( XW_DUtilCtxt* duc, XWEnv xwe, GameRef gr,
                                   GameChangeEvents gces );
    void (*m_dutil_onGroupChanged)( XW_DUtilCtxt* duc, XWEnv xwe, GroupRef grp,
                                    GroupChangeEvents grces );
#ifndef XWFEATURE_DEVICE_STORES
    void (*m_dutil_onGameGoneReceived)( XW_DUtilCtxt* duc, XWEnv xwe, XP_U32 gameID,
                                       const CommsAddrRec* from );
#endif
    void (*m_dutil_onCtrlReceived)( XW_DUtilCtxt* duc, XWEnv xwe,
                                    const XP_U8* buf, XP_U16 len );
    /* Return platform-specific registration keys->values */
    cJSON* (*m_dutil_getRegValues)( XW_DUtilCtxt* duc, XWEnv xwe );
    void (*m_dutil_sendViaWeb)( XW_DUtilCtxt* duc, XWEnv xwe, XP_U32 resultKey,
                                const XP_UCHAR* api, const cJSON* params );

    DictionaryCtxt* (*m_dutil_makeEmptyDict)( XW_DUtilCtxt* duc, XWEnv xwe );
    const DictionaryCtxt* (*m_dutil_makeDict)( XW_DUtilCtxt* duc, XWEnv xwe,
                                               const XP_UCHAR* dictName );
    void (*m_dutil_missingDictAdded)( XW_DUtilCtxt* duc, XWEnv xwe, GameRef gr,
                                      const XP_UCHAR* dictName );
    void (*m_dutil_dictGone)( XW_DUtilCtxt* duc, XWEnv xwe, GameRef gr,
                              const XP_UCHAR* dictName );
    void (*m_dutil_startMQTTListener)( XW_DUtilCtxt* duc, XWEnv xwe,
                                       const MQTTDevID* devID,
                                       const XP_UCHAR** topics, XP_U8 qos );
    XP_S16 (*m_dutil_sendViaMQTT)( XW_DUtilCtxt* duc, XWEnv xwe,
                                   const XP_UCHAR* topic, const XP_U8* buf,
                                   XP_U16 len, XP_U8 qos );
    XP_S16 (*m_dutil_sendViaNBS)( XW_DUtilCtxt* duc, XWEnv xwe,
                                  const XP_U8* buf, XP_U16 len,
                                  const XP_UCHAR* phone, XP_U16 port );
    XP_S16 (*m_dutil_sendViaBT)( XW_DUtilCtxt* duc, XWEnv xwe,
                                 const XP_U8* buf, XP_U16 len,
                                 const XP_UCHAR* hostName,
                                 const XP_BtAddrStr* btAddr );
    XP_S16 (*m_dutil_sendViaNFC)( XW_DUtilCtxt* duc, XWEnv xwe,
                                  const XP_U8* buf, XP_U16 len,
                                  XP_U32 gameID );

    void (*m_dutil_onKnownPlayersChange)( XW_DUtilCtxt* duc, XWEnv xwe );
    void (*m_dutil_getCommonPrefs)( XW_DUtilCtxt* duc, XWEnv xwe, CommonPrefs* cp );
} DUtilVtable;

typedef struct GameMgrState GameMgrState;
typedef struct DictMgrCtxt DictMgrCtxt;

struct XW_DUtilCtxt {
    DUtilVtable vtable;
    DictMgrCtxt* dictMgr;
    MQTTDevID devID;
    void* closure;
    void* devCtxt;              /* owned by device.c */
    void* statsState;           /* owned by stats.c */
    void* timersState;          /* owned by timers.c */
    MsgChunker* smsChunkerState;
    MsgChunker* btChunkerState;
    GameMgrState* gameMgrState; /* owned by gamemgr.c */
#ifdef XWFEATURE_KNOWNPLAYERS   /* owned by knownplyr.c */
    void* kpCtxt;
    MutexState kpMutex;
#endif
    VTableMgr* vtMgr;
#ifdef DEBUG
    XP_U32 magic;
#endif
    MPSLOT
};

void dutil_super_init( MPFORMAL XW_DUtilCtxt* dutil, XWEnv xwe );
void dutil_super_cleanup( XW_DUtilCtxt* dutil, XWEnv xwe );

/* This one cheats: direct access */
#define dutil_getVTManager(duc) (duc)->vtMgr

#define dutil_getCurSeconds(duc, ...)               \
    (duc)->vtable.m_dutil_getCurSeconds((duc), __VA_ARGS__)
#define dutil_getUserString( duc, ... )             \
    (duc)->vtable.m_dutil_getUserString((duc), __VA_ARGS__)
#define dutil_getUserQuantityString( duc, ... )                 \
    (duc)->vtable.m_dutil_getUserQuantityString((duc), __VA_ARGS__)

#define dutil_storeStream(duc, ...)                         \
    (duc)->vtable.m_dutil_storeStream((duc), __VA_ARGS__)
#define dutil_storePtr(duc, ...)                         \
    (duc)->vtable.m_dutil_storePtr((duc), __VA_ARGS__)
#define dutil_loadStream(duc, ...)                      \
    (duc)->vtable.m_dutil_loadStream((duc), __VA_ARGS__)
#define dutil_loadPtr(duc, ...)                         \
    (duc)->vtable.m_dutil_loadPtr((duc), __VA_ARGS__)
# define dutil_forEach( duc, ... )                                      \
    (duc)->vtable.m_dutil_forEach((duc), __VA_ARGS__ )
#define dutil_removeStored(duc, ...)                 \
    (duc)->vtable.m_dutil_removeStored((duc), __VA_ARGS__)

#define dutil_getThumbDraw(duc, ...)                        \
    (duc)->vtable.m_dutil_getThumbDraw((duc), __VA_ARGS__)
#define dutil_getKeysLike( duc, ... )              \
    (duc)->vtable.m_dutil_getKeysLike( (duc), __VA_ARGS__)

#ifdef XWFEATURE_SMS
# define dutil_phoneNumbersSame(duc, ...)                    \
    (duc)->vtable.m_dutil_phoneNumbersSame( (duc), __VA_ARGS__)
#endif

# define dutil_setTimer( duc, ... )                  \
    (duc)->vtable.m_dutil_setTimer((duc), __VA_ARGS__)
# define dutil_clearTimer( duc, ... )                  \
    (duc)->vtable.m_dutil_clearTimer((duc), __VA_ARGS__)

# define dutil_md5sum( duc, ... )                       \
    (duc)->vtable.m_dutil_md5sum((duc), __VA_ARGS__)
#define dutil_getUsername(duc, ...)                         \
    (duc)->vtable.m_dutil_getUsername((duc), __VA_ARGS__)
#define dutil_notifyPause( duc, ... )                       \
    (duc)->vtable.m_dutil_notifyPause( (duc), __VA_ARGS__)
#define dutil_informMove( duc, ... )                        \
    (duc)->vtable.m_dutil_informMove( (duc), __VA_ARGS__)
#define dutil_notifyGameOver( duc, ... )                    \
    (duc)->vtable.m_dutil_notifyGameOver( (duc), __VA_ARGS__)
#define dutil_haveGame( duc, ... )                      \
    (duc)->vtable.m_dutil_haveGame( (duc), __VA_ARGS__)
#define dutil_getSelfAddr(duc, xwe, addr)                   \
    (duc)->vtable.m_dutil_getSelfAddr((duc), (xwe), (addr))

#define dutil_onDupTimerChanged(duc, ...)                           \
    (duc)->vtable.m_dutil_onDupTimerChanged( (duc), __VA_ARGS__)
#define dutil_onGameChanged(duc, ...)                        \
    (duc)->vtable.m_dutil_onGameChanged((duc), __VA_ARGS__)
#define dutil_onGroupChanged(duc, ...)                         \
    (duc)->vtable.m_dutil_onGroupChanged( (duc), __VA_ARGS__)
#define dutil_onCtrlReceived(duc, ... )                         \
    (duc)->vtable.m_dutil_onCtrlReceived((duc), __VA_ARGS__ )
#define dutil_onGameGoneReceived(duc, ...)         \
    (duc)->vtable.m_dutil_onGameGoneReceived((duc), __VA_ARGS__)
#define dutil_sendViaWeb( duc, ... )        \
    (duc)->vtable.m_dutil_sendViaWeb((duc), __VA_ARGS__)
#define dutil_makeEmptyDict(duc, ...)                   \
    (duc)->vtable.m_dutil_makeEmptyDict((duc), __VA_ARGS__)
#define dutil_makeDict(duc, ...)                      \
    (duc)->vtable.m_dutil_makeDict((duc), __VA_ARGS__)
#define dutil_missingDictAdded( duc, ...)                       \
    (duc)->vtable.m_dutil_missingDictAdded((duc), __VA_ARGS__)
#define dutil_dictGone( duc, ...)                       \
    (duc)->vtable.m_dutil_dictGone((duc), __VA_ARGS__)
#define dutil_startMQTTListener(duc, ...)                       \
    (duc)->vtable.m_dutil_startMQTTListener((duc), __VA_ARGS__)
#define dutil_sendViaMQTT(duc, ...)                         \
    (duc)->vtable.m_dutil_sendViaMQTT((duc), __VA_ARGS__)
#define dutil_sendViaNBS(duc, ...)                          \
    (duc)->vtable.m_dutil_sendViaNBS((duc), __VA_ARGS__)
#define dutil_sendViaBT(duc, ...)                       \
    (duc)->vtable.m_dutil_sendViaBT((duc), __VA_ARGS__)
#define dutil_sendViaNFC(duc, ...)                          \
    (duc)->vtable.m_dutil_sendViaNFC((duc), __VA_ARGS__)
#define dutil_onKnownPlayersChange(duc, ...)                        \
    (duc)->vtable.m_dutil_onKnownPlayersChange((duc), __VA_ARGS__)
#define dutil_getCommonPrefs(duc, ...)          \
    (duc)->vtable.m_dutil_getCommonPrefs((duc), __VA_ARGS__)

#define dutil_getRegValues( duc, ... ) \
    (duc)->vtable.m_dutil_getRegValues( (duc), __VA_ARGS__)

#endif
