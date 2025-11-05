/* 
 * Copyright 2001 - 2025 by Eric House (xwords@eehouse.org).  All rights
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

#include "xwstream.h"
#include "comtypes.h"
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

struct XWStreamCtxt {
    XP_U32 curReadPos;
    XP_U32 curWritePos;
    XP_PlayerAddr channelNo;
    XP_U8* buf;
    XP_U16 nBytesWritten;
    XP_U16 nBytesAllocated;
    XP_U16 version;
    XP_U8 nReadBits;
    XP_U8 nWriteBits;
    XP_U8 REFCOUNT;
    MPSLOT
};

XWStreamCtxt* 
strm_make_raw( MPFORMAL_NOCOMMA )
{
    return strm_make( MPPARM(mpool) 0 );
}

XWStreamCtxt*
strm_make( MPFORMAL XP_PlayerAddr channelNo )
{
    XWStreamCtxt* result = (XWStreamCtxt*)XP_CALLOC( mpool, sizeof(*result) );
    MPASSIGN(result->mpool, mpool);

    result->channelNo = channelNo;

#ifdef XWFEATURE_STREAMREF
    result->refCount = 1;
#endif
    return (XWStreamCtxt*)result;
}

XWStreamCtxt* 
strm_make_sized( MPFORMAL XP_U16 startSize, XP_PlayerAddr channelNo )
{
    XWStreamCtxt* result = strm_make( MPPARM(mpool) channelNo );
    if ( 0 < startSize ) {
        result->buf = (XP_U8*)XP_CALLOC( mpool, startSize );
        result->nBytesAllocated = startSize;
    }
    return result;
}

void
strm_getBytes( XWStreamCtxt* stream, void* where, XP_U16 count )
{
    if ( stream->nReadBits != 0 ) {
        stream->nReadBits = 0;
    }

#ifdef MEM_DEBUG
    if( stream->curReadPos + count > stream->nBytesAllocated ) {
        XP_LOGFF( "count %d exceeds buffer", count );
        XP_ASSERT(0);           /* fired */
    }
    if ( stream->curReadPos + count > stream->nBytesWritten ) {
        XP_LOGFF( "count %d exceeds data", count );
        XP_ASSERT(0);
    }
#else
    XP_ASSERT( stream->curReadPos + count <= stream->nBytesAllocated );
    XP_ASSERT( stream->curReadPos + count <= stream->nBytesWritten );
#endif


    XP_MEMCPY( where, stream->buf + stream->curReadPos, count );
    stream->curReadPos += count;
    XP_ASSERT( stream->curReadPos <= stream->nBytesWritten );
} /* strm_getBytes */

XP_U8 
strm_getU8( XWStreamCtxt* stream )
{
    XP_U8 result;
    strm_getBytes( stream, &result, sizeof(result) );
    return result;
} /* strm_getU8 */

XP_U16
strm_getU16( XWStreamCtxt* stream )
{
    XP_U16 result;
    strm_getBytes( stream, &result, sizeof(result) );

    return XP_NTOHS(result);
} /* strm_getU16 */

XP_U32
strm_getU32( XWStreamCtxt* stream )
{
    XP_U32 result;
    strm_getBytes( stream, &result, sizeof(result) );
    return XP_NTOHL( result );
} /* strm_getU32 */

static XP_Bool
getOneBit( XWStreamCtxt* stream )
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

XP_U32
strm_getBits( XWStreamCtxt* stream, XP_U16 nBits )
{
    XP_U32 mask;
    XP_U32 result = 0;

    for ( mask = 1L; nBits--; mask <<= 1 ) {
        if ( getOneBit( stream ) ) {
            result |= mask;
        }
    }

    return result;
} /* strm_getBits */

XP_U32
strm_getU32VL( XWStreamCtxt* stream )
{
    XP_U32 result = 0;
    for ( int ii = 0; ; ++ii ) {
        XP_U8 byt = strm_getBits( stream, 8 * sizeof(byt) );
        result |= (byt & 0x7F) << (7 * ii);
        if ( 0 == (byt & 0x80) ) {
            break;
        }
    }
    return result;
} /* strm_getU32VL */

