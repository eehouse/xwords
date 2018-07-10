/* -*- compile-command: "cd ../linux && make MEMDEBUG=TRUE -j3"; -*- */
/* 
 * Copyright 2018 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#include <unistd.h>
#include <pthread.h>

#include "util.h"
#include "smsproto.h"
#include "comtypes.h"
#include "strutils.h"

# define MAX_WAIT 5
// # define MAX_MSG_LEN 50         /* for testing */
# define MAX_LEN_BINARY 115
/* PENDING: Might want to make SEND_NOW_SIZE smaller; might as well send now
   if even the smallest new message is likely to put us over. */
# define SEND_NOW_SIZE MAX_LEN_BINARY
# define SMS_PROTO_VERSION 1
# define SMS_PROTO_VERSION_COMBO 2

typedef struct _MsgRec {
    XP_U32 createSeconds;
    SMSMsg msg;
} MsgRec;

typedef struct _ToPhoneRec {
    XP_UCHAR phone[32];
    XP_U32 createSeconds;
    XP_U16 nMsgs;
    XP_U16 totalSize;
    MsgRec** msgs;
} ToPhoneRec;

typedef struct _MsgIDRec {
    int msgID;
    int count;
    struct {
        XP_U16 len;
        XP_U8* data;
    }* parts;
} MsgIDRec;

typedef struct _FromPhoneRec {
    XP_UCHAR phone[32];
    int nMsgIDs;
    MsgIDRec* msgIDRecs;
} FromPhoneRec;

struct SMSProto {
    XW_DUtilCtxt* dutil;
    pthread_t creator;
    XP_U16 nNextID;
    int lastStoredSize;
    XP_U16 nToPhones;
    ToPhoneRec* toPhoneRecs;

    int nFromPhones;
    FromPhoneRec* fromPhoneRecs;

    MPSLOT;
};

#define KEY_PARTIALS PERSIST_KEY("partials")
#define KEY_NEXTID PERSIST_KEY("nextID")

static int nextMsgID( SMSProto* state );
static SMSMsgArray* toMsgs( SMSProto* state, ToPhoneRec* rec, XP_Bool forceOld );
static ToPhoneRec* getForPhone( SMSProto* state, const XP_UCHAR* phone,
                                XP_Bool create );
static void addToRec( SMSProto* state, ToPhoneRec* rec, const XP_U8* buf,
                      XP_U16 buflen, XP_U32 nowSeconds );
static void addMessage( SMSProto* state, const XP_UCHAR* fromPhone, int msgID,
                        int indx, int count, const XP_U8* data, XP_U16 len );
static SMSMsgArray* completeMsgs( SMSProto* state, SMSMsgArray* arr,
                                  const XP_UCHAR* fromPhone, int msgID );
static void savePartials( SMSProto* state );
static void restorePartials( SMSProto* state );
static void rmFromPhoneRec( SMSProto* state, int fromPhoneIndex );
static void freeMsgIDRec( SMSProto* state, MsgIDRec* rec, int fromPhoneIndex,
                          int msgIDIndex );
static void freeForPhone( SMSProto* state, const XP_UCHAR* phone );
static void freeMsg( SMSProto* state, MsgRec** msg );
static void freeRec( SMSProto* state, ToPhoneRec* rec );
#ifdef DEBUG
static void checkThread( SMSProto* state );
#else
# define checkThread(p)
#endif

SMSProto*
smsproto_init( MPFORMAL XW_DUtilCtxt* dutil )
{
    SMSProto* state = (SMSProto*)XP_CALLOC( mpool, sizeof(*state) );
    state->dutil = dutil;
    // checkThread( state ); <-- Android's calling this on background thread now
    MPASSIGN( state->mpool, mpool );

    XP_U16 siz = sizeof(state->nNextID);
    dutil_loadPtr( state->dutil, KEY_NEXTID, &state->nNextID, &siz );
    XP_LOGF( "%s(): loaded nextMsgID: %d", __func__, state->nNextID );

    restorePartials( state );

    return state;
}

