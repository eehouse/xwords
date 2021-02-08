/* -*- compile-command: "cd ../linux && make MEMDEBUG=TRUE -j3"; -*- */
/* 
 * Copyright 1997-2011 by Eric House (xwords@eehouse.org).  All rights
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

#ifdef USE_STDIO
# include <stdio.h>
# include <stdlib.h>
#endif

#include "comtypes.h"
#include "dictnryp.h"
#include "dictiter.h"
#include "xwstream.h"
#include "strutils.h"
#include "dictiter.h"
#include "game.h"
#include "dbgutil.h"

#ifdef CPLUS
extern "C" {
#endif

/*****************************************************************************
 *
 ****************************************************************************/

static XP_Bool makeBitmap( XP_U8 const** ptrp, const XP_U8* end );

const DictionaryCtxt*
p_dict_ref( const DictionaryCtxt* dict, XWEnv XP_UNUSED(xwe)
#ifdef DEBUG_REF
            ,const char* func, const char* file, int line
#endif
            )
{
    if ( !!dict ) {
        DictionaryCtxt* _dict = (DictionaryCtxt*)dict;
        pthread_mutex_lock( &_dict->mutex );
        ++_dict->refCount;
#ifdef DEBUG_REF
        XP_LOGFF( "(dict=%p): refCount now %d (from line %d of %s() in %s)",
                 dict, dict->refCount, line, func, file );
#endif
        pthread_mutex_unlock( &_dict->mutex );
    }
    return dict;
}

void
p_dict_unref( const DictionaryCtxt* dict, XWEnv xwe
#ifdef DEBUG_REF
            ,const char* func, const char* file, int line
#endif
              )
{
    if ( !!dict ) {
        DictionaryCtxt* _dict = (DictionaryCtxt*)dict;
        pthread_mutex_lock( &_dict->mutex );
        XP_ASSERT( 0 != _dict->refCount );
        --_dict->refCount;
        XP_ASSERT( 0 <= _dict->refCount );
#ifdef DEBUG_REF
        XP_LOGF( "(dict=%p): refCount now %d  (from line %d of %s() in %s)",
                 dict, dict->refCount, line, func, file );
#endif
        pthread_mutex_unlock( &_dict->mutex );
        if ( 0 == _dict->refCount ) {
            /* There's a race here. If another thread locks the mutex we'll
               still destroy the dict (and the locked mutex!!!) PENDING */
            pthread_mutex_destroy( &_dict->mutex );
            (*dict->destructor)( _dict, xwe );
        }
    }
}

void
dict_unref_all( PlayerDicts* pd, XWEnv xwe )
{
    XP_U16 ii;
    for ( ii = 0; ii < MAX_NUM_PLAYERS; ++ii ) {
        dict_unref( pd->dicts[ii], xwe );
    }
}

static XP_UCHAR*
getNullTermParam( DictionaryCtxt* XP_UNUSED_DBG(dctx), const XP_U8** ptr,
                  XP_U16* headerLen )
{
    XP_U16 len = 1 + XP_STRLEN( (XP_UCHAR*)*ptr );
    XP_UCHAR* result = XP_MALLOC( dctx->mpool, len );
    XP_MEMCPY( result, *ptr, len );
    *ptr += len;
    *headerLen -= len;
    return result;
}

static XP_Bool
loadSpecialData( DictionaryCtxt* ctxt, XP_U8 const** ptrp,
                 const XP_U8* end )
{
    LOG_FUNC();
    XP_Bool success = XP_TRUE;
    XP_U16 nSpecials = countSpecials( ctxt );
    XP_U8 const* ptr = *ptrp;
    XP_UCHAR** texts;
    XP_UCHAR** textEnds;
    SpecialBitmaps* bitmaps;

    texts = (XP_UCHAR**)XP_MALLOC( ctxt->mpool,
                                   nSpecials * sizeof(*texts) );
    textEnds = (XP_UCHAR**)XP_MALLOC( ctxt->mpool,
                                      nSpecials * sizeof(*textEnds) );

    bitmaps = (SpecialBitmaps*)
        XP_CALLOC( ctxt->mpool, nSpecials * sizeof(*bitmaps) );

    for ( Tile ii = 0; ii < ctxt->nFaces; ++ii ) {
        const XP_UCHAR* facep = ctxt->facePtrs[(short)ii];
        if ( IS_SPECIAL(*facep) ) {
            /* get the string */
            CHECK_PTR( ptr, 1, end, error );
            XP_U8 txtlen = *ptr++;
            CHECK_PTR( ptr, txtlen, end, error );
            XP_UCHAR* text = (XP_UCHAR*)XP_MALLOC(ctxt->mpool, txtlen+1);
            texts[(int)*facep] = text;
            textEnds[(int)*facep] = text + txtlen + 1;
            XP_MEMCPY( text, ptr, txtlen );
            ptr += txtlen;
            text[txtlen] = '\0';
            XP_ASSERT( *facep < nSpecials ); /* firing */

            /* This little hack is safe because all bytes but the first in a
               multi-byte utf-8 char have the high bit set.  SYNONYM_DELIM
               does not have its high bit set */
            XP_ASSERT( 0 == (SYNONYM_DELIM & 0x80) );
            for ( ; '\0' != *text; ++text ) {
                if ( *text == SYNONYM_DELIM ) {
                    *text = '\0';
                }
            }

            if ( !makeBitmap( &ptr, end ) ) {
                goto error;
            }
            if ( !makeBitmap( &ptr, end ) ) {
                goto error;
            }
        }
    }

    goto done;
 error:
    success = XP_FALSE;
 done:
    ctxt->chars = texts;
    ctxt->charEnds = textEnds;
    ctxt->bitmaps = bitmaps;

    *ptrp = ptr;
    return success;
} /* loadSpecialData */
    
