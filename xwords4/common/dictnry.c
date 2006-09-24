/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */ 
/* 
 * Copyright 1997-2000 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#ifdef CPLUS
extern "C" {
#endif

/*****************************************************************************
 *
 ****************************************************************************/
void
setBlankTile( DictionaryCtxt* dctx ) 
{
    XP_U16 i;

    dctx->blankTile = -1;	/* no known blank */

    for ( i = 0; i < dctx->nFaces; ++i ) {
        if ( dctx->faces16[i] == 0 ) {
            dctx->blankTile = (XP_S8)i;
            XP_LOGF( "blank tile index: %d", i );
            break;
        }
    }    
} /* setBlankTile */

#if defined BLANKS_FIRST || defined DEBUG
XP_Bool
dict_hasBlankTile( const DictionaryCtxt* dict )
{
    return dict->blankTile >= 0;
} /* dict_hasBlankTile */
#endif

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

static XP_UCHAR
dict_getTileChar( const DictionaryCtxt* dict, Tile tile )
{
    XP_ASSERT( tile < dict->nFaces );
    XP_ASSERT( (dict->faces16[tile] & 0xFF00) == 0 ); /* no unicode yet */
    return (XP_UCHAR)dict->faces16[tile];
} /* dict_getTileValue */

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
dict_tilesToString( const DictionaryCtxt* ctxt, const Tile* tiles, 
                    XP_U16 nTiles, XP_UCHAR* buf, XP_U16 bufSize )
{
    XP_UCHAR* bufp = buf;
    XP_UCHAR* end = bufp + bufSize;
    XP_U16 result = 0;

    while ( nTiles-- ) {
        Tile tile = *tiles++;
        XP_UCHAR face = dict_getTileChar(ctxt, tile);

        if ( IS_SPECIAL(face) ) {
            XP_UCHAR* chars = ctxt->chars[(XP_U16)face];
            XP_U16 len = XP_STRLEN( chars );
            if ( bufp + len >= end ) {
                bufp = NULL;
                break;
            }
            XP_MEMCPY( bufp, chars, len );
            bufp += len;
        } else {
            XP_ASSERT ( tile != ctxt->blankTile ); /* printing blank should be
                                                      handled by specials
                                                      mechanism */
            if ( bufp + 1 >= end ) {
                bufp = NULL;
                break;
            }
            *bufp++ = face;
        }
    }
    
    if ( bufp != NULL && bufp < end ) {
        *bufp = '\0';
        result = bufp - buf;
    }
    return result;
} /* dict_tilesToString */

Tile
dict_tileForString( const DictionaryCtxt* dict, const XP_UCHAR* key )
{
    XP_U16 nFaces = dict_numTileFaces( dict );
    Tile tile;
    XP_Bool keyNotSpecial = XP_STRLEN((char*)key) == 1;

    for ( tile = 0; tile < nFaces; ++tile ) {
        XP_UCHAR face = dict_getTileChar( dict, tile );
        if ( IS_SPECIAL(face) ) {
            XP_UCHAR* chars = dict->chars[(XP_U16)face];
            if ( 0 == XP_STRNCMP( chars, key, XP_STRLEN(chars) ) ) {
                return tile;
            }
        } else if ( keyNotSpecial && (face == *key) ) {
            return tile;
        }
    }
    return EMPTY_TILE;
} /* dict_tileForChar */

XP_Bool
dict_tilesAreSame( const DictionaryCtxt* dict1, const DictionaryCtxt* dict2 )
{
    XP_Bool result = XP_FALSE;

    Tile i;
    XP_U16 nTileFaces = dict_numTileFaces( dict1 );

    if ( nTileFaces == dict_numTileFaces( dict2 ) ) {
        for ( i = 0; i < nTileFaces; ++i ) {

            XP_UCHAR face1, face2;

            if ( dict_getTileValue( dict1, i )
                 != dict_getTileValue( dict2, i ) ){
                break;
            }
#if 1
            face1 = dict_getTileChar( dict1, i );
            face2 = dict_getTileChar( dict2, i );
            if ( face1 != face2 ) {
                break;
            }
#else
            face1 = dict_getTileChar( dict1, i );
            face2 = dict_getTileChar( dict2, i );
            if ( IS_SPECIAL(face1) != IS_SPECIAL(face2) ) {
                break;
            }
            if ( IS_SPECIAL(face1) ) {
                XP_UCHAR* chars1 = dict1->chars[face1];
                XP_UCHAR* chars2 = dict2->chars[face2];
                XP_U16 len = XP_STRLEN(chars1);
                if ( 0 != XP_STRNCMP( chars1, chars2, len ) ) {
                    break;
                }
            } else if ( face1 != face2 ) {
                break;
            }
#endif
            if ( dict_numTiles( dict1, i ) != dict_numTiles( dict2, i ) ) {
                break;
            }
        }
        result = i == nTileFaces; /* did we get that far */
    }
    return result;
} /* dict_tilesAreSame */