void
smsproto_free( SMSProto* state )
{
    if ( NULL != state ) {
        // checkThread( state ); <-- risky (see above)
        XP_ASSERT( state->creator == 0 || state->creator == pthread_self() );

        for ( XP_U16 ii = 0; ii < state->nToPhones; ++ii ) {
            freeRec( state, &state->toPhoneRecs[ii] );
        }
        XP_FREEP( state->mpool, &state->toPhoneRecs );

        if ( 0 < state->nFromPhones ) {
            XP_LOGF( "%s(): freeing undelivered partial messages", __func__ );
        }
        while (0 < state->nFromPhones) {
            FromPhoneRec* ffr = &state->fromPhoneRecs[0];
            while ( 0 < ffr->nMsgIDs ) {
                freeMsgIDRec( state, &ffr->msgIDRecs[0], 0, 0 );
            }
        }
        XP_ASSERT( !state->fromPhoneRecs ); /* above nulls this once empty */

        XP_FREEP( state->mpool, &state );
    }
}

/* Maintain a list of pending messages per phone number. When called and it's
 * been at least some amount of time since we last added something, or at
 * least some longer time since the oldest message was added, return an array
 * of messages ready to send via the device's raw SMS (i.e. respecting its
 * size limits.)

 * Pass in the current time, as that's easier than keeping an instance of
 * UtilCtxt around.
 */
SMSMsgArray*
smsproto_prepOutbound( SMSProto* state, const XP_U8* buf,
                       XP_U16 buflen, const XP_UCHAR* toPhone,
                       XP_Bool forceOld, XP_U16* waitSecsP )
{
    SMSMsgArray* result = NULL;
    XP_LOGF( "%s(): len=%d, toPhone=%s", __func__, buflen, toPhone );
    checkThread( state );
    ToPhoneRec* rec = getForPhone( state, toPhone, !!buf );

    /* First, add the new message (if present) to the array */
    XP_U32 nowSeconds = dutil_getCurSeconds( state->dutil );
    if ( !!buf ) {
        addToRec( state, rec, buf, buflen, nowSeconds );
    }

    /* rec will be non-null if there's something in it */
    XP_Bool doSend = XP_FALSE;
    if (rec != NULL) {
        if ( forceOld ) {
            doSend = XP_TRUE;
        } else if ( rec->totalSize > SEND_NOW_SIZE || nowSeconds - rec->createSeconds > MAX_WAIT ) {
            doSend = XP_TRUE;
        }
        /* other criteria? */
    }

    /* Then, see if the array itself is old enough OR if it's been long enough
       since the last was added */
    if ( doSend ) {
        result = toMsgs( state, rec, forceOld );
        freeForPhone( state, toPhone );
    }

    XP_U16 waitSecs = 0;
    if ( !result && !!rec && (rec->nMsgs > 0) ) {
        waitSecs = MAX_WAIT - (nowSeconds - rec->createSeconds);
    }
    *waitSecsP = waitSecs;

    XP_LOGF( "%s() => %p (len=%d, *waitSecs=%d)", __func__, result,
             !!result ? result->nMsgs : 0, *waitSecsP );
    return result;
}

static SMSMsgArray*
appendMsg( SMSProto* state, SMSMsgArray* arr, SMSMsg* msg )
{
    if ( NULL == arr ) {
        arr = XP_CALLOC( state->mpool, sizeof(*arr) );
    }

    arr->msgs = XP_REALLOC( state->mpool, arr->msgs,
                            (arr->nMsgs + 1) * sizeof(*arr->msgs) );
    arr->msgs[arr->nMsgs++] = *msg;
    return arr;
}

SMSMsgArray*
smsproto_prepInbound( SMSProto* state, const XP_UCHAR* fromPhone,
                      const XP_U8* data, XP_U16 len )
{
    XP_LOGF( "%s(): len=%d, fromPhone=%s", __func__, len, fromPhone );
    checkThread( state );

    SMSMsgArray* result = NULL;
    int offset = 0;
    int proto = data[offset++];
    switch ( proto ) {
    case SMS_PROTO_VERSION: {
        int msgID = data[offset++];
        int indx = data[offset++];
        int count = data[offset++];
        /* XP_LOGF( "%s(len=%d, fromPhone=%s)): proto=%d, id=%d, indx=%d of %d)", */
        /*          __func__, len, fromPhone, proto, msgID, indx, count ); */
        addMessage( state, fromPhone, msgID, indx, count, data + offset, len - offset );
        result = completeMsgs( state, result, fromPhone, msgID );
        savePartials( state );
    }
        break;
    case SMS_PROTO_VERSION_COMBO:
        while ( offset < len ) {
            int oneLen = data[offset++];
            int msgID = data[offset++];
            SMSMsg msg = { .len = oneLen,
                           .msgID = msgID,
                           .data = XP_MALLOC( state->mpool, oneLen ),
            };
            XP_MEMCPY( msg.data, &data[offset], oneLen );
            offset += oneLen;

            result = appendMsg( state, result, &msg );
        }
        break;
    default:
        XP_ASSERT(0);
    }
    XP_LOGF( "%s() => %p (len=%d)", __func__, result, (!!result) ? result->nMsgs : 0 );
    return result;
}

