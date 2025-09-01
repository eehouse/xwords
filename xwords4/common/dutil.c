/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/*
 * Copyright 2020 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#include <pthread.h>

#include "mempool.h"
#include "comtypes.h"
#include "dutil.h"
#include "xwstream.h"
#include "knownplyr.h"
#include "device.h"
#include "stats.h"
#include "timersp.h"
#include "xwmutex.h"
#include "gamemgrp.h"
#include "dictmgrp.h"

static void
super_dutil_storeStream( XW_DUtilCtxt* duc, XWEnv xwe, const XP_UCHAR* key,
                         XWStreamCtxt* data )
{
    const void* ptr = stream_getPtr( data );
    XP_U16 len = stream_getSize( data );
    dutil_storePtr( duc, xwe, key, (void*)ptr, len );
}

static void
super_dutil_loadStream( XW_DUtilCtxt* duc, XWEnv xwe,
                        const XP_UCHAR* key, XWStreamCtxt* inOut )
{
    /* get the size */
    XP_U32 len = 0;
    dutil_loadPtr( duc, xwe, key, NULL, &len );

    /* load if it exists */
    if ( 0 < len ) {
        void* buf = XP_MALLOC( duc->mpool, len );
        dutil_loadPtr( duc, xwe, key, buf, &len );

        stream_putBytes( inOut, buf, len );
        XP_FREEP( duc->mpool, &buf );
    }
}

void
dutil_super_init( MPFORMAL XW_DUtilCtxt* dutil )
{
#ifdef XWFEATURE_KNOWNPLAYERS
    MUTEX_INIT( &dutil->kpMutex, XP_FALSE );
#endif

    MPASSIGN( dutil->mpool, mpool );

    XP_ASSERT( !dutil->vtable.m_dutil_loadStream );
    SET_VTABLE_ENTRY( &dutil->vtable, dutil_loadStream, super );
    XP_ASSERT( !dutil->vtable.m_dutil_storeStream );
    SET_VTABLE_ENTRY( &dutil->vtable, dutil_storeStream, super );

    dmgr_make( dutil );
    tmr_init( dutil );
    gmgr_init( dutil );
    sts_init( dutil );
}

void
dutil_super_cleanup( XW_DUtilCtxt* dutil, XWEnv xwe )
{
    dmgr_destroy( dutil, xwe );
    tmr_cleanup( dutil, xwe );
    kplr_cleanup( dutil );
    sts_cleanup( dutil, xwe );
    gmgr_cleanup( dutil, xwe );
    dvc_cleanup( dutil, xwe );
}