void
dict_writeToStream( DictionaryCtxt* dict, XWStreamCtxt* stream )
{
    XP_U16 maxCount = 0;
    XP_U16 maxValue = 0;
    XP_U16 i, nSpecials;
    XP_U16 maxCountBits, maxValueBits;

    stream_putBits( stream, 6, dict->nFaces );

    for ( i = 0; i < dict->nFaces*2; i+=2 ) {
        XP_U16 count, value;

        count = dict->countsAndValues[i];
        if ( maxCount < count ) {
            maxCount = count;
        }

        value = dict->countsAndValues[i+1];
        if ( maxValue < value ) {
            maxValue = value;
        }
    }

    maxCountBits = bitsForMax( maxCount );
    maxValueBits = bitsForMax( maxValue );

    stream_putBits( stream, 3, maxCountBits ); /* won't be bigger than 5 */
    stream_putBits( stream, 3, maxValueBits );

    for ( i = 0; i < dict->nFaces*2; i+=2 ) {
        stream_putBits( stream, maxCountBits, dict->countsAndValues[i] );
        stream_putBits( stream, maxValueBits, dict->countsAndValues[i+1] );
    }

    for ( i = 0; i < dict->nFaces; ++i ) {
        stream_putU8( stream, (XP_U8)dict->faces16[i] );
    }

    for ( nSpecials = i = 0; i < dict->nFaces; ++i ) {
        XP_UCHAR face = dict_getTileChar( dict, (Tile)i );
        if ( IS_SPECIAL( face ) ) {
            stringToStream( stream, dict->chars[nSpecials++] );
        }
    }
} /* dict_writeToStream */