void
smsproto_freeMsgArray( SMSProto* state, SMSMsgArray* arr )
{
    checkThread( state );

    for ( int ii = 0; ii < arr->nMsgs; ++ii ) {
        XP_FREEP( state->mpool, &arr->msgs[ii].data );
    }

    XP_FREEP( state->mpool, &arr->msgs );
    XP_FREEP( state->mpool, &arr );
}

static void
freeMsg( SMSProto* state, MsgRec** msgp )
{
    XP_FREEP( state->mpool, &(*msgp)->msg.data );
    XP_FREEP( state->mpool, msgp );
}

static void
freeRec( SMSProto* state, ToPhoneRec* rec )
{
    for ( XP_U16 jj = 0; jj < rec->nMsgs; ++jj ) {
        freeMsg( state, &rec->msgs[jj] );
    }
    XP_FREEP( state->mpool, &rec->msgs );
}

static ToPhoneRec*
getForPhone( SMSProto* state, const XP_UCHAR* phone, XP_Bool create )
{
    ToPhoneRec* rec = NULL;
    for ( XP_U16 ii = 0; !rec && ii < state->nToPhones; ++ii ) {
        if ( 0 == XP_STRCMP( state->toPhoneRecs[ii].phone, phone ) ) {
            rec = &state->toPhoneRecs[ii];
        }
    }

    if ( !rec && create ) {
        state->toPhoneRecs = XP_REALLOC( state->mpool, state->toPhoneRecs,
                                         (1 + state->nToPhones) * sizeof(*state->toPhoneRecs) );
        rec = &state->toPhoneRecs[state->nToPhones++];
        XP_MEMSET( rec, 0, sizeof(*rec) );
        XP_STRCAT( rec->phone, phone );
    }

    return rec;
}

static void
freeForPhone( SMSProto* state, const XP_UCHAR* phone )
{
    for ( XP_U16 ii = 0; ii < state->nToPhones; ++ii ) {
        if ( 0 == XP_STRCMP( state->toPhoneRecs[ii].phone, phone ) ) {
            freeRec( state, &state->toPhoneRecs[ii] );

            XP_U16 nAbove = state->nToPhones - ii - 1;
            XP_ASSERT( nAbove >= 0 );
            if ( nAbove > 0 ) {
                XP_MEMMOVE( &state->toPhoneRecs[ii], &state->toPhoneRecs[ii+1],
                            nAbove * sizeof(*state->toPhoneRecs) );
            }
            --state->nToPhones;
            if ( 0 == state->nToPhones ) {
                XP_FREEP( state->mpool, &state->toPhoneRecs );
            } else {
                state->toPhoneRecs = XP_REALLOC( state->mpool, state->toPhoneRecs,
                                                 state->nToPhones * sizeof(*state->toPhoneRecs) );
            }
            break;
        }
    }
}

static void
addToRec( SMSProto* state, ToPhoneRec* rec, const XP_U8* buf, XP_U16 buflen,
          XP_U32 nowSeconds )
{
    MsgRec* mRec = XP_CALLOC( state->mpool, sizeof(*rec) );
    mRec->msg.len = buflen;
    mRec->msg.data = XP_MALLOC( state->mpool, buflen );
    XP_MEMCPY( mRec->msg.data, buf, buflen );
    mRec->createSeconds = nowSeconds;

    rec->msgs = XP_REALLOC( state->mpool, rec->msgs, (1 + rec->nMsgs) * sizeof(*rec->msgs) );
    rec->msgs[rec->nMsgs++] = mRec;
    rec->totalSize += buflen;
    XP_LOGF( "%s(): added msg to %s of len %d; total now %d", __func__, rec->phone,
             buflen, rec->totalSize );

    if ( rec->nMsgs == 1 ) {
        rec->createSeconds = nowSeconds;
    }
}

