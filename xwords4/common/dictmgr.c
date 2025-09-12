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
#include "dictmgrp.h"
#include "xwmutex.h"
#include "xwarray.h"

#ifdef CPLUS
extern "C" {
#endif

typedef struct _DictPair {
    XP_UCHAR* key;
    const DictionaryCtxt* dict;
} DictPair;

struct DictMgrCtxt {
    XWArray* pairs;
    MutexState mutex;
};

static int
sortByKeyProc(const void* dl1, const void* dl2,
              XWEnv XP_UNUSED(xwe), void* XP_UNUSED(closure))
{
    const XP_UCHAR* key1 = ((DictPair*)dl1)->key;
    const XP_UCHAR* key2 = ((DictPair*)dl2)->key;
    int result = strcmp( key1, key2 );
    return result;
}

void
dmgr_make( XW_DUtilCtxt* dutil )
{
    LOG_FUNC();
    DictMgrCtxt* dictMgr = XP_CALLOC( dutil->mpool, sizeof(*dictMgr) );
    MUTEX_INIT( &dictMgr->mutex, XP_FALSE );

    dictMgr->pairs = arr_make(dutil->mpool, sortByKeyProc, NULL);

    dutil->dictMgr = dictMgr;
}

typedef struct _DestroyDictData {
    XWEnv xwe;
    XW_DUtilCtxt* dutil;
} DestroyDictData;

static void
destroyDictDataProc( void* elem, void* closure )
{
    DestroyDictData* ddd = (DestroyDictData*)closure;
    DictPair* pair = (DictPair*)elem;
    dict_unref( pair->dict, ddd->xwe );
    XP_FREE( ddd->dutil->mpool, pair->key );
    XP_FREE( ddd->dutil->mpool, pair );
}

void
dmgr_destroy( XW_DUtilCtxt* dutil, XWEnv xwe )
{
    DictMgrCtxt* dictMgr = dutil->dictMgr;
    DestroyDictData ddd = {
        .xwe = xwe,
        .dutil = dutil,
    };
    arr_removeAll( dictMgr->pairs, destroyDictDataProc, &ddd );
    arr_destroy( dictMgr->pairs );
    MUTEX_DESTROY( &dictMgr->mutex );
    XP_FREEP( dutil->mpool, &dutil->dictMgr );
}

static void
putImpl( XW_DUtilCtxt* dutil, XWEnv xwe, const XP_UCHAR* key,
         const DictionaryCtxt* dict )
{
    DictPair* dp = XP_CALLOC( dutil->mpool, sizeof(*dp) );
    dp->dict = dict_ref( dict );
    dp->key = copyString( dutil->mpool, key );
    arr_insert( dutil->dictMgr->pairs, xwe, dp );
}

const DictionaryCtxt*
dmgr_get( XW_DUtilCtxt* dutil, XWEnv xwe, const XP_UCHAR* key )
{
    XP_LOGFF( "(key: %s)", key );
    const DictionaryCtxt* result = NULL;

    DictMgrCtxt* dictMgr = dutil->dictMgr;
    WITH_MUTEX_CHECKED( &dictMgr->mutex, 1 );

    XP_U32 indx;
    DictPair dp = { .key = (XP_UCHAR*)key };
    if ( arr_find( dictMgr->pairs, xwe, &dp, &indx ) ) {
        DictPair* dp = (DictPair*)arr_getNth( dictMgr->pairs, indx );
        result = dict_ref( dp->dict );
    } else {
        const DictionaryCtxt* dict = dutil_makeDict( dutil, xwe, key );
        if ( !!dict ) {
            putImpl( dutil, xwe, key, dict );
            result = dict;
        }
    }
    
    XP_LOGFF( "(key=%s)=>%p", key, result );
    // printInOrder( dmgr );
    END_WITH_MUTEX();
    return result;
}

void
dmgr_remove( XW_DUtilCtxt* dutil, XWEnv xwe, const XP_UCHAR* key )
{
    DictMgrCtxt* dictMgr = dutil->dictMgr;
    WITH_MUTEX_CHECKED( &dictMgr->mutex, 1 );
    XP_U32 indx;
    DictPair dp = { .key = (XP_UCHAR*)key };
    if ( arr_find( dictMgr->pairs, xwe, &dp, &indx ) ) {
        DictPair* dp = (DictPair*)arr_getNth( dictMgr->pairs, indx );
        arr_remove( dictMgr->pairs, xwe, dp );
        DestroyDictData ddd = { .xwe = xwe, .dutil = dutil, };
        destroyDictDataProc( dp, &ddd );
    } else {
        XP_LOGFF( "dict %s not found", key );
        XP_ASSERT(0);
    }
    END_WITH_MUTEX();
}

void
dmgr_put( XW_DUtilCtxt* dutil, XWEnv xwe, const XP_UCHAR* key,
          const DictionaryCtxt* dict )
{
#ifdef DEBUG
    DictMgrCtxt* dictMgr = dutil->dictMgr;
    DictPair dp = { .key = (XP_UCHAR*)key };
    XP_ASSERT( !arr_find( dictMgr->pairs, xwe, &dp, NULL ) );
#endif
    WITH_MUTEX( &dictMgr->mutex );
    putImpl( dutil, xwe, key, dict );
    END_WITH_MUTEX();
}

#ifdef CPLUS
}
#endif
