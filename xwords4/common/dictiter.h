/* 
 * Copyright 1997 - 2020 by Eric House (xwords@eehouse.org).  All rights
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

typedef struct _IndexData {
    DictPosition* indices;
    Tile* prefixes;
    XP_U16 count;    /* in-out: must indicate others are large enough */
} IndexData;

typedef struct _LengthsArray {
    XP_U32 lens[MAX_COLS_DICT+1];
} LengthsArray;

typedef struct DictIter DictIter;

/* A pattern is a list of Tiles (that must be contained in the wordlist) plus
 * regex meta-characters. Tiles are optionally delimited by '.' (to deal with
 * the Hungarian case) Supported are:
 *
 * tile sets (e.g. [A.B.C]); also [^A.B.C] means NOT these, and [+A.B.C] means
 * use at most once [+A.B.C]{3} would mean "use all of them once each"
 *
 * '_' (meaning blank/anything, same as '.' in most regex languages),
 *
 * '*' meaning 0 or more of what's before
 *
 * '+' meaning 1 or more of what's before
 *
 * '{m[,n]}' meaning between m and n of what's before, so _{2,15} matches
 * everything
 *
 * '()' (not implemented) also required to express word length: (AB_*CD){2,15}
 * is "an word beginning with AB and ending with CD from 2 to 15 letters long.
 */

/* di_makeIter: It's either-or: provide the pattern as a reg-ex string, or as
 * an array of three tile-strings representing starts-with, contains, and
 * ends-with. The first is more powerful but I'm not sure it'll ever be part
 * of a shipping UI.*/
typedef struct _PatDesc {
    Tile tiles[MAX_COLS_DICT];
    XP_U16 nTiles;
    XP_Bool anyOrderOk;
} PatDesc;

typedef struct _DIMinMax {
    XP_U16 min;
    XP_U16 max;
} DIMinMax;

DictIter* di_makeIter( const DictionaryCtxt* dict, XWEnv xwe,
                       const DIMinMax* lens, /* NULL means use defaults */
                       const XP_UCHAR** strPats, XP_U16 nStrPats,
                       const PatDesc* pats, XP_U16 nPatDescs );
void di_freeIter( DictIter* iter, XWEnv xwe );

#ifdef XWFEATURE_TESTPATSTR
XP_Bool di_stringMatches( DictIter* iter, const XP_UCHAR* string );
#endif

XP_U32 di_countWords( const DictIter* iter, LengthsArray* lens );
void di_makeIndex( const DictIter* iter, XP_U16 depth, IndexData* data );
XP_Bool di_firstWord( DictIter* iter );
XP_Bool di_lastWord( DictIter* iter );
XP_Bool di_getNextWord( DictIter* iter );
XP_Bool di_getPrevWord( DictIter* iter );
XP_Bool di_getNthWord( DictIter* iter, XWEnv xwe, DictPosition position, XP_U16 depth,
                       const IndexData* data );
XP_U32 di_getNWords( const DictIter* iter );
void di_getMinMax( const DictIter* iter, XP_U16* min, XP_U16* max );

void di_wordToString( const DictIter* iter, XP_UCHAR* buf, XP_U16 buflen,
                      const XP_UCHAR* delim );
void di_stringToTiles( const XP_UCHAR* str, Tile out[], XP_U16* nTiles );
DictPosition di_getPosition( const DictIter* iter );
#ifdef CPLUS
}
#endif

#endif  /* XWFEATURE_WALKDICT */
#endif