XP_Bool
parseCommon( DictionaryCtxt* dctx, XWEnv xwe, const XP_U8** ptrp, const XP_U8* end )
{
    const XP_U8* ptr = *ptrp;
    XP_Bool hasHeader = XP_FALSE;
    XP_Bool isUTF8 = XP_FALSE;
    XP_U16 charSize;

    XP_U16 flags;
    XP_Bool formatOk = sizeof(flags) <= end - ptr;
    if ( formatOk ) {
        XP_MEMCPY( &flags, ptr, sizeof(flags) );
        ptr += sizeof( flags );
        flags = XP_NTOHS(flags);

        XP_LOGFF( "flags=0X%X", flags );
        hasHeader = 0 != (DICT_HEADER_MASK & flags);
        /* if ( hasHeader ) { */
        /*     flags &= ~DICT_HEADER_MASK; */
        /* } */

        XP_U8 nodeSize = 4;
        switch ( flags & 0x0007 ) {
        case 0x0001:
            nodeSize = 3;
            charSize = 1;
            dctx->is_4_byte = XP_FALSE;
            break;
        case 0x0002:
            nodeSize = 3;
            charSize = 2;
            dctx->is_4_byte = XP_FALSE;
            break;
        case 0x0003:
            charSize = 2;
            dctx->is_4_byte = XP_TRUE;
            break;
        case 0x0004:
            nodeSize = 3;
            isUTF8 = XP_TRUE;
            dctx->is_4_byte = XP_FALSE;
            break;
        case 0x0005:
            isUTF8 = XP_TRUE;
            dctx->is_4_byte = XP_TRUE;
            break;
        default:
            formatOk = XP_FALSE;
            break;
        }
        dctx->isUTF8 = isUTF8;
        dctx->nodeSize = nodeSize;
    }

    if ( formatOk ) {
        XP_U8 numFaceBytes, numFaces;

        if ( hasHeader ) {
            XP_U16 headerLen;
            XP_U32 wordCount;

            XP_MEMCPY( &headerLen, ptr, sizeof(headerLen) );
            ptr += sizeof(headerLen);
            headerLen = XP_NTOHS( headerLen );

            XP_MEMCPY( &wordCount, ptr, sizeof(wordCount) );
            ptr += sizeof(wordCount);
            headerLen -= sizeof(wordCount);
            dctx->nWords = XP_NTOHL( wordCount );
            XP_DEBUGF( "dict contains %d words", dctx->nWords );

            if ( 0 < headerLen ) {
                dctx->desc = getNullTermParam( dctx, &ptr, &headerLen );
            } else {
                XP_LOGF( "%s: no note", __func__ );
            }
            if ( 0 < headerLen ) {
                dctx->md5Sum = getNullTermParam( dctx, &ptr, &headerLen );
            } else {
                XP_LOGF( "%s: no md5Sum", __func__ );
            }

            XP_U16 headerFlags = 0;
            if ( sizeof(headerFlags) <= headerLen ) {
                XP_MEMCPY( &headerFlags, ptr, sizeof(headerFlags) );
                headerFlags = XP_NTOHS( headerFlags );
                ptr += sizeof(headerFlags);
                headerLen -= sizeof(headerFlags);
            }
            XP_LOGFF( "setting headerFlags: 0x%x", headerFlags );
            dctx->headerFlags = headerFlags;

            if ( 0 < headerLen ) {
                XP_LOGFF( "skipping %d bytes of header", headerLen );
            }
            ptr += headerLen;
        }

        if ( isUTF8 ) {
            numFaceBytes = *ptr++;
        }
        numFaces = *ptr++;
        if ( !isUTF8 ) {
            numFaceBytes = numFaces * charSize;
        }

        if ( NULL == dctx->md5Sum
#ifdef DEBUG
             || XP_TRUE
#endif
             ) {
            XP_UCHAR checksum[256];
            // XP_LOGFF( "figuring checksum with len: %uz", end - ptr );
            computeChecksum( dctx, xwe, ptr, end - ptr, checksum );
            if ( NULL == dctx->md5Sum ) {
                dctx->md5Sum = copyString( dctx->mpool, checksum );
            } else {
#ifndef PLATFORM_WASM
                XP_ASSERT( 0 == XP_STRCMP( dctx->md5Sum, checksum ) );
#endif
            }
        }

        dctx->nFaces = numFaces;

        dctx->countsAndValues = XP_MALLOC( dctx->mpool, numFaces * 2 );
        XP_U16 facesSize = numFaceBytes;
        if ( !isUTF8 ) {
            facesSize /= 2;
        }

        XP_U8 tmp[numFaceBytes];
        XP_MEMCPY( tmp, ptr, numFaceBytes );
        ptr += numFaceBytes;

        dict_splitFaces( dctx, xwe, tmp, numFaceBytes, numFaces );

        unsigned short xloc;
        XP_MEMCPY( &xloc, ptr, sizeof(xloc) );
        ptr += sizeof(xloc);
        XP_MEMCPY( dctx->countsAndValues, ptr, numFaces*2 );
        ptr += numFaces*2;

        dctx->langCode = xloc & 0x7F;
    }

    if ( formatOk ) {
        formatOk = loadSpecialData( dctx, &ptr, end );
    }

    if ( formatOk ) {
        XP_ASSERT( ptr < end );
        *ptrp = ptr;
    }

    LOG_RETURNF( "%s", boolToStr(formatOk) );
    return formatOk;
}

