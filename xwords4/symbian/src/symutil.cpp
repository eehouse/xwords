/* -*-mode: C; fill-column: 78; c-basic-offset: 4;-*- */ 
/* 
 * Copyright 2005 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#include <flogger.h>
#include <stdarg_e.h>
#include <e32def.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "comtypes.h"
#include "mempool.h"


void
symReplaceStrIfDiff( MPFORMAL XP_UCHAR** loc, const TDesC16& desc )
{
    TBuf8<256> tmp;
    tmp.Copy( desc );

    if ( *loc ) {
        TPtrC8 forCmp;
        forCmp.Set( *loc, XP_STRLEN( *loc ) );

        if ( tmp == forCmp ) {
            return;
        }

        XP_FREE( mpool, *loc );
    }

    TInt len = desc.Length();
    XP_UCHAR* newStr = (XP_UCHAR*)XP_MALLOC( mpool, len + 1 );
    XP_MEMCPY( newStr, (void*)tmp.Ptr(), len );
    newStr[len] = '\0';
    *loc = newStr;
} /* symReplaceStr */

void
symReplaceStrIfDiff( MPFORMAL XP_UCHAR** loc, const XP_UCHAR* str )
{
    if ( *loc != NULL ) {
        if ( 0 == XP_STRCMP( *loc, str ) ) {
            return;             /* nothing to do */
        }
        /* need to free */
        XP_FREE( mpool, *loc );
    }

    TInt len = XP_STRLEN( str ) + 1;
    *loc = (XP_UCHAR*)XP_MALLOC( mpool, len );
    XP_MEMCPY( (void*)*loc, (void*)str, len );
} /* symReplaceStrIfDiff */

#ifdef DEBUG
_LIT( KXWLogdir, "xwords" );
_LIT( KXWLogfile, "xwdebug.txt" );
/* The emulator, anyway, doesn't do descs well even with the %S directive.
   So convert 'em to desc8s... */
void
XP_LOGDESC16( const TDesC16* desc )
{
    TBuf8<256> buf8;
    buf8.Copy( *desc );
    buf8.Append( (const unsigned char*)"\0", 1 );
    TBuf8<256> fmtDesc((unsigned char*)"des16: %s");

    RFileLogger::WriteFormat( KXWLogdir, KXWLogfile, 
                              EFileLoggingModeAppend, 
                              fmtDesc, buf8.Ptr() );
}
#endif

extern "C" {

int
sym_snprintf( XP_UCHAR* aBuf, XP_U16 aLen, const XP_UCHAR* aFmt, ... )
{
    __e32_va_list ap;
    va_start( ap, aFmt );

    int result = vsprintf( (char*)aBuf, (const char*)aFmt, ap );
    XP_ASSERT( XP_STRLEN(aBuf) < aLen ); // this may not work....
    va_end(ap);
    return result;
}

#ifdef DEBUG
void sym_debugf( char* aFmt, ... )
{
    VA_LIST ap;
    VA_START( ap, aFmt );

    TBuf8<256> fmtDesc((unsigned char*)aFmt);

    RFileLogger::WriteFormat( KXWLogdir, KXWLogfile, 
                              EFileLoggingModeAppend, 
                              fmtDesc, ap );
    VA_END(ap);
}

#else 

void p_ignore( char* , ...) {}

#endif


void*
sym_malloc(XP_U32 nbytes )
{
    return malloc( nbytes );
}

void* sym_realloc(void* p, XP_U32 nbytes) 
{
    return realloc( p, nbytes );
}

void
sym_free( void* p )
{
    free( p );
}

void
sym_assert( XP_Bool b, XP_U32 line, const char* file )
{
    if ( !b ) {
        XP_LOGF( "ASSERTION FAILED: line %d, file %s",
                 line, file );
    }
}

void
sym_memcpy( void* dest, const void* src, XP_U32 nbytes )
{
    memcpy( dest, src, nbytes );
}

XP_U32
sym_strlen( XP_UCHAR* str )
{
    return strlen( (const char*)str );
}

XP_S16
sym_strncmp( XP_UCHAR* str1, XP_UCHAR* str2, XP_U32 len )
{
    return (XP_S16)strncmp( (const char*)str1, (const char*)str2, len );
}

void
sym_memset( void* dest, XP_UCHAR val, XP_U32 nBytes )
{
    memset( dest, val, nBytes );
}

XP_S16
sym_strcmp( XP_UCHAR* str1, XP_UCHAR* str2 )
{
    return (XP_S16)strcmp( (const char*)str1, (const char*)str2 );
}

char*
sym_strcat( XP_UCHAR* dest, const XP_UCHAR* src )
{
    return strcat( (char*)dest, (const char*) src );
}

XP_S16
sym_memcmp( void* m1, void* m2, XP_U32 nbytes )
{
    return (XP_S16)memcmp( m1, m2, nbytes );
}

XP_U32
sym_flip_long( XP_U32 l )
{
    XP_U32 result =
        ((l & 0x000000FF) << 24) | 
        ((l & 0x0000FF00) << 8) | 
        ((l & 0x00FF0000) >> 8) | 
        ((l & 0xFF000000) >> 24);
    return result;
}

XP_U16
sym_flip_short(XP_U16 s)
{
    XP_U16 result = 
        ((s & 0x00FF) << 8) | 
        ((s & 0xFF00) >> 8);
    
    return result;
}

} // extern "C"