static MsgIDRec*
getMsgIDRec( SMSProto* state, const XP_UCHAR* fromPhone, int msgID,
             XP_Bool addMissing, int* fromPhoneIndex, int* msgIDIndex )
{
    MsgIDRec* result = NULL;

    FromPhoneRec* fromPhoneRec = NULL;
    for ( int ii = 0; ii < state->nFromPhones; ++ii ) {
        if ( 0 == XP_STRCMP( state->fromPhoneRecs[ii].phone, fromPhone ) ) {
            fromPhoneRec = &state->fromPhoneRecs[ii];
            *fromPhoneIndex = ii;
            break;
        }
    }

    // create and add if not found
    if ( NULL == fromPhoneRec && addMissing ) {
        state->fromPhoneRecs =
            XP_REALLOC( state->mpool, state->fromPhoneRecs,
                        (state->nFromPhones + 1) * sizeof(*state->fromPhoneRecs) );
        *fromPhoneIndex = state->nFromPhones;
        fromPhoneRec = &state->fromPhoneRecs[state->nFromPhones++];
        XP_MEMSET( fromPhoneRec, 0, sizeof(*fromPhoneRec) );
        XP_STRCAT( fromPhoneRec->phone, fromPhone );
    }

    // Now find msgID record
    if ( NULL != fromPhoneRec ) {
        for ( int ii = 0; ii < fromPhoneRec->nMsgIDs; ++ii ) {
            if ( fromPhoneRec->msgIDRecs[ii].msgID == msgID ) {
                result = &fromPhoneRec->msgIDRecs[ii];
                *msgIDIndex = ii;
                break;
            }
        }

        // create and add if not found
        if ( NULL == result && addMissing ) {
            fromPhoneRec->msgIDRecs = XP_REALLOC( state->mpool, fromPhoneRec->msgIDRecs,
                                                  (fromPhoneRec->nMsgIDs + 1)
                                                  * sizeof(*fromPhoneRec->msgIDRecs) );
            MsgIDRec newRec = { .msgID = msgID };
            *msgIDIndex = fromPhoneRec->nMsgIDs;
            result = &fromPhoneRec->msgIDRecs[fromPhoneRec->nMsgIDs];
            fromPhoneRec->msgIDRecs[fromPhoneRec->nMsgIDs++] = newRec;
        }
    }

    return result;
}

/* Messages that are split gather here until complete
 */
static void
addMessage( SMSProto* state, const XP_UCHAR* fromPhone, int msgID, int indx,
            int count, const XP_U8* data, XP_U16 len )
{
    XP_LOGF( "phone=%s, msgID=%d, %d/%d", fromPhone, msgID, indx, count );
    XP_ASSERT( 0 < len );
    int ignore;
    MsgIDRec* msgIDRec = getMsgIDRec( state, fromPhone, msgID, XP_TRUE,
                                      &ignore, &ignore );
    /* if it's new, fill in missing fields */
    if ( msgIDRec->count == 0 ) {
        msgIDRec->count = count;    /* in case it's new */
        msgIDRec->parts = XP_CALLOC( state->mpool, count * sizeof(*msgIDRec->parts));
    } else {
        XP_ASSERT( count == msgIDRec->count );
    }

    XP_ASSERT( msgIDRec->parts[indx].len == 0
               || msgIDRec->parts[indx].len == len ); /* replace with same ok */
    msgIDRec->parts[indx].len = len;
    XP_FREEP( state->mpool, &msgIDRec->parts[indx].data ); /* in case non-null (replacement) */
    msgIDRec->parts[indx].data = XP_MALLOC( state->mpool, len );
    XP_MEMCPY( msgIDRec->parts[indx].data, data, len );
}