static XP_Bool
makeBitmap( XP_U8 const** ptrp, const XP_U8* end )
{
    XP_Bool success = XP_TRUE;
    XP_U8 const* ptr = *ptrp;
    CHECK_PTR( ptr, 1, end, error );
    XP_U8 nCols = *ptr++;
    if ( nCols > 0 ) {
        CHECK_PTR( ptr, 1, end, error );
        XP_U8 nRows = *ptr++;
        CHECK_PTR( ptr, ((nRows*nCols)+7) / 8, end, error );
#ifdef DROP_BITMAPS
        ptr += ((nRows*nCols)+7) / 8;
#else
        do not compile
#endif
    }
    goto done;
 error:
    success = XP_FALSE;
 done:
    *ptrp = ptr;
    return success;
}

XP_U16
countSpecials( DictionaryCtxt* ctxt )
{
    XP_U16 result = 0;

    for ( int ii = 0; ii < ctxt->nFaces; ++ii ) {
        if ( IS_SPECIAL( ctxt->facePtrs[ii][0] ) ) {
            ++result;
        }
    }

    return result;
} /* countSpecials */

void
setBlankTile( DictionaryCtxt* dict ) 
{
    dict->blankTile = -1; /* no known blank */
    XP_U16 maxLen = 0;
    for ( int tile = 0; tile < dict->nFaces; ++tile ) {
        const XP_UCHAR* facePtr = dict->facePtrs[tile];
        if ( facePtr[0] == 0 ) {
            XP_ASSERT( dict->blankTile == -1 ); /* only one passes test? */
            dict->blankTile = (XP_S8)tile;
        }

        if ( IS_SPECIAL( *facePtr ) ) {
            facePtr = dict_getTileString( dict, tile );
        }
        XP_U16 thisLen = XP_STRLEN( facePtr );
        if ( thisLen > maxLen ) {
            maxLen = thisLen;
        }
    }
    dict->maxChars = maxLen;
} /* setBlankTile */

/* #if defined BLANKS_FIRST || defined DEBUG */
XP_Bool
dict_hasBlankTile( const DictionaryCtxt* dict )
{
    return dict->blankTile >= 0;
} /* dict_hasBlankTile */
/* #endif */

Tile
dict_getBlankTile( const DictionaryCtxt* dict )
{
    XP_ASSERT( dict_hasBlankTile(dict) );
    return (Tile)dict->blankTile;
} /* dict_getBlankTile */

XP_U16
dict_getTileValue( const DictionaryCtxt* dict, Tile tile )
{
    XP_ASSERT( !!dict );
    if ( (tile & TILE_VALUE_MASK) != tile ) {
        XP_ASSERT( tile == 32 && 
                   tile == dict_getBlankTile( dict ) );
    }
    XP_ASSERT( tile < dict->nFaces );
    tile *= 2;
    XP_ASSERT( !!dict->countsAndValues );
    return dict->countsAndValues[tile+1];    
} /* dict_getTileValue */

static const XP_UCHAR*
dict_getTileStringRaw( const DictionaryCtxt* dict, Tile tile )
{
    XP_ASSERT( tile < dict->nFaces );
    return dict->facePtrs[tile];
}

const XP_UCHAR* 
dict_getTileString( const DictionaryCtxt* dict, Tile tile )
{
    const XP_UCHAR* facep = dict_getTileStringRaw( dict, tile );
    if ( IS_SPECIAL(*facep) ) {
        facep = dict->chars[(XP_U16)*facep];
    }
    return facep;
}

