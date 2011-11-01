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

        while ( IS_LAST_EDGE( dict, edges->edges[nTiles-1] )
                && 0 < --nTiles ) {
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
            success = 0 < edges->nEdges
                && ISACCEPTING( dict, edges->edges[edges->nEdges-1] );
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
findStartsWith( const DictionaryCtxt* dict, const Tile* tiles, XP_U16 nTiles, EdgeArray* edges )
{
    XP_Bool success = XP_TRUE;
    array_edge* edge = dict_getTopEdge( dict );
    edges->nEdges = 0;

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

static DictIndex
placeWordClose( const DictionaryCtxt* dict, DictIndex position, 
                XP_U16 depth, IndexData* data, EdgeArray* result )
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

    /* DictIndex top = mid < count-1? indices[mid+1] : -1; */
    /* XP_LOGF( "found %ld at %d, in range between %ld and %ld", position, */
    /*          mid, indices[mid], top ); */

    /* Now we have the index immediately below the position we want.  But we
       may be better off starting with the next if it's closer.  The last
       index is a special case since we use lastWord rather than a prefix to
       init */
    if ( ( index + 1 < data->count ) 
         && (data->indices[index + 1] - position) 
         < (position - data->indices[index]) ) {
        ++index;
    }
    XP_Bool success = findStartsWith( dict, &data->prefixes[depth*index], 
                                      depth, result )
        && ( ISACCEPTING( dict, result->edges[result->nEdges-1] )
             || nextWord( dict, result ) );
    XP_ASSERT( success );

    return data->indices[index];
} /* placeWordClose */

static void
indexOne( const DictionaryCtxt* dict, XP_U16 depth, Tile* tiles, 
          IndexData* data, EdgeArray* prevEdges, DictIndex* prevIndex )
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
        data->indices[data->count] = *prevIndex;
        if ( NULL != data->prefixes ) {
            XP_MEMCPY( data->prefixes + (data->count * depth), tiles, depth );
        }
        ++data->count;
    }
}

static void
doOneDepth( const DictionaryCtxt* dict, 
            const Tile* allTiles, XP_U16 nTiles, Tile* prefix, 
            XP_U16 curDepth, XP_U16 maxDepth, IndexData* data, 
            EdgeArray* prevEdges, DictIndex* prevIndex )
{
    XP_U16 ii;
    for ( ii = 0; ii < nTiles; ++ii ) {
        prefix[curDepth] = allTiles[ii];
        if ( curDepth + 1 == maxDepth ) {
            indexOne( dict, maxDepth, prefix, data, prevEdges, prevIndex );
        } else {
            doOneDepth( dict, allTiles, nTiles, prefix, curDepth+1, maxDepth,
                        data, prevEdges, prevIndex );
        }
    }
}

void
dict_makeIndex( const DictionaryCtxt* dict, XP_U16 depth, IndexData* data )
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
    XP_ASSERT( needCount <= data->count );

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
    data->count = 0;
    DictWord firstWord;
    if ( dict_firstWord( dict, &firstWord ) ) {
        EdgeArray prevEdges;
        DictIndex prevIndex = 0;
        Tile prefix[depth];
        indicesToEdges( dict, &firstWord, &prevEdges );

        doOneDepth( dict, allTiles, nFaces, prefix, 0, depth, 
                    data, &prevEdges, &prevIndex );

    }
}

static void
initWord( const DictionaryCtxt* dict, DictWord* word )
{
    word->wordCount = dict_getWordCount( dict );
}

XP_Bool
dict_firstWord( const DictionaryCtxt* dict, DictWord* word )
{
    EdgeArray edges = { .nEdges = 0 };
    edges.edges[edges.nEdges++] = dict_getTopEdge( dict );

    XP_Bool success = ISACCEPTING( dict, edges.edges[0] )
        || nextWord( dict, &edges );
    if ( success ) {
        initWord( dict, word );
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
        initWord( dict, word );

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
dict_getNthWord( const DictionaryCtxt* dict, DictWord* word, XP_U32 nn,
                 XP_U16 depth, IndexData* data )
{
    XP_U32 wordCount;
    XP_Bool validWord = 0 < word->nTiles;
    if ( validWord ) {        /* uninitialized */
        wordCount = word->wordCount;
        XP_ASSERT( wordCount == dict_getWordCount( dict ) );
    } else {
        wordCount = dict_getWordCount( dict );
    }
    XP_Bool success = nn < wordCount;
    if ( success ) {
        /* super common cases first */
        success = XP_FALSE;
        if ( validWord ) {
            if ( word->index == nn ) {
                success = XP_TRUE;
                /* do nothing; we're done */
            } else if ( word->index == nn + 1 ) {
                success = dict_getNextWord( dict, word );
            } else if ( word->index == nn - 1 ) {
                success = dict_getPrevWord( dict, word );
            }
        }

        if ( !success ) {
            EdgeArray edges;
            XP_U32 wordIndex;
            if ( !!data ) {
                wordIndex = placeWordClose( dict, nn, depth, data, &edges );
                if ( !validWord ) {
                    initWord( dict, word );
                }
            } else {
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
                indicesToEdges( dict, word, &edges );
                wordIndex = word->index;
            }

            XP_U32 ii;
            if ( wordIndex < nn ) {
                for ( ii = nn - wordIndex; ii > 0; --ii ) {
                    if ( !nextWord( dict, &edges ) ) {
                        XP_ASSERT( 0 );
                    }
                }
            } else if ( wordIndex > nn ) {
                for ( ii = wordIndex - nn; ii > 0; --ii ) {
                    if ( !prevWord( dict, &edges ) ) {
                        XP_ASSERT( 0 );
                    }
                }
            }
            edgesToIndices( dict, &edges, word );
            word->index = nn;
            success = XP_TRUE;
        }
    }
    return success;
} /* dict_getNthWord */

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

#ifdef CPLUS
}
#endif
#endif /* XWFEATURE_WALKDICT */