static void
rmFromPhoneRec( SMSProto* state, int fromPhoneIndex )
{
    FromPhoneRec* fromPhoneRec = &state->fromPhoneRecs[fromPhoneIndex];
    XP_ASSERT( fromPhoneRec->nMsgIDs == 0 );
    XP_FREEP( state->mpool, &fromPhoneRec->msgIDRecs );

    if ( --state->nFromPhones == 0 ) {
        XP_FREEP( state->mpool, &state->fromPhoneRecs );
    } else {
        XP_U16 nAbove = state->nFromPhones - fromPhoneIndex;
        XP_ASSERT( nAbove >= 0 );
        if ( nAbove > 0 ) {
            XP_MEMMOVE( &state->fromPhoneRecs[fromPhoneIndex], &state->fromPhoneRecs[fromPhoneIndex+1],
                        nAbove * sizeof(*state->fromPhoneRecs) );
        }
        state->fromPhoneRecs = XP_REALLOC( state->mpool, state->fromPhoneRecs,
                                           state->nFromPhones * sizeof(*state->fromPhoneRecs));
    }
}

static void
freeMsgIDRec( SMSProto* state, MsgIDRec* rec, int fromPhoneIndex, int msgIDIndex )
{
    FromPhoneRec* fromPhoneRec = &state->fromPhoneRecs[fromPhoneIndex];
    MsgIDRec* msgIDRec = &fromPhoneRec->msgIDRecs[msgIDIndex];
    XP_ASSERT( msgIDRec == rec );

    for ( int ii = 0; ii < msgIDRec->count; ++ii ) {
        XP_FREEP( state->mpool, &msgIDRec->parts[ii].data );
    }
    XP_FREEP( state->mpool, &msgIDRec->parts );

    if ( --fromPhoneRec->nMsgIDs > 0 ) {
        XP_U16 nAbove = fromPhoneRec->nMsgIDs - msgIDIndex;
        XP_ASSERT( nAbove >= 0 );
        if ( nAbove > 0 ) {
            XP_MEMMOVE( &fromPhoneRec->msgIDRecs[msgIDIndex], &fromPhoneRec->msgIDRecs[msgIDIndex+1],
                        nAbove * sizeof(*fromPhoneRec->msgIDRecs) );
        }
        fromPhoneRec->msgIDRecs = XP_REALLOC( state->mpool, fromPhoneRec->msgIDRecs,
                                              fromPhoneRec->nMsgIDs
                                              * sizeof(*fromPhoneRec->msgIDRecs));
    } else {
        rmFromPhoneRec( state, fromPhoneIndex );
    }
}

static void
savePartials( SMSProto* state )
{
    checkThread( state );

    XWStreamCtxt* stream
        = mem_stream_make_raw( MPPARM(state->mpool)
                               dutil_getVTManager(state->dutil) );
    stream_putU8( stream, 0 );  /* version */

    stream_putU8( stream, state->nFromPhones );
    for ( int ii = 0; ii < state->nFromPhones; ++ii ) {
        const FromPhoneRec* rec = &state->fromPhoneRecs[ii];
        stringToStream( stream, rec->phone );
        stream_putU8( stream, rec->nMsgIDs );
        for ( int jj = 0; jj < rec->nMsgIDs; ++jj ) {
            MsgIDRec* mir = &rec->msgIDRecs[jj];
            stream_putU16( stream, mir->msgID );
            stream_putU8( stream, mir->count );

            /* There's an array here. It may be sparse. Save a len of 0 */
            for ( int kk = 0; kk < mir->count; ++kk ) {
                int len = mir->parts[kk].len;
                stream_putU8( stream, len );
                stream_putBytes( stream, mir->parts[kk].data, len );
            }
        }
    }

    XP_U16 newSize = stream_getSize( stream );
    if ( state->lastStoredSize == 2 && newSize == 2 ) {
        XP_LOGF( "%s(): not storing empty again", __func__ );
    } else {
        dutil_store( state->dutil, KEY_PARTIALS, stream );
        state->lastStoredSize = newSize;
    }

    stream_destroy( stream );

    LOG_RETURN_VOID();
} /* savePartials */

