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

#include "mempool.h"
#include "comtypes.h"
#include "xwrelay.h"
#include "vtabmgr.h"

typedef struct _DUtilVtable {
    XP_U32 (*m_dutil_getCurSeconds)( XW_DUtilCtxt* duc );
    const XP_UCHAR* (*m_dutil_getUserString)( XW_DUtilCtxt* duc,
                                              XP_U16 stringCode );
    const XP_UCHAR* (*m_dutil_getUserQuantityString)( XW_DUtilCtxt* duc,
                                                      XP_U16 stringCode,
                                                      XP_U16 quantity );
    void (*m_dutil_storeStream)( XW_DUtilCtxt* duc, const XP_UCHAR* key,
                           XWStreamCtxt* data );
    /* Pass in an empty stream, and it'll be returned full */
    void (*m_dutil_loadStream)( XW_DUtilCtxt* duc, const XP_UCHAR* key,
                                XWStreamCtxt* inOut );
    void (*m_dutil_storePtr)( XW_DUtilCtxt* duc, const XP_UCHAR* key,
                              const void* data, XP_U16 len );
    void (*m_dutil_loadPtr)( XW_DUtilCtxt* duc, const XP_UCHAR* key,
                             void* data, XP_U16* lenp );
#ifdef XWFEATURE_SMS
    XP_Bool (*m_dutil_phoneNumbersSame)( XW_DUtilCtxt* uc, const XP_UCHAR* p1,
                                         const XP_UCHAR* p2 );
#endif

#ifdef XWFEATURE_DEVID
    const XP_UCHAR* (*m_dutil_getDevID)( XW_DUtilCtxt* duc, DevIDType* typ );
    void (*m_dutil_deviceRegistered)( XW_DUtilCtxt* duc, DevIDType typ, 
                                     const XP_UCHAR* idRelay );
#endif

#ifdef COMMS_CHECKSUM
    XP_UCHAR* (*m_dutil_md5sum)( XW_DUtilCtxt* duc, const XP_U8* ptr, XP_U16 len );
#endif
} DUtilVtable;

struct XW_DUtilCtxt {
    DUtilVtable vtable;
    void* closure;
    void* devCtxt;              /* owned by device.c */
    VTableMgr* vtMgr;
    MPSLOT
};

/* This one cheats: direct access */
#define dutil_getVTManager(duc) (duc)->vtMgr

#define dutil_getCurSeconds(duc)                    \
    (duc)->vtable.m_dutil_getCurSeconds((duc))
#define dutil_getUserString( duc, c )               \
    (duc)->vtable.m_dutil_getUserString((duc),(c))
#define dutil_getUserQuantityString( duc, c, q )                \
    (duc)->vtable.m_dutil_getUserQuantityString((duc),(c),(q))

#define dutil_storeStream(duc, k, s)                \
    (duc)->vtable.m_dutil_storeStream((duc), (k), (s));
#define dutil_storePtr(duc, k, p, l)                \
    (duc)->vtable.m_dutil_storePtr((duc), (k), (p), (l));
#define dutil_loadStream(duc, k, s)                 \
    (duc)->vtable.m_dutil_loadStream((duc), (k), (s));
#define dutil_loadPtr(duc, k, p, l)                 \
    (duc)->vtable.m_dutil_loadPtr((duc), (k), (p), (l));

#ifdef XWFEATURE_SMS
# define dutil_phoneNumbersSame(duc,p1,p2)                      \
    (duc)->vtable.m_dutil_phoneNumbersSame( (duc), (p1), (p2) )
#endif

#ifdef XWFEATURE_DEVID
# define dutil_getDevID( duc, t )                     \
         (duc)->vtable.m_dutil_getDevID((duc),(t))
# define dutil_deviceRegistered( duc, typ, id )                       \
         (duc)->vtable.m_dutil_deviceRegistered( (duc), (typ), (id) )
#endif

#ifdef COMMS_CHECKSUM
# define dutil_md5sum( duc, p, l ) (duc)->vtable.m_dutil_md5sum((duc), (p), (l))
#endif

#endif
