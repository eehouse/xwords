/* -*- compile-command: "cd ../linux && make -j5 MEMDEBUG=TRUE"; -*- */
/* 
 * Copyright 2014 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#include <pthread.h>     /* we'll see how long this can stay cross-platform */

#include "dictnry.h"
#include "strutils.h"
#include "dictmgr.h"

#ifdef CPLUS
extern "C" {
#endif

#ifndef DMGR_MAX_DICTS
# define DMGR_MAX_DICTS 4
#endif

    /* Maintains a LRU list of DMGR_MAX_DICTS dicts and their key.  There's no
       crossplatform linked list implementation for Android and a short list
       will probably do, so we'll keep this really simple: Any mention of a
       key, whether lookup or addition, moves the item to the front of the
       list.  If somebody has to die he comes off the end. */

typedef struct _DictPair {
    XP_UCHAR* key;
    const DictionaryCtxt* dict;
} DictPair;

struct DictMgrCtxt {
    DictPair pairs[DMGR_MAX_DICTS];
    pthread_mutex_t mutex;
    MPSLOT
};

static void moveToFront( DictMgrCtxt* dmgr, XP_U16 indx );
static XP_S16 findFor( DictMgrCtxt* dmgr, const XP_UCHAR* key );
#if defined DEBUG && defined PRINT_LOTS
    static void printInOrder( const DictMgrCtxt* dmgr );
#else
# define printInOrder( dmgr )
#endif

#define NOT_FOUND -1


DictMgrCtxt* 
dmgr_make( MPFORMAL_NOCOMMA )
{
    DictMgrCtxt* dmgr = XP_CALLOC( mpool, sizeof(*dmgr) );
    pthread_mutex_init( &dmgr->mutex, NULL );
    MPASSIGN( dmgr->mpool, mpool );
    return dmgr;
}

void
dmgr_destroy( DictMgrCtxt* dmgr, XWEnv xwe )
{
    XP_U16 ii;
    for ( ii = 0; ii < DMGR_MAX_DICTS; ++ii ) {
        DictPair* pair = &dmgr->pairs[ii];
        dict_unref( pair->dict, xwe );
        XP_FREEP( dmgr->mpool, &pair->key );
    }
    pthread_mutex_destroy( &dmgr->mutex );
    XP_FREE( dmgr->mpool, dmgr );
}

const DictionaryCtxt*
dmgr_get( DictMgrCtxt* dmgr, XWEnv xwe, const XP_UCHAR* key )
{
    const DictionaryCtxt* result = NULL;

    pthread_mutex_lock( &dmgr->mutex );

    XP_S16 index = findFor( dmgr, key );
    if ( 0 <= index ) {
        result = dict_ref( dmgr->pairs[index].dict, xwe ); /* so doesn't get nuked in a race */
        moveToFront( dmgr, index );
    }

    XP_LOGFF( "(key=%s)=>%p", key, result );
    printInOrder( dmgr );
    pthread_mutex_unlock( &dmgr->mutex );
    return result;
}

void
dmgr_put( DictMgrCtxt* dmgr, XWEnv xwe, const XP_UCHAR* key, const DictionaryCtxt* dict )
{
    pthread_mutex_lock( &dmgr->mutex );

    XP_S16 loc = findFor( dmgr, key );
    if ( NOT_FOUND == loc ) { /* reuse the last one */
        moveToFront( dmgr, VSIZE(dmgr->pairs) - 1 );
        DictPair* pair = dmgr->pairs; /* the head */
        dict_unref( pair->dict, xwe );
        pair->dict = dict_ref( dict, xwe );
        replaceStringIfDifferent( dmgr->mpool, &pair->key, key );
    } else {
        moveToFront( dmgr, loc );
    }
    XP_LOGFF( "(key=%s, dict=%p)", key, dict );
    printInOrder( dmgr );

    pthread_mutex_unlock( &dmgr->mutex );
}

static XP_S16
findFor( DictMgrCtxt* dmgr, const XP_UCHAR* key )
{
    XP_S16 result = NOT_FOUND;
    XP_U16 ii;
    for ( ii = 0; ii < VSIZE(dmgr->pairs); ++ii ) {
        DictPair* pair = &dmgr->pairs[ii];
        if ( !!pair->key && 0 == XP_STRCMP( key, pair->key ) ) {
            result = ii;
            break;
        }
    }
    return result;
}

static void 
moveToFront( DictMgrCtxt* dmgr, XP_U16 indx )
{
    if ( 0 < indx ) {
        DictPair tmp = dmgr->pairs[indx];
        XP_MEMMOVE( &dmgr->pairs[1], &dmgr->pairs[0], indx * sizeof(tmp));
        dmgr->pairs[0] = tmp;
    }
}

#if defined DEBUG && defined PRINT_LOTS
static void
printInOrder( const DictMgrCtxt* dmgr )
{
    XP_U16 ii;
    for ( ii = 0; ii < VSIZE(dmgr->pairs); ++ii ) {
        const XP_UCHAR* name = dmgr->pairs[ii].key;
        XP_LOGFF( "dict[%d]: %s", ii, (NULL == name)? "<empty>" : name );
    }
}
#endif

#ifdef CPLUS
}
#endif