const XP_UCHAR* 
dict_getNextTileString( const DictionaryCtxt* dict, Tile tile, 
                        const XP_UCHAR* cur )
{
    const XP_UCHAR* result = NULL;
    if ( NULL == cur ) {
        result = dict_getTileString( dict, tile );
    } else {
        cur += XP_STRLEN( cur ) + 1;
        XP_Bool isSpecial = dict_faceIsBitmap( dict, tile );
        if ( isSpecial || tile == dict->blankTile ) {
            const XP_UCHAR* facep = dict_getTileStringRaw( dict, tile );
            if ( cur < dict->charEnds[(XP_U16)*facep] ) {
                result = cur;
            }
        } else {
            /* use cur only if it is is not now off the end or pointing to to the
               next tile */
            if ( ++tile == dict->nFaces ) {
                if ( cur < dict->facesEnd ) {
                    result = cur;
                }
            } else {
                const XP_UCHAR* nxt = dict_getTileStringRaw( dict, tile );
                if ( nxt != cur ) {
                    result = cur;
                }
            }
        }
    }
    return result;
}

XP_U16
dict_numTiles( const DictionaryCtxt* dict, Tile tile )
{
    tile *= 2;
    return dict->countsAndValues[tile];
} /* dict_numTiles */

XP_U16
dict_numTileFaces( const DictionaryCtxt* dict )
{
    return dict->nFaces;
} /* dict_numTileFaces */

XP_U16
dict_getMaxTileChars( const DictionaryCtxt* ctxt )
{
    XP_ASSERT( 0 != ctxt->maxChars );
    return ctxt->maxChars;
}

static void
appendIfSpace( XP_UCHAR** bufp, const XP_UCHAR* end, const XP_UCHAR* newtxt )
{
    XP_U16 len = XP_STRLEN( newtxt );
    if ( *bufp + len < end ) {
        XP_MEMCPY( *bufp, newtxt, len );
        *bufp += len;
    } else {
        *bufp = NULL;
    }
}

XP_U16
dict_tilesToString( const DictionaryCtxt* dict, const Tile* tiles, 
                    XP_U16 nTiles, XP_UCHAR* buf, XP_U16 bufSize,
                    const XP_UCHAR* delim )
{
    XP_UCHAR* bufp = buf;
    const XP_UCHAR* end = bufp + bufSize;
    XP_U16 delimLen = NULL == delim ? 0 : XP_STRLEN(delim);

    for ( int ii = 0; ii < nTiles && !!bufp; ++ii ) {

        if ( 0 < delimLen && 0 < ii ) {
            appendIfSpace( &bufp, end, delim );
        }

        Tile tile = tiles[ii];
        const XP_UCHAR* facep = dict_getTileStringRaw( dict, tile );
        if ( IS_SPECIAL(*facep) ) {
            XP_UCHAR* chars = dict->chars[(XP_U16)*facep];
            appendIfSpace( &bufp, end, chars );
        } else {
            XP_ASSERT ( tile != dict->blankTile ); /* printing blank should be
                                                      handled by specials
                                                      mechanism */
            appendIfSpace( &bufp, end, facep );
        }
    }
    
    XP_U16 result = 0;
    if ( !!bufp && bufp < end ) {
        *bufp = '\0';
        result = bufp - buf;
    }
    return result;
} /* dict_tilesToString */

/* Convert str to an array of tiles, continuing until we fail to match or we
 * run out of room in which to return tiles.  Failure to match means return of
 * XP_FALSE, but if we run out of room before failing we return XP_TRUE.
 */

static XP_Bool
tilesForStringImpl( const DictionaryCtxt* dict,
                    const XP_UCHAR* str, XP_U16 strLen,
                    Tile* tiles, XP_U16 nTiles, XP_U16 nFound,
                    OnFoundTiles proc, void* closure )
{
    XP_Bool goOn;
    if ( nFound == nTiles || 0 == strLen ) {
        /* We've recursed to the end and have found a tile! */
        goOn = (*proc)( closure, tiles, nFound );
    } else {
        goOn = XP_TRUE;

        XP_U16 nFaces = dict_numTileFaces( dict );
        for ( Tile tile = 0; goOn && tile < nFaces; ++tile ) {
            for ( const XP_UCHAR* facep = NULL; ; ) {
                facep = dict_getNextTileString( dict, tile, facep );
                if ( !facep ) {
                    break;
                }
                XP_U16 faceLen = XP_STRLEN( facep );
                if ( 0 == XP_STRNCMP( facep, str, faceLen ) ) {
                    tiles[nFound] = tile;
                    goOn = tilesForStringImpl( dict, str + faceLen,
                                               strLen - faceLen,
                                               tiles, nTiles, nFound + 1,
                                               proc, closure );
                    break;  /* impossible to have than one match per tile */
                }
            }
        }
    }
    return goOn;
} /* tilesForStringImpl */

void
dict_tilesForString( const DictionaryCtxt* dict, const XP_UCHAR* str,
                     XP_U16 strLen, OnFoundTiles proc, void* closure )
{
    Tile tiles[32];
    if ( 0 == strLen ) {
        strLen = XP_STRLEN( str );
    }
    tilesForStringImpl( dict, str, strLen, tiles, VSIZE(tiles), 0, proc, closure );
} /* dict_tilesForString */

