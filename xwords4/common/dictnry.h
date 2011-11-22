/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 1997 - 2009 by Eric House (xwords@eehouse.org).  All rights
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

#ifndef __DICTNRY_H__
#define __DICTNRY_H__

#include "comtypes.h"

#include "dawg.h"
#include "model.h"
#include "mempool.h"

#ifdef CPLUS
extern "C" {
#endif

#define LETTER_NONE '\0'
/* cast to unsigned in case XP_UCHAR is signed */
#define IS_SPECIAL(face) ((XP_U16)(face) < 0x0020)

#define DICT_HEADER_MASK 0x08

typedef XP_U8 XP_LangCode;

typedef enum {
    INTRADE_MW_TEXT = BONUS_LAST
} XWMiniTextType;

typedef struct SpecialBitmaps {
    XP_Bitmap largeBM;
    XP_Bitmap smallBM;
} SpecialBitmaps;

typedef struct _XP_Bitmaps {
    XP_U16 nBitmaps;
    XP_Bitmap bmps[2];      /* 2 is private, may change */
} XP_Bitmaps;

struct DictionaryCtxt {
    void (*destructor)( DictionaryCtxt* dict );

    array_edge* (*func_edge_for_index)( const DictionaryCtxt* dict, 
                                        XP_U32 index );
    array_edge* (*func_dict_getTopEdge)( const DictionaryCtxt* dict );
    unsigned long (*func_dict_index_from)( const DictionaryCtxt* dict, 
                                           array_edge* p_edge );
    array_edge* (*func_dict_follow)( const DictionaryCtxt* dict, 
                                     array_edge* in );
    array_edge* (*func_dict_edge_with_tile)( const DictionaryCtxt* dict, 
                                             array_edge* from, Tile tile );
    const XP_UCHAR* (*func_dict_getShortName)( const DictionaryCtxt* dict );

    array_edge* topEdge;
    array_edge* base; /* the physical beginning of the dictionary; not
                         necessarily the entry point for search!! */
    XP_UCHAR* name;
    XP_UCHAR* langName;
    XP_UCHAR* faces;
    const XP_UCHAR** facePtrs;
    XP_U8* countsAndValues;

    SpecialBitmaps* bitmaps;
    XP_UCHAR** chars;
    XP_U32 nWords;

    XP_LangCode langCode;

    XP_U8 nFaces;
#ifdef NODE_CAN_4
    XP_U8 nodeSize;
    XP_Bool is_4_byte;
#endif

    XP_S8 blankTile; /* negative means there's no known blank */
    XP_Bool isUTF8;
#ifdef DEBUG
    XP_U32 numEdges;
#endif
    MPSLOT
};

/* This is the datastructure that allows access to a DAWG in a
 * platform-independent way.
 */
/* typedef struct DictionaryVtable { */
/*     XP_U16 (*m_getTileValue)( DictionaryCtxt* ctxt, CellTile tile ); */
/*     unsigned char (*m_getTileChar)( DictionaryCtxt* ctxt, CellTile tile, */
/*    XP_FontCode* fontCode ); */
/*     XP_U16 (*m_numTiles)( DictionaryCtxt* ctxt, Tile tile ); */
/*     XP_U16 (*m_numTileFaces)( DictionaryCtxt* ctxt ); */
/* } DictionaryVtable; */


/* struct DictionaryCtxt { */
/*     DictionaryVtable* vtable; */
/* }; */

/* #define dict_getTileValue(dc,t) \ */
/*          (dc)->vtable->m_getTileValue((dc),(t)) */

/* #define dict_getTileChar(dc,t,fc) \ */
/*          (dc)->vtable->m_getTileChar((dc),(t),(fc)) */

/* #define dict_numTiles(dc,t) (dc)->vtable->m_numTiles((dc),(t)) */

/* #define dict_numTileFaces(dc) (dc)->vtable->m_numTileFaces(dc) */

#define dict_destroy(d) (*((d)->destructor))(d)
#define dict_edge_for_index(d, i) (*((d)->func_edge_for_index))((d), (i))
#define dict_getTopEdge(d)        (*((d)->func_dict_getTopEdge))(d)
#define dict_index_from(d,e)        (*((d)->func_dict_index_from))(d,e)
#define dict_follow(d,e)        (*((d)->func_dict_follow))(d,e)
#define dict_edge_with_tile(d,e,t) (*((d)->func_dict_edge_with_tile))(d,e,t)
#define dict_getShortName(d)      (*((d)->func_dict_getShortName))(d)

#ifdef NODE_CAN_4
# define ISACCEPTING(d,e) \
    ((ACCEPTINGMASK_NEW & ((array_edge_old*)(e))->bits) != 0)
# define IS_LAST_EDGE(d,e) \
    ((LASTEDGEMASK_NEW & ((array_edge_old*)(e))->bits) != 0)
# define EDGETILE(d,edge) \
    ((Tile)(((array_edge_old*)(edge))->bits & \
            ((d)->is_4_byte?LETTERMASK_NEW_4:LETTERMASK_NEW_3)))
#else
# define ISACCEPTING(d,e) \
    ((ACCEPTINGMASK_OLD & ((array_edge_old*)(e))->bits) != 0)
# define IS_LAST_EDGE(d,e) \
    ((LASTEDGEMASK_OLD & ((array_edge_old*)(e))->bits) != 0)
# define EDGETILE(d,edge) \
    ((Tile)(((array_edge_old*)(edge))->bits & LETTERMASK_OLD))
#endif

XP_Bool dict_tilesAreSame( const DictionaryCtxt* dict1, 
                           const DictionaryCtxt* dict2 );

XP_Bool dict_hasBlankTile( const DictionaryCtxt* dict );
Tile dict_getBlankTile( const DictionaryCtxt* dict );
XP_U16 dict_getTileValue( const DictionaryCtxt* ctxt, Tile tile );
XP_U16 dict_numTiles( const DictionaryCtxt* ctxt, Tile tile );
XP_U16 dict_numTileFaces( const DictionaryCtxt* ctxt );

XP_U16 dict_tilesToString( const DictionaryCtxt* ctxt, const Tile* tiles, 
                           XP_U16 nTiles, XP_UCHAR* buf, XP_U16 bufSize );
const XP_UCHAR* dict_getTileString( const DictionaryCtxt* ctxt, Tile tile );
const XP_UCHAR* dict_getName( const DictionaryCtxt* ctxt );
const XP_UCHAR* dict_getLangName(const DictionaryCtxt* ctxt );

XP_Bool dict_isUTF8( const DictionaryCtxt* ctxt );

XP_Bool dict_tilesForString( const DictionaryCtxt* dict, const XP_UCHAR* key,
                             Tile* tiles, XP_U16* nTiles );

XP_Bool dict_faceIsBitmap( const DictionaryCtxt* dict, Tile tile );
void dict_getFaceBitmaps( const DictionaryCtxt* dict, Tile tile, 
                          XP_Bitmaps* bmps );

XP_LangCode dict_getLangCode( const DictionaryCtxt* dict );
XP_U32 dict_getWordCount( const DictionaryCtxt* dict );

void dict_writeToStream( const DictionaryCtxt* ctxt, XWStreamCtxt* stream );
void dict_loadFromStream( DictionaryCtxt* dict, XWStreamCtxt* stream );

#ifdef TEXT_MODEL
/* Return the strlen of the longest face, e.g. 1 for English and Italian;
   2 for Spanish; 3 for Catalan */
XP_U16 dict_getMaxWidth( const DictionaryCtxt* dict );
#endif


/* These methods get "overridden" by subclasses.  That is, they must be
 implemented by each platform. */

#ifdef STUBBED_DICT
DictionaryCtxt* make_stubbed_dict( MPFORMAL_NOCOMMA );
#endif

/* To be called only by subclasses!!! */
void dict_super_init( DictionaryCtxt* ctxt );
/* Must be implemented by subclass */
void dict_splitFaces( DictionaryCtxt* dict, const XP_U8* bytes, 
                      XP_U16 nBytes, XP_U16 nFaces );

XP_Bool checkSanity( DictionaryCtxt* dict, XP_U32 numEdges );

#ifdef CPLUS
}
#endif

#endif