#if defined DEBUG
void
strm_copyBits( const XWStreamCtxt* stream, XWStreamPos endPos,
               XP_U8* buf, XP_U16* lenp )
{
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

void
strm_putBytes( XWStreamCtxt* stream, const void* whence, 
                     XP_U16 count )
{
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
} /* strm_putBytes */

void
strm_catf( XWStreamCtxt* stream, const char* format, ... )
{
    va_list args;
    va_start( args, format );

    /* First, determine the required buffer size */
    va_list args_copy;
    va_copy( args_copy, args );
    int len = vsnprintf( NULL, 0, format, args_copy );
    va_end( args_copy );

    if ( len > 0 ) {
        char buffer[len+1];
        vsnprintf( buffer, len + 1, format, args );
        strm_putBytes( stream, buffer, len );
    }

    va_end( args );
}

void
strm_putU8( XWStreamCtxt* stream, XP_U8 data )
{
    strm_putBytes( stream, &data, sizeof(data) );
} /* strm_putU8 */

void
strm_putU16( XWStreamCtxt* stream, XP_U16 data )
{
    data = XP_HTONS( data );
    strm_putBytes( stream, &data, sizeof(data) );
} /* linux_common_stream_putU16 */

void
strm_putU32( XWStreamCtxt* stream, XP_U32 data )
{
    data = XP_HTONL( data );
    strm_putBytes( stream, &data, sizeof(data) );
} /* strm_putU32 */

static void
putOneBit( XWStreamCtxt* stream, XP_U16 bit )
{
    XP_U8 mask, rack;

    if ( stream->nWriteBits == 0 ) {
        if ( stream->curWritePos == stream->nBytesWritten ) {
            strm_putU8( (XWStreamCtxt*)stream, 0 ); /* increments curPos */
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

void
strm_putBits( XWStreamCtxt* stream, XP_U16 nBits, XP_U32 data )
{
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
        XP_LOGF( "%s: nBits was %d ", //from line %d, %s",
                 __func__, origBits/*, lin, fil*/ );
    }
#endif
    XP_ASSERT( data == 0 );     /* otherwise nBits was too small */
} /* strm_putBits */

/* Variable-length format: each 7 bits goes in a byte, with the 8th bit
 * reserved for signaling whether there's another byte to come. */
void
strm_putU32VL( XWStreamCtxt* stream, XP_U32 data )
{
    for ( ; ; ) {
        XP_U8 byt = data & 0x7F;
        data >>= 7;
        XP_Bool haveMore = 0 != data;
        if ( haveMore ) {
            byt |= 0x80;
        }
        strm_putBits( stream, 8 * sizeof(byt), byt );
        if ( !haveMore ) {
            break;
        }
    }
} /* strm_putU32VL */

void
strm_getFromStream( XWStreamCtxt* stream, XWStreamCtxt* src,
                        XP_U16 nBytes )
{
    while ( nBytes > 0 ) {
        XP_U8 buf[256];
        XP_U16 len = sizeof(buf);
        if ( nBytes < len ) {
            len = nBytes;
        }
        strm_getBytes( src, buf, len );// fix to use strm_getPtr()?
        strm_putBytes( stream, buf, len );
        nBytes -= len;
    }
} /* strm_getFromStream */

XP_U16
strm_getSize( const XWStreamCtxt* stream )
{
    XP_U16 size = stream->nBytesWritten - stream->curReadPos;
    return size;
} /* strm_getSize */

XP_U32
strm_getHash( const XWStreamCtxt* stream, XWStreamPos pos )
{
    XP_U32 hash = 0;
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
} /* strm_getHash */

const XP_U8*
strm_getPtr( const XWStreamCtxt* stream )
{
    return stream->buf;
} /* strm_getPtr */

XP_PlayerAddr
strm_getAddress( const XWStreamCtxt* stream )
{
    return stream->channelNo;
} /* strm_getAddress */

void
strm_setAddress( XWStreamCtxt* stream, XP_PlayerAddr channelNo )
{
    stream->channelNo = channelNo;
} /* strm_setAddress */

void
strm_setVersion( XWStreamCtxt* stream, XP_U16 vers )
{
    /* Something's wrong if we're changing it */
    XP_ASSERT( 0 == stream->version || vers == stream->version );
    stream->version = vers;
} /* strm_setVersion */

XP_U16
strm_getVersion( const XWStreamCtxt* stream )
{
    return stream->version;
} /* strm_getVersion */

XWStreamPos
strm_getPos( const XWStreamCtxt* stream, PosWhich which )
{
    XWStreamPos result;
    
    if ( which == POS_WRITE ) {
        result = (stream->curWritePos << 3) | stream->nWriteBits;
    } else {
        result = (stream->curReadPos << 3) | stream->nReadBits;
    }

    return result;
} /* strm_getPos */

XWStreamPos
strm_setPos( XWStreamCtxt* stream, PosWhich which, XWStreamPos newpos )
{
    XP_ASSERT( END_OF_STREAM != newpos ); /* not handling this yet */
    XWStreamPos oldPos = strm_getPos( stream, which );

    if ( which == POS_WRITE ) {
        stream->nWriteBits = (XP_U8)BIT_PART(newpos);
        stream->curWritePos = (XP_U32)BYTE_PART(newpos);
    } else {
        stream->nReadBits = (XP_U8)BIT_PART(newpos);
        stream->curReadPos = (XP_U32)BYTE_PART(newpos);
    }

    return oldPos;
} /* strm_setPos */

#ifdef XWFEATURE_STREAMREF
XWStreamCtxt*
strm_ref( XWStreamCtxt* stream )
{
    ++stream->refCount;
    return stream;
}
#endif

void
strm_destroy( XWStreamCtxt* stream )
{
    if ( 0 ) {
#ifdef XWFEATURE_STREAMREF
    } else if ( 0 == --stream->refCount ) {
#else
    } else {
#endif
        XP_FREEP( stream->mpool, &stream->buf );
    
        XP_FREE( stream->mpool, stream );
    }
} /* strm_destroy */

XP_Bool
strm_gotU8( XWStreamCtxt* stream, XP_U8* ptr )
{
    XP_Bool success = sizeof(*ptr) <= strm_getSize( stream );
    if ( success ) {
        *ptr = strm_getU8( stream );
    }
    return success;
}

XP_Bool
strm_gotU16( XWStreamCtxt* stream, XP_U16* ptr )
{
    XP_Bool success = sizeof(*ptr) <= strm_getSize( stream );
    if ( success ) {
        *ptr = strm_getU16( stream );
    }
    return success;
}

XP_Bool
strm_gotU32( XWStreamCtxt* stream, XP_U32* ptr )
{
    XP_Bool success = sizeof(*ptr) <= strm_getSize( stream );
    if ( success ) {
        *ptr = strm_getU32( stream );
    }
    return success;
}

XP_Bool
strm_gotBytes( XWStreamCtxt* stream, void* ptr, XP_U16 len )
{
    XP_Bool success = len <= strm_getSize( stream );
    if ( success ) {
        strm_getBytes( stream, ptr, len );
    }
    return success;
}

#ifdef CPLUS
}
#endif
