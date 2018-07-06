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

#ifndef __SMSPROTO_H__
#define __SMSPROTO_H__

// The sms protocol is getting more complicated as I add message combining. So
// let's try to move it into C where it can be tested in linux.
//
// How the protocol works. Clients want to send data packets to phones and
// receive from phones. They use a send(byte[] data, String phone) method, and
// provide a callback that's called when packets arrive. Internally this
// module buffers messages on a per-number basis (new post-java feature) and
// periodically, based on a timer that's set when buffered data is waiting to
// be sent, gathers messages and transmits them. Probably it'll wait a few
// seconds after the last message was enqueued, the idea being that SMS is not
// expected to be fast and that sending lots of small messages is bad.
//
// Because there's a max size to SMS messages any [combined] message that's
// too big is broken up. To keep things simple the two processes, combining
// and breaking, are independent; there's no attempt to avoid combining
// messages even when doing so creates something that will have to be broken
// up.
//
// Received messages (buffers) are recombined (if the result of breaking up)
// then split (if the result of combining), and the constituent data packets
// are returned to the app as an array of byte[].

#include "xptypes.h"
#include "mempool.h" /* debug only */

typedef struct SMSProto SMSProto;

typedef struct _SMSMsg {
    XP_U16 len;
    XP_U16 msgID;
    XP_U8* data;
} SMSMsg;

typedef struct _SMSMsgArray {
    XP_U16 nMsgs;
    SMSMsg* msgs;
} SMSMsgArray;

struct SMSProto* smsproto_init( MPFORMAL XW_DUtilCtxt* dutil );
void smsproto_free( SMSProto* state );

/* Return ptr to structure if one's ready to be sent, otherwise null. Caller *
 * should interpret null as meaning it's meant to call again. To support that,
 * null buf is legit.
 *
 * When send() returns a non-null value, that value must be passed to
 * freeMsgArray() or there will be leakage.
*/
SMSMsgArray* smsproto_prepOutbound( SMSProto* state, const XP_U8* buf,
                                    XP_U16 buflen, const XP_UCHAR* toPhone,
                                    XP_Bool forceOld, XP_U16* waitSecs );

/* When a message is received, pass it in for reassambly. Non-null return
   means one or more messages is ready for consumption. */
SMSMsgArray* smsproto_prepInbound( SMSProto* state, const XP_UCHAR* fromPhone,
                                   const XP_U8* data, XP_U16 len );

void smsproto_freeMsgArray( SMSProto* state, SMSMsgArray* arr );

# ifdef DEBUG
void smsproto_runTests( MPFORMAL XW_DUtilCtxt* dutil );
# endif
#endif
