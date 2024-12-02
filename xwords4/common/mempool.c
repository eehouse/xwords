/* 
 * Copyright 2001-2009 by Eric House (xwords@eehouse.org).  All rights
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

#ifdef MEM_DEBUG

#include "mempool.h"
#include "comtypes.h"
#include "xwstream.h"

// #define MUTEX_LOG_VERBOSE
#include "xwmutex.h"

/* #define MPOOL_DEBUG */

#ifdef CPLUS
extern "C" {
#endif


typedef struct MemPoolEntry {
    struct MemPoolEntry* next;
    const char* fileName;
    const char* func;
    XP_U32 lineNo;
    XP_U32 size;
    void* ptr;
    XP_U16 index;
} MemPoolEntry;

struct MemPoolCtx {
    MutexState mutex;
    MemPoolEntry* freeList;
    MemPoolEntry* usedList;

    XP_U16 nFree;
    XP_U16 nUsed;
    XP_U16 nAllocs;
    MPStatsBuf stats;

    XP_UCHAR tag[64];
};

/*--------------------------------------------------------------------------*/

MemPoolCtx*
mpool_make( const XP_UCHAR* tag )
{
    MemPoolCtx* result = (MemPoolCtx*)XP_PLATMALLOC( sizeof(*result) );
    XP_MEMSET( result, 0, sizeof(*result) );
    MUTEX_INIT( &result->mutex, XP_TRUE );
    mpool_setTag( result, tag );
    return result;
} /* mpool_make */

void 
mpool_setTag( MemPoolCtx* mpool, const XP_UCHAR* tag )
{
    if ( !!tag ) {
        if( !!mpool->tag[0] ) {
            XP_LOGF( "%s: tag changing from %s to %s", __func__,
                     mpool->tag, tag );
        }
        XP_ASSERT( XP_STRLEN(tag) < sizeof(mpool->tag) + 1 );
        XP_MEMCPY( &mpool->tag, tag, XP_STRLEN(tag) + 1 );
    } else {
        mpool->tag[0] = '\0';
    }
}

const XP_UCHAR* 
mpool_getTag( const MemPoolCtx* mpool )
{
    return mpool->tag;
}

static void
freeList( MemPoolEntry* entry )
{
    while ( !!entry ) {
        MemPoolEntry* next = entry->next;

        XP_ASSERT( !entry->ptr );
        XP_PLATFREE( entry );

        entry = next;
    }
} /* freeList */

#ifdef DEBUG
static char*
checkIsText( MemPoolEntry* entry )
{
    unsigned char* txt = (unsigned char*)entry->ptr;
    XP_U32 len = entry->size;
    char* result = NULL;

    if ( 0 < len ) {
        while ( len-- ) {
            unsigned char c = *txt++;
            if ( c < 32 || c > 127 ) {
                if ( len == 0 && c == '\0' ) {
                    result = (char*)entry->ptr;
                }
                break;
            }
        }
    }

    return result;
} /* checkIsText */
#endif

void
mpool_destroy( MemPoolCtx* mpool )
{
    if ( mpool->nUsed > 0 ) {
        XP_WARNF( "leaking %d blocks (of %d allocs)", mpool->nUsed, 
                  mpool->nAllocs );
    }
    if ( !!mpool->usedList ) {
        MemPoolEntry* entry;
        for ( entry = mpool->usedList; !!entry; entry = entry->next ) {
#ifndef FOR_GREMLINS /* I don't want to hear about this right now */
            XP_LOGFF( "ptr: " XP_P "; index=%d, allocated %s(), ln %d of %s\n",
                     entry->ptr, entry->index, 
                     entry->func, entry->lineNo, entry->fileName );
#ifdef DEBUG
            {
                char* tryTxt;
                tryTxt = checkIsText( entry );
                if ( !!tryTxt ) {
                    XP_WARNF( "--- looks like text: %s\n", tryTxt );
                }
            }
#endif
#endif
        }
    }

#ifndef FOR_GREMLINS
    XP_ASSERT( !mpool->usedList );
    XP_ASSERT( mpool->nUsed == 0 );
#endif

    freeList( mpool->freeList );
    MUTEX_DESTROY( &mpool->mutex );
    XP_PLATFREE( mpool );
} /* mpool_destroy */

void*
mpool_alloc( MemPoolCtx* mpool, XP_U32 size, const char* file, 
             const char* func, XP_U32 lineNo )
{
    void* result = NULL;

    WITH_MUTEX( &mpool->mutex );
    MemPoolEntry* entry;
    if ( mpool->nFree > 0 ) {
        entry = mpool->freeList;
        mpool->freeList = entry->next;
        --mpool->nFree;
    } else {
        entry = (MemPoolEntry*)XP_PLATMALLOC( sizeof(*entry) );
    }

    entry->next = mpool->usedList;
    mpool->usedList = entry;

    entry->fileName = file;
    entry->func = func;
    entry->lineNo = lineNo;
    entry->size = size;
    entry->ptr = XP_PLATMALLOC( size );
    XP_ASSERT( !!entry->ptr );
    entry->index = ++mpool->nAllocs;

    ++mpool->nUsed;
    mpool->stats.curBytes += size;
    if ( mpool->stats.curBytes > mpool->stats.maxBytes ) {
        mpool->stats.maxBytes = mpool->stats.curBytes;
    }

    result = entry->ptr;
    END_WITH_MUTEX();

#ifdef MPOOL_DEBUG
    XP_LOGFF( "(size=%d,index=%d,file=%s,lineNo=%d)=>%p",
             size, entry->index, file, lineNo, entry->ptr );
#endif

    return result;
} /* mpool_alloc */

void*
mpool_calloc( MemPoolCtx* mpool, XP_U32 size, const char* file, 
             const char* func, XP_U32 lineNo )
{
    void* ptr = mpool_alloc( mpool, size, file, func, lineNo );
    XP_MEMSET( ptr, 0, size );
    return ptr;
}

static MemPoolEntry*
findEntryFor( MemPoolCtx* mpool, void* ptr, MemPoolEntry** prevP )
{
    MemPoolEntry* result = NULL;
    MemPoolEntry* entry;
    MemPoolEntry* prev;

    for ( prev = (MemPoolEntry*)NULL, entry = mpool->usedList;
          !!entry && !result;
          prev = entry, entry = prev->next ) {

        if ( entry->ptr == ptr ) {

            if ( !!prevP ) {
                *prevP = prev;
            }

            result = entry;
        }
    }
    if ( !result ) {
        XP_LOGFF( "(mpool=%p, ptr=%p): NOT FOUND", mpool, ptr );
    }
    return result;
} /* findEntryFor */

void* 
mpool_realloc( MemPoolCtx* mpool, void* ptr, XP_U32 newsize, const char* file, 
               const char* func, XP_U32 lineNo )
{
    // XP_LOGF( "%s(func=%s, line=%d): newsize: %d", __func__, func, lineNo, newsize );
    void* result;
    if ( ptr == NULL ) {
        result = mpool_alloc( mpool, newsize, file, func, lineNo );
    } else {
        MemPoolEntry* entry = findEntryFor( mpool, ptr, (MemPoolEntry**)NULL );

        if ( !entry ) {
            XP_LOGFF( "findEntryFor(ptr: %p) failed; called from %s in %s, line %d",
                      ptr, func, file, lineNo );
        } else {
            entry->ptr = XP_PLATREALLOC( entry->ptr, newsize );
            XP_ASSERT( !!entry->ptr );
            entry->fileName = file;
            entry->func = func;
            entry->lineNo = lineNo;
            mpool->stats.curBytes += newsize - entry->size;
            if ( mpool->stats.curBytes > mpool->stats.maxBytes ) {
                mpool->stats.maxBytes = mpool->stats.curBytes;
            }
            entry->size = newsize;
        }
        result = entry->ptr;
    }
    return result;
} /* mpool_realloc */

void
mpool_free( MemPoolCtx* mpool, void* ptr, const char* file, 
            const char* func, XP_U32 lineNo )
{
    MemPoolEntry* prev;

    WITH_MUTEX( &mpool->mutex );

    MemPoolEntry* entry = findEntryFor( mpool, ptr, &prev );

    if ( !entry ) {
        XP_LOGFF( "findEntryFor failed; pool %p, line %d in %s", mpool,
                  lineNo, file );
        XP_ASSERT( 0 );
    } else {

#ifdef MPOOL_DEBUG
    XP_LOGFF( "(ptr=%p):size=%d,index=%d,func=%s,file=%s,lineNo=%d); "
              "called from %s",
              entry->ptr, entry->size, entry->index, entry->func,
              entry->fileName, entry->lineNo, func );
#else
    XP_USE(func);               /* shut up, compiler */
#endif

        if ( !!prev ) {
            prev->next = entry->next;
        } else {
            mpool->usedList = entry->next;
        }
        mpool->stats.curBytes -= entry->size;

        XP_MEMSET( entry->ptr, 0x00, entry->size );
        XP_PLATFREE( entry->ptr );
        entry->ptr = NULL;

        entry->next = mpool->freeList;
        mpool->freeList = entry;

        ++mpool->nFree;
        --mpool->nUsed;
    }
    END_WITH_MUTEX();
} /* mpool_free */

void
mpool_freep( MemPoolCtx* mpool, void** ptr, const char* file, 
             const char* func, XP_U32 lineNo )
{
    if ( !!*ptr ) {
        mpool_free( mpool, *ptr, file, func, lineNo );
        *ptr = NULL;
    }
}

#define STREAM_OR_LOG(stream,buf) \
    if ( !!stream ) { \
        stream_catString( stream, buf ); \
    } else { \
        XP_LOGF( "%s", buf ); \
    } \

XP_Bool
mpool_getStats( const MemPoolCtx* mpool, MPStatsBuf* io )
{
    XP_Bool changed = 0 != XP_MEMCMP( &mpool->stats, io, sizeof(mpool->stats) );
    if ( changed ) {
        *io = mpool->stats;
    }
    return changed;
}

void
mpool_stats( MemPoolCtx* mpool, XWStreamCtxt* stream )
{
    XP_UCHAR buf[128];
    MemPoolEntry* entry;
    XP_U32 total = 0;
    
    XP_SNPRINTF( buf, sizeof(buf), (XP_UCHAR*)"Number of blocks in use: %d\n"
                 "Number of free blocks: %d\n"
                 "Total number of blocks allocated: %d\n",
                 mpool->nUsed, mpool->nFree, mpool->nAllocs );
    STREAM_OR_LOG( stream, buf );

    for ( entry = mpool->usedList; !!entry; entry = entry->next ) {
        XP_SNPRINTF( buf, sizeof(buf), 
                     (XP_UCHAR*)"%d byte block allocated at %p, at line %d "
                     "in %s, %s\n", entry->size, entry->ptr, entry->lineNo, 
                     entry->func, entry->fileName );
        STREAM_OR_LOG( stream, buf );
        total += entry->size;
    }

    XP_SNPRINTF( buf, sizeof(buf), "total bytes allocated: %d\n", total );
    STREAM_OR_LOG( stream, buf );

} /* mpool_stats */

XP_U16
mpool_getNUsed( MemPoolCtx* mpool )
{
    return mpool->nUsed;
} /* mpool_getNUsed */

#ifdef CPLUS
}
#endif

#endif /* MEM_DEBUG */
