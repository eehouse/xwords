/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 1997 - 2000 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#ifndef _XWSTREAM_H_
#define _XWSTREAM_H_

#include "comtypes.h"
#include "memstream.h"
/* #include "xptypes.h" */

#define START_OF_STREAM 0
#define END_OF_STREAM -1

typedef XP_U32 XWStreamPos;     /* low 3 bits are bit offset; rest byte offset */
enum { POS_READ, POS_WRITE };
typedef XP_U8 PosWhich;

#ifdef DEBUG
# define DBG_LINE_FILE_FORMAL  , XP_U16 lin, const char* fil
# define DBG_LINE_FILE_PARM  , __LINE__, __FILE__
#else
# define DBG_LINE_FILE_FORMAL
# define DBG_LINE_FILE_PARM
#endif

typedef struct StreamCtxVTable {
    void (*m_stream_destroy)( XWStreamCtxt* dctx );

    XP_U8 (*m_stream_getU8)( XWStreamCtxt* dctx );
    void (*m_stream_getBytes)( XWStreamCtxt* dctx, void* where, 
                               XP_U16 count );
    XP_U16 (*m_stream_getU16)( XWStreamCtxt* dctx );
    XP_U32 (*m_stream_getU32)( XWStreamCtxt* dctx );
    XP_U32 (*m_stream_getBits)( XWStreamCtxt* dctx, XP_U16 nBits );

    void (*m_stream_putU8)( XWStreamCtxt* dctx, XP_U8 byt );
    void (*m_stream_putBytes)( XWStreamCtxt* dctx, const void* whence, 
                               XP_U16 count );
    void (*m_stream_catString)( XWStreamCtxt* dctx, const char* whence );
    void (*m_stream_putU16)( XWStreamCtxt* dctx, XP_U16 data );
    void (*m_stream_putU32)( XWStreamCtxt* dctx, XP_U32 data );
    void (*m_stream_putBits)( XWStreamCtxt* dctx, XP_U16 nBits, XP_U32 bits
                              DBG_LINE_FILE_FORMAL );

    void (*m_stream_copyFromStream)( XWStreamCtxt* dctx, XWStreamCtxt* src,
                                     XP_U16 nBytes );

    XWStreamPos (*m_stream_getPos)( XWStreamCtxt* dctx, PosWhich which );
    XWStreamPos (*m_stream_setPos)( XWStreamCtxt* dctx, XWStreamPos newpos,
                                    PosWhich which );

    void (*m_stream_open)( XWStreamCtxt* dctx );
    void (*m_stream_close)( XWStreamCtxt* dctx );

    XP_U16 (*m_stream_getSize)( const XWStreamCtxt* dctx );

/*     void (*m_stream_makeReturnAddr)( XWStreamCtxt* dctx, XP_PlayerAddr* addr, */
/*                                      XP_U16* addrLen ); */

    XP_PlayerAddr (*m_stream_getAddress)( XWStreamCtxt* dctx );
    void (*m_stream_setAddress)( XWStreamCtxt* dctx, XP_PlayerAddr channelNo );

    void (*m_stream_setVersion)( XWStreamCtxt* dctx, XP_U16 vers );
    XP_U16  (*m_stream_getVersion)( XWStreamCtxt* dctx );

    void  (*m_stream_setOnCloseProc)( XWStreamCtxt* dctx, 
                                      MemStreamCloseCallback proc );
} StreamCtxVTable;


struct XWStreamCtxt {
    StreamCtxVTable* vtable;
};


#define stream_destroy(sc) \
         (sc)->vtable->m_stream_destroy(sc)

#define stream_getU8(sc) \
         (sc)->vtable->m_stream_getU8(sc)

#define stream_getBytes(sc, wh, c ) \
         (sc)->vtable->m_stream_getBytes((sc), (wh), (c))

#define stream_getU16(sc) \
         (sc)->vtable->m_stream_getU16(sc)

#define stream_getU32(sc) \
         (sc)->vtable->m_stream_getU32(sc)

#define stream_getBits(sc, n) \
         (sc)->vtable->m_stream_getBits((sc), (n))

#define stream_putU8(sc, b) \
         (sc)->vtable->m_stream_putU8((sc), (b))

#define stream_putBytes( sc, w, c ) \
         (sc)->vtable->m_stream_putBytes((sc), (w), (c))

#define stream_catString( sc, w ) \
         (sc)->vtable->m_stream_catString((sc), (w))

#define stream_putU16(sc, d) \
         (sc)->vtable->m_stream_putU16((sc), (d))

#define stream_putU32(sc, d) \
         (sc)->vtable->m_stream_putU32((sc), (d))

#define stream_putBits(sc, n, b) \
         (sc)->vtable->m_stream_putBits((sc), (n), (b) DBG_LINE_FILE_PARM )

#define stream_copyFromStream( sc, src, nb ) \
         (sc)->vtable->m_stream_copyFromStream((sc), (src), (nb))

#define stream_getPos(sc, w) \
         (sc)->vtable->m_stream_getPos((sc), (w))

#define stream_setPos(sc, p, w) \
         (sc)->vtable->m_stream_setPos((sc), (p), (w))

#define stream_open(sc) \
         (sc)->vtable->m_stream_open((sc))

#define stream_close(sc) \
         (sc)->vtable->m_stream_close((sc))

#define stream_getSize(sc) \
         (sc)->vtable->m_stream_getSize((sc))

#define stream_makeReturnAddr(sc,addr,len) \
         (sc)->vtable->m_stream_makeReturnAddr((sc),(addr),(len))

#define stream_getAddress(sc) \
         (sc)->vtable->m_stream_getAddress((sc))

#define stream_setAddress(sc,ch) \
         (sc)->vtable->m_stream_setAddress((sc),(ch))

#define stream_setVersion(sc,ch) \
         (sc)->vtable->m_stream_setVersion((sc),(ch))

#define stream_getVersion(sc) \
         (sc)->vtable->m_stream_getVersion((sc))

#define stream_setOnCloseProc(sc, p) \
         (sc)->vtable->m_stream_setOnCloseProc((sc), (p))

#endif /* _XWSTREAM_H_ */