XP_Bool
dict_tilesAreSame( const DictionaryCtxt* dict1, const DictionaryCtxt* dict2 )
{
    XP_Bool result = XP_FALSE;

    XP_ASSERT( !!dict1 );
    XP_ASSERT( !!dict2 );

    Tile ii;
    XP_U16 nTileFaces = dict_numTileFaces( dict1 );

    if ( nTileFaces == dict_numTileFaces( dict2 ) ) {
        for ( ii = 0; ii < nTileFaces; ++ii ) {

            const XP_UCHAR* face1;
            const XP_UCHAR* face2;

            if ( dict_getTileValue( dict1, ii )
                 != dict_getTileValue( dict2, ii ) ){
                break;
            }
            face1 = dict_getTileStringRaw( dict1, ii );
            face2 = dict_getTileStringRaw( dict2, ii );
            if ( IS_SPECIAL(*face1) != IS_SPECIAL(*face2) ) {
                break;
            }
            if ( IS_SPECIAL(*face1) ) {
                XP_UCHAR* chars1 = dict1->chars[(int)*face1];
                XP_UCHAR* chars2 = dict2->chars[(int)*face2];
                XP_U16 len = XP_STRLEN(chars1);
                if ( 0 != XP_STRNCMP( chars1, chars2, len ) ) {
                    break;
                }
            } else if ( 0 != XP_STRCMP( face1, face2 ) ) {
                break;
            }
            if ( dict_numTiles( dict1, ii ) != dict_numTiles( dict2, ii ) ) {
                break;
            }
        }
        result = ii == nTileFaces; /* did we get that far */
    }
    return result;
} /* dict_tilesAreSame */

#ifndef XWFEATURE_STANDALONE_ONLY
static void
ucharsToNarrow( const DictionaryCtxt* dict, XP_UCHAR* buf, XP_U16* bufsizep )
{
    XP_U16 ii;
    XP_U16 nUsed = 0;
    XP_U16 bufsize = *bufsizep;
    for ( ii = 0; ii < dict->nFaces; ++ii ) {
        const XP_UCHAR* facep = dict_getTileStringRaw( dict, ii );
        if ( IS_SPECIAL(*facep) ) {
            buf[nUsed++] = *facep;
        } else {
            nUsed += XP_SNPRINTF( &buf[nUsed], bufsize - nUsed, "%s", facep );
        }
        XP_ASSERT( nUsed < bufsize );
    }
    buf[nUsed] = 0;
    *bufsizep = nUsed;
}

/* Summarize tile info in a way it can be presented to users */
void
dict_writeTilesInfo( const DictionaryCtxt* dict, XWStreamCtxt* stream )
{
    XP_U16 nFaces = dict_numTileFaces( dict );
    for ( Tile tile = 0; tile < nFaces; ++tile ) {
        XP_U16 val = dict_getTileValue( dict, tile );
        XP_U16 count = dict_numTiles( dict, tile );
        const XP_UCHAR* face = dict_getTileString( dict, tile );
        XP_UCHAR buf[32];
        XP_SNPRINTF( buf, VSIZE(buf), "%s\t%d\t%d\n", face, count, val );
        stream_catString( stream, buf );
    }
}

void
dict_writeToStream( const DictionaryCtxt* dict, XWStreamCtxt* stream )
{
    XP_U16 maxCount = 0;
    XP_U16 maxValue = 0;
    XP_U16 ii, nSpecials;
    XP_U16 maxCountBits, maxValueBits;
    XP_UCHAR buf[64];
    XP_U16 nBytes;

    stream_putBits( stream, 6, dict->nFaces );

    for ( ii = 0; ii < dict->nFaces*2; ii+=2 ) {
        XP_U16 count, value;

        count = dict->countsAndValues[ii];
        if ( maxCount < count ) {
            maxCount = count;
        }

        value = dict->countsAndValues[ii+1];
        if ( maxValue < value ) {
            maxValue = value;
        }
    }

    maxCountBits = bitsForMax( maxCount );
    maxValueBits = bitsForMax( maxValue );

    stream_putBits( stream, 3, maxCountBits ); /* won't be bigger than 5 */
    stream_putBits( stream, 3, maxValueBits );

    for ( ii = 0; ii < dict->nFaces*2; ii+=2 ) {
        stream_putBits( stream, maxCountBits, dict->countsAndValues[ii] );
        stream_putBits( stream, maxValueBits, dict->countsAndValues[ii+1] );
    }

    /* Stream format of the faces is unchanged: chars run together, which
     * happens to equal utf-8 for ascii.  But now there may be more than one
     * byte per face.  Old code assumes that, but compatibility is ensured by
     * the caller which will not accept an incoming message if the version's
     * too new.  And utf-8 dicts are flagged as newer by the sender.
     */

    nBytes = sizeof(buf);
    ucharsToNarrow( dict, buf, &nBytes );
    stream_putU8( stream, nBytes );
    stream_putBytes( stream, buf, nBytes );

    for ( nSpecials = ii = 0; ii < dict->nFaces; ++ii ) {
        const XP_UCHAR* facep = dict_getTileStringRaw( dict, (Tile)ii );
        if ( IS_SPECIAL( *facep ) ) {
            stringToStream( stream, dict->chars[nSpecials++] );
        }
    }
} /* dict_writeToStream */
#endif

