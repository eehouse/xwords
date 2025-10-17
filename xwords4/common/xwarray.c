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
    void* procClosure;
#ifdef DEBUG
    const char* caller;
    int line;
#endif
    MPSLOT
};

static void ensureRoom( XWArray* array, XP_U32 forNew );
static XP_U32 findFit( XWArray* array, XWEnv xwe, const void* node );
static void moveUpOne( XWArray* array, int from );
static void moveDownOne( XWArray* array, int from );
#ifdef DEBUG
static void assertSorted( XWArray* array, XWEnv xwe );
#else
# define assertSorted(X, xwe)
#endif

XWArray*
arr_make_impl(
#ifdef MEM_DEBUG
               MemPoolCtx* mpool,
#endif
               ArCompProc sortProc, void* procClosure
#ifdef DEBUG
              ,const char* caller, int line
#endif
              )
{
    XWArray* array = XP_CALLOC(mpool, sizeof(*array) );
#ifdef DEBUG
    array->caller = caller;
    array->line = line;
#endif
#ifdef MEM_DEBUG
    array->mpool = mpool;
#endif
    arr_setSort( array, NULL, sortProc, procClosure );
    return array;
}

void
arr_destroyp( XWArray** array )
{
    if ( !!*array ) {
        arr_destroy( *array );
        *array = NULL;
    }
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

const void*
arr_getData(const XWArray* array)
{
    return array->elems;
}

static int
callProc( XWArray* array, XWEnv xwe, const void* elem1, const void* elem2 )
{
    int result = 0;
    if ( elem1 != elem2 ) {
        result = (*array->proc)(elem1, elem2, xwe, array->procClosure);
    }
    return result;
}

XP_Bool
arr_find( XWArray* array, XWEnv xwe, const void* target, XP_U32* locp )
{
    XP_U32 indx = findFit( array, xwe, target );
    XP_Bool found = indx < array->nElems
        && 0 == callProc(array, xwe, array->elems[indx], target );
    if ( found && !!locp ) {
        *locp = indx;
    }
    return found;
}

void*
arr_remove( XWArray* array, XWEnv xwe, void* node )
{
    assertSorted( array, xwe );
    int loc = findFit( array, xwe, node );
    XP_ASSERT( 0 <= loc );
    void* found = arr_removeAt( array, xwe, loc );
    assertSorted( array, xwe );
    return found;
}

void*
arr_removeAt( XWArray* array, XWEnv XP_UNUSED_DBG(xwe), XP_U32 loc )
{
    assertSorted( array, xwe );
    void* removed = array->elems[loc];
    moveDownOne( array, loc );
    --array->nElems;
    assertSorted( array, xwe );
    return removed;
}

XP_U32
arr_insert( XWArray* array, XWEnv xwe, void* node )
{
    assertSorted( array, xwe );
    XP_U32 indx = ( !array->proc || 0 == array->nElems )
        ? array->nElems
        : findFit( array, xwe, node );
    arr_insertAt( array, node, indx );
    assertSorted( array, xwe );
    return indx;
}

void
arr_insertAt( XWArray* array, void* node, XP_U32 loc )
{
    XP_ASSERT( 0 <= loc && loc <= array->nElems );
    ensureRoom( array, 1 );
    moveUpOne( array, loc );
    array->elems[loc] = node;
    ++array->nElems;
}

void*
arr_getNth( XWArray* array, XP_U32 nn )
{
    // XP_LOGFF( "nn=%d; nElems=%d", nn, array->nElems );
    XP_ASSERT( nn < array->nElems );
    return array->elems[nn];
}

void
arr_setSort( XWArray* array, XWEnv xwe, ArCompProc newProc, void* procClosure )
{
    if ( newProc != array->proc ) {
        array->proc = newProc;
        array->procClosure = procClosure;
        if ( 0 < array->nElems ) {
            void** oldElems = array->elems;
            XP_U32 oldNElems = array->nElems;
            array->elems = NULL;
            array->nElems = array->capacity = 0;

            for ( int ii = 0; ii < oldNElems; ++ii ) {
                arr_insert( array, xwe, oldElems[ii] );
            }
            XP_FREE( array->mpool, oldElems );
        }
    }
    assertSorted( array, xwe );
}

void
arr_map( XWArray* array, XWEnv xwe, ArMapProc mapProc, void* closure )
{
    assertSorted( array, xwe );
    for ( int indx = 0; indx < array->nElems; ++indx ) {
        ForEachAct fea = (*mapProc)(array->elems[indx], closure, xwe);
        if ( FEA_REMOVE & fea ) {
            moveDownOne( array, indx );
            --array->nElems;
            --indx;
        }
        if ( FEA_EXIT & fea ) {
            break;
        }
    }
    assertSorted( array, xwe );
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

int
PtrCmpProc(const void* dl1, const void* dl2,
           XWEnv XP_UNUSED(xwe), void* XP_UNUSED(closure))
{
    int result;
    if ( dl1 < dl2 ) result = -1;
    else if ( dl1 > dl2 ) result = 1;
    else result = 0;
    return result;
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
assertSorted( XWArray* array, XWEnv xwe )
{
    ArCompProc proc = array->proc;
    if ( !!proc ) {
        for ( int ii = 1; ii < array->nElems; ++ii ) {
            int res = callProc( array, xwe, array->elems[ii-1], array->elems[ii] );
            if ( 0 < res ) {
                XP_LOGFF( "ERROR: array from %s line %d out-of-order", array->caller, array->line );
                XP_LOGFF( "bad elems are #%d,#%d of %d", ii-1, ii, array->nElems );
                XP_ASSERT( 0 );
            }
        }
    }
}
#endif

/* Via an AI summary, so how to attribute? */
static int
findFitBinary( XWArray* array, XWEnv xwe, const void* node )
{
    int result = -1;
    if ( 0 == array->nElems ) {
        result = 0;
    } else {
        int low = 0;
        int high = array->nElems - 1;

        while ( low <= high ) {
            int mid = (low + high) / 2;
            int comp = callProc(array, xwe, array->elems[mid], node );
            if ( 0 == comp ) {
                result = mid;
                break;
            } else if ( 0 > comp ) {
                low = mid + 1;
                result = low;
            } else {
                high = mid - 1;
                result = high + 1;
            }
        }
    }
    XP_ASSERT( 0 <= result );
    XP_ASSERT( result < 0xFFFF );
    return (XP_U32)result;
}

typedef struct _MatchData {
    const void* sought;
    int index;
} MatchData;

static ForEachAct
findExact( void* elem, void* closure, XWEnv XP_UNUSED(xwe) )
{
    ForEachAct result = FEA_OK;
    MatchData* md = (MatchData*)closure;
    if ( elem == md->sought ) {
        result |= FEA_EXIT;
    } else {
        ++md->index;
    }
    return result;
}

static XP_U32
findFitMapped( XWArray* array, XWEnv xwe, const void* node )
{
    MatchData md = { .sought = node, };
    arr_map( array, xwe, findExact, &md );
    return md.index;
}

static XP_U32
findFit( XWArray* array, XWEnv xwe, const void* node )
{
    XP_U32 result = !!array->proc
        ? findFitBinary( array, xwe, node )
        : findFitMapped( array, xwe, node );
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
