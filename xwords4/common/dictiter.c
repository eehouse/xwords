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

#ifdef XWFEATURE_WALKDICT

#ifdef USE_STDIO
# include <stdio.h>
# include <stdlib.h>
#endif

#include "comtypes.h"
#include "dictnryp.h"
#include "xwstream.h"
#include "strutils.h"
#include "dictnry.h"
#include "dictiter.h"
#include "game.h"

#ifdef CPLUS
extern "C" {
#endif

typedef struct _EdgeArray {
    array_edge* edges[MAX_COLS_DICT];
    XP_U16 nEdges;
} EdgeArray;

static XP_Bool prevWord( DictIter* iter );

#ifdef XWFEATURE_WALKDICT_FILTER
#define LENOK( iter, nEdges )                                              \
    (iter)->min <= (nEdges) && (nEdges) <= (iter)->max

static XP_Bool
_isAccepting( DictIter* iter, XP_U16 nEdges )
{
    return ISACCEPTING( iter->dict, iter->edges[nEdges-1] )
        && LENOK( iter, nEdges );
}
# define ACCEPT_ITER( iter, nEdges) _isAccepting( iter, nEdges )
# define ACCEPT_NODE( iter, node, nEdges )                      \
    ISACCEPTING( iter->dict, node ) && LENOK(iter,nEdges)
# define FILTER_TEST(iter,nEdges) ((nEdges) <= (iter)->max)
#else
# define ACCEPT_ITER(iter, nEdges)                                        \
    ISACCEPTING( (iter)->dict, (iter)->edges[(nEdges)-1] )
# define ACCEPT_NODE( iter, node, nEdges ) ISACCEPTING( iter->dict, node )
# define FILTER_TEST(iter, nEdges) XP_TRUE
#endif

/* On entry and exit, edge at end of array should be ACCEPTING.  The job of
 * this function is to iterate from one such edge to the next.  Steps are: 1)
 * try to follow the edge, to expand to a longer word with the last one as a
 * prefix.  2) If we're at the end of the array, back off the top tile (and
 * repeat while at end of array); 3) Once the current top edge is not a
 * LAST_EDGE, try with its next-letter neighbor.
 */
static XP_Bool
nextWord( DictIter* iter )
{
    const DictionaryCtxt* dict = iter->dict;
    XP_Bool success = XP_FALSE;
    XP_U16 nEdges = iter->nEdges;
    while ( 0 < nEdges && ! success ) {
        if ( FILTER_TEST( iter, nEdges ) ) {
            array_edge* next = dict_follow( dict, iter->edges[nEdges-1] );
            if ( !!next ) {
                iter->edges[nEdges++] = next;
                success = ACCEPT_NODE( iter, next, nEdges );
                continue;		/* try with longer word */
            }
        }

        while ( IS_LAST_EDGE( dict, iter->edges[nEdges-1] )
                && 0 < --nEdges ) {
        }

        if ( 0 < nEdges ) {
            iter->edges[nEdges-1] += dict->nodeSize;
            success = ACCEPT_NODE( iter, iter->edges[nEdges-1], nEdges );
        }
    }
    iter->nEdges = nEdges;
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
lastEdges( DictIter* iter, XP_U16* nEdgesP )
{
    const DictionaryCtxt* dict = iter->dict;
    XP_U16 nEdges = *nEdgesP;
    array_edge* edge = iter->edges[nEdges-1];
    for ( ; ; ) {
        while ( !IS_LAST_EDGE( dict, edge ) ) {
            edge += dict->nodeSize;
        }
        iter->edges[nEdges-1] = edge;

        edge = dict_follow( dict, edge );
        if ( NULL == edge ) {
            break;
        }
        if ( !FILTER_TEST( iter, nEdges + 1 ) ) {
            break;
        }
        ++nEdges;
    }
    *nEdgesP = nEdges;
    return ACCEPT_ITER( iter, nEdges );
}

static XP_Bool
prevWord( DictIter* iter )
{
    const DictionaryCtxt* dict = iter->dict;
    XP_U16 nEdges = iter->nEdges;
    XP_Bool success = XP_FALSE;
    while ( 0 < nEdges && ! success ) {
        if ( isFirstEdge( dict, iter->edges[nEdges-1] ) ) {
            --nEdges;
            success = 0 < nEdges 
                && ACCEPT_NODE( iter, iter->edges[nEdges-1], nEdges );
            continue;
        }

        iter->edges[nEdges-1] -= dict->nodeSize;
        
        if ( FILTER_TEST( iter, nEdges ) ) {
            array_edge* next = dict_follow( dict, iter->edges[nEdges-1] );
            if ( NULL != next ) {
                iter->edges[nEdges++] = next;
                success = lastEdges( iter, &nEdges );
                if ( success ) {
                    continue;
                }
            }
        }

        success = ACCEPT_NODE( iter, iter->edges[nEdges-1], nEdges );
    }
    iter->nEdges = nEdges;
    return success;
}

static XP_Bool
findStartsWith( DictIter* iter, const Tile* tiles, XP_U16 nTiles )
{
    const DictionaryCtxt* dict = iter->dict;
    array_edge* edge = dict_getTopEdge( dict );
    iter->nEdges = 0;

    while ( FILTER_TEST( iter, iter->nEdges ) && nTiles > 0 ) {
        Tile tile = *tiles++;
        edge = dict_edge_with_tile( dict, edge, tile );
        if ( NULL == edge ) {
            break;
        }
        iter->edges[iter->nEdges++] = edge;
        edge = dict_follow( dict, edge );
        --nTiles;
    }
    return 0 == nTiles;
}

static XP_Bool
startsWith( const DictIter* iter, const Tile* tiles, XP_U16 nTiles )
{
    XP_Bool success = nTiles <= iter->nEdges;
    while ( success && nTiles-- ) {
        success = tiles[nTiles] == EDGETILE( iter->dict, iter->edges[nTiles] );
    }
    return success;
}

static XP_Bool
findWordStartsWith( DictIter* iter, const Tile* tiles, XP_U16 nTiles )
{
    XP_Bool found = XP_FALSE;
    if ( findStartsWith( iter, tiles, nTiles ) ) {
        found = ACCEPT_ITER( iter, iter->nEdges );
        if ( !found ) {
            found = nextWord( iter ) && startsWith( iter, tiles, nTiles );
        }
    }
    return found;
}

static XP_Bool
wordsEqual( const DictIter* word1, const DictIter* word2 )
{
    XP_Bool success = word1->nEdges == word2->nEdges;
    if ( success ) {
        success = 0 == memcmp( word1->edges, word2->edges,
                               word1->nEdges * sizeof(word1->edges[0]) );
    }
    return success;
}

static void 
dict_initIterFrom( DictIter* dest, const DictIter* src )
{
    dict_initIter( dest, src->dict,
#ifdef XWFEATURE_WALKDICT_FILTER
                   src->min, src->max 
#else
                   0, 0
#endif
                   );
}

static XP_Bool
firstWord( DictIter* iter )
{
    array_edge* top = dict_getTopEdge( iter->dict );
    XP_Bool success = !!top;
    if ( success ) {
        iter->nEdges = 1;
        iter->edges[0] = top;
        success = ACCEPT_ITER( iter, 1 ) || nextWord( iter );
    }
    return success;
}

XP_U32
dict_countWords( const DictIter* iter, LengthsArray* lens )
{
    DictIter counter;
    dict_initIterFrom( &counter, iter );

    if ( NULL != lens ) {
        XP_MEMSET( lens, 0, sizeof(*lens) );
    }

    XP_U32 count;
    XP_Bool ok;
    for ( count = 0, ok = firstWord( &counter ); 
          ok; ok = nextWord( &counter) ) {
        ++count;

        if ( NULL != lens ) {
            ++lens->lens[counter.nEdges];
        }
    }
    return count;
}

#define GUARD_VALUE 0x12345678
#define ASSERT_INITED( iter ) XP_ASSERT( (iter)->guard == GUARD_VALUE )

void 
dict_initIter( DictIter* iter, const DictionaryCtxt* dict,
               XP_U16 min, XP_U16 max )
{
    XP_MEMSET( iter, 0, sizeof(*iter) );
    iter->dict = dict;
#ifdef DEBUG
    iter->guard = GUARD_VALUE;
#endif
#ifdef XWFEATURE_WALKDICT_FILTER
    iter->min = min;
    iter->max = max;
#else
    XP_USE( min );
    XP_USE( max );
#endif
}

static void
copyIter( DictIter* dest, const DictIter* src )
{
    XP_U16 nEdges = src->nEdges;
    dest->nEdges = nEdges;
    XP_MEMCPY( dest->edges, src->edges, nEdges * sizeof(dest->edges[0]) );
}

static DictPosition
placeWordClose( DictIter* iter, const DictPosition position, XP_U16 depth,
                const IndexData* data )
{
    XP_S16 low = 0;
    XP_S16 high = data->count - 1;
    XP_S16 index = -1;
    for ( ; ; ) {
        if ( low > high ) {
            break;
        }
        index = low + ( (high - low) / 2);
        if ( position < data->indices[index] ) {
            high = index - 1;
        } else if ( data->indices[index+1] <= position) {
            low = index + 1;
        } else {
            break;
        }
    }

    /* Now we have the index immediately below the position we want.  But we
       may be better off starting with the next if it's closer.  The last
       index is a special case since we use lastWord rather than a prefix to
       init */
    if ( ( index + 1 < data->count ) 
         && (data->indices[index + 1] - position) 
         < (position - data->indices[index]) ) {
        ++index;
    }
    if ( !findWordStartsWith( iter, &data->prefixes[depth*index], depth ) ) {
        XP_ASSERT(0);
    }
    return data->indices[index];
} /* placeWordClose */

static void
iterToString( const DictIter* iter, XP_UCHAR* buf, XP_U16 buflen )
{
    XP_U16 ii;
    XP_U16 nEdges = iter->nEdges;
    Tile tiles[nEdges];
    for ( ii = 0; ii < nEdges; ++ii ) {
        tiles[ii] = EDGETILE( iter->dict, iter->edges[ii] );
    }
    (void)dict_tilesToString( iter->dict, tiles, nEdges, buf, buflen );
}

#if 0
static void
printEdges( DictIter* iter, char* comment )
{
    XP_UCHAR buf[32];
    iterToString( dict, edges, buf, VSIZE(buf) );
    XP_LOGF( "%s: %s", comment, buf );
}
#endif

static void
indexOne( XP_U16 depth, Tile* tiles, IndexData* data, DictIter* prevIter, 
          DictPosition* prevIndex )
{
    DictIter curIter;
    dict_initIterFrom( &curIter, prevIter );
    if ( findWordStartsWith( &curIter, tiles, depth ) ) {
        while ( !wordsEqual( &curIter, prevIter ) ) {
            ++*prevIndex;
            if ( !nextWord( prevIter ) ) {
                XP_ASSERT( 0 );
            }
        }
        XP_ASSERT( data->count == 0 || 
                   data->indices[data->count-1] < *prevIndex );
        data->indices[data->count] = *prevIndex;

        if ( NULL != data->prefixes ) {
            XP_MEMCPY( data->prefixes + (data->count * depth), tiles, depth );
        }
        ++data->count;
    }
}

static void
doOneDepth( const Tile* allTiles, XP_U16 nTiles, Tile* prefix, 
            XP_U16 curDepth, XP_U16 maxDepth, IndexData* data, 
            DictIter* prevIter, DictPosition* prevIndex )
{
    XP_U16 ii;
    for ( ii = 0; ii < nTiles; ++ii ) {
        prefix[curDepth] = allTiles[ii];
        if ( curDepth + 1 == maxDepth ) {
            indexOne( maxDepth, prefix, data, prevIter, prevIndex );
        } else {
            doOneDepth( allTiles, nTiles, prefix, curDepth+1, maxDepth,
                        data, prevIter, prevIndex );
        }
    }
}

void
dict_makeIndex( const DictIter* iter, XP_U16 depth, IndexData* data )
{
    ASSERT_INITED( iter );
    const DictionaryCtxt* dict = iter->dict;
    XP_ASSERT( depth < MAX_COLS_DICT );
    XP_U16 ii, needCount;
    const XP_U16 nFaces = dict_numTileFaces( dict );
    XP_U16 nNonBlankFaces = nFaces;
    XP_Bool hasBlank = dict_hasBlankTile( dict );
    if ( hasBlank ) {
        --nNonBlankFaces;
    }
    for ( ii = 1, needCount = nNonBlankFaces; ii < depth; ++ii ) {
        needCount *= nNonBlankFaces;
    }
    XP_ASSERT( needCount <= data->count );

    Tile allTiles[nNonBlankFaces];
    XP_U16 nTiles = 0;
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
    data->count = 0;
    DictIter prevIter;
    dict_initIterFrom( &prevIter, iter );
    if ( firstWord( &prevIter ) ) {
        DictPosition prevIndex = 0;
        Tile prefix[depth];
        doOneDepth( allTiles, nNonBlankFaces, prefix, 0, depth, 
                    data, &prevIter, &prevIndex );
    }

#ifdef DEBUG
    DictPosition pos;
    for ( pos = 1; pos < data->count; ++pos ) {
        XP_ASSERT( data->indices[pos-1] < data->indices[pos] );
    }
#endif
} /* dict_makeIndex */

static void
initWord( DictIter* iter )
{
    iter->nWords = dict_countWords( iter, NULL );
}

XP_Bool
dict_firstWord( DictIter* iter )
{
    ASSERT_INITED( iter );
    XP_Bool success = firstWord( iter );
    if ( success ) {
        initWord( iter );
        iter->position = 0;
    }

    return success;
}

XP_Bool
dict_getNextWord( DictIter* iter )
{
    ASSERT_INITED( iter );
    XP_Bool success = nextWord( iter );
    if ( success ) {
        ++iter->position;
    }
    return success;
}

XP_Bool
dict_lastWord( DictIter* iter )
{
    ASSERT_INITED( iter );
    iter->nEdges = 1;
    iter->edges[0] = dict_getTopEdge( iter->dict );

    XP_Bool success = lastEdges( iter, &iter->nEdges ) || prevWord( iter );
    if ( success ) {
        initWord( iter );
        iter->position = iter->nWords - 1;
    }

    return success;
}

XP_Bool
dict_getPrevWord( DictIter* iter )
{
    ASSERT_INITED( iter );
    XP_Bool success = prevWord( iter );
    if ( success ) {
        --iter->position;
    }
    return success;
}

/* If we start without an initialized word, init it to be closer to what's
   sought.  OR if we're father than necessary from what's sought, start over
   at the closer end.  Then move as many steps as necessary to reach it. */
XP_Bool
dict_getNthWord( DictIter* iter, DictPosition position, XP_U16 depth, 
                 const IndexData* data )
{
    ASSERT_INITED( iter );
    const DictionaryCtxt* dict = iter->dict;
    XP_U32 wordCount;
    XP_Bool validWord = 0 < iter->nEdges;
    if ( validWord ) {        /* uninitialized */
        wordCount = iter->nWords;
        XP_ASSERT( wordCount == dict_countWords( iter, NULL ) );
    } else {
        wordCount = dict_getWordCount( dict );
    }
    XP_Bool success = position < wordCount;
    if ( success ) {
        /* super common cases first */
        success = XP_FALSE;
        if ( validWord ) {
            if ( iter->position == position ) {
                success = XP_TRUE;
                /* do nothing; we're done */
            } else if ( iter->position == position - 1 ) {
                success = dict_getNextWord( iter );
            } else if ( iter->position == position + 1 ) {
                success = dict_getPrevWord( iter );
            }
        }

        if ( !success ) {
            XP_U32 wordIndex;
            if ( !!data && !!data->prefixes && !!data->indices ) {
                wordIndex = placeWordClose( iter, position, depth, data );
                if ( !validWord ) {
                    initWord( iter );
                }
            } else {
                wordCount /= 2;             /* mid-point */

                /* If word's inited but farther from target than either
                   endpoint, better to start with an endpoint */
                if ( validWord && 
                     XP_ABS( position - iter->position ) > wordCount ) {
                    validWord = XP_FALSE;
                }

                if ( !validWord ) {
                    if ( position >= wordCount ) {
                        dict_lastWord( iter );
                    } else {
                        dict_firstWord( iter );
                    }
                }
                wordIndex = iter->position;
            }

            XP_Bool (*finder)( DictIter* iter ) = NULL;/* stupid compiler */
            XP_U32 repeats = 0;
            if ( wordIndex < position ) {
                finder = nextWord;
                repeats = position - wordIndex;
            } else if ( wordIndex > position ) {
                finder = prevWord;
                repeats = wordIndex - position;
            }
            while ( repeats-- ) {
                if ( !(*finder)( iter ) ) {
                    XP_ASSERT(0);
                }
            }

            iter->position = position;
            success = XP_TRUE;
        }
    }
    return success;
} /* dict_getNthWord */

XP_Bool 
dict_findStartsWith( DictIter* iter, const IndexData* data, 
                     const Tile* prefix, XP_U16 len )
{
    XP_Bool success = XP_FALSE;
    ASSERT_INITED( iter );
    XP_USE(data);
    XP_LOGF( "%s: not using data", __func__ );

    DictIter targetIter;
    dict_initIterFrom( &targetIter, iter );
    if ( findWordStartsWith( &targetIter, prefix, len ) ) {

        DictPosition result = 0;
        DictIter iterZero;
        dict_initIterFrom( &iterZero, iter );
        if ( !firstWord( &iterZero ) ) {
            XP_ASSERT( 0 );
        }

        while ( ! wordsEqual( &iterZero, &targetIter ) ) {
            ++result;
            if ( !nextWord( &iterZero ) ) {
                XP_ASSERT( 0 );
            }
        }
        copyIter( iter, &iterZero );
        iter->position = result;
        success = XP_TRUE;
    }
    return success;
}

void
dict_wordToString( const DictIter* iter, XP_UCHAR* buf, XP_U16 buflen )
{
    ASSERT_INITED( iter );
    iterToString( iter, buf, buflen );
}

DictPosition
dict_getPosition( const DictIter* iter )
{
    ASSERT_INITED( iter );
    return iter->position;
}

#ifdef CPLUS
}
#endif
#endif /* XWFEATURE_WALKDICT */
