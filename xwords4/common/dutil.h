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

typedef enum { UNPAUSED,
               PAUSED,
               AUTOPAUSED,
} DupPauseType;

typedef XP_Bool (*OnOneProc)(void* closure, const XP_UCHAR* keys[]);

typedef struct _Md5SumBuf {
    XP_UCHAR buf[33];
} Md5SumBuf;

#define KEY_WILDCARD "*"

#ifdef DUTIL_TIMERS
typedef XP_U32 TimerKey;
#endif
typedef struct _DUtilVtable {
    XP_U32 (*m_dutil_getCurSeconds)( XW_DUtilCtxt* duc, XWEnv xwe );
    const XP_UCHAR* (*m_dutil_getUserString)( XW_DUtilCtxt* duc, XWEnv xwe,
                                              XP_U16 stringCode );
    const XP_UCHAR* (*m_dutil_getUserQuantityString)( XW_DUtilCtxt* duc,
                                                      XWEnv xwe,
                                                      XP_U16 stringCode,
                                                      XP_U16 quantity );
    void (*m_dutil_storeStream)( XW_DUtilCtxt* duc, XWEnv xwe,
                                 const XP_UCHAR* key,
                                 XWStreamCtxt* stream );
    void (*m_dutil_loadStream)( XW_DUtilCtxt* duc, XWEnv xwe,
                                const XP_UCHAR* key,
                                XWStreamCtxt* inOut );
    void (*m_dutil_storePtr)( XW_DUtilCtxt* duc, XWEnv xwe,
                              const XP_UCHAR* key,
                              const void* data, XP_U32 len);
    void (*m_dutil_loadPtr)( XW_DUtilCtxt* duc, XWEnv xwe,
                             const XP_UCHAR* key,
                             void* data, XP_U32* lenp );
# ifdef XWFEATURE_DEVICE
    void (*m_dutil_forEach)( XW_DUtilCtxt* duc, XWEnv xwe,
                             const XP_UCHAR* keys[],
                             OnOneProc proc, void* closure );
    void (*m_dutil_remove)( XW_DUtilCtxt* duc, const XP_UCHAR* keys[] );
#endif
#ifdef XWFEATURE_SMS
    XP_Bool (*m_dutil_phoneNumbersSame)( XW_DUtilCtxt* uc, XWEnv xwe, const XP_UCHAR* p1,
                                         const XP_UCHAR* p2 );
#endif

#if defined XWFEATURE_DEVID && defined XWFEATURE_RELAY
    const XP_UCHAR* (*m_dutil_getDevID)( XW_DUtilCtxt* duc, XWEnv xwe, DevIDType* typ );
    void (*m_dutil_deviceRegistered)( XW_DUtilCtxt* duc, XWEnv xwe, DevIDType typ,
                                     const XP_UCHAR* idRelay );
#endif

#ifdef DUTIL_TIMERS
    void (*m_dutil_setTimer)( XW_DUtilCtxt* duc, XWEnv xwe, XP_U32 when, TimerKey key );
    void (*m_dutil_clearTimer)( XW_DUtilCtxt* duc, XWEnv xwe, TimerKey key );
#endif
    void (*m_dutil_md5sum)( XW_DUtilCtxt* duc, XWEnv xwe, const XP_U8* ptr,
                            XP_U32 len, Md5SumBuf* sb );
    void (*m_dutil_getUsername)( XW_DUtilCtxt* duc, XWEnv xwe, XP_U16 num,
                                 XP_Bool isLocal, XP_Bool isRobot,
                                 XP_UCHAR* buf, XP_U16* len );
    void (*m_dutil_notifyPause)( XW_DUtilCtxt* duc, XWEnv xwe, XP_U32 gameID,
                                 DupPauseType pauseTyp, XP_U16 pauser,
                                 const XP_UCHAR* name, const XP_UCHAR* msg );
    XP_Bool (*m_dutil_haveGame)( XW_DUtilCtxt* duc, XWEnv xwe, XP_U32 gameID,XP_U8 channel );
    void (*m_dutil_onDupTimerChanged)( XW_DUtilCtxt* duc, XWEnv xwe, XP_U32 gameID,
                                       XP_U32 oldVal, XP_U32 newVal );

    void (*m_dutil_onInviteReceived)( XW_DUtilCtxt* duc, XWEnv xwe,
                                      const NetLaunchInfo* nli );
    void (*m_dutil_onMessageReceived)( XW_DUtilCtxt* duc, XWEnv xwe, XP_U32 gameID,
                                       const CommsAddrRec* from, const XP_U8* buf, XP_U16 len );
    void (*m_dutil_onCtrlReceived)( XW_DUtilCtxt* duc, XWEnv xwe, const XP_U8* buf, XP_U16 len );
    void (*m_dutil_onGameGoneReceived)( XW_DUtilCtxt* duc, XWEnv xwe, XP_U32 gameID,
                                       const CommsAddrRec* from );
    /* Return platform-specific registration keys->values */
    cJSON* (*m_dutil_getRegValues)( XW_DUtilCtxt* duc, XWEnv xwe );
    void (*m_dutil_sendViaWeb)( XW_DUtilCtxt* duc, XWEnv xwe, XP_U32 resultKey,
                                const XP_UCHAR* api, const cJSON* params );

    DictionaryCtxt* (*m_dutil_makeEmptyDict)( XW_DUtilCtxt* duc, XWEnv xwe );
    const DictionaryCtxt* (*m_dutil_getDict)( XW_DUtilCtxt* duc, XWEnv xwe,
                                              const XP_UCHAR* isoCode,
                                              const XP_UCHAR* dictName );
} DUtilVtable;

