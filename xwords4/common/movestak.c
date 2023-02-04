/* -*- compile-command: "cd ../linux && make -j5 MEMDEBUG=TRUE"; -*- */
/* 
 * Copyright 2001 - 2019 by Eric House (xwords@eehouse.org). All rights
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

//#include "modelp.h"

#include "mempool.h"
#include "xwstream.h"
#include "movestak.h"
#include "memstream.h"
#include "strutils.h"
#include "dbgutil.h"

/* HASH_STREAM: It should be possible to hash the move stack by simply hashing
   the stream from the beginning to the top of the undo stack (excluding
   what's in the redo area), avoiding iterating over it and doing a ton of
   bitwise operations to read it into entries.  But I don't currently seem to
   have a XWStreamPos that corresponds to the undo-top and so I can't figure
   out the length.  Hashing that includes the redo part of the stack doesn't
   work once there's been undo activity.  (Not sure why...)  */

#ifdef CPLUS
extern "C" {
#endif

struct StackCtxt {
    VTableMgr* vtmgr;
    XWStreamCtxt* data;
    XWStreamPos   top;
    XWStreamPos cachedPos;
    XP_U16 cacheNext;
    XP_U16 nEntries;
    XP_U16 bitsPerTile;
    XP_U16 highWaterMark;
    XP_U16 typeBits;
    XP_U16 nPlayers;
    XP_U8 flags;

    XP_Bool inDuplicateMode;

    DIRTY_SLOT
    MPSLOT
};

#define HAVE_FLAGS_MASK ((XP_U16)0x8000)
#define VERS_7TILES_BIT 0x01

static XP_Bool popEntryImpl( StackCtxt* stack, StackEntry* entry );

void
stack_init( StackCtxt* stack, XP_U16 nPlayers, XP_Bool inDuplicateMode )
{
    stack->nEntries = stack->highWaterMark = 0;
    stack->top = START_OF_STREAM;
    stack->nPlayers = nPlayers;
    stack->inDuplicateMode = inDuplicateMode;

    /* I see little point in freeing or shrinking stack->data.  It'll get
       shrunk to fit as soon as we serialize/deserialize anyway. */
} /* stack_init */


void
stack_set7Tiles( StackCtxt* stack )
{
    XP_ASSERT( !stack->data );
    stack->flags |= VERS_7TILES_BIT;
}

XP_U16
stack_getVersion( const StackCtxt* stack )
{
    XP_ASSERT( !!stack->data );
    return stream_getVersion( stack->data );
}

#ifdef STREAM_VERS_HASHSTREAM
XP_U32
stack_getHash( const StackCtxt* stack )
{
    XP_U32 hash = 0;
    if ( !!stack->data ) {
        hash = stream_getHash( stack->data, stack->top );
    }
    return hash;
} /* stack_getHash */
#endif

void
stack_setBitsPerTile( StackCtxt* stack, XP_U16 bitsPerTile )
{
    XP_ASSERT( !!stack );
    XP_ASSERT( bitsPerTile == 5 || bitsPerTile == 6 );
    stack->bitsPerTile = bitsPerTile;
}

StackCtxt*
stack_make( MPFORMAL VTableMgr* vtmgr, XP_U16 nPlayers, XP_Bool inDuplicateMode )
{
    StackCtxt* result = (StackCtxt*)XP_MALLOC( mpool, sizeof( *result ) );
    if ( !!result ) {
        XP_MEMSET( result, 0, sizeof(*result) );
        MPASSIGN(result->mpool, mpool);
        result->vtmgr = vtmgr;
        result->nPlayers = nPlayers;
        result->inDuplicateMode = inDuplicateMode;
    }

    return result;
} /* stack_make */

void
stack_destroy( StackCtxt* stack )
{
    if ( !!stack->data ) {
        stream_destroy( stack->data );
    }
    /* Ok to close with a dirty stack, e.g. if not saving a deleted game */
    // ASSERT_NOT_DIRTY( stack );
    XP_FREE( stack->mpool, stack );
} /* stack_destroy */

void
stack_loadFromStream( StackCtxt* stack, XWStreamCtxt* stream )
{
    /* Problem: the moveType field is getting bigger to support
     * DUP_MOVE_TYPE. So 3 bits are needed rather than 2.  I can't use the
     * parent stream's version since the parent stream is re-written each time
     * the game's saved (with the new version) but the stack is not rewritten,
     * only appended to (normally). The solution is to take advantage of the
     * extra bits at the top of the stack's data size (nBytes below). If the
     * first bit's set, the stream was created by code that assumes 3 bits for
     * the moveType field.
     */
    XP_U16 nBytes = stream_getU16( stream );
    if ( (HAVE_FLAGS_MASK & nBytes) != 0 ) {
        stack->flags = stream_getU8( stream );
        stack->typeBits = 3;
    } else {
        XP_ASSERT( 0 == stack->flags );
        stack->typeBits = 2;
    }
    nBytes &= ~HAVE_FLAGS_MASK;

    if ( nBytes > 0 ) {
        XP_U8 stackVersion = STREAM_VERS_NINETILES - 1;
        if ( STREAM_VERS_NINETILES <= stream_getVersion(stream) ) {
            stackVersion = stream_getU8( stream );
            XP_LOGFF( "read stackVersion: %d from stream", stackVersion );
            XP_ASSERT( stackVersion <= CUR_STREAM_VERS );
        }
        stack->highWaterMark = stream_getU16( stream );
        stack->nEntries = stream_getU16( stream );
        stack->top = stream_getU32( stream );
        stack->data = mem_stream_make_raw( MPPARM(stack->mpool) stack->vtmgr );

        stream_getFromStream( stack->data, stream, nBytes );
        stream_setVersion( stack->data, stackVersion );
    } else {
        XP_ASSERT( stack->nEntries == 0 );
        XP_ASSERT( stack->top == 0 );
    }
    CLEAR_DIRTY( stack );
} /* stack_makeFromStream */

void
stack_writeToStream( const StackCtxt* stack, XWStreamCtxt* stream )
{
    XP_U16 nBytes = 0;
    XWStreamCtxt* data = stack->data;
    XWStreamPos oldPos = START_OF_STREAM;

    /* XP_LOGF( "%s(): writing stream; hash: %X", __func__, hash ); */
    /* XP_U32 hash = stream_getHash( data, START_OF_STREAM, XP_TRUE ); */

    if ( !!data ) {
        oldPos = stream_setPos( data, POS_READ, START_OF_STREAM );
        nBytes = stream_getSize( data );
    }

    XP_ASSERT( 0 == (HAVE_FLAGS_MASK & nBytes) ); /* under 32K? I hope so */
    stream_putU16( stream, nBytes | (stack->typeBits == 3 ? HAVE_FLAGS_MASK : 0) );
    if ( stack->typeBits == 3 ) {
        stream_putU8( stream, stack->flags );
    }

    if ( nBytes > 0 ) {
        if ( STREAM_VERS_NINETILES <= stream_getVersion(stream) ) {
            stream_putU8( stream, stream_getVersion(data) );
        }
        stream_putU16( stream, stack->highWaterMark );
        stream_putU16( stream, stack->nEntries );
        stream_putU32( stream, stack->top );

        stream_getFromStream( stream, data, nBytes );
        /* in case it'll be used further */
        (void)stream_setPos( data, POS_READ, oldPos );
    }
    CLEAR_DIRTY( stack );
} /* stack_writeToStream */

StackCtxt*
stack_copy( const StackCtxt* stack )
{
    StackCtxt* newStack = NULL;
    XWStreamCtxt* stream = mem_stream_make_raw( MPPARM(stack->mpool)
                                                stack->vtmgr );
    stack_writeToStream( stack, stream );

    newStack = stack_make( MPPARM(stack->mpool) stack->vtmgr,
                           stack->nPlayers, stack->inDuplicateMode );
    stack_loadFromStream( newStack, stream );
    stack_setBitsPerTile( newStack, stack->bitsPerTile );
    stream_destroy( stream );
    return newStack;
}

static void
pushEntryImpl( StackCtxt* stack, const StackEntry* entry )
{
    XP_LOGFF( "(typ=%s, player=%d)", StackMoveType_2str(entry->moveType),
              entry->playerNum );

    XWStreamCtxt* stream = stack->data;
    if ( !stream ) {
        stream = stack->data =
            mem_stream_make_raw( MPPARM(stack->mpool) stack->vtmgr );
        XP_U16 version = 0 == (stack->flags & VERS_7TILES_BIT)
            ? CUR_STREAM_VERS : STREAM_VERS_NINETILES - 1;
        stream_setVersion( stream, version );
        stack->typeBits = stack->inDuplicateMode ? 3 : 2;     /* the new size */
        XP_ASSERT( 0 == (~VERS_7TILES_BIT & stack->flags) );
    }

    XWStreamPos oldLoc = stream_setPos( stream, POS_WRITE, stack->top );

    stream_putBits( stream, stack->typeBits, entry->moveType );
    stream_putBits( stream, 2, entry->playerNum );

    switch( entry->moveType ) {
    case MOVE_TYPE:
        moveInfoToStream( stream, &entry->u.move.moveInfo, stack->bitsPerTile );
        traySetToStream( stream, &entry->u.move.newTiles );
        if ( stack->inDuplicateMode ) {
            stream_putBits( stream, NPLAYERS_NBITS, entry->u.move.dup.nScores );
            scoresToStream( stream, entry->u.move.dup.nScores, entry->u.move.dup.scores );
        }
        break;
    case PHONY_TYPE:
        moveInfoToStream( stream, &entry->u.phony.moveInfo, stack->bitsPerTile );
        break;

    case ASSIGN_TYPE:
        traySetToStream( stream, &entry->u.assign.tiles );
        XP_ASSERT( entry->playerNum == DUP_PLAYER || !stack->inDuplicateMode );
        break;

    case TRADE_TYPE:
        XP_ASSERT( entry->u.trade.newTiles.nTiles
                   == entry->u.trade.oldTiles.nTiles );
        traySetToStream( stream, &entry->u.trade.oldTiles );
        /* could save three bits per trade by just writing the tiles of the
           second guy */
        traySetToStream( stream, &entry->u.trade.newTiles );
        break;
    case PAUSE_TYPE:
        stream_putBits( stream, 2, entry->u.pause.pauseType );
        stream_putU32( stream, entry->u.pause.when );
        stringToStream( stream, entry->u.pause.msg );
        break;
    default:
        XP_ASSERT(0);
    }

    ++stack->nEntries;
    stack->highWaterMark = stack->nEntries;
    stack->top = stream_setPos( stream, POS_WRITE, oldLoc );
    SET_DIRTY( stack );
} /* pushEntryImpl */

static void
pushEntry( StackCtxt* stack, const StackEntry* entry )
{
#ifdef DEBUG_HASHING
    XP_U32 origHash = stack_getHash( stack );

    StackEntry prevTop;
    if ( 1 < stack->nPlayers &&
         stack_getNthEntry( stack, stack->nEntries - 1, &prevTop ) ) {
        XP_ASSERT( stack->inDuplicateMode || prevTop.playerNum != entry->playerNum );
    }
#endif

    pushEntryImpl( stack, entry );

#ifdef DEBUG_HASHING
    XP_U32 newHash = stack_getHash( stack );
    StackEntry lastEntry;
    if ( popEntryImpl( stack, &lastEntry ) ) {
        XP_ASSERT( origHash == stack_getHash( stack ) );
        pushEntryImpl( stack, &lastEntry );
        XP_ASSERT( newHash == stack_getHash( stack ) );
        XP_LOGFF( "all ok; pushed type %s for player %d into pos #%d, hash now %X (was %X)",
                  StackMoveType_2str(entry->moveType), entry->playerNum,
                  stack->nEntries, newHash, origHash );
    } else {
        XP_ASSERT(0);
    }
#endif
    XP_LOGFF( "hash now %X", stack_getHash( stack ) );
}

static void
readEntry( const StackCtxt* stack, StackEntry* entry )
{
    XWStreamCtxt* stream = stack->data;

    entry->moveType = (StackMoveType)stream_getBits( stream, stack->typeBits );
    entry->playerNum = (XP_U8)stream_getBits( stream, 2 );

    switch( entry->moveType ) {
    case MOVE_TYPE:
        moveInfoFromStream( stream, &entry->u.move.moveInfo, stack->bitsPerTile );
        traySetFromStream( stream, &entry->u.move.newTiles );
        if ( stack->inDuplicateMode ) {
            entry->u.move.dup.nScores = stream_getBits( stream, NPLAYERS_NBITS );
            scoresFromStream( stream, entry->u.move.dup.nScores, entry->u.move.dup.scores );
        }
        break;
    case PHONY_TYPE:
        moveInfoFromStream( stream, &entry->u.phony.moveInfo, stack->bitsPerTile );
        break;

    case ASSIGN_TYPE:
        traySetFromStream( stream, &entry->u.assign.tiles );
        break;

    case TRADE_TYPE:
        traySetFromStream( stream, &entry->u.trade.oldTiles );
        traySetFromStream( stream, &entry->u.trade.newTiles );
        XP_ASSERT( entry->u.trade.newTiles.nTiles
                   == entry->u.trade.oldTiles.nTiles );
        break;

    case PAUSE_TYPE:
        entry->u.pause.pauseType = (DupPauseType)stream_getBits( stream, 2 );
        entry->u.pause.when = stream_getU32( stream );
        entry->u.pause.msg = stringFromStream( stack->mpool, stream );
        break;

    default:
        XP_ASSERT(0);
    }

} /* readEntry */


static void
addMove( StackCtxt* stack, XP_U16 turn, const MoveInfo* moveInfo,
         XP_U16 nScores, XP_U16* scores, const TrayTileSet* newTiles )
{
    StackEntry move = {.playerNum = (XP_U8)turn,
                       .moveType = MOVE_TYPE,
    };

    XP_MEMCPY( &move.u.move.moveInfo, moveInfo, sizeof(move.u.move.moveInfo));
    move.u.move.newTiles = *newTiles;

    XP_ASSERT( 0 == nScores || stack->inDuplicateMode );
    if ( stack->inDuplicateMode ) {
        move.u.move.dup.nScores = nScores;
        XP_MEMCPY( &move.u.move.dup.scores[0], scores,
                   nScores * sizeof(move.u.move.dup.scores[0]) );
    }

    pushEntry( stack, &move );
}

void
stack_addMove( StackCtxt* stack, XP_U16 turn, const MoveInfo* moveInfo,
               const TrayTileSet* newTiles )
{
    addMove( stack, turn, moveInfo, 0, NULL, newTiles );
} /* stack_addMove */

void
stack_addDupMove( StackCtxt* stack, const MoveInfo* moveInfo,
                  XP_U16 nScores, XP_U16* scores, const TrayTileSet* newTiles )
{
    XP_ASSERT( stack->inDuplicateMode );
    addMove( stack, DUP_PLAYER, moveInfo, nScores, scores, newTiles );
}

void
stack_addPhony( StackCtxt* stack, XP_U16 turn, const MoveInfo* moveInfo )
{
    StackEntry move = {.playerNum = (XP_U8)turn,
                       .moveType = PHONY_TYPE,
    };

    XP_MEMCPY( &move.u.phony.moveInfo, moveInfo, 
               sizeof(move.u.phony.moveInfo));

    pushEntry( stack, &move );
} /* stack_addPhony */

void
stack_addDupTrade( StackCtxt* stack, const TrayTileSet* oldTiles,
                   const TrayTileSet* newTiles )
{
    XP_ASSERT( stack->inDuplicateMode );
    XP_ASSERT( oldTiles->nTiles == newTiles->nTiles );

    stack_addTrade( stack, DUP_PLAYER, oldTiles, newTiles );
}

void
stack_addTrade( StackCtxt* stack, XP_U16 turn, 
                const TrayTileSet* oldTiles, const TrayTileSet* newTiles )
{
    XP_ASSERT( oldTiles->nTiles == newTiles->nTiles );
    StackEntry move = { .playerNum = (XP_U8)turn,
                        .moveType = TRADE_TYPE,
    };

    move.u.trade.oldTiles = *oldTiles;
    move.u.trade.newTiles = *newTiles;

    pushEntry( stack, &move );
} /* stack_addTrade */

void
stack_addAssign( StackCtxt* stack, XP_U16 turn, const TrayTileSet* tiles )
{
    StackEntry move = { .playerNum = (XP_U8)turn,
                        .moveType = ASSIGN_TYPE,
    };

    move.u.assign.tiles = *tiles;

    pushEntry( stack, &move );
} /* stack_addAssign */

void
stack_addPause( StackCtxt* stack, DupPauseType pauseType, XP_S16 turn,
                XP_U32 when, const XP_UCHAR* msg )
{
    StackEntry move = { .moveType = PAUSE_TYPE,
                        .u.pause.pauseType = pauseType,
                        .u.pause.when = when,
                        .u.pause.msg = copyString( stack->mpool, msg ),
    };

    if ( 0 <= turn ) {
        move.playerNum = turn;  /* don't store the -1 case (pauseType==AUTOPAUSED) */
    } else {
        XP_ASSERT( AUTOPAUSED == pauseType );
    }

    pushEntry( stack, &move );
    stack_freeEntry( stack, &move );
}

static XP_Bool
setCacheReadyFor( StackCtxt* stack, XP_U16 nn )
{
    XP_U16 ii;
    
    stream_setPos( stack->data, POS_READ, START_OF_STREAM );
    for ( ii = 0; ii < nn; ++ii ) {
        StackEntry dummy;
        readEntry( stack, &dummy );
        stack_freeEntry( stack, &dummy );
    }

    stack->cacheNext = nn;
    stack->cachedPos = stream_getPos( stack->data, POS_READ );

    return XP_TRUE;
} /* setCacheReadyFor */

XP_U16
stack_getNEntries( const StackCtxt* stack )
{
    return stack->nEntries;
} /* stack_getNEntries */

XP_Bool
stack_getNthEntry( StackCtxt* stack, const XP_U16 nn, StackEntry* entry )
{
    XP_Bool found;

    if ( nn >= stack->nEntries ) {
        found = XP_FALSE;
    } else if ( stack->cacheNext != nn ) {
        XP_ASSERT( !!stack->data );
        found = setCacheReadyFor( stack, nn );
        XP_ASSERT( stack->cacheNext == nn );
    } else {
        found = XP_TRUE;
    }

    if ( found ) {
        XWStreamPos oldPos = stream_setPos( stack->data, POS_READ, 
                                            stack->cachedPos );

        readEntry( stack, entry );
        entry->moveNum = (XP_U8)nn;

        stack->cachedPos = stream_setPos( stack->data, POS_READ, oldPos );
        ++stack->cacheNext;

        /* XP_LOGF( "%s(%d) (typ=%s, player=%d, num=%d)", __func__, nn, */
        /*          StackMoveType_2str(entry->moveType), entry->playerNum, entry->moveNum ); */
    }
    return found;
} /* stack_getNthEntry */

static XP_Bool
popEntryImpl( StackCtxt* stack, StackEntry* entry )
{
    XP_U16 nn = stack->nEntries - 1;
    XP_Bool found = stack_getNthEntry( stack, nn, entry );
    if ( found ) {
        stack->nEntries = nn;

        setCacheReadyFor( stack, nn ); /* set cachedPos by side-effect */
        stack->top = stack->cachedPos;
    }
    return found;
}

XP_Bool
stack_popEntry( StackCtxt* stack, StackEntry* entry )
{
    XP_Bool result = popEntryImpl( stack, entry );
    if ( result ) {
        XP_LOGFF( "hash now %X", stack_getHash( stack ) );
    }
    return result;
} /* stack_popEntry */

XP_S16
stack_getNextTurn( StackCtxt* stack )
{
    XP_ASSERT( !stack->inDuplicateMode );
    XP_S16 result = -1;
    XP_U16 nn = stack->nEntries - 1;

    StackEntry dummy;
    if ( stack_getNthEntry( stack, nn, &dummy ) ) {
        result = (dummy.playerNum + 1) % stack->nPlayers;
        stack_freeEntry( stack, &dummy );
    }

    // LOG_RETURNF( "%d", result );
    return result;
}

XP_Bool
stack_redo( StackCtxt* stack, StackEntry* entry )
{
    XP_Bool canRedo = (stack->nEntries + 1) <= stack->highWaterMark;
    if ( canRedo ) {
        ++stack->nEntries;
        if ( NULL != entry ) {
            stack_getNthEntry( stack, stack->nEntries-1, entry );
        }
        setCacheReadyFor( stack, stack->nEntries );
        stack->top = stack->cachedPos;
    }
    return canRedo;
} /* stack_redo */

void
stack_freeEntry( StackCtxt* XP_UNUSED_DBG(stack), StackEntry* entry )
{
    XP_ASSERT( entry->moveType != __BOGUS );
    switch( entry->moveType ) {
    case PAUSE_TYPE:
        XP_FREEP( stack->mpool, &entry->u.pause.msg );
        break;
    }
    entry->moveType = __BOGUS;
}

#ifdef CPLUS
}
#endif
