/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 1997 - 2000 by Eric House (fixin@peak.org).  All rights reserved.
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
#define IS_SPECIAL(face) (((XP_CHAR16)(face)) < 0x0020)

typedef XP_U16 XP_CHAR16;

typedef enum {
    BONUS_NONE,
    BONUS_DOUBLE_LETTER,
    BONUS_DOUBLE_WORD,
    BONUS_TRIPLE_LETTER,
    BONUS_TRIPLE_WORD,

    BONUS_LAST
} XWBonusType;

typedef enum {
    INTRADE_MW_TEXT = BONUS_LAST
} XWMiniTextType;

typedef struct SpecialBitmaps {
    XP_Bitmap largeBM;
    XP_Bitmap smallBM;
} SpecialBitmaps;


struct DictionaryCtxt {
    void (*destructor)( DictionaryCtxt* dict );
    array_edge* topEdge;
    array_edge* base;		/* the physical beginning of the dictionary; not
                               necessarily the entry point for search!! */
    XP_UCHAR* name;
    XP_CHAR16* faces16;          /* 16 for unicode */
    XP_U8* countsAndValues;

    SpecialBitmaps* bitmaps;
    XP_UCHAR** chars;

    XP_U8 nFaces;
#ifdef NODE_CAN_4
    XP_U8 nodeSize;
    XP_Bool is_4_byte;
#endif

    XP_S8 blankTile;		/* negative means there's no known blank */
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
/* 				   XP_FontCode* fontCode ); */
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

XP_Bool dict_tilesAreSame( DictionaryCtxt* dict1, DictionaryCtxt* dict2 );

XP_Bool dict_hasBlankTile( DictionaryCtxt* dict );
Tile dict_getBlankTile( DictionaryCtxt* dict );
XP_U16 dict_getTileValue( DictionaryCtxt* ctxt, Tile tile );
XP_U16 dict_numTiles( DictionaryCtxt* ctxt, Tile tile );
XP_U16 dict_numTileFaces( DictionaryCtxt* ctxt );

XP_U16 dict_tilesToString( DictionaryCtxt* ctxt, Tile* tiles, XP_U16 nTiles,
                           XP_UCHAR* buf );
XP_UCHAR* dict_getName( DictionaryCtxt* ctxt );

Tile dict_tileForString( DictionaryCtxt* dict, XP_UCHAR* key );

XP_Bool dict_faceIsBitmap( DictionaryCtxt* dict, Tile tile );
XP_Bitmap dict_getFaceBitmap( DictionaryCtxt* dict, Tile tile, 
                              XP_Bool isLarge );

void dict_writeToStream( DictionaryCtxt* ctxt, XWStreamCtxt* stream );
void dict_loadFromStream( DictionaryCtxt* dict, XWStreamCtxt* stream );


/* These methods get "overridden" by subclasses.  That is, they must be
 implemented by each platform. */

array_edge* dict_edge_for_index( DictionaryCtxt* dict, XP_U32 index );

#ifdef OVERRIDE_GETTOPEDGE
/* platform code will implement this */
array_edge* dict_getTopEdge( DictionaryCtxt* dict );
#else
# define dict_getTopEdge( dict ) ((dict)->topEdge)
#endif


#ifdef STUBBED_DICT
DictionaryCtxt* make_stubbed_dict( MPFORMAL_NOCOMMA );
#endif

#ifdef CPLUS
}
#endif

#endif
