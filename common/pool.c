/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 2000 by Eric House (xwords@eehouse.org).  All rights reserved.
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
/* #include <assert.h> */

#include "pool.h"
#include "dictnry.h"
#include "xwstream.h"

#define pEND 0x70454e44

// #define BLANKS_FIRST 1

struct PoolContext {
    XP_U8* lettersLeft;
    XP_U16 numTilesLeft;
    XP_U16 numFaces;
#ifdef BLANKS_FIRST
    XP_S16 blankIndex;
#endif
    MPSLOT
};

PoolContext*
pool_make( MPFORMAL_NOCOMMA )
{
    PoolContext* result = (PoolContext*)XP_MALLOC(mpool, sizeof(*result) );

    if ( result != NULL ) {
        XP_MEMSET( result, 0, sizeof( *result ) );
        MPASSIGN(result->mpool, mpool);

#ifdef BLANKS_FIRST
        result->blankIndex = -1;
#endif
    }

    return result;
} /* pool_make */

void
pool_writeToStream( PoolContext* pool, XWStreamCtxt* stream )
{
    stream_putU16( stream, pool->numTilesLeft );
    stream_putU16( stream, pool->numFaces );
    stream_putBytes( stream, pool->lettersLeft, 
                     (XP_U16)(pool->numFaces * sizeof(pool->lettersLeft[0])) );
#ifdef DEBUG
    stream_putU32( stream, pEND );
#endif
} /* pool_writeToStream */

PoolContext*
pool_makeFromStream( MPFORMAL XWStreamCtxt* stream )
{
    PoolContext* pool = pool_make( MPPARM_NOCOMMA(mpool) );

    pool->numTilesLeft = stream_getU16( stream );
    pool->numFaces = stream_getU16( stream );
    pool->lettersLeft = (XP_U8*)
        XP_MALLOC( mpool, pool->numFaces * sizeof(pool->lettersLeft[0]) );
    stream_getBytes( stream, pool->lettersLeft, 
                     (XP_U16)(pool->numFaces * sizeof(pool->lettersLeft[0])) );

    XP_ASSERT( stream_getU32( stream ) == pEND );

    return pool;
} /* pool_makeFromStream */

void
pool_destroy( PoolContext* pool )
{
    XP_ASSERT( pool != NULL );
    XP_FREE( pool->mpool, pool->lettersLeft );
    XP_FREE( pool->mpool, pool );
} /* pool_destroy */

static Tile
getNthPoolTile( PoolContext* pool, short index ) 
{
    Tile result;

    /* given an array of counts of remaining letters, subtract each in turn
       from the total we seek until that total is at or below zero.  The count
       that put it (or would put it) under 0 is the one to pick. */

    if ( 0 ) {
#ifdef BLANKS_FIRST
    } else if ( pool->blankIndex >= 0 && pool->lettersLeft[pool->blankIndex] > 0 ) {
        result = pool->blankIndex;
#endif
    } else {
        XP_S16 nextCount = index;
        Tile curLetter = 0;
        for ( ; ; ) {
            nextCount -= pool->lettersLeft[(short)curLetter];
            if ( nextCount < 0 ) {
                XP_ASSERT( pool->lettersLeft[(short)curLetter] > 0 );
                result = curLetter;
                break;
            } else {
                ++curLetter;
            }
        }
    }
    return result;
} /* getNthPoolTile */

static Tile
getRandomTile( PoolContext* pool )
{
    /* There's a good little article on shuffling algorithms here:
     * http://en.wikipedia.org/wiki/Shuffle#Shuffling_algorithms This puppy
     * can definitely be improved.  PENDING.  But note that what's here still
     * works when tiles are re-inserted in the pool.  Will need to reshuffle
     * in that case if move to shuffling once and just taking tiles off the
     * top thereafter.
     */
    
    XP_U16 r = (XP_U16)XP_RANDOM();
    XP_U16 index = (XP_U16)(r % pool->numTilesLeft);
    Tile result = getNthPoolTile( pool, index );

    --pool->lettersLeft[result];
    --pool->numTilesLeft;
    return result;
} /* getRandomTile */

void
pool_requestTiles( PoolContext* pool, Tile* tiles, XP_U8* maxNum )
{
    XP_S16 numWanted = *maxNum;
    XP_U16 numWritten = 0;

    XP_ASSERT( numWanted >= 0 );

    while ( pool->numTilesLeft > 0 && numWanted-- ) {
        Tile t = getRandomTile( pool );
        *tiles++ = t;
        ++numWritten;
    }
    *maxNum = (XP_U8)numWritten;
} /* pool_requestTiles */

void
pool_replaceTiles( PoolContext* pool, TrayTileSet* tiles )
{
    XP_U16 nTiles = tiles->nTiles;
    Tile* tilesP = tiles->tiles;

    while ( nTiles-- ) {
        Tile tile = *tilesP++; /* do I need to filter off high bits? */

        XP_ASSERT( nTiles < MAX_TRAY_TILES );
        XP_ASSERT( tile < pool->numFaces );

        ++pool->lettersLeft[tile];
        ++pool->numTilesLeft;
    }
} /* pool_replaceTiles */

void
pool_removeTiles( PoolContext* pool, TrayTileSet* tiles )
{
    XP_U16 nTiles = tiles->nTiles;
    Tile* tilesP = tiles->tiles;

    XP_ASSERT( nTiles <= MAX_TRAY_TILES );

    while ( nTiles-- ) {
        Tile tile = *tilesP++; /* do I need to filter off high bits? */

        XP_ASSERT( tile < pool->numFaces );
        XP_ASSERT( pool->lettersLeft[tile] > 0 );
        XP_ASSERT( pool->numTilesLeft > 0 );

        --pool->lettersLeft[tile];
        --pool->numTilesLeft;
    }
} /* pool_removeTiles */

XP_U16
pool_getNTilesLeft( PoolContext* pool )
{
    return pool->numTilesLeft;
} /* pool_remainingTileCount */

XP_U16
pool_getNTilesLeftFor( PoolContext* pool, Tile tile )
{
    return pool->lettersLeft[tile];
} /* pool_remainingTileCount */

void 
pool_initFromDict( PoolContext* pool, DictionaryCtxt* dict )
{
    XP_U16 numFaces = dict_numTileFaces( dict );
    Tile i;

    if ( pool->lettersLeft != NULL ) {
        XP_FREE( pool->mpool, pool->lettersLeft );
    }

    pool->lettersLeft
        = (XP_U8*)XP_MALLOC( pool->mpool, 
                             numFaces * sizeof(pool->lettersLeft[0]) );
    pool->numTilesLeft = 0;

    for ( i = 0; i < numFaces; ++i ) {
        XP_U16 numTiles = dict_numTiles( dict, i );
        pool->lettersLeft[i] = (XP_U8)numTiles;
        pool->numTilesLeft += numTiles;
    }

    pool->numFaces = numFaces;

#ifdef BLANKS_FIRST
    if ( dict_hasBlankTile( dict ) ) {
        pool->blankIndex = dict_getBlankTile(dict);
    } else {
        pool->blankIndex = -1;
    }
#endif
} /* pool_initFromDict */