struct XW_DUtilCtxt {
    DUtilVtable vtable;
    MQTTDevID devID;
    void* closure;
    void* devCtxt;              /* owned by device.c */
    void* statsState;           /* owned by stats.c */
    void* timersState;          /* owned by timers.c */
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

void dutil_super_init( MPFORMAL XW_DUtilCtxt* dutil );
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
#define dutil_remove(duc, ...)                 \
    (duc)->vtable.m_dutil_remove((duc), __VA_ARGS__)

#ifdef XWFEATURE_SMS
# define dutil_phoneNumbersSame(duc, ...)                    \
    (duc)->vtable.m_dutil_phoneNumbersSame( (duc), __VA_ARGS__)
#endif

#ifdef DUTIL_TIMERS
# define dutil_setTimer( duc, ... )                  \
    (duc)->vtable.m_dutil_setTimer((duc), __VA_ARGS__)
# define dutil_clearTimer( duc, ... )                  \
    (duc)->vtable.m_dutil_clearTimer((duc), __VA_ARGS__)
#endif

# define dutil_md5sum( duc, ... )                    \
    (duc)->vtable.m_dutil_md5sum((duc), __VA_ARGS__)
#define dutil_getUsername(duc, ...)                                     \
    (duc)->vtable.m_dutil_getUsername((duc), __VA_ARGS__)
#define dutil_notifyPause( duc, ... )                     \
    (duc)->vtable.m_dutil_notifyPause( (duc), __VA_ARGS__)

#define dutil_haveGame( duc, ... )                      \
    (duc)->vtable.m_dutil_haveGame( (duc), __VA_ARGS__)

#define dutil_onDupTimerChanged(duc, ...)                           \
    (duc)->vtable.m_dutil_onDupTimerChanged( (duc), __VA_ARGS__)
#define dutil_onInviteReceived(duc, ...)                        \
    (duc)->vtable.m_dutil_onInviteReceived( (duc), __VA_ARGS__)
#define dutil_onMessageReceived(duc, ...)                       \
    (duc)->vtable.m_dutil_onMessageReceived((duc), __VA_ARGS__)
#define dutil_onCtrlReceived(duc, ... )                         \
    (duc)->vtable.m_dutil_onCtrlReceived((duc), __VA_ARGS__ )
#define dutil_onGameGoneReceived(duc, ...)         \
    (duc)->vtable.m_dutil_onGameGoneReceived((duc), __VA_ARGS__)
#define dutil_sendViaWeb( duc, ... )        \
    (duc)->vtable.m_dutil_sendViaWeb((duc), __VA_ARGS__)
#define dutil_makeEmptyDict(duc, ...)                   \
    (duc)->vtable.m_dutil_makeEmptyDict((duc), __VA_ARGS__)
#define dutil_getDict(duc, ...)                      \
    (duc)->vtable.m_dutil_getDict((duc), __VA_ARGS__)

#define dutil_getRegValues( duc, ... ) \
    (duc)->vtable.m_dutil_getRegValues( (duc), __VA_ARGS__)

#endif
