/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 2001 by Eric House (xwords@eehouse.org).  All rights reserved.
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

/* #include <PalmTypes.h> */
/* #include <SystemMgr.h> */
/* #include <IrLib.h> */

#include "xwstream.h"
#include "comtypes.h"
#include "memstream.h"
#include "vtabmgr.h"

#ifdef CPLUS
extern "C" {
#endif

#define BIT_PART(pos)  ((pos)&0x00000007)
#define BYTE_PART(pos)  ((pos)>>3)

#define MIN_PACKETBUF_SIZE (1<<6)

#define STREAM_INCR_SIZE 100

#define SOCKET_STREAM_SUPER_COMMON_SLOTS \
    StreamCtxVTable* vtable; \
    void* closure; \
    XP_U32 curReadPos; \
    XP_U32 curWritePos; \
    XP_PlayerAddr channelNo; \
    XP_U8* buf; \
    MemStreamCloseCallback onClose; \
    XP_U16 nBytesWritten; \
    XP_U16 nBytesAllocated; \
    XP_U16 version; \
    XP_U8 nReadBits; \
    XP_U8 nWriteBits; \
    XP_Bool isOpen; \
    MPSLOT

#define SOCKET_STREAM_SUPER_SLOTS \
    SOCKET_STREAM_SUPER_COMMON_SLOTS

typedef struct MemStreamCtxt {
    SOCKET_STREAM_SUPER_SLOTS
} MemStreamCtxt;

static StreamCtxVTable* make_vtable( MemStreamCtxt* stream );

/* Try to keep this the only entry point to this file, and to keep it at the
 * top of the file (first executable code).
 */
XWStreamCtxt* 
mem_stream_make( MPFORMAL VTableMgr* vtmgr, void* closure, 
                 XP_PlayerAddr channelNo, MemStreamCloseCallback onClose )
{
    StreamCtxVTable* vtable;
    MemStreamCtxt* result = (MemStreamCtxt*)XP_MALLOC( mpool, 
                                                       sizeof(*result) );
    XP_MEMSET( result, 0, sizeof(*result) );

    MPASSIGN(result->mpool, mpool);

    vtable = (StreamCtxVTable*)vtmgr_getVTable( vtmgr, VTABLE_MEM_STREAM );
    if ( !vtable ) {
        vtable = make_vtable( result );
        vtmgr_setVTable( vtmgr, VTABLE_MEM_STREAM, vtable );
    }
    result->vtable = vtable;

    result->closure = closure;
    result->channelNo = channelNo;
    result->onClose = onClose;

    result->isOpen = XP_TRUE;

    return (XWStreamCtxt*)result;
} /* make_mem_stream */

static void
mem_stream_getBytes( XWStreamCtxt* p_sctx, void* where, XP_U16 count )
{
    MemStreamCtxt* stream = (MemStreamCtxt*)p_sctx;
    
    if ( stream->nReadBits != 0 ) {
        stream->nReadBits = 0;
    }

    XP_ASSERT( stream->curReadPos + count <= stream->nBytesAllocated );
    XP_ASSERT( stream->curReadPos + count <= stream->nBytesWritten );

    XP_MEMCPY( where, stream->buf + stream->curReadPos, count );
    stream->curReadPos += count;
    XP_ASSERT( stream->curReadPos <= stream->nBytesWritten );
} /* mem_stream_getBytes */

static XP_U8 
mem_stream_getU8( XWStreamCtxt* p_sctx )
{
    XP_U8 result;
    mem_stream_getBytes( p_sctx, &result, sizeof(result) );
    return result;
} /* mem_stream_getU8 */

static XP_U16
mem_stream_getU16( XWStreamCtxt* p_sctx )
{
    XP_U16 result;
    mem_stream_getBytes( p_sctx, &result, sizeof(result) );

    return XP_NTOHS(result);
} /* mem_stream_getU16 */

static XP_U32
mem_stream_getU32( XWStreamCtxt* p_sctx )
{
    XP_U32 result;
    mem_stream_getBytes( p_sctx, &result, sizeof(result) );
    return XP_NTOHL( result );
} /* mem_stream_getU32 */

static XP_Bool
getOneBit( MemStreamCtxt* stream )
{
    XP_U8 mask, rack;
    XP_Bool result;

    if ( stream->nReadBits == 0 ) {
        ++stream->curReadPos;
    }
    XP_ASSERT( stream->curReadPos <= stream->nBytesWritten );

    rack = stream->buf[stream->curReadPos-1];
    mask = 1 << stream->nReadBits++;
    result = (rack & mask) != 0;

    if ( stream->nReadBits == 8 ) {
        stream->nReadBits = 0;
    }
    return result;
} /* getOneBit */

static XP_U32
mem_stream_getBits( XWStreamCtxt* p_sctx, XP_U16 nBits )
{
    MemStreamCtxt* stream = (MemStreamCtxt*)p_sctx;
    XP_U32 mask;
    XP_U32 result = 0;

    for ( mask = 1L; nBits--; mask <<= 1 ) {
        if ( getOneBit( stream ) ) {
            result |= mask;
        }
    }

    return result;
} /* stream_getBits */

static void
mem_stream_putBytes( XWStreamCtxt* p_sctx, const void* whence, 
                     XP_U16 count )
{
    MemStreamCtxt* stream = (MemStreamCtxt*)p_sctx;
    XP_U32 newSize;

    if ( !stream->buf ) {
        XP_ASSERT( stream->nBytesAllocated == 0 );
        stream->buf = (XP_U8*)XP_MALLOC( stream->mpool, STREAM_INCR_SIZE );
        stream->nBytesAllocated = STREAM_INCR_SIZE;
    }

    /* I don't yet deal with getting asked to get/put a byte when in the
       middle of doing bitwise stuff.  It's probably just a matter of skipping
       to the next byte, though -- and curPos should already be there. */
    if ( stream->nWriteBits != 0 ) {
        stream->nWriteBits = 0;
    }

    /* Reallocation.  We may be writing into the middle of an existing stream,
       and doing so may still require expanding the stream.  So figure out if
       the new size is bigger than what we have, and if so expand to hold it
       plus something. */

    newSize = stream->nBytesWritten + count;
    if ( stream->curWritePos < stream->nBytesWritten ) {
        newSize -= stream->nBytesWritten - stream->curWritePos;
    }

    if ( newSize > stream->nBytesAllocated ) {
        XP_ASSERT( newSize + STREAM_INCR_SIZE < 0xFFFF );
        stream->nBytesAllocated = (XP_U16)newSize + STREAM_INCR_SIZE;
        stream->buf = 
            (XP_U8*)XP_REALLOC( stream->mpool, stream->buf, 
                                stream->nBytesAllocated );
    }
    
    XP_MEMCPY( stream->buf + stream->curWritePos, whence, count );
    stream->nBytesWritten = (XP_U16)newSize;
    stream->curWritePos += count;
} /* mem_stream_putBytes */

static void
mem_stream_catString( XWStreamCtxt* p_sctx, const char* whence )
{
    XP_U16 len = XP_STRLEN( whence );
    mem_stream_putBytes( p_sctx, (void*)whence, len );
}

static void
mem_stream_putU8( XWStreamCtxt* p_sctx, XP_U8 data )
{
    mem_stream_putBytes( p_sctx, &data, sizeof(data) );
} /* mem_stream_putU8 */

static void
mem_stream_putU16( XWStreamCtxt* p_sctx, XP_U16 data )
{
    data = XP_HTONS( data );
    mem_stream_putBytes( p_sctx, &data, sizeof(data) );
} /* linux_common_stream_putU16 */

static void
mem_stream_putU32( XWStreamCtxt* p_sctx, XP_U32 data )
{
    data = XP_HTONL( data );
    mem_stream_putBytes( p_sctx, &data, sizeof(data) );
} /* mem_stream_putU32 */

static void
putOneBit( MemStreamCtxt* stream, XP_U16 bit )
{
    XP_U8 mask, rack;

    if ( stream->nWriteBits == 0 ) {
        if ( stream->curWritePos == stream->nBytesWritten ) {
            stream_putU8( (XWStreamCtxt*)stream, 0 ); /* increments curPos */
        } else {
            ++stream->curWritePos;
        }
    }

    XP_ASSERT( stream->curWritePos > 0 );
    rack = stream->buf[stream->curWritePos-1];
    mask = 1 << stream->nWriteBits++;
    if ( bit ) {
        rack |= mask;
    } else {
        rack &= ~mask;
    }
    stream->buf[stream->curWritePos-1] = rack;

    stream->nWriteBits %= 8;
} /* putOneBit */

static void
mem_stream_putBits( XWStreamCtxt* p_sctx, XP_U16 nBits, XP_U32 data
                    DBG_LINE_FILE_FORMAL )
{
    MemStreamCtxt* stream = (MemStreamCtxt*)p_sctx;
#ifdef DEBUG
    XP_U16 origBits = nBits;
#endif

    XP_ASSERT( nBits > 0 );

    while ( nBits-- ) {
        putOneBit( stream, (XP_U16)(((data & 1L) != 0)? 1:0) );
        data >>= 1;
    }
    XP_ASSERT( data == 0 );     /* otherwise nBits was too small */
#ifdef DEBUG
    if ( data != 0 ) {
        XP_LOGF( "%s: nBits was %d from line %d, %s", __func__, 
                 origBits, lin, fil );
    }
#endif
} /* mem_stream_putBits */

static void
mem_stream_copyFromStream( XWStreamCtxt* p_sctx, XWStreamCtxt* src, 
                           XP_U16 nBytes )
{
    while ( nBytes > 0 ) {
        XP_U8 buf[256];
        XP_U16 len = sizeof(buf);
        if ( nBytes < len ) {
            len = nBytes;
        }
        stream_getBytes( src, buf, len );
        stream_putBytes( p_sctx, buf, len );
        nBytes -= len;
    }
} /* mem_stream_copyFromStream */

static void
mem_stream_open( XWStreamCtxt* p_sctx )
{
    MemStreamCtxt* stream = (MemStreamCtxt*)p_sctx;

    stream->nBytesWritten = 0;
    stream->curReadPos = START_OF_STREAM;
    stream->curWritePos = START_OF_STREAM;
} /* mem_stream_open */

static void
mem_stream_close( XWStreamCtxt* p_sctx )
{
    MemStreamCtxt* stream = (MemStreamCtxt*)p_sctx;

    XP_ASSERT( stream->isOpen );

    if ( !!stream->onClose ) {
        (*stream->onClose)( p_sctx, stream->closure );
    }
    stream->isOpen = XP_FALSE;
} /* mem_stream_close */

static XP_U16
mem_stream_getSize( const XWStreamCtxt* p_sctx )
{
    MemStreamCtxt* stream = (MemStreamCtxt*)p_sctx;
    XP_U16 size = stream->nBytesWritten - stream->curReadPos;
    return size;
} /* mem_stream_getSize */

static XP_PlayerAddr
mem_stream_getAddress( XWStreamCtxt* p_sctx )
{
    MemStreamCtxt* stream = (MemStreamCtxt*)p_sctx;
    return stream->channelNo;
} /* mem_stream_getAddress */

static void
mem_stream_setAddress( XWStreamCtxt* p_sctx, XP_PlayerAddr channelNo )
{
    MemStreamCtxt* stream = (MemStreamCtxt*)p_sctx;
    stream->channelNo = channelNo;
} /* mem_stream_getAddress */

static void
mem_stream_setVersion( XWStreamCtxt* p_sctx, XP_U16 vers )
{
    MemStreamCtxt* stream = (MemStreamCtxt*)p_sctx;
    stream->version = vers;
} /* mem_stream_setVersion */

static XP_U16
mem_stream_getVersion( XWStreamCtxt* p_sctx )
{
    MemStreamCtxt* stream = (MemStreamCtxt*)p_sctx;
    return stream->version;
} /* mem_stream_getVersion */

static void
mem_stream_setOnCloseProc( XWStreamCtxt* p_sctx, MemStreamCloseCallback proc )
{
    MemStreamCtxt* stream = (MemStreamCtxt*)p_sctx;
    stream->onClose = proc;
}

static XWStreamPos
mem_stream_getPos( XWStreamCtxt* p_sctx, PosWhich which )
{
    XWStreamPos result;
    MemStreamCtxt* stream = (MemStreamCtxt*)p_sctx;
    
    if ( which == POS_WRITE ) {
        result = (stream->curWritePos << 3) | stream->nWriteBits;
    } else {
        result = (stream->curReadPos << 3) | stream->nReadBits;
    }

    return result;
} /* mem_stream_getPos */

static XWStreamPos
mem_stream_setPos( XWStreamCtxt* p_sctx, XWStreamPos newpos, PosWhich which )
{
    MemStreamCtxt* stream = (MemStreamCtxt*)p_sctx;
    XWStreamPos oldPos = mem_stream_getPos( p_sctx, which );

    if ( which == POS_WRITE ) {
        stream->nWriteBits = (XP_U8)BIT_PART(newpos);
        stream->curWritePos = (XP_U32)BYTE_PART(newpos);
    } else {
        stream->nReadBits = (XP_U8)BIT_PART(newpos);
        stream->curReadPos = (XP_U32)BYTE_PART(newpos);
    }

    return oldPos;
} /* mem_stream_setPos */

static void
mem_stream_destroy( XWStreamCtxt* p_sctx )
{
    MemStreamCtxt* stream = (MemStreamCtxt*)p_sctx;

    if ( stream->isOpen ) {
        stream_close( p_sctx );
    }

    if ( !!stream->buf ) {
        XP_FREE( stream->mpool, stream->buf );
    }
    
    XP_FREE( stream->mpool, stream );
} /* mem_stream_destroy */

static StreamCtxVTable*
make_vtable( MemStreamCtxt* stream )
{
    StreamCtxVTable* vtable;
    XP_ASSERT( !stream->vtable );
    XP_ASSERT( sizeof(stream->vtable) == sizeof(vtable) );
    vtable = (StreamCtxVTable*)XP_MALLOC( stream->mpool, 
                                          sizeof(*stream->vtable) );

    SET_VTABLE_ENTRY( vtable, stream_getU8, mem );
    SET_VTABLE_ENTRY( vtable, stream_getBytes, mem );
    SET_VTABLE_ENTRY( vtable, stream_getU16, mem );
    SET_VTABLE_ENTRY( vtable, stream_getU32, mem );
    SET_VTABLE_ENTRY( vtable, stream_getBits, mem );

    SET_VTABLE_ENTRY( vtable, stream_putU8, mem );
    SET_VTABLE_ENTRY( vtable, stream_putBytes, mem );
    SET_VTABLE_ENTRY( vtable, stream_catString, mem );
    SET_VTABLE_ENTRY( vtable, stream_putU16, mem );
    SET_VTABLE_ENTRY( vtable, stream_putU32, mem );
    SET_VTABLE_ENTRY( vtable, stream_putBits, mem );

    SET_VTABLE_ENTRY( vtable, stream_copyFromStream, mem );

    SET_VTABLE_ENTRY( vtable, stream_setPos, mem );
    SET_VTABLE_ENTRY( vtable, stream_getPos, mem );

    SET_VTABLE_ENTRY( vtable, stream_destroy, mem );
    SET_VTABLE_ENTRY( vtable, stream_open, mem );
    SET_VTABLE_ENTRY( vtable, stream_close, mem );

    SET_VTABLE_ENTRY( vtable, stream_getSize, mem );
    SET_VTABLE_ENTRY( vtable, stream_getAddress, mem );
    SET_VTABLE_ENTRY( vtable, stream_setAddress, mem );

    SET_VTABLE_ENTRY( vtable, stream_setVersion, mem );
    SET_VTABLE_ENTRY( vtable, stream_getVersion, mem );

    SET_VTABLE_ENTRY( vtable, stream_setOnCloseProc, mem );

    return vtable;
} /* make_vtable */

#ifdef CPLUS
}
#endif
