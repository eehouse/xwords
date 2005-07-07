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

#ifdef TALL_FONTS
    /* This guy's used to vertically center glyphs within cells.  The impetus
       is the circle-A char in Danish which starts higher than most and must
       be drawn (on Palm) starting at a smaller y value if the bottom is to be
       visible.  */
typedef struct XP_FontBounds {
    XP_S16 topOffset;           /* how many pixels from the top of the drawing
                                   area is the first pixel set in the glyph */
    XP_U16 height;              /* How many rows tall is the image? */
} XP_FontBounds;

typedef XP_U16 XP_LangCode;     /* corresponds to the XLOC_HEADER field in
                                   dawg/./info.txt files */
#endif

struct DictionaryCtxt {
    void (*destructor)( DictionaryCtxt* dict );

    array_edge* (*func_edge_for_index)( DictionaryCtxt* dict, XP_U32 index );
    array_edge* (*func_dict_getTopEdge)( DictionaryCtxt* dict );
    XP_UCHAR* (*func_dict_getShortName)( DictionaryCtxt* dict );
#ifdef TALL_FONTS
    void (*func_dict_getFaceBounds)( DictionaryCtxt* dict, Tile tile,
                                     XP_FontBounds* bounds );
#endif

    array_edge* topEdge;
    array_edge* base;		/* the physical beginning of the dictionary; not
                               necessarily the entry point for search!! */
    XP_UCHAR* name;
    XP_CHAR16* faces16;          /* 16 for unicode */
    XP_U8* countsAndValues;

    SpecialBitmaps* bitmaps;
    XP_UCHAR** chars;

#ifdef TALL_FONTS
    XP_LangCode langCode;
#endif

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
#define dict_edge_for_index(d, i) (*((d)->func_edge_for_index))((d), (i))
#define dict_getTopEdge(d)        (*((d)->func_dict_getTopEdge))(d)
#define dict_getShortName(d)      (*((d)->func_dict_getShortName))(d)
#define dict_getFaceBounds(d,n,p) (*((d)->func_dict_getFaceBounds))((d),(n),(p))

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

#ifdef TALL_FONTS
XP_LangCode dict_getLangCode( DictionaryCtxt* dict );
#endif

void dict_writeToStream( DictionaryCtxt* ctxt, XWStreamCtxt* stream );
void dict_loadFromStream( DictionaryCtxt* dict, XWStreamCtxt* stream );


/* These methods get "overridden" by subclasses.  That is, they must be
 implemented by each platform. */

#ifdef STUBBED_DICT
DictionaryCtxt* make_stubbed_dict( MPFORMAL_NOCOMMA );
#endif

/* To be called only by subclasses!!! */
void dict_super_init( DictionaryCtxt* ctxt );


#ifdef CPLUS
}
#endif

#endif
