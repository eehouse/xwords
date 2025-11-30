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

#include "strutils.h"
#include "xwstream.h"
#include "mempool.h"
#include "xptypes.h"
#include "device.h"
#include "dbgutil.h"

#ifdef CPLUS
extern "C" {
#endif

XP_U16
bitsForMax( XP_U32 nn )
{
    XP_U16 result = 0;
    XP_ASSERT( nn > 0 );

    while ( nn != 0 ) {
        nn >>= 1;
        ++result;
    }

    return result;
} /* bitsForMax */

static void
tilesToStream( XWStreamCtxt* stream, const Tile* tiles, XP_U16 nTiles )
{
    while ( nTiles-- ) {
        strm_putBits( stream, TILE_NBITS, *tiles++ );
    }
} /* tilesToStream */

void
traySetToStream( XWStreamCtxt* stream, const TrayTileSet* ts )
{
    XP_U16 nTiles = ts->nTiles;
    strm_putBits( stream, tilesNBits(stream), nTiles );
    tilesToStream( stream, ts->tiles, nTiles );
} /* traySetFromStream */

static void
tilesFromStream( XWStreamCtxt* stream, Tile* tiles, XP_U16 nTiles )
{
    while ( nTiles-- ) {
        *tiles++ = (Tile)strm_getBits( stream, TILE_NBITS );
    }
} /* tilesFromStream */

void
scoresToStream( XWStreamCtxt* stream, XP_U16 nScores, const XP_U16* scores )
{
    if ( 0 < nScores ) {
        XP_U16 maxScore = 1;    /* 0 will confuse bitsForMax */
        for ( XP_U16 ii = 0; ii < nScores; ++ii ) {
            XP_U16 score = scores[ii];
            if ( score > maxScore ) {
                maxScore = score;
            }
        }

        XP_U16 bits = bitsForMax( maxScore );
        strm_putBits( stream, 4, bits );
        for ( XP_U16 ii = 0; ii < nScores; ++ii ) {
            strm_putBits( stream, bits, scores[ii] );
        }
    }
}

void
scoresFromStream( XWStreamCtxt* stream, XP_U16 nScores, XP_U16* scores )
{
    if ( 0 < nScores ) {
        XP_U16 bits = (XP_U16)strm_getBits( stream, 4 );
        for ( XP_U16 ii = 0; ii < nScores; ++ii ) {
            scores[ii] = strm_getBits( stream, bits );
        }
    }
}

void
traySetFromStream( XWStreamCtxt* stream, TrayTileSet* ts )
{
    XP_U16 nTiles = (XP_U16)strm_getBits( stream, tilesNBits( stream ) );
    tilesFromStream( stream, ts->tiles, nTiles );
    ts->nTiles = (XP_U8)nTiles;
} /* traySetFromStream */

#ifdef DEBUG
void
assertSorted( const MoveInfo* mi )
{
    for ( XP_U16 ii = 1; ii < mi->nTiles; ++ii ) {
        XP_ASSERT( mi->tiles[ii-1].varCoord < mi->tiles[ii].varCoord );
    }
}
#endif

void
moveInfoToStream( XWStreamCtxt* stream, const MoveInfo* mi, XP_U16 bitsPerTile )
{
#ifdef DEBUG
    /* XP_UCHAR buf[64] = {}; */
    /* XP_U16 offset = 0; */
#endif
    assertSorted( mi );

    strm_putBits( stream, tilesNBits( stream ), mi->nTiles );
    strm_putBits( stream, NUMCOLS_NBITS_5, mi->commonCoord );
    strm_putBits( stream, 1, mi->isHorizontal );

    XP_ASSERT( bitsPerTile == 5 || bitsPerTile == 6 );
    for ( XP_U16 ii = 0; ii < mi->nTiles; ++ii ) {
        strm_putBits( stream, NUMCOLS_NBITS_5, mi->tiles[ii].varCoord );

        Tile tile = mi->tiles[ii].tile;
#ifdef DEBUG
        /* offset += XP_SNPRINTF( &buf[offset], VSIZE(buf)-offset, "%x,", tile ); */
#endif
        strm_putBits( stream, bitsPerTile, tile & TILE_VALUE_MASK );
        strm_putBits( stream, 1, (tile & TILE_BLANK_BIT) != 0 );
    }
    // XP_LOGF( "%s(): tiles: %s", __func__, buf );
}

void
moveInfoFromStream( XWStreamCtxt* stream, MoveInfo* mi, XP_U16 bitsPerTile )
{
#ifdef DEBUG
    /* XP_UCHAR buf[64] = {}; */
    /* XP_U16 offset = 0; */
#endif
    mi->nTiles = strm_getBits( stream, tilesNBits( stream ) );
    XP_ASSERT( mi->nTiles <= MAX_TRAY_TILES );
    mi->commonCoord = strm_getBits( stream, NUMCOLS_NBITS_5 );
    mi->isHorizontal = strm_getBits( stream, 1 );
    for ( XP_U16 ii = 0; ii < mi->nTiles; ++ii ) {
        mi->tiles[ii].varCoord = strm_getBits( stream, NUMCOLS_NBITS_5 );
        Tile tile = strm_getBits( stream, bitsPerTile );
        if ( 0 != strm_getBits( stream, 1 ) ) {
            tile |= TILE_BLANK_BIT;
        }
        mi->tiles[ii].tile = tile;
#ifdef DEBUG
        /* offset += XP_SNPRINTF( &buf[offset], VSIZE(buf)-offset, "%x,", tile ); */
#endif
    }
    assertSorted( mi );
    // XP_LOGF( "%s(): tiles: %s", __func__, buf );
}

/* URL encoding for JNI-safe string parameters.
 * Encodes characters that need to be percent-encoded in URL parameters.
 */
static void
urlEncodeToStream( XWStreamCtxt* stream, UrlParamType typ, va_list* ap )
{
    const char* specials = "!*'();:@&=+$,/?#[]%";

    XP_UCHAR numBuf[16];
    XWStreamCtxt* inStream = NULL;
    const XP_UCHAR* str = NULL;

    switch ( typ ) {
    case UPT_U8:
    case UPT_U32: {
        XP_U32 tmp = va_arg( *ap, XP_U32 );
        XP_ASSERT( typ == UPT_U32 || tmp <= 0xFF );
        XP_SNPRINTF( numBuf, VSIZE(numBuf), "%d", tmp );
        str = numBuf;
    }
        break;
    case UPT_STRING:
        str = va_arg( *ap, const XP_UCHAR* );
        break;
    case UPT_STREAM:
        inStream = va_arg( *ap, XWStreamCtxt* );
        break;
    default:
        XP_ASSERT(0);
    }

    for ( XP_U16 ii = 0; ; ++ii ) {
        XP_U8 ch;
        if ( !!str ) {
            ch = str[ii];
        } else if ( !strm_gotU8( inStream, &ch ) ) {
            break;
        }
        if ( !ch ) {
            break;
        } else if ( ch <= 32 || ch >= 127 || strchr(specials, ch)) {
            XP_UCHAR buf[4];
            XP_U16 len = XP_SNPRINTF( buf, VSIZE(buf), "%%%02X", (unsigned char)ch );
            XP_ASSERT( len == 3 );
            strm_putBytes( stream, buf, len );
        } else {
            strm_putU8( stream, ch );
        }
    }
}

void
urlParamToStream( XWStreamCtxt* stream, UrlParamState* state, const XP_UCHAR* key,
                  UrlParamType typ, ... )
{
    va_list ap;
    va_start( ap, typ );

    XP_Bool haveData;
    switch ( typ ) {
    case UPT_U8:
    case UPT_U32:
        haveData = XP_TRUE; break;
    case UPT_STRING: {
        const XP_UCHAR* param = va_arg( ap, const XP_UCHAR*);
        haveData = '\0' != ((XP_UCHAR*)param)[0];
    }
        break;
    case UPT_STREAM: {
        XWStreamCtxt* param = va_arg( ap, XWStreamCtxt*);
        haveData = 0 != strm_getSize(param);
    }
        break;
    default:
        XP_ASSERT(0);
    }
    va_end( ap );

    if ( haveData ) {
        const char* prefix;
        if ( !state->firstDone ) {
            state->firstDone = XP_TRUE;
            prefix = "?";
        } else {
            prefix = "&";
        }
        strm_catString( stream, prefix );
        strm_catString( stream, key );
        strm_catString( stream, "=" );

        va_list ap;
        va_start( ap, typ );
        urlEncodeToStream( stream, typ, &ap );
        va_end( ap );
    } else {
        XP_LOGFF( "nothing to print for key %s", key );
    }
}

static XP_Bool
getUntil( XWStreamCtxt* stream, XP_UCHAR sought, XP_U8 buf[], XP_U16* bufLen )
{
    XP_Bool success = XP_FALSE;
    XP_U8 got;
    int index = 0;
    while ( strm_gotU8( stream, &got ) ) {
        if ( sought == got ) {
            success = XP_TRUE;
            if ( !!bufLen ) {
                *bufLen = index;
            }
            break;
        } else if ( !!buf ) {
            buf[index++] = got;
            XP_ASSERT( index < *bufLen );
        }
    }
    return success;
}

static XP_Bool
decodeUntil( XW_DUtilCtxt* duc, XWStreamCtxt* stream, XP_U8 sought,
             UrlParamType typ, va_list* app )
{
    XP_U8* outU8 = NULL;
    XP_U32* outU32 = NULL;
    XWStreamCtxt** streamP = NULL;
    XP_UCHAR* outStr = NULL;
    int outLen;
    int outStrIndex = 0;

    switch ( typ ) {
    case UPT_U8:
        outU8 = va_arg( *app, XP_U8* );
        break;
    case UPT_U32:
        outU32 = va_arg( *app, XP_U32* );
        break;
    case UPT_STREAM:
        streamP = va_arg( *app, XWStreamCtxt** );
        break;
    case UPT_STRING:
        outStr = va_arg( *app, XP_UCHAR* );
        outLen = va_arg( *app, int );
        XP_ASSERT( outLen < 64 );
        break;
    default:
        XP_ASSERT(0);           /* firing */
    }

    XP_Bool success = XP_TRUE;
    XP_UCHAR numBuf[16];
    int numIndx = 0;
    XP_U8 got;
    while ( success && strm_gotU8( stream, &got ) ) {
        if ( got == sought ) {
            break;
        } else if ( got == '%' ) {
            /* Get the next: it's an error if they're missing */
            XP_U8 buf[3];
            if ( !strm_gotBytes( stream, buf, 2 ) ) {
                XP_LOGFF( "premature end??" );
                success = XP_FALSE;
                break;
            }
            buf[2] = '\0';
            int tmp;
            int nGot = sscanf( (char*)buf, "%02X", &tmp );
            if ( 1 != nGot ) {
                XP_LOGFF( "bad hex format in '%s'?", buf );
                success = XP_FALSE;
                break;
            }
            XP_ASSERT( tmp <= 0xFF );
            got = (XP_U8)tmp;
        }

        /* Now we have one converted byte. Save appropriately*/
        switch ( typ ) {
        case UPT_U8:
        case UPT_U32:
            numBuf[numIndx++] = got;
            break;
        case UPT_STRING:
            if ( outStrIndex+1 >= outLen ) {
                success = XP_FALSE;
            } else {
                outStr[outStrIndex++] = got;
            }
            break;
        case UPT_STREAM:
            if ( !*streamP ) {
                *streamP = dvc_makeStream( duc );
            }
            strm_putU8( *streamP, got );
            break;
        default:
            XP_ASSERT(0);
        }
    }

    if ( success ) {
        switch ( typ ) {
        case UPT_U8:
        case UPT_U32: {
            XP_U32 num;
            numBuf[numIndx] = '\0';
            sscanf( numBuf, "%d", &num );
            if ( !!outU32 ) {
                *outU32 = num;
            } else if ( !!outU8 ) {
                XP_ASSERT( num <= 0xFF );
                *outU8 = (XP_U8)num;
            } else {
                XP_ASSERT(0);
            }
        }
            break;
        case UPT_STREAM:
            // XP_LOGFF( "nothing to do for string/stream ");
            break;
        case UPT_STRING:
            outStr[outStrIndex] = '\0';
            break;
         default:
            XP_ASSERT(0);
            break;
        }
    }

    return success;
} /* decodeUntil */

static XP_Bool
urlDecodeFromStreamAP( XW_DUtilCtxt* duc, XWStreamCtxt* stream,
                       UrlDecodeIter* iter, UrlParamType typ, va_list* ap )
{
    XP_Bool found = XP_FALSE;
    XWStreamPos startPos = strm_getPos( stream, POS_READ );
    if ( iter->pos ) {
        strm_setPos( stream, POS_READ, iter->pos );
    }

    for ( ; ; ) {
        XP_U8 key[32] = {};
        XP_U16 keyLen = VSIZE(key);
        if ( !getUntil( stream, '=', key, &keyLen ) ) {
            break;
        } else {
            key[keyLen] = '\0';
            if ( 0 == XP_MEMCMP( key, iter->key, keyLen ) ) {
                XP_LOGFF( "matched key %s", key );
                /* va_list ap; */
                /* va_start( ap, typ ); */
                found = decodeUntil( duc, stream, '&', typ, ap );
                /* va_end( ap ); */
                break;
            } else {
                XP_LOGFF( "matched some other key? %s", key );
                getUntil( stream, '&', NULL, 0 );
            }
        }
    }

    if ( found ) {
        ++iter->count;
        iter->pos = strm_getPos( stream, POS_READ );
    } else {
        strm_setPos( stream, POS_READ, startPos );
    }
    return found;
}

XP_Bool
urlParamFromStream( XW_DUtilCtxt* duc, XWStreamCtxt* stream,
                    const XP_UCHAR* key, UrlParamType typ, ... )
{
    XWStreamPos startPos = strm_getPos( stream, POS_READ );

    UrlDecodeIter iter = { .key = key, };
    va_list ap;
    va_start( ap, typ );
    XP_Bool success = urlDecodeFromStreamAP( duc, stream, &iter, typ, &ap );
    va_end( ap );

    strm_setPos( stream, POS_READ, startPos );
    return success;
}

void
removeTile( TrayTileSet* tiles, XP_U16 index )
{
    XP_U16 ii;
    --tiles->nTiles;
    for ( ii = index; ii < tiles->nTiles; ++ii ) {
        tiles->tiles[ii] = tiles->tiles[ii+1];
    }
}

void
sortTiles( TrayTileSet* dest, const TrayTileSet* src, XP_U16 skip )
{
    XP_ASSERT( src->nTiles >= skip );
    TrayTileSet tmp = *src;

    /* Copy in the ones we're not sorting */
    dest->nTiles = skip;
    if ( 0 < skip ) {
        XP_MEMCPY( &dest->tiles, &tmp.tiles, skip * sizeof(tmp.tiles[0]) );
    }

    while ( skip < tmp.nTiles ) {
        XP_U16 ii, smallest;
        for ( smallest = ii = skip; ii < tmp.nTiles; ++ii ) {
            if ( tmp.tiles[ii] < tmp.tiles[smallest] ) {
                smallest = ii;
            }
        }
        dest->tiles[dest->nTiles++] = tmp.tiles[smallest];
        removeTile( &tmp, smallest );
    }
}

#if 0
static void
signedToStream( XWStreamCtxt* stream, XP_U16 nBits, XP_S32 num )
{
    XP_Bool negative = num < 0;
    strm_putBits( stream, 1, negative );
    if ( negative ) {
        num *= -1;
    }
    strm_putBits( stream, nBits, num );
} /* signedToStream */

XP_S32
signedFromStream( XWStreamCtxt* stream, XP_U16 nBits )
{
    XP_S32 result;
    XP_Bool negative = strm_getBits( stream, 1 );
    result = strm_getBits( stream, nBits );
    if ( negative ) {
        result *= -1;
    }
    return result;
} /* signedFromStream */
#endif

XP_UCHAR*
p_stringFromStream( MPFORMAL XWStreamCtxt* stream
#ifdef MEM_DEBUG
                    , const char* file, const char* func, XP_U32 lineNo 
#endif
                    )
{
    XP_U16 version = strm_getVersion( stream );
    XP_U32 len = version < STREAM_VERS_NORELAY ? strm_getU8( stream )
        : strm_getU32VL( stream );

    XP_UCHAR* str = NULL;
    if ( 0 < len ) {
#ifdef MEM_DEBUG
        str = mpool_alloc( mpool, len + 1, file, func, lineNo );
#else
        str = (XP_UCHAR*)XP_MALLOC( mpool, len + 1 );
#endif
        strm_getBytes( stream, str, len );
        str[len] = '\0';
    }
    return str;
} /* makeStringFromStream */

void
stringFromStreamHereImpl( XWStreamCtxt* stream, XP_UCHAR* buf, XP_U16 buflen
#ifdef DEBUG
                          , const char* func, int line
#endif
                          )
{
    XP_USE(func);
    XP_USE(line);
#ifdef DEBUG
    XP_Bool success =
#endif
        gotStringFromStreamHere( stream, buf, buflen );
    XP_ASSERT( success );
}

XP_Bool
gotStringFromStreamHere( XWStreamCtxt* stream, XP_UCHAR* buf, XP_U16 buflen )
{
    XP_Bool success = XP_FALSE;

    XP_U16 version = strm_getVersion( stream );

    XP_U32 len;
    if ( version < STREAM_VERS_NORELAY ) {
        XP_U8 len8;
        if ( !strm_gotU8( stream, &len8 ) ) {
            goto failure;
        }
        len = len8;
    } else if ( !strm_gotU32VL( stream, &len ) ) {
        goto failure;
    }

    if ( len > 0 ) {
        if ( buflen < len ) {
            goto failure;
        }
        if ( len >= buflen ) {
            /* better to leave stream in bad state than overwrite stack */
            len = buflen - 1;
        }
        if ( !strm_gotBytes( stream, buf, len ) ) {
            goto failure;
        }
    }
    buf[len] = '\0';
    success = XP_TRUE;
 failure:
    return success;
}

void
stringToStream( XWStreamCtxt* stream, const XP_UCHAR* str )
{
    XP_U16 version = strm_getVersion( stream );

    XP_U32 len = str == NULL? 0: XP_STRLEN( str );
    if ( version < STREAM_VERS_NORELAY ) {
        if ( len > 0xFF ) {
            XP_LOGFF( "truncating string '%s', dropping len from %d to %d",
                      str, len, 0xFF );
            XP_ASSERT(0);
            len = 0xFF;
        }
        strm_putU8( stream, (XP_U8)len );
    } else {
        strm_putU32VL( stream, len );
    }
    strm_putBytes( stream, str, len );
} /* putStringToStream */

XP_Bool
matchFromStream( XWStreamCtxt* stream, const XP_U8* bytes, XP_U16 nBytes )
{
    XP_Bool found = XP_TRUE;
    XWStreamPos start = strm_getPos( stream, POS_READ );
    for ( int ii = 0; found && ii < nBytes; ++ii ) {
        XP_U8 byte;
        if ( !strm_gotU8( stream, &byte ) ) {
            XP_LOGFF( "early end of stream at indx=%d", ii );
            found = XP_FALSE;
        } else if ( byte != bytes[ii] ) {
            XP_LOGFF( "mismatch at indx=%d", ii );
            found = XP_FALSE;
        }
    }

    if ( !found ) {
        strm_setPos( stream, POS_READ, start );
    }
    return found;
}
/*****************************************************************************
 *
 ****************************************************************************/
XP_UCHAR* 
p_copyString( MPFORMAL const XP_UCHAR* instr
#ifdef MEM_DEBUG
            , const char* file, const char* func, XP_U32 lineNo 
#endif
            )
{
    XP_UCHAR* result = (XP_UCHAR*)NULL;
    if ( !!instr ) {
        XP_U16 len = 1 + XP_STRLEN( (const char*)instr );
#ifdef MEM_DEBUG
        result = mpool_alloc( mpool, len, file, func, lineNo );
#else
        result = XP_MALLOC( ignore, len );
#endif

        XP_ASSERT( !!result );
        XP_MEMCPY( result, instr, len );
    }
    return result;
} /* copyString */

void
p_replaceStringIfDifferent( MPFORMAL XP_UCHAR** curLoc, const XP_UCHAR* newStr
#ifdef MEM_DEBUG
            , const char* file, const char* func, XP_U32 lineNo 
#endif
                          )
{
    XP_UCHAR* curStr = *curLoc;

    if ( !!newStr && !!curStr && 
         (0 == XP_STRCMP( (const char*)curStr, (const char*)newStr ) ) ) {
        /* do nothing; we're golden */
    } else {
        XP_FREEP( mpool, &curStr );
#ifdef MEM_DEBUG
        curStr = p_copyString( mpool, newStr, file, func, lineNo );
#else
        curStr = p_copyString( newStr );
#endif
    }

    *curLoc = curStr;
} /* replaceStringIfDifferent */

void
insetRect( XP_Rect* rect, XP_S16 byWidth, XP_S16 byHeight )
{
    rect->width -= byWidth * 2;
    rect->left += byWidth;
    rect->height -= byHeight * 2;
    rect->top += byHeight;
}

XP_U32
augmentHash( XP_U32 hash, const XP_U8* ptr, XP_U16 len )
{
    // see http://en.wikipedia.org/wiki/Jenkins_hash_function
    for ( XP_U16 ii = 0; ii < len; ++ii ) {
        hash += *ptr++;
        hash += (hash << 10);
        hash ^= (hash >> 6);
    }
    return hash;
}

XP_U32
finishHash( XP_U32 hash )
{
    hash += (hash << 3);
    hash ^= (hash >> 11);
    hash += (hash << 15);
    return hash;
}

XP_U16
tilesNBits( const XWStreamCtxt* stream )
{
    XP_U16 version = strm_getVersion( stream );
    XP_ASSERT( 0 < version );
    if ( 0 == version ) {
        XP_LOGFF( "version is 0" );
    }
    XP_U16 result = STREAM_VERS_NINETILES <= version
        ? NTILES_NBITS_9 : NTILES_NBITS_7;
    return result;
}

/* sMap: Exists as a backup until everybody's running code that knows isoCode
   is in wordlists. */
static struct {
    XP_LangCode lc;
    XP_UCHAR* isoCode;
} sMap[] = {
            { .lc = 0x01, .isoCode = "en", },
            { .lc = 0x02, .isoCode = "fr", },
            { .lc = 0x03, .isoCode = "de", },
            { .lc = 0x04, .isoCode = "tr", },
            { .lc = 0x05, .isoCode = "ar", },
            { .lc = 0x06, .isoCode = "es", },
            { .lc = 0x07, .isoCode = "sv", },
            { .lc = 0x08, .isoCode = "pl", },
            { .lc = 0x09, .isoCode = "da", },
            { .lc = 0x0A, .isoCode = "it", },
            { .lc = 0x0B, .isoCode = "nl", },
            { .lc = 0x0C, .isoCode = "ca", },
            { .lc = 0x0D, .isoCode = "pt", },
            { .lc = 0x0F, .isoCode = "ru", },
            { .lc = 0x11, .isoCode = "cs", },
            { .lc = 0x12, .isoCode = "el", },
            { .lc = 0x13, .isoCode = "sk", },
            { .lc = 0x14, .isoCode = "hu", },
            { .lc = 0x15, .isoCode = "ro", },
            { .lc = 0x19, .isoCode = "fi", },
            { .lc = 0x1A, .isoCode = "eu", },
};

XP_Bool
haveLocaleToLc( const XP_UCHAR* isoCode, XP_LangCode* lc )
{
    XP_Bool result = XP_FALSE;
    for ( int ii = 0; !result && ii < VSIZE(sMap); ++ii ) {
        if ( 0 == XP_STRCMP( isoCode, sMap[ii].isoCode ) ) {
            result = XP_TRUE;
            *lc = sMap[ii].lc;
        }
    }
    return result;
}

const XP_UCHAR*
lcToLocale( XP_LangCode lc )
{
    const XP_UCHAR* result = NULL;
    for ( int ii = 0; NULL == result && ii < VSIZE(sMap); ++ii ) {
        if ( lc == sMap[ii].lc ) {
            result = sMap[ii].isoCode;
        }
    }
    if ( !result ) {
        XP_LOGFF( "(%d/0x%x) => NULL", lc, lc );
    }
    return result;
}

/* 
 * A wrapper for printing etc. potentially null strings.
 */
const XP_UCHAR*
emptyStringIfNull( const XP_UCHAR* str )
{
    return !!str? str : (XP_UCHAR*)"";
} /* emptyStringIfNull */

XP_Bool
randIntArray( XP_U16* rnums, XP_U16 count )
{
    XP_Bool changed = XP_FALSE;
    XP_U16 ii;

    for ( ii = 0; ii < count; ++ii ) {
        rnums[ii] = ii;
    }

    for ( ii = count; ii > 0 ; ) {
        XP_U16 rIndex = ((XP_U16)XP_RANDOM()) % ii;
        if ( --ii != rIndex ) {
            XP_U16 tmp = rnums[rIndex];
            rnums[rIndex] = rnums[ii];
            rnums[ii] = tmp;
            if ( !changed ) {
                changed = XP_TRUE;
            }
        }
    }

    return changed;
} /* randIntArray */

XP_U16
countBits( XP_U32 mask )
{
    XP_U16 result = 0;
    while ( 0 != mask ) {
        ++result;
        mask &= mask - 1;
    }
    return result;
}

GameRef
formatGR( XP_U32 gameID, DeviceRole role )
{
    XP_ASSERT( gameID );
    GameRef gr = role;
    gr <<= 32;
    gr |= gameID;
    // XP_LOGFF( "(gameID: %X; role: %d) => " GR_FMT, gameID, role, gr );
    return gr;
}

#ifdef XWFEATURE_BASE64
/* base-64 encode binary data as a message legal for SMS.  See
 * http://www.ietf.org/rfc/rfc2045.txt for the algorithm.  glib uses this and
 * so it's not needed on linux, but unless all platforms provided identical
 * implementations it's needed for messages to be cross-platform.
*/

static const XP_UCHAR* 
getSMSTable( void )
{
    return "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
}
#define PADCHAR '='

static void
bitsToChars( const XP_U8* bytesIn, XP_U16 nValidBytes, XP_UCHAR* out, 
             XP_U16* outlen )
{
    XP_U16 nValidSextets = ((nValidBytes * 8) + 5) / 6; /* +5: round up */
    XP_U8 local[4] = { 0, 0, 0, 0 };
    XP_U16 bits[4];
    XP_MEMCPY( local, bytesIn, nValidBytes );
    /* top 6 bits of first byte */
    bits[0] = local[0] >> 2;
    /* bottom 2 bits of first byte, top four of second */ 
    bits[1] = ((local[0] << 4) & 0x30 ) | (local[1] >> 4);
    /* bottom four bits of second byte, top two of third */ 
    bits[2] = ((local[1] << 2) & 0x3C) | (local[2] >> 6);
    /* bottom six bits of third */
    bits[3] = local[2] & 0x3F;

    const XP_UCHAR* table = getSMSTable();

    XP_U16 ii;
    for ( ii = 0; ii < 4; ++ii ) {
        XP_UCHAR ch;
        if ( ii < nValidSextets ) {
            XP_U16 index = bits[ii];
            ch = table[index];
        } else {
            ch = PADCHAR;
        }
        out[(*outlen)++] = ch;
    }
} /* bitsToChars */

void
binToB64( XP_UCHAR* out, XP_U16* outlenp, const XP_U8* in, const XP_U16 inlen )
{
    XP_U16 outlen = 0;

    for ( XP_U16 inConsumed = 0; ; /*inConsumed += 3*/ ) {
        XP_U16 validBytes = XP_MIN( 3, inlen - inConsumed );
        bitsToChars( &in[inConsumed], validBytes, out, &outlen );
        XP_ASSERT( outlen <= *outlenp );

        inConsumed += validBytes;
        if ( inConsumed >= inlen ) {
            break;
        }
    }
    XP_ASSERT( outlen < *outlenp );
    out[outlen] = '\0';
    *outlenp = outlen;
    XP_ASSERT( *outlenp >= inlen );
} /* binToB64 */

void
binToB64Streams( XWStreamCtxt* out, XWStreamCtxt* in )
{
    const XP_U16 inSize = strm_getSize(in);
    const int MAX_CHUNK = 3 * 128; /* Must be multiple of 3 */
    for ( XP_U16 nRead = 0; nRead < inSize; ) {
        XP_U16 inlen = XP_MIN(inSize-nRead, MAX_CHUNK);
        XP_U8 inBuf[inlen];
        strm_getBytes( in, inBuf, inlen );
        XP_U16 outlen = 4 + ((inlen * 4) / 3);
        XP_UCHAR outBuf[outlen];
        binToB64( outBuf, &outlen, inBuf, inlen );
        /* binToB64() adds a null byte, but it's not in outlen */
        strm_putBytes( out, outBuf, outlen );
        nRead += inlen;
    }
}

/* Return false if illegal, e.g. contains bad characters.
 */

static XP_Bool
findRank( XP_UCHAR ch, XP_U8* rankp )
{
    XP_Bool success;
    if ( ch == PADCHAR ) {
        success = XP_TRUE;
        *rankp = 0;
    } else {
        success = XP_FALSE;
        const XP_UCHAR* table = getSMSTable();
        for ( XP_U8 rank = 0; rank < 64; ++rank ) {
            if ( table[rank] == ch ) {
                success = XP_TRUE;
                *rankp = rank;
                break;
            }
        }
    }
    return success;
}

/* This function stolen from glib file glib/gbase64.c.  It's also GPL'd, so
 * that may not matter.  But does my copyright need to change?  PENDING
 *
 * Also, need to check there's space before writing!  PENDING
 */
XP_Bool
b64ToBin( XP_U8* out, XP_U16* outlenp, XP_UCHAR b64in[], XP_U16 b64Len )
{
    XP_Bool success = 0 == (b64Len % 4);
    XP_U8* outptr = out;
    XP_U8 last[2] = {};
    unsigned int vv = 0;

    for ( int ii = 0; success && ii < b64Len; ++ii ) {
        XP_U8 ch = b64in[ii];
        XP_U8 rank;
        if ( !findRank( ch, &rank ) ) {
            success = XP_FALSE;
        } else {
            last[1] = last[0];
            last[0] = ch;
            vv = (vv<<6) | rank;
            if ( 3 == (ii%4) ) {
                *outptr++ = vv >> 16;
                if (last[1] != PADCHAR ) {
                    *outptr++ = vv >> 8;
                }
                if (last[0] != PADCHAR ) {
                    *outptr++ = vv;
                }
            }
        }
    }

    XP_ASSERT( *outlenp >= (outptr - out) );
    *outlenp = outptr - out;
    return success;
} /* b64ToBin */

XP_Bool
b64ToBinStreams( XWStreamCtxt* out, XWStreamCtxt* in )
{
    XWStreamPos startPosIn = strm_getPos( in, POS_READ );
    XWStreamPos startPosOut = strm_getPos( out, POS_WRITE );
    const int MAX_CHUNK = 4 * 64; /* Must be multiple of 4 */
    const XP_U16 inSize = strm_getSize( in );
    XP_Bool success = 0 == (inSize % 4);

    for ( XP_U16 nRead = 0; success && nRead < inSize; ) {
        XP_U16 chunkSize = XP_MIN(MAX_CHUNK, inSize - nRead);
        XP_UCHAR inBuf[chunkSize];
        strm_getBytes( in, inBuf, chunkSize );
        /* bin output is always smaller than b64 input */
        XP_U16 outLen = chunkSize;
        XP_U8 outBuf[outLen];
        success = b64ToBin( outBuf, &outLen, inBuf, chunkSize );
        if ( success ) {
            strm_putBytes( out, outBuf, outLen );
            nRead += chunkSize;
        }
    }

    if ( !success ) {
        XP_LOGFF( "bad input? resetting streams" );
        strm_setPos( in, POS_READ, startPosIn );
        strm_setPos( out, POS_WRITE, startPosOut );
    }
    return success;
}

#endif

XP_UCHAR*
formatMQTTDevID( const MQTTDevID* devid, XP_UCHAR* buf, XP_U16 bufLen )
{
    XP_ASSERT( bufLen >= 17 );
#ifdef DEBUG
    int len =
#endif
        XP_SNPRINTF( buf, bufLen, MQTTDevID_FMT, *devid );
    XP_ASSERT( len < bufLen );
    // LOG_RETURNF( "%s", buf );
    return buf;
}

XP_UCHAR*
formatMQTTDevTopic( const MQTTDevID* devid, XP_UCHAR* buf, XP_U16 bufLen )
{
#ifdef DEBUG
    int len =
#endif
        XP_SNPRINTF( buf, bufLen, MQTTTopic_FMT, *devid );
    XP_ASSERT( len < bufLen );
    // LOG_RETURNF( "%s", buf );
    return buf;
}

XP_UCHAR*
formatMQTTCtrlTopic( const MQTTDevID* devid, XP_UCHAR* buf, XP_U16 bufLen )
{
    XP_SNPRINTF( buf, bufLen, MQTTCtrlTopic_FMT, *devid );
    return buf;
}

XP_Bool
strToMQTTCDevID( const XP_UCHAR* str, MQTTDevID* result )
{
    XP_Bool success = XP_FALSE;
    if ( 16 == strlen(str) ) {
        MQTTDevID tmp;
        int nMatched = sscanf( str, MQTTDevID_FMT, &tmp );
        success = nMatched == 1;
        if ( success ) {
            *result = tmp;
        }
    }
    return success;
}

#ifdef DEBUG
#define NUM_PER_LINE 8
void
log_hex( const XP_U8* memp, XP_U16 len, const char* tag )
{
    XP_LOGF( "%s(len=%d[0x%x])", __func__, len, len );
    const char* hex = "0123456789ABCDEF";
    XP_U16 ii, jj;
    XP_U16 offset = 0;

    while ( offset < len ) {
        XP_UCHAR buf[128];
        XP_UCHAR vals[NUM_PER_LINE*3];
        XP_UCHAR* valsp = vals;
        XP_UCHAR chars[NUM_PER_LINE+1];
        XP_UCHAR* charsp = chars;
        XP_U16 oldOffset = offset;

        for ( ii = 0; ii < NUM_PER_LINE && offset < len; ++ii ) {
            XP_U8 byte = memp[offset];
            for ( jj = 0; jj < 2; ++jj ) {
                *valsp++ = hex[(byte & 0xF0) >> 4];
                byte <<= 4;
            }
            *valsp++ = ':';

            byte = memp[offset];
            if ( (byte >= 'A' && byte <= 'Z')
                 || (byte >= 'a' && byte <= 'z')
                 || (byte >= '0' && byte <= '9') ) {
                /* keep it */
            } else {
                byte = '.';
            }
            *charsp++ = byte;
            ++offset;
        }
        *(valsp-1) = '\0';      /* -1 to overwrite ':' */
        *charsp = '\0';

        if ( (NULL == tag) || (XP_STRLEN(tag) + sizeof(vals) >= sizeof(buf)) ) {
            tag = "<tag>";
        }
        XP_SNPRINTF( buf, sizeof(buf), "%s[%.3d]: %-24s %s", tag, oldOffset,
                     vals, chars );
        XP_LOGF( "%s", buf );
    }
}

void
log_devid( const MQTTDevID* devID, const XP_UCHAR* tag )
{
    XP_UCHAR buf[32];
    XP_SNPRINTF( buf, VSIZE(buf), MQTTDevID_FMT, *devID );
    XP_LOGFF( "%s: id: %s", tag, buf );
}
#endif

XP_Bool
urlDecodeFromStream( XW_DUtilCtxt* duc, XWStreamCtxt* stream,
                     UrlDecodeIter* iter, UrlParamType typ, ... )
{
    XP_Bool found = XP_FALSE;
    va_list ap;
    va_start( ap, typ );
    found = urlDecodeFromStreamAP( duc, stream, iter, typ, &ap );
    va_end( ap );
    return found;
}

#ifdef CPLUS
}
#endif
