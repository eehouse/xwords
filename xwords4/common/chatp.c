/* 
 * Copyright 2024 by Eric House (xwords@eehouse.org).  All rights
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
n *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "chatp.h"
#include "xwarray.h"
#include "strutils.h"
#include "device.h"
#include "util.h"

typedef struct _ChatEntry {
    XP_UCHAR* msg;
    XP_S16 from;
    XP_U32 timestamp;
} ChatEntry;

struct ChatState {
    XWArray* entries;
    XW_UtilCtxt** utilp;
    MPSLOT
};

static int
sortByTimestamp(const void* dl1, const void* dl2,
                XWEnv XP_UNUSED(xwe), void* XP_UNUSED(closure))
{
    ChatEntry* ce1 = (ChatEntry*)dl1;
    ChatEntry* ce2 = (ChatEntry*)dl2;
    return ce1->timestamp - ce2->timestamp;
}

ChatState*
cht_init( XWEnv XP_UNUSED_DBG(xwe), XW_UtilCtxt** utilp )
{
#ifdef MEM_DEBUG
    MemPoolCtx* mpool = util_getMemPool( *utilp, xwe );
#endif
    ChatState* state = XP_CALLOC(mpool, sizeof(*state));
    state->utilp = utilp;
    state->entries = arr_make(mpool, sortByTimestamp, NULL);
    MPASSIGN( state->mpool, mpool );
    return state;
}

static void
addEntry( ChatState* chat, XWEnv xwe, const XP_UCHAR* msg,
          XP_U16 from, XP_U32 timestamp )
{
    ChatEntry* entry = XP_CALLOC( chat->mpool, sizeof(*entry) );
    entry->msg = copyString( chat->mpool, msg );
    entry->from = from;
    entry->timestamp = timestamp;
    arr_insert( chat->entries, xwe, entry );
}

static void
disposeEntry( void* elem, void* closure )
{
#ifdef MEM_DEBUG
    ChatState* state = (ChatState*)closure;
    MemPoolCtx* mpool = state->mpool;
#else
    XP_USE(closure);
#endif
    ChatEntry* entry = (ChatEntry*)elem;
    XP_FREE( mpool, entry->msg );
    XP_FREE( mpool, entry );
}

void
cht_destroy( ChatState* chat )
{
    cht_deleteAll( chat );
    arr_destroy( chat->entries );
    XP_FREE( chat->mpool, chat );
}

static ForEachAct
writeChatProc( void* elem, void* closure, XWEnv XP_UNUSED(xwe) )
{
    XWStreamCtxt* stream = (XWStreamCtxt*)closure;
    ChatEntry* ce = (ChatEntry*)elem;
    stringToStream( stream, ce->msg );
    XP_LOGFF( "wrote msg: %s", ce->msg );
    XP_ASSERT( 0 <= ce->from );
    stream_putU8( stream, ce->from );
    stream_putU32( stream, ce->timestamp );
    return FEA_OK;
}

XP_Bool
cht_haveToWrite( const ChatState* state )
{
    XP_Bool result = !!state
        && !!state->entries
        && 0 < arr_length( state->entries );
    return result;
}

void
cht_writeToStream( const ChatState* state, XWEnv xwe, XWStreamCtxt* stream )
{
    XP_ASSERT( cht_haveToWrite(state) );
    XP_U16 count = arr_length( state->entries );
    if ( 0 == count ) {
        XP_ASSERT( 0 );      /* cht_haveToWrite catches this */
    } else {
        XWStreamCtxt* tmpStream =
            dvc_makeStream( util_getDevUtilCtxt(*state->utilp) );

        arr_map( state->entries, xwe, writeChatProc, tmpStream );

        XP_U16 size = stream_getSize( tmpStream );
        stream_putU32VL( stream, size );
        stream_putBytes( stream, stream_getPtr(tmpStream), size );

        stream_destroy( tmpStream );
    }
}

void
cht_loadFromStream( ChatState* chat, XWEnv xwe, XWStreamCtxt* stream )
{
    XP_U32 size = stream_getU32VL( stream ); /* size/count of 0 */
    XP_ASSERT( 0 == arr_length( chat->entries ) );
    XP_ASSERT( 0 < size );
    if ( 0 < size ) {
        XP_U8* bytes = XP_MALLOC( chat->mpool, size );
        stream_getBytes( stream, bytes, size );

        XW_DUtilCtxt* dutil = util_getDevUtilCtxt(*chat->utilp);
        XWStreamCtxt* tmpStream = dvc_makeStream( dutil );
        stream_putBytes( tmpStream, bytes, size );
        XP_FREE( chat->mpool, bytes );

        while ( 0 < stream_getSize(tmpStream) ) {
            XP_UCHAR* msg = stringFromStream( chat->mpool, tmpStream );
            if ( !!msg ) {
                XP_LOGFF( "read msg: %s", msg );
                XP_U8 from = stream_getU8( tmpStream );
                XP_U32 timestamp = stream_getU32( tmpStream );
                addEntry( chat, xwe, msg, from, timestamp );
                XP_FREE( chat->mpool, msg );
            }
        }

        stream_destroy( tmpStream );
    }
}

void
cht_chatReceived( ChatState* state, XWEnv xwe, XP_UCHAR* msg,
                  XP_S16 from, XP_U32 timestamp )
{
    XP_LOGFF( "got msg: %s", msg );
    addEntry( state, xwe, msg, from, timestamp );
}

XP_U16
cht_countChats( ChatState* state )
{
    return arr_length( state->entries );
}

void
cht_getChat( ChatState* state, XP_U16 nn, XP_UCHAR* buf, XP_U16* bufLen,
             XP_S16* from, XP_U32* timestamp )
{
    ChatEntry* entry = (ChatEntry*)arr_getNth( state->entries, nn );
    XP_ASSERT( !!entry );
    *from = entry->from;
    if ( !!timestamp ) {
        *timestamp = entry->timestamp;
    }
    int wrote = XP_SNPRINTF( buf, *bufLen, "%s", entry->msg );
    if ( wrote < *bufLen ) {
        XP_ASSERT( buf[wrote] == '\0' );
        *bufLen = wrote;
    }
}

void
cht_deleteAll( ChatState* state )
{
    arr_removeAll( state->entries, disposeEntry, state );
}

void
cht_addChat( ChatState* state, XWEnv xwe, const XP_UCHAR* msg,
             XP_S16 from, XP_U32 timestamp )
{
    addEntry( state, xwe, msg, from, timestamp );
}
