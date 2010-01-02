/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
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
} MemPoolEntry;

struct MemPoolCtx {
    MemPoolEntry* freeList;
    MemPoolEntry* usedList;

    XP_U16 nFree;
    XP_U16 nUsed;
    XP_U16 nAllocs;
};

/*--------------------------------------------------------------------------*/

MemPoolCtx*
mpool_make( void )
{
    MemPoolCtx* result = (MemPoolCtx*)XP_PLATMALLOC( sizeof(*result) );
    XP_MEMSET( result, 0, sizeof(*result) );
    return result;
} /* mpool_make */

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

    while ( len-- ) {
        unsigned char c = *txt++;
        if ( c < 32 || c > 127 ) {
            if ( len == 0 && c == '\0' ) {
                return (char*)entry->ptr;
            } else {
                return (char*)NULL;
            }
        }
    }

    return (char*)NULL;
} /* checkIsText */
#endif

void
mpool_destroy( MemPoolCtx* mpool )
{
    if ( mpool->nUsed > 0 ) {
        XP_WARNF( "leaking %d blocks", mpool->nUsed );
    }
    if ( !!mpool->usedList ) {
        MemPoolEntry* entry;
        for ( entry = mpool->usedList; !!entry; entry = entry->next ) {
#ifndef FOR_GREMLINS /* I don't want to hear about this right now */
            XP_LOGF( "%s: " XP_P " in %s, ln %ld of %s\n", __func__, 
                     entry->ptr, entry->func, entry->lineNo, entry->fileName );
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
    XP_ASSERT( !mpool->usedList && mpool->nUsed == 0 );
#endif

    freeList( mpool->freeList );
    XP_PLATFREE( mpool );
} /* mpool_destroy */

void*
mpool_alloc( MemPoolCtx* mpool, XP_U32 size, const char* file, 
             const char* func, XP_U32 lineNo )
{
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

    ++mpool->nUsed;
    ++mpool->nAllocs;

#ifdef MPOOL_DEBUG
    XP_LOGF( "%s(size=%ld,file=%s,lineNo=%ld)=>%p",
             __func__, size, file, lineNo, entry->ptr );
#endif

    return entry->ptr;
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
    MemPoolEntry* entry;
    MemPoolEntry* prev;

    for ( prev = (MemPoolEntry*)NULL, entry = mpool->usedList; !!entry; 
          prev = entry, entry = prev->next ) {

        if ( entry->ptr == ptr ) {

            if ( !!prevP ) {
                *prevP = prev;
            }

            return entry;
        }
    }
    return (MemPoolEntry*)NULL;
} /* findEntryFor */

void* 
mpool_realloc( MemPoolCtx* mpool, void* ptr, XP_U32 newsize, const char* file, 
               const char* func, XP_U32 lineNo )
{
    MemPoolEntry* entry = findEntryFor( mpool, ptr, (MemPoolEntry**)NULL );

    if ( !entry ) {
        XP_LOGF( "findEntryFor failed; called from %s, line %ld",
                 file, lineNo );
    } else {
        entry->ptr = XP_PLATREALLOC( entry->ptr, newsize );
        XP_ASSERT( !!entry->ptr );
        entry->fileName = file;
        entry->func = func;
        entry->lineNo = lineNo;
    }
    return entry->ptr;
} /* mpool_realloc */

void
mpool_free( MemPoolCtx* mpool, void* ptr, const char* file, 
            const char* func, XP_U32 lineNo )
{
    MemPoolEntry* entry;
    MemPoolEntry* prev;

    entry = findEntryFor( mpool, ptr, &prev );

    if ( !entry ) {
        XP_LOGF( "findEntryFor failed; called from %s, line %ld in %s",
                 func, lineNo, file );
    } else {

#ifdef MPOOL_DEBUG
    XP_LOGF( "%s(ptr=%p):size=%ld,func=%s,file=%s,lineNo=%ld)", __func__, 
             entry->ptr, entry->size, entry->func, entry->fileName, 
             entry->lineNo );
#endif

        if ( !!prev ) {
            prev->next = entry->next;
        } else {
            mpool->usedList = entry->next;
        }

        XP_MEMSET( entry->ptr, 0x00, entry->size );
        XP_PLATFREE( entry->ptr );
        entry->ptr = NULL;

        entry->next = mpool->freeList;
        mpool->freeList = entry;

        ++mpool->nFree;
        --mpool->nUsed;

        return;
    }

    XP_ASSERT( 0 );
} /* mpool_free */

#define STREAM_OR_LOG(stream,buf) \
    if ( !!stream ) { \
        stream_catString( stream, buf ); \
    } else { \
        XP_LOGF( "%s", buf ); \
    } \

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
                     (XP_UCHAR*)"%ld byte block allocated at %p, at line %ld "
                     "in %s, %s\n", entry->size, entry->ptr, entry->lineNo, 
                     entry->func, entry->fileName );
        STREAM_OR_LOG( stream, buf );
        total += entry->size;
    }

    XP_SNPRINTF( buf, sizeof(buf), "total bytes allocated: %ld\n", total );
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