static void
restorePartials( SMSProto* state )
{
    // LOG_FUNC();
    XWStreamCtxt* stream = mem_stream_make_raw( MPPARM(state->mpool)
                                                dutil_getVTManager(state->dutil) );
    dutil_load( state->dutil, KEY_PARTIALS, stream );
    if ( stream_getSize( stream ) >= 1 ) {
        XP_ASSERT( 0 == stream_getU8( stream ) );
        int nFromPhones = stream_getU8( stream );
        for ( int ii = 0; ii < nFromPhones; ++ii ) {
            XP_UCHAR phone[32];
            (void)stringFromStreamHere( stream, phone, VSIZE(phone) );
            int nMsgIDs = stream_getU8( stream );
            XP_LOGF( "%s(): got %d message records for phone %s", __func__,
                     nMsgIDs, phone );
            for ( int jj = 0; jj < nMsgIDs; ++jj ) {
                XP_U16 msgID = stream_getU16( stream );
                int count = stream_getU8( stream );
                XP_LOGF( "%s(): got %d records for msgID %d", __func__, count, msgID );
                for ( int kk = 0; kk < count; ++kk ) {
                    int len = stream_getU8( stream );
                    if ( 0 < len ) {
                        XP_U8 buf[len];
                        stream_getBytes( stream, buf, len );
                        addMessage( state, phone, msgID, kk, count, buf, len );
                    }
                }
            }
        }
    }
    stream_destroy( stream );

    // LOG_RETURN_VOID();
}

static SMSMsgArray*
completeMsgs( SMSProto* state, SMSMsgArray* arr, const XP_UCHAR* fromPhone,
              int msgID )
{
    int fromPhoneIndex, msgIDIndex;
    MsgIDRec* rec = getMsgIDRec( state, fromPhone, msgID, XP_FALSE,
                                 &fromPhoneIndex, &msgIDIndex);
    if ( !rec ) {
        XP_LOGF( "%s(): no rec for phone %s, msgID %d", __func__, fromPhone, msgID );
        XP_ASSERT( 0 );
    }

    int len = 0;
    XP_Bool haveAll = XP_TRUE;
    for ( int ii = 0; ii < rec->count; ++ii ) {
        if ( rec->parts[ii].len == 0 ) {
            haveAll = XP_FALSE;
            break;
        } else {
            len += rec->parts[ii].len;
        }
    }

    if ( haveAll ) {
        SMSMsg msg = { .len = len,
                       .msgID = msgID,
                       .data = XP_MALLOC( state->mpool, len ),
        };
        XP_U8* ptr = msg.data;
        for ( int ii = 0; ii < rec->count; ++ii ) {
            XP_MEMCPY( ptr, rec->parts[ii].data, rec->parts[ii].len );
            ptr += rec->parts[ii].len;
        }
        arr = appendMsg( state, arr, &msg );

        freeMsgIDRec( state, rec, fromPhoneIndex, msgIDIndex );
    }

    return arr;
}

static SMSMsgArray*
toMsgs( SMSProto* state, ToPhoneRec* rec, XP_Bool forceOld )
{
    LOG_FUNC();
    SMSMsgArray* result = NULL;

    for ( XP_U16 ii = 0; ii < rec->nMsgs; ) {
        // XP_LOGF( "%s(): looking at msg %d of %d", __func__, ii, rec->nMsgs );
        XP_U16 count = (rec->msgs[ii]->msg.len + (MAX_LEN_BINARY-1)) / MAX_LEN_BINARY;

        /* First, see if this message and some number of its neighbors can be
           combined */
        int last = ii;
        int sum = 0;
        if ( count == 1 && !forceOld ) {
            for ( ; last < rec->nMsgs; ++last ) {
                int nextLen = rec->msgs[last]->msg.len;
                if ( sum + nextLen > MAX_LEN_BINARY ) {
                    break;
                }
                sum += nextLen;
            }
        }

        if ( last > ii ) {
            int nMsgs = last - ii;
            if ( nMsgs > 1 ) {
                XP_LOGF( "%s(): combining %d through %d (%d msgs)", __func__, ii,
                         last - 1, nMsgs );
            }
            int len = 1 + sum + (nMsgs * 2); /* 1: len & msgID */
            SMSMsg newMsg = { .len = len,
                              .data = XP_MALLOC( state->mpool, len )
            };
            int indx = 0;
            newMsg.data[indx++] = SMS_PROTO_VERSION_COMBO;
            for ( int jj = ii; jj < last; ++jj ) {
                const SMSMsg* msg = &rec->msgs[jj]->msg;
                newMsg.data[indx++] = msg->len;
                newMsg.data[indx++] = nextMsgID( state );
                XP_MEMCPY( &newMsg.data[indx], msg->data, msg->len ); /* bad! */
                indx += msg->len;
            }
            result = appendMsg( state, result, &newMsg );
            ii = last;
        } else {
            int msgID = nextMsgID( state );
            const SMSMsg* msg = &rec->msgs[ii]->msg;
            XP_U8* nextStart = msg->data;
            XP_U16 lenLeft = msg->len;
            for ( XP_U16 indx = 0; indx < count; ++indx ) {
                XP_ASSERT( lenLeft > 0 );
                XP_U16 useLen = lenLeft;
                if ( useLen >= MAX_LEN_BINARY ) {
                    useLen = MAX_LEN_BINARY;
                }
                lenLeft -= useLen;

                SMSMsg newMsg = { .len = useLen + 4,
                                  .data = XP_MALLOC( state->mpool, useLen + 4 )
                };
                newMsg.data[0] = SMS_PROTO_VERSION;
                newMsg.data[1] = msgID;
                newMsg.data[2] = indx;
                newMsg.data[3] = count;
                XP_MEMCPY( newMsg.data + 4, nextStart, useLen );
                nextStart += useLen;

                result = appendMsg( state, result, &newMsg );
            }
            ++ii;
        }
    }

    return result;
} /* toMsgs */

