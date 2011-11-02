/* -*- compile-command: "cd ../linux && make MEMDEBUG=TRUE -j3"; -*- */
/* 
 * Copyright 1997 - 2011 by Eric House (xwords@eehouse.org).  All rights
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

#ifndef __DICTITER_H__
#define __DICTITER_H__

#ifdef XWFEATURE_WALKDICT

#include "comtypes.h"

#include "dawg.h"
#include "model.h"
#include "mempool.h"

#ifdef CPLUS
extern "C" {
#endif


/* API for iterating over a dict */
typedef XP_S32 DictPosition;
typedef struct _DictWord {
    XP_U32 wordCount;
    DictPosition position;
    XP_U16 nTiles;
    XP_U32 indices[MAX_COLS];
} DictWord;

typedef struct _IndexData {
    DictPosition* indices;
    Tile* prefixes;
    XP_U16 count;    /* in-out: must indicate others are large enough */
} IndexData;

XP_U32 dict_countWords( const DictionaryCtxt* dict );
void dict_makeIndex( const DictionaryCtxt* dict, XP_U16 depth, 
                     IndexData* data );
XP_Bool dict_firstWord( const DictionaryCtxt* dict, DictWord* word );
XP_Bool dict_lastWord( const DictionaryCtxt* dict, DictWord* word );
XP_Bool dict_getNextWord( const DictionaryCtxt* dict, DictWord* word );
XP_Bool dict_getPrevWord( const DictionaryCtxt* dict, DictWord* word );
XP_Bool dict_getNthWord( const DictionaryCtxt* dict, DictWord* word,
                         DictPosition position, XP_U16 depth, 
                         const IndexData* data );
void dict_wordToString( const DictionaryCtxt* dict, const DictWord* word,
                        XP_UCHAR* buf, XP_U16 buflen );
DictPosition dict_getStartsWith( const DictionaryCtxt* dict, 
                                 const IndexData* data, 
                                 Tile* prefix, XP_U16 len );
#ifdef CPLUS
}
#endif

#endif  /* XWFEATURE_WALKDICT */
#endif
