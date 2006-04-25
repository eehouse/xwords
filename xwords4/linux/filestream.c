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
#if 0
#include <stdio.h>

#include "xwstream.h"

typedef struct LinuxFileStreamCtxt {
    StreamCtxVTable* vtable;

    FILE* file;
} LinuxFileStreamCtxt;

static void make_vtable( LinuxFileStreamCtxt* stream );


XWStreamCtxt* 
linux_make_fileStream( char* fileName, XP_Bool forWriting )
{
    LinuxFileStreamCtxt* result = malloc( sizeof(*result) );
    XP_MEMSET( result, 0, sizeof(*result) );
 
    make_vtable( result );

    result->file = fopen( fileName, forWriting? "w":"r" );
    XP_ASSERT( result->file );

    return (XWStreamCtxt*)result;
} /* linux_make_fileStream */

static void
linux_file_stream_getBytes( XWStreamCtxt* p_sctx, void* where, 
			    XP_U16 count )
{
    LinuxFileStreamCtxt* stream = (LinuxFileStreamCtxt*)p_sctx;
    XP_ASSERT( !!stream->file );

    fread( where, count, 1, stream->file );
} /* linux_file_stream_getBytes */

static XP_U8 
linux_file_stream_getU8( XWStreamCtxt* p_sctx )
{
    XP_U8 result;
    linux_file_stream_getBytes( p_sctx, &result, sizeof(result) );
    return result;
} /* linux_file_stream_getU8 */

static XP_U16
linux_file_stream_getU16( XWStreamCtxt* p_sctx )
{
    XP_U16 result;
    linux_file_stream_getBytes( p_sctx, &result, sizeof(result) );
    return result;
} /* linux_file_stream_getU16 */

static XP_U32
linux_file_stream_getU32( XWStreamCtxt* p_sctx )
{
    XP_U32 result;
    linux_file_stream_getBytes( p_sctx, &result, sizeof(result) );
    return result;
} /* linux_file_stream_getU32 */

static void
linux_file_stream_putBytes( XWStreamCtxt* p_sctx, void* where, 
			    XP_U16 count )
{
    LinuxFileStreamCtxt* stream = (LinuxFileStreamCtxt*)p_sctx;
    size_t written;
    XP_ASSERT( !!stream->file );

    written = fwrite( where, count, 1, stream->file );
    XP_ASSERT( written != 0 );
} /* linux_file_stream_putBytes */

static void
linux_file_stream_putString( XWStreamCtxt* p_sctx, const char* where )
{
    linux_file_stream_putBytes( p_sctx, (void*)where, XP_STRLEN(where) );
}

static void
linux_file_stream_putU8( XWStreamCtxt* p_sctx, XP_U8 data )
{
    linux_file_stream_putBytes( p_sctx, &data, sizeof(data) );
} /* linux_file_stream_putU8 */

static void
linux_file_stream_putU16( XWStreamCtxt* p_sctx, XP_U16 data )
{
    linux_file_stream_putBytes( p_sctx, &data, sizeof(data) );
} /* linux_common_stream_putUI16 */

static void
linux_file_stream_putU32( XWStreamCtxt* p_sctx, XP_U32 data )
{
    linux_file_stream_putBytes( p_sctx, &data, sizeof(data) );
} /* linux_file_stream_putUI32 */

static void
linux_file_stream_open( XWStreamCtxt* p_sctx )
{
    LinuxFileStreamCtxt* stream = (LinuxFileStreamCtxt*)p_sctx;
    XP_ASSERT( !!stream->file );
    rewind( stream->file );
} /* linux_file_stream_open */

static void
linux_file_stream_close( XWStreamCtxt* p_sctx )
{
    LinuxFileStreamCtxt* stream = (LinuxFileStreamCtxt*)p_sctx;    
    XP_ASSERT( !!stream->file );
    fclose( stream->file );
    stream->file = NULL;
} /* linux_file_stream_close */

static void
linux_file_stream_destroy( XWStreamCtxt* p_sctx )
{
    LinuxFileStreamCtxt* stream;

    stream = (LinuxFileStreamCtxt*)p_sctx;
    if ( !!stream->file ) {
	stream_close( stream );
    }

    free( p_sctx->vtable );
    free( stream );
} /* linux_file_stream_destroy */

static void
make_vtable( LinuxFileStreamCtxt* stream )
{
    XP_ASSERT( !stream->vtable );
    stream->vtable = malloc( sizeof(*stream->vtable) );

    SET_VTABLE_ENTRY( stream, stream_getU8, linux_file );
    SET_VTABLE_ENTRY( stream, stream_getBytes, linux_file );
    SET_VTABLE_ENTRY( stream, stream_getU16, linux_file );
    SET_VTABLE_ENTRY( stream, stream_getU32, linux_file );

    SET_VTABLE_ENTRY( stream, stream_putU8, linux_file );
    SET_VTABLE_ENTRY( stream, stream_putBytes, linux_file );
    SET_VTABLE_ENTRY( stream, stream_putString, linux_file );
    SET_VTABLE_ENTRY( stream, stream_putU16, linux_file );
    SET_VTABLE_ENTRY( stream, stream_putU32, linux_file );

    SET_VTABLE_ENTRY( stream, stream_destroy, linux_file );
    SET_VTABLE_ENTRY( stream, stream_open, linux_file );
    SET_VTABLE_ENTRY( stream, stream_close, linux_file );

    /* PENDING(ehouse) These are part of some subclass, not of stream
       overall */
/*     SET_VTABLE_ENTRY( stream, stream_getSize, linux_file ); */
/*     SET_VTABLE_ENTRY( stream, stream_getAddress, linux_file ); */
/*     SET_VTABLE_ENTRY( stream, stream_setAddress, linux_file ); */
} /* make_vtable */
#endif

