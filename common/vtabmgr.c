/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 2001 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#include "vtabmgr.h"

#define VTABLE_NUM_SLOTS VTABLE_LAST_ENTRY

#ifdef CPLUS
extern "C" {
#endif

struct VTableMgr {
    void* slots[VTABLE_NUM_SLOTS];
};

VTableMgr*
make_vtablemgr( MPFORMAL_NOCOMMA )
{
    VTableMgr* result = (VTableMgr*)XP_MALLOC( mpool, sizeof(*result) );
    XP_MEMSET( result, 0, sizeof(*result) );

    return result;
} /* make_vtablemgr */

void
vtmgr_destroy( MPFORMAL VTableMgr* vtmgr )
{
    XP_U16 i;

    XP_ASSERT( !!vtmgr );

    for ( i = 0; i < VTABLE_NUM_SLOTS; ++i ) {
        void* vtable = vtmgr->slots[i];
        if ( !!vtable ) {
            XP_FREE( mpool, vtable );
        }
    }

    XP_FREE( mpool, vtmgr );
} /* vtmgr_destroy */

void
vtmgr_setVTable( VTableMgr* vtmgr, VtableType typ, void* vtable )
{
    XP_ASSERT( typ < VTABLE_NUM_SLOTS );
    XP_ASSERT( !vtmgr->slots[typ] );
    vtmgr->slots[typ] = vtable;
} /* VTMSetVtable */

void*
vtmgr_getVTable( VTableMgr* vtmgr, VtableType typ )
{
    XP_ASSERT( typ < VTABLE_NUM_SLOTS );
    return vtmgr->slots[typ];
} /* VTMGetVtable */

#ifdef CPLUS
}
#endif
