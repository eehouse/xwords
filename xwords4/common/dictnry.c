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
#include "xwstream.h"
#include "strutils.h"
#include "game.h"

#ifdef CPLUS
extern "C" {
#endif

/*****************************************************************************
 *
 ****************************************************************************/
void
setBlankTile( DictionaryCtxt* dict ) 
{
    XP_U16 ii;

    dict->blankTile = -1; /* no known blank */

    for ( ii = 0; ii < dict->nFaces; ++ii ) {
        if ( dict->facePtrs[ii][0] == 0 ) {
            XP_ASSERT( dict->blankTile == -1 ); /* only one passes test? */
            dict->blankTile = (XP_S8)ii;
#ifndef DEBUG
            break;
#endif
        }
    }    
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
    if ( (tile & TILE_VALUE_MASK) != tile ) {
        XP_ASSERT( tile == 32 && 
                   tile == dict_getBlankTile( dict ) );
    }
    XP_ASSERT( tile < dict->nFaces );
    tile *= 2;
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
dict_tilesToString( const DictionaryCtxt* dict, const Tile* tiles, 
                    XP_U16 nTiles, XP_UCHAR* buf, XP_U16 bufSize )
{
    XP_UCHAR* bufp = buf;
    XP_UCHAR* end = bufp + bufSize;
    XP_U16 result = 0;

    while ( nTiles-- ) {
        Tile tile = *tiles++;
        const XP_UCHAR* facep = dict_getTileStringRaw( dict, tile );

        if ( IS_SPECIAL(*facep) ) {
            XP_UCHAR* chars = dict->chars[(XP_U16)*facep];
            XP_U16 len = XP_STRLEN( chars );
            if ( bufp + len >= end ) {
                bufp = NULL;
                break;
            }
            XP_MEMCPY( bufp, chars, len );
            bufp += len;
        } else {
            XP_ASSERT ( tile != dict->blankTile ); /* printing blank should be
                                                      handled by specials
                                                      mechanism */
            if ( bufp + 1 >= end ) {
                bufp = NULL;
                break;
            }
            bufp += XP_SNPRINTF( bufp, end - bufp, XP_S, facep );
        }
    }
    
    if ( bufp != NULL && bufp < end ) {
        *bufp = '\0';
        result = bufp - buf;
    }
    return result;
} /* dict_tilesToString */

/* dict_tileForString: used to map user keys to tiles in the tray.  Returns
 * EMPTY_TILE if no match found.
 */
Tile
dict_tileForString( const DictionaryCtxt* dict, const XP_UCHAR* key )
{
    XP_U16 nFaces = dict_numTileFaces( dict );
    Tile tile = EMPTY_TILE;
    XP_U16 ii;

    for ( ii = 0; ii < nFaces; ++ii ) {
        if ( ii != dict->blankTile ) {
            const XP_UCHAR* facep = dict_getTileString( dict, ii );
            if ( 0 == XP_STRCMP( facep, key ) ) {
                tile = (Tile)ii;
                break;
            }
        }
    }
    return tile;
} /* dict_tileForChar */

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
common_destructor( DictionaryCtxt* dict )
{
    freeSpecials( dict );

    XP_FREE( dict->mpool, dict->countsAndValues );
    XP_FREE( dict->mpool, dict->faces );
    XP_FREE( dict->mpool, dict->facePtrs );

    XP_FREE( dict->mpool, dict );
} /* common_destructor */

#ifndef XWFEATURE_STANDALONE_ONLY
void
dict_loadFromStream( DictionaryCtxt* dict, XWStreamCtxt* stream )
{
    XP_U8 nFaces, nFaceBytes;
    XP_U16 maxCountBits, maxValueBits;
    XP_U16 ii, nSpecials;
    XP_UCHAR* localTexts[32];
    XP_U8 utf8[MAX_UNIQUE_TILES];

    XP_ASSERT( !dict->destructor );
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
    dict_splitFaces( dict, utf8, nFaceBytes, nFaces );

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
#endif

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
dict_getWordCount( const DictionaryCtxt* dict )
{
    XP_U32 nWords = dict->nWords;
#ifdef XWFEATURE_WALKDICT
    nWords = dict_countWords( dict );
#endif
    return nWords;
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
#ifdef NODE_CAN_4
        /* avoid long-multiplication lib call on Palm... */
        if ( dict->nodeSize == 3 ) {
            index += (index << 1);
        } else {
            XP_ASSERT( dict->nodeSize == 4 );
            index <<= 2;
        }
#else
        index += (index << 1);
#endif
        result = &dict->base[index];
    }
    return result;
} /* dict_edge_for_index */

static array_edge*
dict_super_getTopEdge( const DictionaryCtxt* dict )
{
    return dict->topEdge;
} /* dict_super_getTopEdge */

static unsigned long
dict_super_index_from( const DictionaryCtxt* dict, array_edge* p_edge ) 
{
    unsigned long result;

#ifdef NODE_CAN_4
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
#else
    array_edge_old* edge = (array_edge_old*)p_edge;
    result = ((edge->highByte << 8) | edge->lowByte) & 0x0000FFFF;
    if ( (edge->bits & EXTRABITMASK_OLD) != 0 ) { 
        result |= 0x00010000; /* using | instead of + saves 4 bytes */
    }
#endif

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
#ifdef NODE_CAN_4
        from += dict->nodeSize;
#else
        from += 3;
#endif
    }

    return from;
} /* edge_with_tile */

void
dict_super_init( DictionaryCtxt* dict )
{
    /* subclass may change these later.... */
    dict->func_edge_for_index = dict_super_edge_for_index;
    dict->func_dict_getTopEdge = dict_super_getTopEdge;
    dict->func_dict_index_from = dict_super_index_from;
    dict->func_dict_follow = dict_super_follow;
    dict->func_dict_edge_with_tile = dict_super_edge_with_tile;
    dict->func_dict_getShortName = dict_getName;
} /* dict_super_init */

const XP_UCHAR* 
dict_getLangName( const DictionaryCtxt* ctxt )
{
    return ctxt->langName;
}

#ifdef XWFEATURE_WALKDICT
typedef struct _EdgeArray {
    array_edge* edges[MAX_COLS];
    XP_U16 nEdges;
} EdgeArray;

static void
edgesToIndices( const DictionaryCtxt* dict, const EdgeArray* edges, 
                DictWord* word )
{
    XP_U16 ii;

    word->nTiles = edges->nEdges;
    for ( ii = 0; ii < edges->nEdges; ++ii ) {
        word->indices[ii] = edges->edges[ii] - dict->base;
    }
}

static void
indicesToEdges( const DictionaryCtxt* dict, 
                const DictWord* word, EdgeArray* edges )
{
    XP_U16 nEdges = word->nTiles;
    XP_U16 ii;
    for ( ii = 0; ii < nEdges; ++ii ) {
        edges->edges[ii] = &dict->base[word->indices[ii]];
    }
    edges->nEdges = nEdges;
}

/* On entry and exit, edge at end of array should be ACCEPTING.  The job of
 * this function is to iterate from one such edge to the next.  Steps are: 1)
 * try to follow the edge, to expand to a longer word with the last one as a
 * prefix.  2) If we're at the end of the array, back off the top tile (and
 * repeat while at end of array); 3) Once the current top edge is not a
 * LAST_EDGE, try with its next-letter neighbor.
 */
static XP_Bool
nextWord( const DictionaryCtxt* dict, EdgeArray* edges )
{
    XP_U16 nTiles = edges->nEdges;
    XP_Bool success = XP_FALSE;
    while ( 0 < nTiles && ! success ) {
        array_edge* next = dict_follow( dict, edges->edges[nTiles-1] );
        if ( !!next ) {
            edges->edges[nTiles++] = next;
            success = ISACCEPTING( dict, next );
            continue;		/* try with longer word */
    	}

        while ( IS_LAST_EDGE( dict, edges->edges[nTiles-1] ) && 0 < --nTiles ) {
        }

        if ( 0 < nTiles ) {
            edges->edges[nTiles-1] += dict->nodeSize;
            success = ISACCEPTING( dict, edges->edges[nTiles-1] );
        }
    }

    edges->nEdges = nTiles;
    return success;
}

static XP_Bool
isFirstEdge( const DictionaryCtxt* dict, array_edge* edge )
{
    XP_Bool result = edge == dict->base; /* can't back up from first node */
    if ( !result ) {
        result = IS_LAST_EDGE( dict, edge - dict->nodeSize );
    }
    return result;
}

static XP_Bool
lastEdges( const DictionaryCtxt* dict, EdgeArray* edges )
{
    array_edge* edge = edges->edges[edges->nEdges-1];
    for ( ; ; ) {
        while ( !IS_LAST_EDGE( dict, edge ) ) {
            edge += dict->nodeSize;
        }
        edges->edges[edges->nEdges-1] = edge;

        edge = dict_follow( dict, edge );
        if ( NULL == edge ) {
            break;
        }
        ++edges->nEdges;
    }
    return ISACCEPTING( dict, edges->edges[edges->nEdges-1] );
}

static XP_Bool
prevWord( const DictionaryCtxt* dict, EdgeArray* edges )
{
    XP_Bool success = XP_FALSE;
    while ( 0 < edges->nEdges && ! success ) {
        if ( isFirstEdge( dict, edges->edges[edges->nEdges-1] ) ) {
            --edges->nEdges;
            success = 0 < edges->nEdges && ISACCEPTING( dict, edges->edges[edges->nEdges-1] );
            continue;
        }
        edges->edges[edges->nEdges-1] -= dict->nodeSize;
        array_edge* next = dict_follow( dict, edges->edges[edges->nEdges-1] );
        if ( NULL != next ) {
            edges->edges[edges->nEdges++] = next;
            success = lastEdges( dict, edges );
            if ( success ) {
                continue;
            }
        }
        success = ISACCEPTING( dict, edges->edges[edges->nEdges-1] );
    }
    return success;
}

typedef XP_Bool (*WordFinder)(const DictionaryCtxt* dict, EdgeArray* edges );

static XP_Bool
dict_getWord( const DictionaryCtxt* dict, DictWord* word, WordFinder finder )
{
    EdgeArray edges;
    indicesToEdges( dict, word, &edges );
    XP_Bool success = (*finder)( dict, &edges );
    if ( success ) {
        edgesToIndices( dict, &edges, word );
    }
    return success;
}

static XP_Bool
findStartsWith( const DictionaryCtxt* dict, Tile* tiles, XP_U16 nTiles, EdgeArray* edges )
{
    XP_Bool success = XP_TRUE;
    array_edge* edge = dict_getTopEdge( dict );

    while ( nTiles-- > 0 ) {
        Tile tile = *tiles++;
        edge = dict_edge_with_tile( dict, edge, tile );
        if ( NULL == edge ) {
            success = XP_FALSE;
            break;
        }
        edges->edges[edges->nEdges++] = edge;
        edge = dict_follow( dict, edge );
    }
    return success;
}

static XP_Bool
wordsEqual( EdgeArray* word1, EdgeArray* word2 )
{
    XP_Bool success = word1->nEdges == word2->nEdges;
    if ( success ) {
        success = 0 == memcmp( word1->edges, word2->edges,
                               word1->nEdges * sizeof(word1->edges[0]) );
    }
    return success;
}

XP_U32
dict_countWords( const DictionaryCtxt* dict )
{
    EdgeArray edges = { .nEdges = 0 };
    XP_U32 count = 0;
    edges.edges[edges.nEdges++] = dict_getTopEdge( dict );
    if ( ISACCEPTING( dict, edges.edges[0] ) ) {
        ++count;
    }
    while ( nextWord( dict, &edges ) ) {
        ++count;
    }
    return count;
}

static void
indexOne( const DictionaryCtxt* dict, XP_U16 depth, Tile* tiles, 
          DictIndex* indices, XP_U16* nextIndex, 
          XWStreamCtxt* stream, EdgeArray* prevEdges, DictIndex* prevIndex )
{
    EdgeArray curEdges = { .nEdges = 0 };
    if ( findStartsWith( dict, tiles, depth, &curEdges ) ) {
        XP_ASSERT( curEdges.nEdges == depth );
        if ( ! ISACCEPTING( dict, curEdges.edges[curEdges.nEdges-1] ) ) {
            if ( !nextWord( dict, &curEdges ) ) {
                XP_ASSERT( 0 );
            }
        }
        if ( wordsEqual( &curEdges, prevEdges ) ) {
            XP_ASSERT( *prevIndex == 0 );
        } else {
            /* Walk the prev word forward until they're the same */
            while ( nextWord( dict, prevEdges ) ) {
                ++*prevIndex;
                if ( wordsEqual( &curEdges, prevEdges ) ) {
                    break;
                }
            }
        }
        indices[(*nextIndex)++] = *prevIndex;

        if ( NULL != stream ) {
            XP_UCHAR prefix[8];
            (void)dict_tilesToString( dict, tiles, depth, prefix, VSIZE(prefix) );
            stream_catString( stream, prefix );
            stream_catString( stream, "\n" );
        }
    }
}

static void
doOneDepth( const DictionaryCtxt* dict, 
            const Tile* allTiles, XP_U16 nTiles, Tile* prefix, 
            XP_U16 curDepth, XP_U16 maxDepth,
            DictIndex* indices, XP_U16* nextEntry, 
            XWStreamCtxt* stream, EdgeArray* prevEdges, DictIndex* prevIndex )
{
    XP_U16 ii;
    for ( ii = 0; ii < nTiles; ++ii ) {
        prefix[curDepth] = allTiles[ii];
        if ( curDepth + 1 == maxDepth ) {
            indexOne( dict, maxDepth, prefix, indices, nextEntry, 
                      stream, prevEdges, prevIndex );
        } else {
            doOneDepth( dict, allTiles, nTiles, prefix, curDepth+1, maxDepth,
                        indices, nextEntry, stream, prevEdges, prevIndex );
        }
    }
}

XP_U16
dict_makeIndex( const DictionaryCtxt* dict, XP_U16 depth, 
                DictIndex* indices, XP_U16 count, XWStreamCtxt* stream )
{
    XP_ASSERT( depth < MAX_COLS );
    XP_U16 ii, needCount, nTiles;
    XP_U16 nFaces = dict_numTileFaces( dict );
    XP_Bool hasBlank = dict_hasBlankTile( dict );
    if ( hasBlank ) {
        --nFaces;
    }
    for ( ii = 1, needCount = nFaces; ii < depth; ++ii ) {
        needCount *= nFaces;
    }
    XP_ASSERT( needCount <= count );

    Tile allTiles[nFaces];
    nTiles = 0;
    for ( ii = 0; ii < nFaces; ++ii ) {
        if ( hasBlank && ii == dict_getBlankTile( dict ) ) {
            continue;
        }
        allTiles[nTiles++] = (Tile)ii;
    }

    /* For each tile string implied by depth (A if depth == 1, AAA if == 3 ),
     * find the first word starting with that IF EXISTS.  If it does, find its
     * index.  As an optimization, find index starting with the previous word.
     */
    XP_U16 nextIndex = 0;
    DictWord firstWord;
    if ( dict_firstWord( dict, &firstWord ) ) {
        EdgeArray prevEdges;
        DictIndex prevIndex = 0;
        Tile prefix[depth];
        indicesToEdges( dict, &firstWord, &prevEdges );

        doOneDepth( dict, allTiles, nFaces, prefix, 0, depth, 
                    indices, &nextIndex, stream, &prevEdges, &prevIndex );

    }
    return nextIndex;
}

XP_Bool
dict_firstWord( const DictionaryCtxt* dict, DictWord* word )
{
    EdgeArray edges = { .nEdges = 0 };
    edges.edges[edges.nEdges++] = dict_getTopEdge( dict );

    XP_Bool success = ISACCEPTING( dict, edges.edges[0] )
        || nextWord( dict, &edges );
    if ( success ) {
        word->wordCount = dict_getWordCount( dict );

        edgesToIndices( dict, &edges, word );
        word->index = 0;
    }

    return success;
}

XP_Bool
dict_getNextWord( const DictionaryCtxt* dict, DictWord* word )
{
    XP_Bool success = dict_getWord( dict, word, nextWord );
    if ( success ) {
        ++word->index;
    }
    return success;
}

XP_Bool
dict_lastWord( const DictionaryCtxt* dict, DictWord* word )
{
    EdgeArray edges = { .nEdges = 0 };
    edges.edges[edges.nEdges++] = dict_getTopEdge( dict );

    XP_Bool success = lastEdges( dict, &edges );
    if ( success ) {
        word->wordCount = dict_getWordCount( dict );

        edgesToIndices( dict, &edges, word );
        word->index = word->wordCount - 1;
    }

    return success;
}

XP_Bool
dict_getPrevWord( const DictionaryCtxt* dict, DictWord* word )
{
    XP_Bool success = dict_getWord( dict, word, prevWord );
    if ( success ) {
        --word->index;
    }
    return success;
}

/* If we start without an initialized word, init it to be closer to what's
   sought.  OR if we're father than necessary from what's sought, start over
   at the closer end.  Then move as many steps as necessary to reach it. */
XP_Bool
dict_getNthWord( const DictionaryCtxt* dict, DictWord* word, XP_U32 nn )
{
    XP_U32 wordCount;
    XP_Bool validWord = 0 < word->nTiles;
    XP_U32 ii;
    if ( validWord ) {        /* uninitialized */
        wordCount = word->wordCount;
    } else {
        wordCount = dict_getWordCount( dict );
    }
    XP_Bool success = nn < wordCount;
    if ( success ) {
        wordCount /= 2;             /* mid-point */

        /* If word's inited but farther from target than either endpoint,
           better to start with an endpoint */
        if ( validWord && XP_ABS( nn - word->index ) > wordCount ) {
            /* XP_LOGF( "%s: clearing word: nn=%ld; word->index=%ld", */
            /*          __func__, nn, word->index ); */
            validWord = XP_FALSE;
        }

        if ( !validWord ) {
            if ( nn >= wordCount ) {
                dict_lastWord( dict, word );
            } else {
                dict_firstWord( dict, word );
            }
        }

        EdgeArray edges;
        indicesToEdges( dict, word, &edges );
        if ( word->index < nn ) {
            for ( ii = nn - word->index; ii > 0; --ii ) {
                if ( !nextWord( dict, &edges ) ) {
                    XP_ASSERT( 0 );
                }
            }
        } else if ( word->index > nn ) {
            for ( ii = word->index - nn; ii > 0; --ii ) {
                if ( !prevWord( dict, &edges ) ) {
                    XP_ASSERT( 0 );
                }
            }
        }
        edgesToIndices( dict, &edges, word );
        word->index = nn;
    }
    return success;
}

void
dict_wordToString( const DictionaryCtxt* dict, const DictWord* word,
                   XP_UCHAR* buf, XP_U16 buflen )
{
    XP_U16 ii;
    const XP_U16 nTiles = word->nTiles;
    Tile tiles[MAX_COLS];
    EdgeArray edges;

    indicesToEdges( dict, word, &edges );

    for ( ii = 0; ii < nTiles; ++ii ) {
        tiles[ii] = EDGETILE( dict, edges.edges[ii] );
    }
    (void)dict_tilesToString( dict, tiles, nTiles, buf, buflen );
}
#endif /* XWFEATURE_WALKDICT */

#ifdef CPLUS
}
#endif
