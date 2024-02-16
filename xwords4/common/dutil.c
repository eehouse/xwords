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

static void
super_dutil_storeStream( XW_DUtilCtxt* duc, XWEnv xwe, const XP_UCHAR* keys[],
                         XWStreamCtxt* data )
{
    const void* ptr = stream_getPtr( data );
    XP_U16 len = stream_getSize( data );
    dutil_storePtr( duc, xwe, keys, (void*)ptr, len );
}

static void
super_dutil_loadStream( XW_DUtilCtxt* duc, XWEnv xwe,
                        const XP_UCHAR* keys[], XWStreamCtxt* inOut )
{
    /* get the size */
    XP_U32 len = 0;
    dutil_loadPtr( duc, xwe, keys, NULL, &len );

    /* load if it exists */
    if ( 0 < len ) {
        void* buf = XP_MALLOC( duc->mpool, len );
        dutil_loadPtr( duc, xwe, keys, buf, &len );

        stream_putBytes( inOut, buf, len );
        XP_FREEP( duc->mpool, &buf );
    }
}

void
dutil_super_init( MPFORMAL XW_DUtilCtxt* dutil, XWEnv xwe )
{
#ifdef XWFEATURE_KNOWNPLAYERS
    pthread_mutex_init( &dutil->kpMutex, NULL );
#endif

    MPASSIGN( dutil->mpool, mpool );

    SET_VTABLE_ENTRY( &dutil->vtable, dutil_loadStream, super );
    SET_VTABLE_ENTRY( &dutil->vtable, dutil_storeStream, super );

    sts_init( dutil, xwe );
}

void
dutil_super_cleanup( XW_DUtilCtxt* dutil, XWEnv xwe )
{
    kplr_cleanup( dutil );
    sts_cleanup( dutil );
    dvc_cleanup( dutil, xwe );
}