static void
freeSpecials( DictionaryCtxt* dict )
{
    Tile tt;
    XP_U16 nSpecials;

    for ( nSpecials = tt = 0; tt < dict->nFaces; ++tt ) {
        const XP_UCHAR* facep = dict_getTileStringRaw( dict, tt );
        if ( IS_SPECIAL( *facep ) ) {

            XP_ASSERT( !!dict->chars[nSpecials] );
            XP_FREE( dict->mpool, dict->chars[nSpecials] );

            XP_FREEP( dict->mpool, &dict->bitmaps[nSpecials].largeBM );
            XP_FREEP( dict->mpool, &dict->bitmaps[nSpecials].smallBM );

            ++nSpecials;
        }
    }
    if ( nSpecials > 0 ) {
        XP_FREE( dict->mpool, dict->chars );
        XP_FREE( dict->mpool, dict->bitmaps );
    }
} /* freeSpecials */

static void
common_destructor( DictionaryCtxt* dict, XWEnv XP_UNUSED(xwe) )
{
    freeSpecials( dict );

    XP_FREE( dict->mpool, dict->countsAndValues );
    XP_FREE( dict->mpool, dict->faces );
    XP_FREE( dict->mpool, dict->facePtrs );

    XP_FREE( dict->mpool, dict );
} /* common_destructor */

void
dict_loadFromStream( DictionaryCtxt* dict, XWEnv xwe, XWStreamCtxt* stream )
{
    XP_U8 nFaces, nFaceBytes;
    XP_U16 maxCountBits, maxValueBits;
    XP_U16 ii, nSpecials;
    XP_UCHAR* localTexts[32];
    XP_U8 utf8[MAX_UNIQUE_TILES];

    if ( !!dict->destructor ) {
        XP_LOGF( "%s(): replacing destructor!!", __func__ );
    }
    dict->destructor = common_destructor;
    dict->func_dict_getShortName = dict_getName; /* default */

    nFaces = (XP_U8)stream_getBits( stream, 6 );
    maxCountBits = (XP_U16)stream_getBits( stream, 3 );
    maxValueBits = (XP_U16)stream_getBits( stream, 3 );

    dict->nFaces = nFaces;

    dict->countsAndValues =
        (XP_U8*)XP_MALLOC( dict->mpool, 
                           sizeof(dict->countsAndValues[0]) * nFaces * 2  );

    for ( ii = 0; ii < dict->nFaces*2; ii+=2 ) {
        dict->countsAndValues[ii] = (XP_U8)stream_getBits( stream, 
                                                          maxCountBits );
        dict->countsAndValues[ii+1] = (XP_U8)stream_getBits( stream, 
                                                            maxValueBits );
    }

    nFaceBytes = (XP_U8)stream_getU8( stream );
    XP_ASSERT( nFaceBytes < VSIZE(utf8) );
    stream_getBytes( stream, utf8, nFaceBytes );
    dict->isUTF8 = XP_TRUE;     /* need to communicate this in stream */
    dict_splitFaces( dict, xwe, utf8, nFaceBytes, nFaces );

    for ( nSpecials = ii = 0; ii < nFaces; ++ii ) {
        const XP_UCHAR* facep = dict_getTileStringRaw( dict, (Tile)ii );
        if ( IS_SPECIAL( *facep ) ) {
            XP_UCHAR* txt = stringFromStream( dict->mpool, stream );
            XP_ASSERT( !!txt );
            localTexts[nSpecials] = txt;

            ++nSpecials;
        }
    }
    if ( nSpecials > 0 ) {
        dict->bitmaps = 
            (SpecialBitmaps*)XP_MALLOC( dict->mpool,
                                        nSpecials * sizeof(*dict->bitmaps) );
        XP_MEMSET( dict->bitmaps, 0, nSpecials * sizeof(*dict->bitmaps) );

        dict->chars = (XP_UCHAR**)XP_MALLOC( dict->mpool,
                                             nSpecials * sizeof(*dict->chars) );
        XP_MEMCPY(dict->chars, localTexts, nSpecials * sizeof(*dict->chars));
    }
    setBlankTile( dict );
} /* dict_loadFromStream */

#ifdef TEXT_MODEL
/* Return the strlen of the longest face, e.g. 1 for English and Italian;
   2 for Spanish; 3 for Catalan */
XP_U16
dict_getMaxWidth( const DictionaryCtxt* dict )
{
    XP_U16 result = 0;
    Tile tile;
    XP_U16 nFaces = dict_numTileFaces( dict );

    for ( tile = 0; tile < nFaces; ++tile ) {
        const XP_UCHAR* face = dict_getTileString( dict, tile );
        XP_U16 len = XP_STRLEN( face );
        if ( len > result ) {
            result = len;
        }
    }
    return result;
}
#endif


const XP_UCHAR*
dict_getName( const DictionaryCtxt* dict )
{
    XP_ASSERT( !!dict );
    XP_ASSERT( !!dict->name );
    return dict->name;
} /* dict_getName */

XP_Bool
dict_isUTF8( const DictionaryCtxt* dict )
{
    XP_ASSERT( !!dict );
    return dict->isUTF8;
}

