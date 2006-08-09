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

#include "strutils.h"
#include "xwstream.h"

#ifdef CPLUS
extern "C" {
#endif

XP_U16
bitsForMax( XP_U32 n )
{
    XP_U16 result = 0;
    XP_ASSERT( n > 0 );

    while ( n != 0 ) {
        n >>= 1;
        ++result;
    }

    return result;
} /* bitsForMax */

static void
tilesToStream( XWStreamCtxt* stream, const Tile* tiles, XP_U16 nTiles )
{
    while ( nTiles-- ) {
        stream_putBits( stream, TILE_NBITS, *tiles++ );
    }
} /* tilesToStream */

void
traySetToStream( XWStreamCtxt* stream, const TrayTileSet* ts )
{
    XP_U16 nTiles = ts->nTiles;
    stream_putBits( stream, NTILES_NBITS, nTiles );
    tilesToStream( stream, ts->tiles, nTiles );
} /* traySetFromStream */

static void
tilesFromStream( XWStreamCtxt* stream, Tile* tiles, XP_U16 nTiles )
{
    while ( nTiles-- ) {
        *tiles++ = (Tile)stream_getBits( stream, TILE_NBITS );
    }
} /* tilesFromStream */

void
traySetFromStream( XWStreamCtxt* stream, TrayTileSet* ts )
{
    XP_U16 nTiles = (XP_U16)stream_getBits( stream, NTILES_NBITS );
    tilesFromStream( stream, ts->tiles, nTiles );
    ts->nTiles = (XP_U8)nTiles;
} /* traySetFromStream */

#if 0
static void
signedToStream( XWStreamCtxt* stream, XP_U16 nBits, XP_S32 num )
{
    XP_Bool negative = num < 0;
    stream_putBits( stream, 1, negative );
    if ( negative ) {
        num *= -1;
    }
    stream_putBits( stream, nBits, num );
} /* signedToStream */

XP_S32
signedFromStream( XWStreamCtxt* stream, XP_U16 nBits )
{
    XP_S32 result;
    XP_Bool negative = stream_getBits( stream, 1 );
    result = stream_getBits( stream, nBits );
    if ( negative ) {
        result *= -1;
    }
    return result;
} /* signedFromStream */
#endif

XP_UCHAR*
stringFromStream( MPFORMAL XWStreamCtxt* stream )
{
    XP_UCHAR buf[0xFF];
    XP_UCHAR* str = (XP_UCHAR*)NULL;
    XP_U16 len = stringFromStreamHere( stream, buf, sizeof(buf) );

    if ( len > 0 ) {
        str = (XP_UCHAR*)XP_MALLOC( mpool, len + 1 );
        XP_MEMCPY( str, buf, len + 1 );
    }
    return str;
} /* makeStringFromStream */

XP_U16
stringFromStreamHere( XWStreamCtxt* stream, XP_UCHAR* buf, XP_U16 buflen )
{
    XP_U16 len = stream_getU8( stream );
    if ( len > 0 ) {
        XP_ASSERT( len < buflen );
        if ( len >= buflen ) {
            /* better to leave stream in bad state than overwrite stack */
            len = buflen - 1;
        }
        stream_getBytes( stream, buf, len );
        buf[len] = '\0';
    }
    return len;
}

void
stringToStream( XWStreamCtxt* stream, XP_UCHAR* str )
{
    XP_U16 len = str==NULL? 0: XP_STRLEN( (const char*)str );
    XP_ASSERT( len < 0xFF );
    stream_putU8( stream, (XP_U8)len );
    stream_putBytes( stream, str, len );
} /* putStringToStream */

/*****************************************************************************
 *
 ****************************************************************************/
XP_UCHAR* 
copyString( MPFORMAL const XP_UCHAR* instr )
{
    XP_UCHAR* result = (XP_UCHAR*)NULL;
    if ( !!instr ) {
        XP_U16 len = 1 + XP_STRLEN( (const char*)instr );
        result = (XP_UCHAR*)XP_MALLOC( (MemPoolCtx*)mpool, len );
        XP_ASSERT( !!result );
        XP_MEMCPY( result, instr, len );
    }
    return result;
} /* copyString */

void
replaceStringIfDifferent( MPFORMAL XP_UCHAR** curLoc, const XP_UCHAR* newStr )
{
    XP_UCHAR* curStr = *curLoc;

    if ( !!newStr && !!curStr && 
         (0 == XP_STRCMP( (const char*)curStr, (const char*)newStr ) ) ) {
        /* do nothing; we're golden */
    } else {
        if ( !!curStr ) {
            XP_FREE( mpool, curStr );
        }
        curStr = copyString( MPPARM(mpool) newStr );
    }

    *curLoc = curStr;
} /* replaceStringIfDifferent */

/* 
 * A wrapper for printing etc. potentially null strings.
 */
XP_UCHAR* 
emptyStringIfNull( XP_UCHAR* str )
{
    return !!str? str : (XP_UCHAR*)"";
} /* emptyStringIfNull */

XP_Bool
randIntArray( XP_U16* rnums, XP_U16 count )
{
    XP_Bool changed = XP_FALSE;
    XP_U16 i;

    for ( i = 0; i < count; ++i ) {
        rnums[i] = i;
    }

    for ( i = count; i > 0 ; ) {
        XP_U16 rIndex = ((XP_U16)XP_RANDOM()) % i;
        if ( --i != rIndex ) {
            XP_U16 tmp = rnums[rIndex];
            rnums[rIndex] = rnums[i];
            rnums[i] = tmp;
            if ( !changed ) {
                changed = XP_TRUE;
            }
        }
    }

    return changed;
} /* randIntArray */

#ifdef CPLUS
}
#endif
