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

#ifndef _MEMPOOL_H_
#define _MEMPOOL_H_

#ifdef MEM_DEBUG

#include "comtypes.h"

#ifdef CPLUS
extern "C" {
#endif

typedef struct MemPoolCtx MemPoolCtx;

void mempool_dbg_init();
void mempool_dbg_checkall();

MemPoolCtx* mpool_make( const XP_UCHAR* tag );
void mpool_destroy( MemPoolCtx* mpool );
const XP_UCHAR* mpool_getTag( const MemPoolCtx* mpool );
void mpool_setTag( MemPoolCtx* mpool, const XP_UCHAR* tag );

void* mpool_alloc( MemPoolCtx* mpool, XP_U32 size, 
                   const char* file, const char* func, XP_U32 lineNo );
void* mpool_calloc( MemPoolCtx* mpool, XP_U32 size, const char* file, 
                    const char* func, XP_U32 lineNo );
void* mpool_realloc( MemPoolCtx* mpool, void* ptr, XP_U32 newsize, 
                     const char* file, const char* func, XP_U32 lineNo );
void mpool_free( MemPoolCtx* mpool, void* ptr, const char* file, 
                 const char* func, XP_U32 lineNo );
void mpool_freep( MemPoolCtx* mpool, void** ptr, const char* file, 
                  const char* func, XP_U32 lineNo );

void mpool_stats( MemPoolCtx* mpool, XWStreamCtxt* stream );
XP_U16 mpool_getNUsed( MemPoolCtx* mpool );

typedef struct _MPStatsBuf {
    XP_U32 curBytes;
    XP_U32 maxBytes;
} MPStatsBuf;
/* Returns true IFF any fields have changed (so assumes you use the same
   buffer over and over */
XP_Bool mpool_getStats( const MemPoolCtx* mpool, MPStatsBuf* io );

#ifdef CPLUS
}
#endif

#else

# define mpool_destroy(p)
# define mpool_setTag(t)
#endif /* MEM_DEBUG */
#endif /* _MEMPOOL_H_ */