XP_Bool
dict_faceIsBitmap( const DictionaryCtxt* dict, Tile tile )
{
    const XP_UCHAR* facep = dict_getTileStringRaw( dict, tile );
    return IS_SPECIAL(*facep) && (tile != dict->blankTile);
} /* dict_faceIsBitmap */

void
dict_getFaceBitmaps( const DictionaryCtxt* dict, Tile tile, XP_Bitmaps* bmps )
{
    SpecialBitmaps* bitmaps;
    const XP_UCHAR* facep = dict_getTileStringRaw( dict, tile );

    XP_ASSERT( dict_faceIsBitmap( dict, tile ) );
    XP_ASSERT( !!dict->bitmaps );

    bitmaps = &dict->bitmaps[(XP_U16)*facep];
    bmps->nBitmaps = 2;
    bmps->bmps[0] = bitmaps->smallBM;
    bmps->bmps[1] = bitmaps->largeBM;
} /* dict_getFaceBitmaps */

XP_LangCode
dict_getLangCode( const DictionaryCtxt* dict )
{
    return dict->langCode;
}

XP_U32
dict_getWordCount( const DictionaryCtxt* dict, XWEnv xwe )
{
    XP_U32 nWords = dict->nWords;
#ifdef XWFEATURE_WALKDICT
    if ( 0 == nWords ) {
        DictIter* iter = di_makeIter( dict, xwe, NULL, NULL, 0, NULL, 0 );
        nWords = di_countWords( iter, NULL );
        di_freeIter( iter, xwe );
    }
#endif
    return nWords;
}

const XP_UCHAR* 
dict_getDesc( const DictionaryCtxt* dict )
{
    return dict->desc;
}

const XP_UCHAR* 
dict_getMd5Sum( const DictionaryCtxt* dict )
{
    return dict->md5Sum;
}

XP_Bool
dict_hasDuplicates( const DictionaryCtxt* dict )
{
    return 0 != (dict->headerFlags & HEADERFLAGS_DUPS_SUPPORTED_BIT);
}

#ifdef STUBBED_DICT

#define BLANK_FACE '\0'

static XP_U8 stub_english_data[] = {
    /* count            value           face */
    9,   1,  'A',
    2,   3,  'B',
    2,   3,  'C',
    4,   2,  'D',
    12,  1,  'E',
    2,   4,  'F',
    3,   2,  'G',
    2,   4,  'H',
    9,   1,  'I',
    1,   8,  'J',
    1,   5,  'K',
    4,   1,  'L',
    2,   3,  'M',
    6,   1,  'N',
    8,   1,  'O',
    2,   3,  'P',
    1,   10, 'Q',
    6,   1,  'R',
    4,   1,  'S',
    6,   1,  'T',
    4,   1,  'U',
    2,   4,  'V',
    2,   4,  'W',
    1,   8,  'X',
    2,   4,  'Y',
    1,   10, 'Z',
    2,   0,   BLANK_FACE, /* BLANK1 */
};

void
setStubbedSpecials( DictionaryCtxt* dict )
{
    dict->chars = (XP_UCHAR**)XP_MALLOC( dict->mpool, sizeof(char*) );
    dict->chars[0] = "_";

} /* setStubbedSpecials */

void
destroy_stubbed_dict( DictionaryCtxt* dict )
{
    XP_FREE( dict->mpool, dict->countsAndValues );
    XP_FREE( dict->mpool, dict->faces );
    XP_FREE( dict->mpool, dict->chars );
    XP_FREE( dict->mpool, dict->name );
    XP_FREE( dict->mpool, dict->langName );
    XP_FREE( dict->mpool, dict->bitmaps );
    XP_FREE( dict->mpool, dict );
} /* destroy_stubbed_dict */

DictionaryCtxt*
make_stubbed_dict( MPFORMAL_NOCOMMA )
{
    DictionaryCtxt* dict = (DictionaryCtxt*)XP_MALLOC( mpool, sizeof(*dict) );
    XP_U8* data = stub_english_data;
    XP_U16 datasize = sizeof(stub_english_data);
    XP_U16 ii;

    XP_MEMSET( dict, 0, sizeof(*dict) );

    MPASSIGN( dict->mpool, mpool );
    dict->name = copyString( mpool, "Stub dictionary" );
    dict->nFaces = datasize/3;

    dict->destructor = destroy_stubbed_dict;

    dict->faces = (XP_UCHAR*)
        XP_MALLOC( mpool, 2 * dict->nFaces * sizeof(dict->faces[0]) );
    dict->facePtrs = (XP_UCHAR**)
        XP_MALLOC( mpool, dict->nFaces * sizeof(dict->facePtrs[0]) );
    
    XP_UCHAR* nextChar = dict->faces;
    XP_UCHAR** nextPtr = dict->facePtrs;
    for ( ii = 0; ii < datasize/3; ++ii ) {
        *nextPtr++ = nextChar;
        *nextChar++ = (XP_UCHAR)data[(ii*3)+2];
        *nextChar++ = '\0';
    }
    
    dict->countsAndValues = (XP_U8*)XP_MALLOC( mpool, dict->nFaces*2 );
    for ( ii = 0; ii < datasize/3; ++ii ) {
        dict->countsAndValues[ii*2] = data[(ii*3)];
        dict->countsAndValues[(ii*2)+1] = data[(ii*3)+1];
    }

    dict->bitmaps = (SpecialBitmaps*)XP_MALLOC( mpool, sizeof(SpecialBitmaps) );
    dict->bitmaps->largeBM = dict->bitmaps->largeBM = NULL;
    
    setStubbedSpecials( dict );

    setBlankTile( dict );

    return dict;
} /* make_subbed_dict */

