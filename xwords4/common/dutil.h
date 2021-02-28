/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 2018 by Eric House (xwords@eehouse.org).  All rights
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

typedef enum { UNPAUSED,
               PAUSED,
               AUTOPAUSED,
} DupPauseType;

typedef XP_Bool (*OnOneProc)(void* closure, const XP_UCHAR* indx);

typedef struct _DUtilVtable {
    XP_U32 (*m_dutil_getCurSeconds)( XW_DUtilCtxt* duc, XWEnv xwe );
    const XP_UCHAR* (*m_dutil_getUserString)( XW_DUtilCtxt* duc, XWEnv xwe,
                                              XP_U16 stringCode );
    const XP_UCHAR* (*m_dutil_getUserQuantityString)( XW_DUtilCtxt* duc,
                                                      XWEnv xwe,
                                                      XP_U16 stringCode,
                                                      XP_U16 quantity );
    void (*m_dutil_storeStream)( XW_DUtilCtxt* duc, XWEnv xwe, const XP_UCHAR* key,
                                 XWStreamCtxt* data );
    /* Pass in an empty stream, and it'll be returned full */
    void (*m_dutil_loadStream)( XW_DUtilCtxt* duc, XWEnv xwe, const XP_UCHAR* key,
                                const XP_UCHAR* fallbackKey,   // PENDING() remove this after a few months.
                                XWStreamCtxt* inOut );
    void (*m_dutil_storePtr)( XW_DUtilCtxt* duc, XWEnv xwe, const XP_UCHAR* key,
                              const void* data, XP_U32 len );
    void (*m_dutil_loadPtr)( XW_DUtilCtxt* duc, XWEnv xwe, const XP_UCHAR* key,
                             const XP_UCHAR* fallbackKey,   // PENDING() remove this after a few months.
                             void* data, XP_U32* lenp );

#ifdef XWFEATURE_INDEXSTORE
    void (*m_dutil_storeIndxStream)( XW_DUtilCtxt* duc, XWEnv xwe, const XP_UCHAR* key,
                                     const XP_UCHAR* indx, XWStreamCtxt* data );
    void (*m_dutil_loadIndxStream)( XW_DUtilCtxt* duc, XWEnv xwe, const XP_UCHAR* key,
                                    const XP_UCHAR* fallbackKey,   // PENDING() remove this after a few months.
                                    const char* indx, XWStreamCtxt* inOut );
    void (*m_dutil_storeIndxPtr)( XW_DUtilCtxt* duc, XWEnv xwe, const XP_UCHAR* key,
                                  const XP_UCHAR* indx, const void* data, XP_U32 len );
    void (*m_dutil_loadIndxPtr)( XW_DUtilCtxt* duc, XWEnv xwe, const XP_UCHAR* key,
                                 const XP_UCHAR* indx, void* data, XP_U32* lenp );

    void (*m_dutil_forEachIndx)( XW_DUtilCtxt* duc, XWEnv xwe, const XP_UCHAR* key,
                                 OnOneProc proc, void* closure );
    /* remove everything with the index, regardless of key. Only applies to
       storeIndx* cases */
    void (*m_dutil_removeAllIndx)(XW_DUtilCtxt* duc, const XP_UCHAR* indx);
#endif

#ifdef XWFEATURE_SMS
    XP_Bool (*m_dutil_phoneNumbersSame)( XW_DUtilCtxt* uc, XWEnv xwe, const XP_UCHAR* p1,
                                         const XP_UCHAR* p2 );
#endif

#ifdef XWFEATURE_DEVID
    const XP_UCHAR* (*m_dutil_getDevID)( XW_DUtilCtxt* duc, XWEnv xwe, DevIDType* typ );
    void (*m_dutil_deviceRegistered)( XW_DUtilCtxt* duc, XWEnv xwe, DevIDType typ,
                                     const XP_UCHAR* idRelay );
#endif

#ifdef COMMS_CHECKSUM
    XP_UCHAR* (*m_dutil_md5sum)( XW_DUtilCtxt* duc, XWEnv xwe, const XP_U8* ptr, XP_U32 len );
#endif

    const DictionaryCtxt* (*m_dutil_getDict)( XW_DUtilCtxt* duc, XWEnv xwe,
                                              XP_LangCode lang, const XP_UCHAR* dictName );

    void (*m_dutil_notifyPause)( XW_DUtilCtxt* duc, XWEnv xwe, XP_U32 gameID,
                                 DupPauseType pauseTyp, XP_U16 pauser,
                                 const XP_UCHAR* name, const XP_UCHAR* msg );
    void (*m_dutil_onDupTimerChanged)( XW_DUtilCtxt* duc, XWEnv xwe, XP_U32 gameID,
                                       XP_U32 oldVal, XP_U32 newVal );

    void (*m_dutil_onInviteReceived)( XW_DUtilCtxt* duc, XWEnv xwe,
                                      const NetLaunchInfo* nli );
    void (*m_dutil_onMessageReceived)( XW_DUtilCtxt* duc, XWEnv xwe, XP_U32 gameID,
                                       const CommsAddrRec* from, XWStreamCtxt* stream );
    void (*m_dutil_onGameGoneReceived)( XW_DUtilCtxt* duc, XWEnv xwe, XP_U32 gameID,
                                       const CommsAddrRec* from );
} DUtilVtable;

