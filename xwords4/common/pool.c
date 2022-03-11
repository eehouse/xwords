/* -*- compile-command: "cd ../linux && make -j3 MEMDEBUG=TRUE"; -*- */
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

#ifdef DEBUG
static void checkTilesLeft( const PoolContext* pool );
#else
# define checkTilesLeft( pool )
#endif


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
        Tile curLetter;
        for ( curLetter = 0; ; ++curLetter ) {
            nextCount -= pool->lettersLeft[(short)curLetter];
            if ( nextCount < 0 ) {
                XP_ASSERT( pool->lettersLeft[(short)curLetter] > 0 );
                result = curLetter;
                break;
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
    
#if defined PLATFORM_PALM && ! defined XW_TARGET_PNO
    XP_U16 rr = XP_RANDOM();
#else
    XP_U16 rr = (XP_U16)(XP_RANDOM()>>16);
#endif
    XP_U16 index = (XP_U16)(rr % pool->numTilesLeft);
    Tile result = getNthPoolTile( pool, index );

    --pool->lettersLeft[result];
    --pool->numTilesLeft;
    return result;
} /* getRandomTile */

void
pool_requestTiles( PoolContext* pool, Tile* tiles, XP_U16* maxNum )
{
    XP_S16 numWanted = *maxNum;
    XP_U16 numWritten = 0;

    XP_ASSERT( numWanted >= 0 );

#ifdef BLANKS_FIRST
    XP_U16 oldCount = pool->lettersLeft[pool->blankIndex];
    if ( oldCount > 1 ) {
        pool->lettersLeft[pool->blankIndex] = 1;
    }
#endif

    while ( pool->numTilesLeft > 0 && numWanted-- ) {
        Tile t = getRandomTile( pool );
        *tiles++ = t;
        ++numWritten;
    }
    *maxNum = numWritten;

#ifdef BLANKS_FIRST
    pool->lettersLeft[pool->blankIndex] = oldCount - 1;
#endif

    XP_LOGF( "%s: %d tiles left in pool", __func__, pool->numTilesLeft );
} /* pool_requestTiles */

void
pool_replaceTiles( PoolContext* pool, const TrayTileSet* tiles )
{
    pool_replaceTiles2( pool, tiles->nTiles, tiles->tiles );
}

void
pool_replaceTiles2( PoolContext* pool, XP_U16 nTiles, const Tile* tilesP )
{
    while ( nTiles-- ) {
        Tile tile = *tilesP++; /* do I need to filter off high bits? */

        XP_ASSERT( nTiles < MAX_TRAY_TILES );
        XP_ASSERT( tile < pool->numFaces );

        ++pool->lettersLeft[tile];
        ++pool->numTilesLeft;
    }
} /* pool_replaceTiles */

void
pool_removeTiles( PoolContext* pool, const TrayTileSet* tiles )
{
    XP_U16 nTiles = tiles->nTiles;
    const Tile* tilesP = tiles->tiles;

    XP_ASSERT( nTiles <= MAX_TRAY_TILES );

    while ( nTiles-- ) {
        Tile tile = *tilesP++; /* do I need to filter off high bits? */

        XP_ASSERT( tile < pool->numFaces );
        XP_ASSERT( pool->lettersLeft[tile] > 0 );
        XP_ASSERT( pool->numTilesLeft > 0 );

        --pool->lettersLeft[tile];
        --pool->numTilesLeft;
    }
    XP_LOGF( "%s: %d tiles left in pool", __func__, pool->numTilesLeft );
} /* pool_removeTiles */

XP_Bool
pool_containsTiles( const PoolContext* pool, const TrayTileSet* tiles )
{
    XP_Bool allThere = XP_TRUE;
    XP_U16 ii;
    XP_U8 counts[pool->numFaces];
    XP_MEMCPY( counts, pool->lettersLeft, sizeof(counts) );

    for ( ii = 0; allThere && ii < tiles->nTiles; ++ii ) {
        allThere = 0 < counts[tiles->tiles[ii]]--;
    }

    return allThere;
}

XP_U16
pool_getNTilesLeft( const PoolContext* pool )
{
    XP_ASSERT( !!pool );
    return NULL == pool ? 0 : pool->numTilesLeft;
} /* pool_remainingTileCount */

XP_U16
pool_getNTilesLeftFor( const PoolContext* pool, Tile tile )
{
    return pool->lettersLeft[tile];
} /* pool_remainingTileCount */

void 
pool_initFromDict( PoolContext* pool, const DictionaryCtxt* dict, XP_U16 nCols )
{
    const XP_U16 numFaces = dict_numTileFaces( dict );

    XP_FREEP( pool->mpool, &pool->lettersLeft );

    pool->lettersLeft
        = (XP_U8*)XP_MALLOC( pool->mpool, 
                             numFaces * sizeof(pool->lettersLeft[0]) );
    pool->numTilesLeft = 0;

    for ( Tile tile = 0; tile < numFaces; ++tile ) {
        XP_U16 numTiles = dict_numTilesForSize( dict, tile, nCols );
        pool->lettersLeft[tile] = (XP_U8)numTiles;
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
    checkTilesLeft( pool );
} /* pool_initFromDict */

#ifdef DEBUG
static void
checkTilesLeft( const PoolContext* pool )
{
    XP_U16 ii, count;
    for ( count = 0, ii = 0; ii < pool->numFaces; ++ii ) {
        count += pool->lettersLeft[ii];
    }
    XP_ASSERT( count == pool->numTilesLeft );
}

void
pool_dumpSelf( const PoolContext* pool )
{
    XP_UCHAR buf[256] = {0};
    XP_U16 offset = 0;
    for ( Tile tile = 0; tile < pool->numFaces; ++tile ) {
        XP_U16 count = pool->lettersLeft[tile];
        if ( count > 0 ) {
            offset += XP_SNPRINTF( &buf[offset], VSIZE(buf) - offset, "%x/%d,", tile, count );
        }
    }
    XP_LOGF( "%s(): {numTiles: %d, pool: %s}", __func__,
             pool->numTilesLeft,  buf );
}
#endif