static int
nextMsgID( SMSProto* state )
{
    int result = ++state->nNextID % 0x000000FF;
    dutil_storePtr( state->dutil, KEY_NEXTID, &state->nNextID,
                    sizeof(state->nNextID) );
    LOG_RETURNF( "%d", result );
    return result;
}

#ifdef DEBUG
static void
checkThread( SMSProto* state )
{
    pthread_t curThread = pthread_self();
    if ( 0 == state->creator ) {
        state->creator = curThread;
    } else {
        /* If this is firing will need to use a mutex to make SMSProto access
           thread-safe */
        XP_ASSERT( state->creator == curThread );
    }
}

void
smsproto_runTests( MPFORMAL XW_DUtilCtxt* dutil )
{
    LOG_FUNC();
    SMSProto* state = smsproto_init( mpool, dutil );

    const int smallSiz = 20;
    const char* phones[] = {"1234", "3456", "5467", "9877"};
    const char* buf = "asoidfaisdfoausdf aiousdfoiu asodfu oiuasdofi oiuaosiduf oaisudf oiasd f"
        ";oiaisdjfljiojaklj asdlkjalskdjf laksjd flkjasdlfkj aldsjkf lsakdjf lkjsad flkjsd fl;kj"
        "asdifaoaosidfoiauosidufoaus doifuoaiusdoifu aoisudfoaisd foia sdoifuasodfu aosiud foiuas odfiu asd"
        "aosdoiaosdoiisidfoiosi isoidufoisu doifuoisud oiuoi98a90iu-asjdfoiasdfij"
        ;
    const XP_Bool forceOld = XP_TRUE;

    SMSMsgArray* arrs[VSIZE(phones)];
    for ( int ii = 0; ii < VSIZE(arrs); ++ii ) {
        arrs[ii] = NULL;
    }

    /* Loop until all the messages are ready. */
    for ( XP_Bool firstTime = XP_TRUE; ; firstTime = XP_FALSE) {
        XP_Bool allDone = XP_TRUE;
        for ( int ii = 0; ii < VSIZE(arrs); ++ii ) {
            XP_U16 waitSecs;
            if ( firstTime ) {
                XP_U16 len = (ii + 1) * 30;
                arrs[ii] = smsproto_prepOutbound( state, (XP_U8*)buf, len, phones[ii],
                                                  forceOld, &waitSecs );
            } else if ( NULL == arrs[ii]) {
                arrs[ii] = smsproto_prepOutbound( state, NULL, 0, phones[ii],
                                                  forceOld, &waitSecs );
            } else {
                continue;
            }
            allDone = allDone & (waitSecs == 0 && !!arrs[ii]);
        }
        if ( allDone ) {
            break;
        } else {
            (void)sleep( 2 );
        }
    }

    for ( int indx = 0; ; ++indx ) {
        XP_Bool haveOne = XP_FALSE;
        for ( int ii = 0; ii < VSIZE(arrs); ++ii ) {
            if (!!arrs[ii] && indx < arrs[ii]->nMsgs) {
                haveOne = XP_TRUE;
                SMSMsgArray* outArr = smsproto_prepInbound( state, phones[ii],
                                                            arrs[ii]->msgs[indx].data,
                                                            arrs[ii]->msgs[indx].len );
                if ( !!outArr ) {
                    SMSMsg* msg = &outArr->msgs[0];
                    XP_LOGF( "%s(): got msgID %d", __func__, msg->msgID );
                    XP_ASSERT( outArr->nMsgs == 1 );
                    XP_ASSERT( 0 == memcmp(buf, msg->data, (ii + 1) * 30) );
                    smsproto_freeMsgArray( state, outArr );

                    smsproto_freeMsgArray( state, arrs[ii] );
                    arrs[ii] = NULL;
                }
            }
        }
        if (!haveOne) {
            break;
        }
    }

    /* Now let's send a bunch of small messages that should get combined */
    for ( int nUsed = 0; ; ++nUsed ) {
        XP_U16 waitSecs;
        SMSMsgArray* sendArr = smsproto_prepOutbound( state, (XP_U8*)&buf[nUsed],
                                                      smallSiz, phones[0],
                                                      XP_FALSE, &waitSecs );
        if ( sendArr == NULL ) {
            XP_LOGF( "%s(): msg[%d] of len %d sent; still not ready", __func__, nUsed, smallSiz );
            continue;
        }

        XP_ASSERT( waitSecs == 0 );
        int totalBack = 0;
        for ( int jj = 0; jj < sendArr->nMsgs; ++jj ) {
            SMSMsgArray* recvArr = smsproto_prepInbound( state, phones[0],
                                                         sendArr->msgs[jj].data,
                                                         sendArr->msgs[jj].len );

            if ( !!recvArr ) {
                XP_LOGF( "%s(): got %d msgs (from %d)", __func__, recvArr->nMsgs, nUsed + 1 );
                for ( int kk = 0; kk < recvArr->nMsgs; ++kk ) {
                    SMSMsg* msg = &recvArr->msgs[kk];
                    XP_LOGF( "%s(): got msgID %d", __func__, msg->msgID );
                    XP_ASSERT( msg->len == smallSiz );
                    XP_ASSERT( 0 == memcmp( msg->data, &buf[totalBack], smallSiz ) );
                    ++totalBack;
                }

                smsproto_freeMsgArray( state, recvArr );
            }
        }
        XP_ASSERT( forceOld || totalBack == nUsed + 1 );
        XP_LOGF( "%s(): %d messages checked out", __func__, totalBack );
        smsproto_freeMsgArray( state, sendArr );
        break;
    }

    /* Now let's add a too-long message and unpack only the first part. Make
       sure it's cleaned up correctly */
    XP_U16 waitSecs;
    SMSMsgArray* arr = smsproto_prepOutbound( state, (XP_U8*)buf, 200, "33333", XP_TRUE, &waitSecs );
    XP_ASSERT( !!arr && arr->nMsgs > 1 );
    /* add only part 1 */
    SMSMsgArray* out = smsproto_prepInbound( state, "33333", arr->msgs[0].data, arr->msgs[0].len );
    XP_ASSERT( !out );
    smsproto_freeMsgArray( state, arr );


    /* now a message that's unpacked across multiple sessions to test store/load */
    XP_LOGF( "%s(): testing store/restore", __func__ );
    arr = smsproto_prepOutbound( state, (XP_U8*)buf, 200, "33333", XP_TRUE, &waitSecs );
    for ( int ii = 0; ii < arr->nMsgs; ++ii ) {
        SMSMsgArray* out = smsproto_prepInbound( state, "33333", arr->msgs[ii].data,
                                                 arr->msgs[ii].len );
        if ( !!out ) {
            XP_ASSERT( out->nMsgs == 1);
            XP_LOGF( "%s(): got the message on the %dth loop", __func__, ii );
            XP_ASSERT( out->msgs[0].len == 200 );
            XP_ASSERT( 0 == memcmp( out->msgs[0].data, buf, 200 ) );
            smsproto_freeMsgArray( state, out );
            break;
        }
        smsproto_free( state ); /* give it a chance to store state */
        state = smsproto_init( mpool, dutil );
    }

    /* Really bad to pass a different state than was created with, but now
       since only mpool is used and it's the same for all states, let it
       go. */
    smsproto_freeMsgArray( state, arr ); /* give it a chance to store state */

    smsproto_free( state );
    LOG_RETURN_VOID();
}
#endif
