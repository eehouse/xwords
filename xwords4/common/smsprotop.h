/* 
 * Copyright 2018 by Eric House (xwords@eehouse.org). All rights reserved.
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
#include "nli.h"

typedef struct SMSProto SMSProto;

typedef enum { NONE, INVITE, DATA, DEATH, ACK_INVITE, } SMS_CMD;

/* Unpacked/local format with relevant fields exposed */
typedef struct _SMSMsgLoc {
    // XP_U16 msgID;
    SMS_CMD cmd;
    XP_U32 gameID;
    XP_U16 len;
    XP_U8* data; // will be NetLaunchInfo* if cmd == INVITE
} SMSMsgLoc;

/* Flattened format suitable for transmission over wire. Encapsulates Loc
   format data */
typedef struct _SMSMsgNet {
    // XP_U16 msgID;
    XP_U16 len;
    XP_U8* data;
} SMSMsgNet;

typedef enum { FORMAT_NONE, FORMAT_LOC, FORMAT_NET } SMS_FORMAT;

typedef struct _SMSMsgArray {
    XP_U16 nMsgs;
    SMS_FORMAT format;
    union {
        SMSMsgNet* msgsNet;
        SMSMsgLoc* msgsLoc;
    } u;
} SMSMsgArray;

SMSProto* smsproto_init( XW_DUtilCtxt* dutil, XWEnv xwe );
void smsproto_free( SMSProto* state );


/* Return ptr to structure if one's ready to be sent, otherwise null. Caller *
 * should interpret null as meaning it's meant to call again. To support that,
 * null buf is legit.
 *
 * When send() returns a non-null value, that value must be passed to
 * freeMsgArray() or there will be leakage.
*/

SMSMsgArray* smsproto_prepOutbound( SMSProto* state, XWEnv xwe, SMS_CMD cmd,
                                    XP_U32 gameID, const void* buf, XP_U16 buflen,
                                    const XP_UCHAR* toPhone, int port,
                                    XP_Bool forceOld, XP_U16* waitSecs );

/* When a message is received, pass it in for reassambly. Non-null return
   means one or more messages is ready for consumption. */
SMSMsgArray* smsproto_prepInbound( SMSProto* state, XWEnv xwe, const XP_UCHAR* fromPhone,
                                   XP_U16 wantPort, const XP_U8* data, XP_U16 len );

void smsproto_freeMsgArray( SMSProto* state, SMSMsgArray* arr );

# ifdef DEBUG
void smsproto_runTests( XW_DUtilCtxt* dutil, XWEnv xwe );
# else
#  define smsproto_runTests(p1,p2 )
# endif
#endif
