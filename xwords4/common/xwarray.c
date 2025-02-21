/*
 * Copyright 2025 by Eric House (xwords@eehouse.org).  All rights
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

#include "xwarray.h"
// #include "dutil.h"              /* for NULL ??? */

#define INITIAL_COUNT 4

struct XWArray {
    void** elems;
    XP_U32 nElems;              /* what's used */
    XP_U32 capacity;            /* what's allocated */

    ArCompProc proc;

    MPSLOT
};

static void ensureRoom( XWArray* array, XP_U32 forNew );
static int findFit( XWArray* array, void* node );
static void moveUpOne( XWArray* array, int from );
static void moveDownOne( XWArray* array, int from );
#ifdef DEBUG
static void assertSorted( XWArray* array );
#else
# define assertSorted(X)
#endif


XWArray*
arr_make(MPFORMAL_NOCOMMA)
{
    XWArray* array = XP_CALLOC(mpool, sizeof(*array) );
#ifdef MEM_DEBUG
    array->mpool = mpool;
#endif
    return array;
}

void
arr_destroy( XWArray* array )
{
#ifdef MEM_DEBUG
    MemPoolCtx* mpool = array->mpool;
#endif
    XP_FREEP( mpool, &array->elems );
    XP_FREE( mpool, array );
}

XP_U32
arr_length( const XWArray* array )
{
    return array->nElems;
}

void
arr_append( XWArray* array, void* node )
{
    ensureRoom( array, 1 );
    array->elems[array->nElems++] = node;
}

void
arr_remove( XWArray* array, void* node )
{
    int loc = findFit( array, node );
    moveDownOne( array, loc );
    --array->nElems;
}

void
arr_insert( XWArray* array, void* node )
{
    assertSorted( array );
    ensureRoom( array, 1 );
    if ( !array->proc || 0 == array->nElems ) {
        array->elems[array->nElems++] = node;
    } else {
        int indx = findFit( array, node );
        XP_ASSERT( 0 <= indx && indx <= array->nElems );
        moveUpOne( array, indx );
        array->elems[indx] = node;
        ++array->nElems;
    }
    assertSorted( array );
}

void*
arr_getNth( XWArray* array, XP_U32 nn )
{
    XP_ASSERT( nn < array->nElems );
    return array->elems[nn];
}

void
arr_setSort( XWArray* array, ArCompProc proc )
{
    if ( proc != array->proc ) {
        array->proc = proc;
        if ( !!proc && 0 < array->nElems ) {
            void** oldElems = array->elems;
            XP_U32 oldNElems = array->nElems;
            array->elems = NULL;
            array->nElems = array->capacity = 0;

            for ( int ii = 0; ii < oldNElems; ++ii ) {
                arr_insert( array, oldElems[ii] );
            }
            XP_FREE( array->mpool, oldElems );
        }
    }
}

void
arr_map( XWArray* array, ArMapProc mapProc, void* closure )
{
    void** elems = array->elems;
    for ( int indx = 0; indx < array->nElems; ++indx ) {
        ForEachAct fea = (*mapProc)(elems[indx], closure);
        if ( FEA_REMOVE & fea ) {
            moveDownOne( array, indx );
            --array->nElems;
            --indx;
        }
        if ( FEA_EXIT & fea ) {
            break;
        }
    }
}

void
arr_removeAll( XWArray* array, ArDisposeProc dispProc, void* closure )
{
    if ( !!dispProc ) {
        void** elems = array->elems;
        XP_U32 nElems = array->nElems;
        for ( int ii = 0; ii < nElems; ++ii ) {
            (*dispProc)( elems[ii], closure );
        }
    }
    XP_FREEP( array->mpool, &array->elems );
    array->nElems = array->capacity = 0;
}

static void
ensureRoom( XWArray* array, XP_U32 forNew )
{
    if ( array->capacity < array->nElems + forNew ) {
        XP_U32 newCapacity = XP_MAX( 4, array->capacity * 2 );
        array->elems = XP_REALLOC( array->mpool, array->elems,
                                   newCapacity * sizeof(*array->elems) );
        array->capacity = newCapacity;
    }
}

#ifdef DEBUG
static void
assertSorted( XWArray* array )
{
    ArCompProc proc = array->proc;
    if ( !!proc ) {
        for ( int ii = 1; ii < array->nElems; ++ii ) {
            int res = (*proc)( array->elems[ii-1], array->elems[ii] );
            XP_ASSERT( res <= 0 );
        }
    }
}
#endif

/* Via an AI summary, so how to attribute? */
static int
findFit( XWArray* array, void* node )
{
    int low = 0;
    int high = array->nElems - 1;
    int result;

    while ( low <= high ) {
        int mid = (low + high) / 2;
        if ( array->elems[mid] == node ) {
            result = mid;
            break;
        } else if ( 0 > (*array->proc)(array->elems[mid], node ) ) {
            low = mid + 1;
            result = low;
        } else {
            high = mid - 1;
            result = high + 1;
        }
    }

    return result;
}

static void
moveUpOne( XWArray* array, int from )
{
    void** elems = array->elems;
    for ( int ii = array->nElems - 1; ii >= from; --ii ) {
        elems[ii+1] = elems[ii];
    }
}

static void
moveDownOne( XWArray* array, int from )
{
    void** elems = array->elems;
    for ( int ii = from; ii < array->nElems - 1; ++ii ) {
        elems[ii] = elems[ii+1];
    }
}
