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

#include "dictmgr.h"
#include "dictnry.h"
#include "strutils.h"

#ifdef CPLUS
extern "C" {
#endif

struct DictMgrCtxt {
    XP_UCHAR* key;
    DictionaryCtxt* dict;
    pthread_mutex_t mutex;
    MPSLOT
};

DictMgrCtxt* 
dmgr_make( MPFORMAL_NOCOMMA )
{
    DictMgrCtxt* dmgr = XP_CALLOC( mpool, sizeof(*dmgr) );
    pthread_mutex_init( &dmgr->mutex, NULL );
    MPASSIGN( dmgr->mpool, mpool );
    return dmgr;
}

void
dmgr_destroy( DictMgrCtxt* dmgr )
{
    if ( !!dmgr->dict ) {
        dict_unref( dmgr->dict );
        XP_FREE( dmgr->mpool, dmgr->key );
    }
    pthread_mutex_destroy( &dmgr->mutex );
    XP_FREE( dmgr->mpool, dmgr );
}

DictionaryCtxt* 
dmgr_get( DictMgrCtxt* dmgr, const XP_UCHAR* key )
{
    DictionaryCtxt* result = NULL;

    pthread_mutex_lock( &dmgr->mutex );
    if ( !!dmgr->key && 0 == XP_STRCMP( key, dmgr->key ) ) {
        result = dmgr->dict;
    }
    pthread_mutex_unlock( &dmgr->mutex );

    XP_LOGF( "%s(key=%s)=>%p", __func__, key, result );
    return result;
}

void
dmgr_put( DictMgrCtxt* dmgr, const XP_UCHAR* key, DictionaryCtxt* dict )
{
    XP_LOGF( "%s(key=%s, dict=%p)", __func__, key, dict );
    pthread_mutex_lock( &dmgr->mutex );
    if ( !dmgr->key ) {        /* just install it */
        dmgr->dict = dict_ref( dict );
        dmgr->key = copyString( dmgr->mpool, key );
    } else if ( 0 == XP_STRCMP( key, dmgr->key ) ) {
        /* do nothing */
        XP_ASSERT( dict == dmgr->dict );
    } else {
        dict_unref( dmgr->dict );
        dmgr->dict = dict_ref( dict );
        replaceStringIfDifferent( dmgr->mpool, &dmgr->key, key );
    }
    pthread_mutex_unlock( &dmgr->mutex );
}

#ifdef CPLUS
}
#endif
