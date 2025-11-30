/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 1997 - 2015 by Eric House (xwords@eehouse.org).  All rights
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

#ifndef _XWSTREAM_H_
#define _XWSTREAM_H_

#include "comtypes.h"

#define START_OF_STREAM 0
#define END_OF_STREAM -1
typedef XP_U32 XWStreamPos;     /* low 3 bits are bit offset; rest byte offset */

enum { POS_READ, POS_WRITE };
typedef XP_U8 PosWhich;

#ifdef XWFEATURE_STREAMREF
XWStreamCtxt* strm_ref( XWStreamCtxt* dctx );
#endif
void strm_destroy( XWStreamCtxt* dctx );
void strm_destroyp( XWStreamCtxt** dctxp );

XP_U8 strm_getU8( XWStreamCtxt* dctx );
void strm_getBytes( XWStreamCtxt* dctx, void* where, XP_U16 count );
XP_U16 strm_getU16( XWStreamCtxt* dctx );
XP_U32 strm_getU32( XWStreamCtxt* dctx );
XP_U32 strm_getU32VL( XWStreamCtxt* dctx );
XP_Bool strm_gotU32VL( XWStreamCtxt* dctx, XP_U32* val );
XP_U32 strm_getBits( XWStreamCtxt* dctx, XP_U16 nBits );
XP_Bool strm_gotBits( XWStreamCtxt* dctx, XP_U16 nBits, XP_U32* bits );
#if defined DEBUG
void strm_copyBits( const XWStreamCtxt* dctx, XWStreamPos endPos,
                    XP_U8* buf, XP_U16* len );
#endif

void strm_putU8( XWStreamCtxt* dctx, XP_U8 byt );
void strm_putBytes( XWStreamCtxt* dctx, const void* whence, 
                    XP_U16 count );
void strm_catf( XWStreamCtxt* dctx, const char* whence, ... );
void strm_putU16( XWStreamCtxt* dctx, XP_U16 data );
void strm_putU32( XWStreamCtxt* dctx, XP_U32 data );
void strm_putU32VL( XWStreamCtxt* dctx, XP_U32 data );
void strm_putBits( XWStreamCtxt* dctx, XP_U16 nBits, XP_U32 bits );

void strm_getFromStream( XWStreamCtxt* dctx, XWStreamCtxt* src,
                         XP_U16 nBytes );
XP_Bool strm_gotFromStream( XWStreamCtxt* dctx, XWStreamCtxt* src,
                            XP_U16 nBytes );

XWStreamPos strm_getPos( const XWStreamCtxt* dctx, PosWhich which );
XWStreamPos strm_setPos( XWStreamCtxt* dctx, PosWhich which, 
                         XWStreamPos newpos );

XP_U16 strm_getSize( const XWStreamCtxt* dctx );
XP_U32 strm_getHash( const XWStreamCtxt* dctx, XWStreamPos pos );
    
const XP_U8* strm_getPtr( const XWStreamCtxt* dctx );

XP_PlayerAddr strm_getAddress( const XWStreamCtxt* dctx );
void strm_setAddress( XWStreamCtxt* dctx, XP_PlayerAddr channelNo );

void strm_setVersion( XWStreamCtxt* dctx, XP_U16 vers );
XP_U16 strm_getVersion( const XWStreamCtxt* dctx );

#define strm_catString( sc, w ) strm_catf((sc), "%s", (w))

XWStreamCtxt* strm_make_raw( MPFORMAL_NOCOMMA);
XWStreamCtxt* strm_make( MPFORMAL XP_PlayerAddr addr );
XWStreamCtxt* strm_make_sized( MPFORMAL XP_U16 initialSize,
                               XP_PlayerAddr addr );
XP_Bool strm_gotU8( XWStreamCtxt* stream, XP_U8* ptr );
XP_Bool strm_gotU32( XWStreamCtxt* stream, XP_U32* ptr );
XP_Bool strm_gotU16( XWStreamCtxt* stream, XP_U16* ptr );
XP_Bool strm_gotBytes( XWStreamCtxt* stream, void* ptr, XP_U16 len );

#endif /* _XWSTREAM_H_ */
