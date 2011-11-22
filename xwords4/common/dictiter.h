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

#define MAX_COLS_DICT 15

/* API for iterating over a dict */
typedef XP_S32 DictPosition;
typedef struct _DictIter {
    XP_U16 nEdges;
    array_edge* edges[MAX_COLS_DICT];
#ifdef XWFEATURE_WALKDICT_FILTER
    XP_U16 min;
    XP_U16 max;
#endif
#ifdef DEBUG
    XP_U32 guard;
#endif
    const DictionaryCtxt* dict;
    XP_U32 nWords;

    DictPosition position;
} DictIter;

typedef struct _IndexData {
    DictPosition* indices;
    Tile* prefixes;
    XP_U16 count;    /* in-out: must indicate others are large enough */
} IndexData;

typedef struct _LengthsArray {
    XP_U32 lens[MAX_COLS_DICT+1];
} LengthsArray;

void dict_initIter( DictIter* iter, const DictionaryCtxt* dict, 
                    XP_U16 min, XP_U16 max );
XP_U32 dict_countWords( const DictIter* iter, LengthsArray* lens );
void dict_makeIndex( const DictIter* iter, XP_U16 depth, IndexData* data );
XP_Bool dict_firstWord( DictIter* iter );
XP_Bool dict_lastWord( DictIter* iter );
XP_Bool dict_getNextWord( DictIter* iter );
XP_Bool dict_getPrevWord( DictIter* iter );
XP_Bool dict_getNthWord( DictIter* iter, DictPosition position, XP_U16 depth, 
                         const IndexData* data );
void dict_wordToString( const DictIter* iter, XP_UCHAR* buf, XP_U16 buflen );
XP_Bool dict_findStartsWith( DictIter* iter, const IndexData* data, 
                             const Tile* prefix, XP_U16 len );
DictPosition dict_getPosition( const DictIter* iter );
#ifdef CPLUS
}
#endif

#endif  /* XWFEATURE_WALKDICT */
#endif
