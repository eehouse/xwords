/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 2009 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#include "xwlist.h"

#define MAX_HERE 16

typedef struct XWList {
    XP_U16 len;
    XP_U16 size;
    elem* list;
    MPSLOT
} XWList;

XWList*
mk_list(MPFORMAL XP_U16 XP_UNUSED(sizeHint))
{
    XWList* list = XP_CALLOC( mpool, sizeof(*list));
    MPASSIGN( list->mpool, mpool);
    return list;
}

void
list_append( XWList* self, elem one )
{
    if ( self->size == 0 ) { /* initial case */
        self->size = 2;
        self->list = XP_MALLOC( self->mpool, self->size * sizeof(self->list[0]) );
    }
    if ( self->len == self->size ) { /* need to grow? */
        self->size *= 2;
        self->list = XP_REALLOC( self->mpool, self->list, self->size * sizeof(self->list[0]) );
    }
        
    self->list[self->len++] = one;
    XP_LOGF( "%s(): put %p at position %d (size: %d)", __func__, one, self->len-1, self->size );
}

XP_U16
list_get_len( const XWList* list )
{
    return list->len;
}

void
list_remove_front( XWList* self, elem* out, XP_U16* countp )
{
    const XP_U16 nMoved = XP_MIN( *countp, self->len );
    XP_MEMCPY( out, self->list, nMoved * sizeof(out[0]) );
    *countp = nMoved;

    // Now copy the survivors down
    self->len -= nMoved;
    XP_MEMMOVE( &self->list[0], &self->list[nMoved], self->len * sizeof(self->list[0]));
}

void
list_remove_back(XWList* XP_UNUSED(self), elem* XP_UNUSED(here), XP_U16* XP_UNUSED(count))
{
}

void
list_free( XWList* self, destructor proc, void* closure )
{
    if ( !!proc ) {
        for ( XP_U16 ii = 0; ii < self->len; ++ii ) {
            (*proc)(self->list[ii], closure);
        }
    }

    if ( !!self->list ) {
        XP_FREE( self->mpool, self->list );
    }
    XP_FREE( self->mpool, self );
}

#ifdef DEBUG

static void
dest(elem elem, void* XP_UNUSED(closure))
{
    XP_LOGF( "%s(%p)", __func__, elem);
}

void
list_test_lists(MPFORMAL_NOCOMMA)
{
    XWList* list = mk_list( mpool, 16 );
    for ( char* ii = 0; ii < (char*)100; ++ii ) {
        (void)list_append( list, ii );
    }

    XP_ASSERT( list_get_len(list) == 100 );

    char* prev = 0;
    while ( 0 < list_get_len( list ) ) {
        elem here;
        XP_U16 count = 1;
        list_remove_front( list, &here, &count );
        XP_LOGF( "%s(): got here: %p", __func__, here );
        XP_ASSERT( count == 1 );
        XP_ASSERT( prev++ == here );
    }

    for ( char* ii = 0; ii < (char*)10; ++ii ) {
        (void)list_append( list, ii );
    }

    list_free( list, dest, NULL );
}
#endif