#endif /* STUBBED_DICT */

static array_edge* 
dict_super_edge_for_index( const DictionaryCtxt* dict, XP_U32 index )
{
    array_edge* result;

    if ( index == 0 ) {
        result = NULL;
    } else {
        XP_ASSERT( index < dict->numEdges );
        /* avoid long-multiplication lib call on Palm... */
        if ( dict->nodeSize == 3 ) {
            index += (index << 1);
        } else {
            XP_ASSERT( dict->nodeSize == 4 );
            index <<= 2;
        }
        result = &dict->base[index];
    }
    return result;
} /* dict_edge_for_index */

static array_edge*
dict_super_getTopEdge( const DictionaryCtxt* dict )
{
    return dict->topEdge;
} /* dict_super_getTopEdge */

static XP_U32
dict_super_index_from( const DictionaryCtxt* dict, array_edge* p_edge ) 
{
    unsigned long result;

    array_edge_new* edge = (array_edge_new*)p_edge;
    result = ((edge->highByte << 8) | edge->lowByte) & 0x0000FFFF;

    if ( dict->is_4_byte ) {
        result |= ((XP_U32)edge->moreBits) << 16;
    } else {
        XP_ASSERT( dict->nodeSize == 3 );
        if ( (edge->bits & EXTRABITMASK_NEW) != 0 ) { 
            result |= 0x00010000; /* using | instead of + saves 4 bytes */
        }
    }
    return result;
} /* dict_super_index_from */

static array_edge*
dict_super_follow( const DictionaryCtxt* dict, array_edge* in ) 
{
    XP_U32 index = dict_index_from( dict, in );
    array_edge* result = index > 0? 
        dict_edge_for_index( dict, index ): (array_edge*)NULL;
    return result;
} /* dict_super_follow */

static array_edge*
dict_super_edge_with_tile( const DictionaryCtxt* dict, array_edge* from, 
                           Tile tile ) 
{
    for ( ; ; ) {
        Tile candidate = EDGETILE(dict,from);
        if ( candidate == tile ) {
            break;
        }

        if ( IS_LAST_EDGE(dict, from ) ) {
            from = NULL;
            break;
        }
        from += dict->nodeSize;
    }

    return from;
} /* edge_with_tile */

void
dict_super_init( MPFORMAL DictionaryCtxt* dict )
{
    dict->mpool = mpool;
    /* subclass may change these later.... */
    dict->func_edge_for_index = dict_super_edge_for_index;
    dict->func_dict_getTopEdge = dict_super_getTopEdge;
    dict->func_dict_index_from = dict_super_index_from;
    dict->func_dict_follow = dict_super_follow;
    dict->func_dict_edge_with_tile = dict_super_edge_with_tile;
    dict->func_dict_getShortName = dict_getName;

    pthread_mutex_init( &dict->mutex, NULL );
} /* dict_super_init */

const XP_UCHAR* 
dict_getLangName( const DictionaryCtxt* ctxt )
{
    return ctxt->langName;
}

#ifdef XWFEATURE_DICTSANITY
XP_Bool
checkSanity( DictionaryCtxt* dict, const XP_U32 numEdges )
{
    XP_U32 ii;
    XP_Bool passed = XP_TRUE;

    array_edge* edge = dict->base;
    if ( NULL != edge ) {       /* not empty dict */
        XP_U16 nFaces = dict_numTileFaces( dict );
        Tile prevTile = 0;
        for ( ii = 0; ii < numEdges && passed; ++ii ) {
            Tile tile = EDGETILE( dict, edge );
            if ( tile < prevTile || tile >= nFaces ) {
                XP_LOGF( "%s: node %d (out of %d) has too-large or "
                         "out-of-order tile", __func__, ii, numEdges );
                passed = XP_FALSE;
                break;
            }
            prevTile = tile;

            XP_U32 index = dict_index_from( dict, edge );
            if ( index >= numEdges ) {
                XP_LOGF( "%s: node %d (out of %d) has too-high index %d",
                         __func__, ii, numEdges, index );
                passed = XP_FALSE;
                break;
            }

            if ( IS_LAST_EDGE( dict, edge ) ) {
                prevTile = 0;
            }
            edge += dict->nodeSize;
        }

        if ( passed ) {
            passed = 0 == prevTile; /* last edge seen was a LAST_EDGE */
        }
    }

    return passed;
} /* checkSanity */
#endif

#ifdef CPLUS
}
#endif
