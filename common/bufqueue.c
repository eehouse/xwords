/* -*-mode: C; compile-command: "cd ../linux && make MEMDEBUG=TRUE"; -*- */
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

#ifdef USE_BUFQUEUE

#include "bufqueue.h"

#ifdef CPLUS
extern "C" {
#endif

static XP_U16
roundUp( XP_U16 num )
{
    /* Keep ptrs word-aligned so can case to XP_U16* */
    return (num + 1) & 0xFFFE;
}

#ifdef DEBUG
static void
printQueue( const BufQueue* bq )
{
    XP_U16 head = bq->head;
    XP_U16 counter;
    for ( counter = 0; head < bq->tail; ++counter ) {
        XP_U16 len = *(XP_U16*)&bq->base[head];
        XP_LOGF( "item %d, len %d", counter, len );
        head += roundUp( len + 2 );
    }
}
#else
# define printQueue( bq )
#endif

static XP_Bool
haveSpace( const BufQueue* bq, XP_U16 len )
{
    return (bq->tail + len + 2) < bq->bufSize;
}

static void
shiftDown( BufQueue* bq )
{
    if ( bq->head > 0 ) {
        XP_ASSERT( bq->tail > bq->head );
        XP_MEMMOVE( bq->base, &bq->base[bq->head], bq->tail - bq->head );
        bq->tail -= bq->head;
        bq->head = 0;
    }
}

void
bqInit( BufQueue* bq, XP_U8* buf, XP_U16 buflen )
{
    bq->base = buf;
    bq->bufSize = buflen;
    bq->head = bq->tail = 0;
}

XP_Bool
bqAdd( BufQueue* bq, const XP_U8* buf, XP_U16 len )
{
    XP_Bool success = XP_FALSE;
    
    if ( !haveSpace( bq, len ) ) {
        shiftDown( bq );
    }
    success = haveSpace( bq, len );
    if ( success ) {
        XP_U8* tailp = &bq->base[bq->tail];
        *(XP_U16*)tailp = len;
        XP_MEMCPY( tailp+2, buf, len );
        bq->tail += roundUp(len + 2);
    }

    printQueue( bq );

    return success;
}

XP_Bool
bqGet( BufQueue* bq, const XP_U8** buf, XP_U16* len )
{
    XP_Bool have = bq->head < bq->tail;
    if ( have ) {
        *len = *(XP_U16*)&bq->base[bq->head];
        *buf = &bq->base[bq->head+2];
    }
    return have;
}

void
bqRemoveOne( BufQueue* bq )
{
    XP_U16 len;
    XP_ASSERT( bq->head != bq->tail );
    len = *(XP_U16*)&bq->base[bq->head];
    bq->head += roundUp( len + 2 );
    XP_ASSERT( bq->head <= bq->tail );

    /* can we reset for free */
    if ( bq->head == bq->tail ) {
        bq->head = bq->tail = 0;
    }
}

void
bqRemoveAll( BufQueue* bq )
{
    bq->head = bq->tail = 0;
}

#ifdef CPLUS
}
#endif

#endif /* USE_BUFQUEUE */