static void
freeSpecials( DictionaryCtxt* dict )
{
    Tile t;
    XP_U16 nSpecials;

    for ( nSpecials = t = 0; t < dict->nFaces; ++t ) {
        XP_UCHAR face = dict_getTileChar( dict, t );
        if ( IS_SPECIAL( face ) ) {

            XP_ASSERT( !!dict->chars[nSpecials] );
            XP_FREE( dict->mpool, dict->chars[nSpecials] );

            if ( !!dict->bitmaps[nSpecials].largeBM ) { 
                XP_FREE( dict->mpool, dict->bitmaps[nSpecials].largeBM );
            }
            if ( !!dict->bitmaps[nSpecials].smallBM ) { 
                XP_FREE( dict->mpool, dict->bitmaps[nSpecials].smallBM );
            }

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
    XP_FREE( dict->mpool, dict->faces16 );

    XP_FREE( dict->mpool, dict );
} /* dict */

void
dict_loadFromStream( DictionaryCtxt* dict, XWStreamCtxt* stream )
{
    XP_U8 nFaces;
    XP_U16 maxCountBits, maxValueBits;
    XP_U16 i, nSpecials;
    XP_UCHAR* localTexts[32];

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

    for ( i = 0; i < dict->nFaces*2; i+=2 ) {
        dict->countsAndValues[i] = (XP_U8)stream_getBits( stream, 
                                                          maxCountBits );
        dict->countsAndValues[i+1] = (XP_U8)stream_getBits( stream, 
                                                            maxValueBits );
    }

    dict->faces16 = (XP_CHAR16*)XP_MALLOC( dict->mpool, 
                                           sizeof(dict->faces16[0]) * nFaces );
    for ( i = 0; i < dict->nFaces; ++i ) {
        dict->faces16[i] = (XP_CHAR16)stream_getU8( stream );
    }

    for ( nSpecials = i = 0; i < nFaces; ++i ) {
        XP_UCHAR face = dict_getTileChar( dict, (Tile)i );
        if ( IS_SPECIAL( face ) ) {
            XP_UCHAR* txt = stringFromStream( MPPARM(dict->mpool) stream );
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

const XP_UCHAR*
dict_getName( const DictionaryCtxt* dict )
{
    XP_ASSERT( !!dict );
    XP_ASSERT( !!dict->name );
    return dict->name;
} /* dict_getName */

XP_Bool
dict_faceIsBitmap( const DictionaryCtxt* dict, Tile tile )
{
    XP_UCHAR face = dict_getTileChar( dict, tile );
    return /* face != 0 &&  */IS_SPECIAL(face);
} /* dict_faceIsBitmap */

XP_Bitmap
dict_getFaceBitmap( const DictionaryCtxt* dict, Tile tile, XP_Bool isLarge )
{
    SpecialBitmaps* bitmaps;
    XP_UCHAR face = dict_getTileChar( dict, tile );

    XP_ASSERT( dict_faceIsBitmap( dict, tile ) );
    XP_ASSERT( !!dict->bitmaps );

    bitmaps = &dict->bitmaps[(XP_U16)face];
    return isLarge? bitmaps->largeBM:bitmaps->smallBM;
} /* dict_getFaceBitmap */

#ifdef TALL_FONTS
XP_LangCode
dict_getLangCode( const DictionaryCtxt* dict )
{
    return dict->langCode;
}
#endif

#ifdef STUBBED_DICT

#define BLANK_FACE '\0'

static XP_U8 stub_english_data[] = {
    /* count            value           face */
    9,			1,		'A',
    2,			3,		'B',
    2,			3,		'C',
    4,			2,		'D',
    12,			1,		'E',
    2,			4,		'F',
    3,			2,		'G',
    2,			4,		'H',
    9,			1,		'I',
    1,			8,		'J',
    1,			5,		'K',
    4,			1,		'L',
    2,			3,		'M',
    6,			1,		'N',
    8,			1,		'O',
    2,			3,		'P',
    1,			10,		'Q',
    6,			1,		'R',
    4,			1,		'S',
    6,			1,		'T',
    4,			1,		'U',
    2,			4,		'V',
    2,			4,		'W',
    1,			8,		'X',
    2,			4,		'Y',
    1,			10,		'Z',
    2,			0,		BLANK_FACE, /* BLANK1 */
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
    XP_FREE( dict->mpool, dict->faces16 );
    XP_FREE( dict->mpool, dict->chars );
    XP_FREE( dict->mpool, dict->name );
    XP_FREE( dict->mpool, dict->bitmaps );
    XP_FREE( dict->mpool, dict );
} /* destroy_stubbed_dict */

DictionaryCtxt*
make_stubbed_dict( MPFORMAL_NOCOMMA )
{
    DictionaryCtxt* dict = (DictionaryCtxt*)XP_MALLOC( mpool, sizeof(*dict) );
    XP_U8* data = stub_english_data;
    XP_U16 datasize = sizeof(stub_english_data);
    XP_U16 i;

    XP_MEMSET( dict, 0, sizeof(*dict) );

    MPASSIGN( dict->mpool, mpool );
    dict->name = copyString( mpool, "Stub dictionary" );
    dict->nFaces = datasize/3;

    dict->destructor = destroy_stubbed_dict;

    dict->faces16 = (XP_CHAR16*)
        XP_MALLOC( mpool, dict->nFaces * sizeof(dict->faces16[0]) );
    for ( i = 0; i < datasize/3; ++i ) {
        dict->faces16[i] = (XP_CHAR16)data[(i*3)+2];
    }
    
    dict->countsAndValues = (XP_U8*)XP_MALLOC( mpool, dict->nFaces*2 );
    for ( i = 0; i < datasize/3; ++i ) {
        dict->countsAndValues[i*2] = data[(i*3)];
        dict->countsAndValues[(i*2)+1] = data[(i*3)+1];
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

void
dict_super_init( DictionaryCtxt* ctxt )
{
    /* subclass may change these later.... */
    ctxt->func_edge_for_index = dict_super_edge_for_index;
    ctxt->func_dict_getTopEdge = dict_super_getTopEdge;
    ctxt->func_dict_getShortName = dict_getName;
} /* dict_super_init */

#ifdef CPLUS
}
#endif
