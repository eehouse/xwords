/* 
 * Copyright 2001 - 2023 by Eric House (xwords@eehouse.org).  All rights
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

/* #include <PalmTypes.h> */
/* #include <SystemMgr.h> */
/* #include <IrLib.h> */

#include "xwstream.h"
#include "comtypes.h"
#include "memstream.h"
#include "vtabmgr.h"
#include "strutils.h"

#ifdef CPLUS
extern "C" {
#endif

#define BIT_PART(pos)  ((pos)&0x00000007)
#define BYTE_PART(pos)  ((pos)>>3)

#define MIN_PACKETBUF_SIZE (1<<6)

#define STREAM_INCR_SIZE 100

#ifdef XWFEATURE_STREAMREF
# define REFCOUNT refCount
#else
# define REFCOUNT _unused_refCount
#endif

#define SOCKET_STREAM_SUPER_COMMON_SLOTS \
    StreamCtxVTable* vtable; \
    XP_U32 curReadPos; \
    XP_U32 curWritePos; \
    XP_PlayerAddr channelNo; \
    XP_U8* buf; \
    XP_U16 nBytesWritten; \
    XP_U16 nBytesAllocated; \
    XP_U16 version; \
    XP_U8 nReadBits; \
    XP_U8 nWriteBits; \
    XP_U8 REFCOUNT; \
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
mem_stream_make_raw( MPFORMAL VTableMgr* vtmgr )
{
    return mem_stream_make( MPPARM(mpool) vtmgr, 0 );
}

XWStreamCtxt*
mem_stream_make( MPFORMAL VTableMgr* vtmgr, XP_PlayerAddr channelNo )
{
    StreamCtxVTable* vtable;
    MemStreamCtxt* result = (MemStreamCtxt*)XP_CALLOC( mpool, 
                                                       sizeof(*result) );
    MPASSIGN(result->mpool, mpool);

    vtable = (StreamCtxVTable*)vtmgr_getVTable( vtmgr, VTABLE_MEM_STREAM );
    if ( !vtable ) {
        vtable = make_vtable( result );
        vtmgr_setVTable( vtmgr, VTABLE_MEM_STREAM, vtable );
    }
    result->vtable = vtable;

    result->channelNo = channelNo;

    result->isOpen = XP_TRUE;
#ifdef XWFEATURE_STREAMREF
    result->refCount = 1;
#endif
    return (XWStreamCtxt*)result;
} /* make_mem_stream */

XWStreamCtxt* 
mem_stream_make_sized( MPFORMAL VTableMgr* vtmgr, XP_U16 startSize, 
                       XP_PlayerAddr channelNo )
{
    MemStreamCtxt* result =
        (MemStreamCtxt*)mem_stream_make( MPPARM(mpool) vtmgr, channelNo );
    if ( 0 < startSize ) {
        result->buf = (XP_U8*)XP_CALLOC( mpool, startSize );
        result->nBytesAllocated = startSize;
    }
    return (XWStreamCtxt*)result;
}

static void
mem_stream_getBytes( DBG_PROC_FORMAL XWStreamCtxt* p_sctx, void* where, XP_U16 count )
{
    MemStreamCtxt* stream = (MemStreamCtxt*)p_sctx;
    
    if ( stream->nReadBits != 0 ) {
        stream->nReadBits = 0;
    }

#ifdef MEM_DEBUG
    if( stream->curReadPos + count > stream->nBytesAllocated ) {
        XP_LOGFF( "count %d exceeds buffer; caller: %s()", count,
                  DBG_PROC_VAL_NOCOMMA );
        XP_ASSERT(0);           /* fired */
    }
    if ( stream->curReadPos + count > stream->nBytesWritten ) {
        XP_LOGFF( "count %d exceeds data; caller: %s()", count,
                  DBG_PROC_VAL_NOCOMMA );
        XP_ASSERT(0);
    }
#else
    XP_ASSERT( stream->curReadPos + count <= stream->nBytesAllocated );
    XP_ASSERT( stream->curReadPos + count <= stream->nBytesWritten );
#endif


    XP_MEMCPY( where, stream->buf + stream->curReadPos, count );
    stream->curReadPos += count;
    XP_ASSERT( stream->curReadPos <= stream->nBytesWritten );
} /* mem_stream_getBytes */

static XP_U8 
mem_stream_getU8( DBG_PROC_FORMAL XWStreamCtxt* p_sctx )
{
    XP_U8 result;
    mem_stream_getBytes( DBG_PROC_VAL p_sctx, &result, sizeof(result) );
    return result;
} /* mem_stream_getU8 */

static XP_U16
mem_stream_getU16( DBG_PROC_FORMAL XWStreamCtxt* p_sctx )
{
    XP_U16 result;
    mem_stream_getBytes( DBG_PROC_VAL p_sctx, &result, sizeof(result) );

    return XP_NTOHS(result);
} /* mem_stream_getU16 */

static XP_U32
mem_stream_getU32( DBG_PROC_FORMAL XWStreamCtxt* p_sctx )
{
    XP_U32 result;
    mem_stream_getBytes( DBG_PROC_VAL p_sctx, &result, sizeof(result) );
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

static XP_U32
mem_stream_getU32VL( XWStreamCtxt* p_sctx )
{
    XP_U32 result = 0;
    for ( int ii = 0; ; ++ii ) {
        XP_U8 byt = mem_stream_getBits( p_sctx, 8 * sizeof(byt) );
        result |= (byt & 0x7F) << (7 * ii);
        if ( 0 == (byt & 0x80) ) {
            break;
        }
    }
    return result;
} /* mem_stream_getU32VL */

#if defined DEBUG
static void
mem_stream_copyBits( const XWStreamCtxt* p_sctx, XWStreamPos endPos,
                     XP_U8* buf, XP_U16* lenp )
{
    MemStreamCtxt* stream = (MemStreamCtxt*)p_sctx;
    XP_U16 len = BYTE_PART(endPos);
    if ( !!buf && len <= *lenp ) {
        XP_ASSERT( len <= stream->nBytesAllocated );
        XP_MEMCPY( buf, stream->buf, len );
        if ( 0 != BIT_PART(endPos) ) {
            buf[len-1] &= ~(0xFF << BIT_PART(endPos));
        }
    }
    *lenp = len;
}
#endif

static void
mem_stream_putBytes( XWStreamCtxt* p_sctx, const void* whence, 
                     XP_U16 count )
{
    MemStreamCtxt* stream = (MemStreamCtxt*)p_sctx;

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

    XP_U32 newSize = stream->nBytesWritten + count;
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
    if ( !!whence ) {
        XP_U16 len = XP_STRLEN( whence );
        mem_stream_putBytes( p_sctx, (void*)whence, len );
    }
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
            stream->buf[stream->curWritePos++] = 0;
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
#ifdef DEBUG
    if ( data != 0 ) {
        XP_LOGF( "%s: nBits was %d from line %d, %s", __func__, 
                 origBits, lin, fil );
    }
#endif
    XP_ASSERT( data == 0 );     /* otherwise nBits was too small */
} /* mem_stream_putBits */

/* Variable-length format: each 7 bits goes in a byte, with the 8th bit
 * reserved for signaling whether there's another byte to come. */
static void
mem_stream_putU32VL( XWStreamCtxt* p_sctx, XP_U32 data )
{
    for ( ; ; ) {
        XP_U8 byt = data & 0x7F;
        data >>= 7;
        XP_Bool haveMore = 0 != data;
        if ( haveMore ) {
            byt |= 0x80;
        }
        stream_putBits( p_sctx, 8 * sizeof(byt), byt );
        if ( !haveMore ) {
            break;
        }
    }
} /* mem_stream_putU32VL */

static void
mem_stream_getFromStream( XWStreamCtxt* p_sctx, XWStreamCtxt* src, 
                          XP_U16 nBytes )
{
    while ( nBytes > 0 ) {
        XP_U8 buf[256];
        XP_U16 len = sizeof(buf);
        if ( nBytes < len ) {
            len = nBytes;
        }
        stream_getBytes( src, buf, len );// fix to use stream_getPtr()?
        stream_putBytes( p_sctx, buf, len );
        nBytes -= len;
    }
} /* mem_stream_getFromStream */

static void
mem_stream_close( XWStreamCtxt* p_sctx )
{
    MemStreamCtxt* stream = (MemStreamCtxt*)p_sctx;

    XP_ASSERT( stream->isOpen );

    stream->isOpen = XP_FALSE;
} /* mem_stream_close */

static XP_U16
mem_stream_getSize( const XWStreamCtxt* p_sctx )
{
    const MemStreamCtxt* stream = (const MemStreamCtxt*)p_sctx;
    XP_U16 size = stream->nBytesWritten - stream->curReadPos;
    return size;
} /* mem_stream_getSize */

static XP_U32
mem_stream_getHash( const XWStreamCtxt* p_sctx, XWStreamPos pos )
{
    XP_U32 hash = 0;
    const MemStreamCtxt* stream = (const MemStreamCtxt*)p_sctx;
    const XP_U8* ptr = stream->buf; 
    XP_U16 len = BYTE_PART(pos);
    XP_U16 bits = BIT_PART(pos);
    if ( 0 != bits ) {
        XP_ASSERT( 0 < len );
        --len;
    }

    hash = augmentHash( 0, ptr, len );
    if ( 0 != bits ) {
        XP_U8 byt = ptr[len];
        byt &= ~(0xFF << bits);
        hash = augmentHash( hash, &byt, 1 );
    }
    hash = finishHash( hash );

    /* XP_LOGF( "%s(nBytes=%d, nBits=%d) => %X", __func__, len, bits, hash ); */
    return hash;
} /* mem_stream_getHash */

static const XP_U8*
mem_stream_getPtr( const XWStreamCtxt* p_sctx )
{
    const MemStreamCtxt* stream = (const MemStreamCtxt*)p_sctx;
    return stream->buf;
} /* mem_stream_getPtr */

static XP_PlayerAddr
mem_stream_getAddress( const XWStreamCtxt* p_sctx )
{
    const MemStreamCtxt* stream = (const MemStreamCtxt*)p_sctx;
    return stream->channelNo;
} /* mem_stream_getAddress */

static void
mem_stream_setAddress( XWStreamCtxt* p_sctx, XP_PlayerAddr channelNo )
{
    MemStreamCtxt* stream = (MemStreamCtxt*)p_sctx;
    stream->channelNo = channelNo;
} /* mem_stream_setAddress */

static void
mem_stream_setVersion( XWStreamCtxt* p_sctx, XP_U16 vers )
{
    MemStreamCtxt* stream = (MemStreamCtxt*)p_sctx;
    /* Something's wrong if we're changing it */
    XP_ASSERT( 0 == stream->version || vers == stream->version );
    stream->version = vers;
} /* mem_stream_setVersion */

static XP_U16
mem_stream_getVersion( const XWStreamCtxt* p_sctx )
{
    const MemStreamCtxt* stream = (const MemStreamCtxt*)p_sctx;
    return stream->version;
} /* mem_stream_getVersion */

static XWStreamPos
mem_stream_getPos( const XWStreamCtxt* p_sctx, PosWhich which )
{
    XWStreamPos result;
    const MemStreamCtxt* stream = (const MemStreamCtxt*)p_sctx;
    
    if ( which == POS_WRITE ) {
        result = (stream->curWritePos << 3) | stream->nWriteBits;
    } else {
        result = (stream->curReadPos << 3) | stream->nReadBits;
    }

    return result;
} /* mem_stream_getPos */

static XWStreamPos
mem_stream_setPos( XWStreamCtxt* p_sctx, PosWhich which, XWStreamPos newpos )
{
    XP_ASSERT( END_OF_STREAM != newpos ); /* not handling this yet */
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

#ifdef XWFEATURE_STREAMREF
static XWStreamCtxt*
mem_stream_ref( XWStreamCtxt* p_sctx )
{
    MemStreamCtxt* stream = (MemStreamCtxt*)p_sctx;
    ++stream->refCount;
    return p_sctx;
}
#endif

static void
mem_stream_destroy( XWStreamCtxt* p_sctx )
{
    MemStreamCtxt* stream = (MemStreamCtxt*)p_sctx;
    if ( 0 ) {
#ifdef XWFEATURE_STREAMREF
    } else if ( 0 == --stream->refCount ) {
#else
    } else {
#endif
        if ( stream->isOpen ) {
            stream_close( p_sctx );
        }

        XP_FREEP( stream->mpool, &stream->buf );
    
        XP_FREE( stream->mpool, stream );
    }
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
    SET_VTABLE_ENTRY( vtable, stream_getU32VL, mem );
    SET_VTABLE_ENTRY( vtable, stream_getBits, mem );
#if defined DEBUG
    SET_VTABLE_ENTRY( vtable, stream_copyBits, mem );
#endif

    SET_VTABLE_ENTRY( vtable, stream_putU8, mem );
    SET_VTABLE_ENTRY( vtable, stream_putBytes, mem );
    SET_VTABLE_ENTRY( vtable, stream_catString, mem );
    SET_VTABLE_ENTRY( vtable, stream_putU16, mem );
    SET_VTABLE_ENTRY( vtable, stream_putU32, mem );
    SET_VTABLE_ENTRY( vtable, stream_putU32VL, mem );
    SET_VTABLE_ENTRY( vtable, stream_putBits, mem );

    SET_VTABLE_ENTRY( vtable, stream_getFromStream, mem );

    SET_VTABLE_ENTRY( vtable, stream_setPos, mem );
    SET_VTABLE_ENTRY( vtable, stream_getPos, mem );
#ifdef XWFEATURE_STREAMREF
    SET_VTABLE_ENTRY( vtable, stream_ref, mem );
#endif
    SET_VTABLE_ENTRY( vtable, stream_destroy, mem );
    SET_VTABLE_ENTRY( vtable, stream_close, mem );

    SET_VTABLE_ENTRY( vtable, stream_getSize, mem );
    SET_VTABLE_ENTRY( vtable, stream_getHash, mem );
    SET_VTABLE_ENTRY( vtable, stream_getPtr, mem );
    SET_VTABLE_ENTRY( vtable, stream_getAddress, mem );
    SET_VTABLE_ENTRY( vtable, stream_setAddress, mem );

    SET_VTABLE_ENTRY( vtable, stream_setVersion, mem );
    SET_VTABLE_ENTRY( vtable, stream_getVersion, mem );

    return vtable;
} /* make_vtable */

#ifdef CPLUS
}
#endif