struct XW_DUtilCtxt {
    DUtilVtable vtable;
    void* closure;
    void* devCtxt;              /* owned by device.c */
#ifdef XWFEATURE_KNOWNPLAYERS   /* owned by knownplyr.c */
    void* kpCtxt;
    pthread_mutex_t kpMutex;
#endif
    VTableMgr* vtMgr;
    MPSLOT
};

void dutil_super_init( MPFORMAL XW_DUtilCtxt* dutil );

/* This one cheats: direct access */
#define dutil_getVTManager(duc) (duc)->vtMgr

#define dutil_getCurSeconds(duc, e)               \
    (duc)->vtable.m_dutil_getCurSeconds((duc), (e))
#define dutil_getUserString( duc, e, c )             \
    (duc)->vtable.m_dutil_getUserString((duc),(e), (c))
#define dutil_getUserQuantityString( duc, e, c, q )                 \
    (duc)->vtable.m_dutil_getUserQuantityString((duc),(e), (c),(q))

#define dutil_storeStream(duc, e, k, s)                         \
    (duc)->vtable.m_dutil_storeStream((duc), (e), (k), (s));
#define dutil_storeIndxStream( duc, xwe, key, indx, data )              \
    (duc)->vtable.m_dutil_storeIndxStream( (duc), (xwe), (key), (indx), (data) );
#define dutil_loadIndxStream( duc, xwe, key, fb, indx, inOut ) \
    (duc)->vtable.m_dutil_loadIndxStream( (duc), (xwe), (key), \
                                          (fb), (indx), (inOut) );
#define dutil_storePtr(duc, e, k, p, l)                         \
    (duc)->vtable.m_dutil_storePtr((duc), (e), (k), (p), (l));
#define dutil_storeIndxPtr(duc, e, k, i, p, l)                      \
    (duc)->vtable.m_dutil_storeIndxPtr((duc), (e), (k), (i), (p), (l));
#define dutil_loadStream(duc, e, k, fk, s)                          \
    (duc)->vtable.m_dutil_loadStream((duc), (e), (k), (fk), (s));
#define dutil_loadPtr(duc, e, k, fk, p, l)                          \
    (duc)->vtable.m_dutil_loadPtr((duc), (e), (k), (fk), (p), (l));
#define dutil_loadIndxPtr(duc, e, k, i, p, l)                       \
    (duc)->vtable.m_dutil_loadIndxPtr((duc), (e), (k), (i), (p), (l));
#define dutil_forEachIndx(duc, e, key, proc, closure)                    \
    (duc)->vtable.m_dutil_forEachIndx( (duc), (e), (key), (proc), (closure) );

#define dutil_removeAllIndx(duc, indx)                  \
    (duc)->vtable.m_dutil_removeAllIndx((duc), (indx));

#ifdef XWFEATURE_SMS
# define dutil_phoneNumbersSame(duc,e,p1,p2)                    \
    (duc)->vtable.m_dutil_phoneNumbersSame( (duc), (e), (p1), (p2) )
#endif

#ifdef XWFEATURE_DEVID
# define dutil_getDevID( duc, e, t )             \
    (duc)->vtable.m_dutil_getDevID((duc), (e),(t))
# define dutil_deviceRegistered( duc, e, typ, id )                       \
    (duc)->vtable.m_dutil_deviceRegistered( (duc), (e), (typ), (id) )
#endif

#ifdef COMMS_CHECKSUM
# define dutil_md5sum( duc, e, p, l )                   \
    (duc)->vtable.m_dutil_md5sum((duc), (e), (p), (l))
#endif

#define dutil_getDict( duc, xwe, lc, dictName )                     \
    (duc)->vtable.m_dutil_getDict( (duc), (xwe), (lc), (dictName) )

#define dutil_notifyPause( duc, e, id, ip, p, n, m )                     \
    (duc)->vtable.m_dutil_notifyPause( (duc), (e), (id), (ip), (p), (n), (m) )

#define dutil_onDupTimerChanged(duc, e, id, ov, nv)                      \
    (duc)->vtable.m_dutil_onDupTimerChanged( (duc), (e), (id), (ov), (nv))

#define dutil_onInviteReceived(duc, xwe, nli)                       \
    (duc)->vtable.m_dutil_onInviteReceived( (duc), (xwe), (nli) )
#define dutil_onMessageReceived(duc, xwe, gameID, from, stream)         \
    (duc)->vtable.m_dutil_onMessageReceived((duc),(xwe),(gameID),(from),(stream))
#define dutil_onGameGoneReceived(duc, xwe, gameID, from)         \
    (duc)->vtable.m_dutil_onGameGoneReceived((duc),(xwe),(gameID),(from))

#endif
