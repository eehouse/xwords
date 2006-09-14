/* -*-mode: C; fill-column: 77; c-basic-offset: 4; -*- */
/* 
 * Copyright 2001 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#ifdef IR_SUPPORT

#include <TimeMgr.h>

#include "palmir.h"

#include "callback.h"
#include "xwords4defines.h"
#include "comms.h"
#include "memstream.h"
#include "palmutil.h"
#include "LocalizedStrIncludes.h"

# ifndef IR_EXCHMGR

#define IR_NO_TIMEOUT 0xFFFFFFFF
#if 1
# define IR_TIMEOUT (15*60)	/* 10 seconds during debugging */
#else
# define IR_TIMEOUT (150*60)	/* 100 seconds during debugging */
#endif

#define RESET_TIMER(g)  (g)->ir_timeout = TimGetTicks() + IR_TIMEOUT

struct MyIrPacket {
    IrPacket packet;
    struct MyIrPacket* next;
    Boolean in_use;
};

/***************************************************************************/
static void receiveData( PalmAppGlobals* globals, UInt8* buf, UInt16 len );
#define addFreeQueue( g,p ) (--(g)->irSendQueueLen)
static void addToSendQueue( PalmAppGlobals* globals, MyIrPacket* packet );
static void clearSendQueue( PalmAppGlobals* globals );
static MyIrPacket* getFreeSendPacket( PalmAppGlobals* globals );
#ifdef DEBUG
static void printStateTransition( PalmAppGlobals* globals );
static void assert_state1( PalmAppGlobals* globals,  short line, 
			   IR_STATE assertState );
static void assert_state2( PalmAppGlobals* globals, short line, 
			   IR_STATE assertState1,IR_STATE assertState2 );
#else
#define printStateTransition( globals )
#define assert_state1( globals,  line, assertState )
#define assert_state2( globals, line, assertState1, assertState2 )
#endif
/***************************************************************************/

static Boolean
storeDiscovery( PalmAppGlobals* globals, IrDeviceList* deviceList,
		IrConnect* con )
{
    short i;

    XP_ASSERT( deviceList->nItems <= 1 );

    for ( i = 0; i < deviceList->nItems; ++i ) {
        globals->irDev = deviceList->dev[i].hDevice;
        XP_ASSERT( &globals->irC_out.irCon == con );
        con->rLsap = deviceList->dev[i].xid[0];
#ifdef DEBUG
        globals->save_rLsap = con->rLsap;
#endif
    }
    return deviceList->nItems > 0;
} /* storeDiscovery */

void
ir_callback_out( IrConnect* con, IrCallBackParms* parms )
{
    PalmAppGlobals* globals;
    IrStatus status;

    CALLBACK_PROLOGUE();

    globals = ((MyIrConnect*)con)->globals;

    switch ( parms->event ) {

    case LEVENT_LAP_DISCON_IND:	/* IrLAP connection has gone down, or
                                   IrConnectIrLap failed */

        XP_STATUSF( "LAP_DISCON_IND received" );
        if ( !!globals->rcvBuffSize ) {
            /* we've received a buffer and now need to do something with it */
            assert_state1( globals, __LINE__, IR_STATE_NONE );
            /* 	    globals->ir_state = IR_STATE_MESSAGE_RECD; */
        } else {
            globals->ir_state = IR_STATE_NONE; /* was IR_STATE_DOLAP */
        }
        break;

    case LEVENT_LM_CON_IND:
        XP_ASSERT( !globals->conPacketInUse );
        XP_STATUSF( "responding to incomming connection" );
        assert_state2( globals, __LINE__, IR_STATE_CONN_RECD, 
                       IR_STATE_LAP_RCV );

        globals->conPacket.buff = globals->conBuff;
        globals->conPacket.len = sizeof(globals->conBuff);
        XP_ASSERT( globals->conPacket.len <= IR_MAX_CON_PACKET );
        status = IrConnectRsp( globals->irLibRefNum, con,
                               &globals->conPacket, 0 /* credit: ignored */); 

        XP_ASSERT( status == IR_STATUS_PENDING );
        if ( status == IR_STATUS_PENDING ) {
            globals->conPacketInUse = true;
            globals->ir_state = IR_STATE_CONN_INCOMMING;
        } else {
            XP_STATUSF( "IrConnectRsp call failed with %d", status );
        }
        break;

    case LEVENT_LM_DISCON_IND:
        XP_WARNF( "LEVENT_LM_DISCON_IND received; failure???" );
        break;

    case LEVENT_PACKET_HANDLED: {
        IrPacket* packetPtr = parms->packet;

        packetPtr->buff = NULL;
        packetPtr->len = 0;

        if ( packetPtr == &globals->conPacket ) {

            /* don't change the state here.  This is just telling us the
               packet's free */
            /* 	    assert_state2( globals, __LINE__, IR_STATE_LMPREQ_SENT, */
            /* 			   IR_STATE_LMPRCV_REQ_SENT ); */
            XP_ASSERT( globals->conPacketInUse );
            /* not true if an incomming connection */
            /* 	    XP_ASSERT( !!getSendQueueHead(globals) ); */
            globals->conPacketInUse = false;
        } else {
            assert_state1( globals, __LINE__, IR_STATE_SEND_DONE );
            ((MyIrPacket*)packetPtr)->in_use = false;
            addFreeQueue( globals, packetPtr );
            /* if we've received notification that a send was successful, and
               if we've no further sends to do, shut down the connection.*/
            if ( !!getSendQueueHead(globals) ) { /* another message? */
                globals->ir_state = IR_STATE_LMP_ESTAB;
            } else {
                globals->ir_state = IR_STATE_CAN_DISCONNECT;
                XP_STATUSF( "state:IR_STATE_CAN_DISCONNECT" );
            }
        }
    }
        break;

    case LEVENT_DATA_IND:
        assert_state1( globals, __LINE__, IR_STATE_CONN_INCOMMING );
        receiveData( globals, parms->rxBuff, parms->rxLen ); 
        globals->ir_state = IR_STATE_MESSAGE_RECD;
        break;

    case LEVENT_STATUS_IND:
        break;
    case LEVENT_DISCOVERY_CNF:	/* both sides must do this */
        assert_state1( globals, __LINE__, IR_STATE_DISCOVERY_SENT );
        if ( storeDiscovery( globals, parms->deviceList, con ) ) {
            if ( !!getSendQueueHead( globals ) ) {
                globals->ir_state = IR_STATE_DOLAP;
            } else {
                globals->ir_state = IR_STATE_DISCOVERY_COMPLETE;
            }
        } else {		/* discovery failed */
            globals->ir_state = IR_STATE_REDO_DISCOVERY;
        }
        break;
    case LEVENT_LAP_CON_CNF: 
        XP_STATUSF( "irlap established" );
        assert_state1( globals, __LINE__, IR_STATE_LAP_SENT );
        globals->ir_state = IR_STATE_LAP_ESTAB;
        break;

    case LEVENT_LM_CON_CNF:	/* requested IrLMP connection successful */

        XP_STATUSF( "IrLMP connection is up" );
        assert_state1( globals, __LINE__, IR_STATE_LMPREQ_SENT );
        XP_ASSERT( ir_work_exists(globals) );
        /* I'm not sure whether we get this event before or after the one
           releasing the packet passed to IrConnectReq.  Both need to happen
           before we can do a send -- though I guess since we're using a
           different packet that's not strictly true. */
        globals->ir_state = IR_STATE_LMP_ESTAB;
        break;

    case LEVENT_LAP_CON_IND:
        /* indicates that the other side's opened up a connection */
        XP_STATUSF( "other side opened up a LAP connection" );
        globals->ir_state = IR_STATE_LAP_RCV;
        break;

        /*     case LEVENT_TEST_CNF: */
        /* 	XP_ASSERT( globals->packet_in_use ); */
        /* 	globals->packet_in_use = false; */
        /* 	XP_DEBUGF( "LEVENT_TEST_CNF: returned %d", parms->status ); */
        /* 	break; */

        /*     case LEVENT_TEST_IND: */
        /* 	XP_DEBUGF( "LEVENT_TEST_IND received" ); */
        /* 	receiveData( globals, parms->rxBuff, parms->rxLen );  */
        /* 	break; */

    default:
    }

    CALLBACK_EPILOGUE();
} /* ir_callback_out */
#endif

# ifndef IR_EXCHMGR
Boolean
ir_do_work( PalmAppGlobals* globals )
{
    IrStatus status;
    Boolean result = false;
    XWStreamCtxt* instream;
    MyIrPacket* packetPtr;

    printStateTransition( globals );

    if ( !!getSendQueueHead(globals) /* we're here to send something */
	 && globals->ir_state > IR_STATE_NOTHING_TO_DO
	 && globals->ir_timeout < TimGetTicks() ) {
	Boolean retry;

	retry = palmaskFromStrId( globals, STR_RESEND_IR, -1, -1 );

	/* why did I do this? */
	if ( IrIsIrLapConnected( globals->irLibRefNum ) ) {
	    status = IrDisconnectIrLap( globals->irLibRefNum );
	    XP_ASSERT( status == IR_STATUS_PENDING );
	}

	if ( retry ) {
	    RESET_TIMER(globals);
	} else {
	    clearSendQueue( globals );
	}
	globals->ir_state = retry? IR_STATE_DO_DISCOVERY : IR_STATE_NONE;

    } else {

	switch( globals->ir_state ) {

	case IR_STATE_NONE:	/* do we need this state anymore? */
	    XP_ASSERT( !!getSendQueueHead( globals ) );
	    if ( IrIsIrLapConnected(globals->irLibRefNum) ) {
		globals->ir_state = IR_STATE_LAP_ESTAB;
	    } else {
		globals->ir_state = IR_STATE_DO_DISCOVERY;
	    }
	    break;

	case IR_STATE_DO_DISCOVERY:
	    /* might a well set it up here */
	    globals->conPacket.buff = globals->conBuff;
	    globals->conPacket.len = IR_MAX_CON_PACKET;

	    RESET_TIMER(globals);
	case IR_STATE_REDO_DISCOVERY:
	    if ( IrIsIrLapConnected(globals->irLibRefNum) ) {
		globals->ir_state = IR_STATE_LAP_ESTAB;
	    } else {
		status = IrDiscoverReq( globals->irLibRefNum,
					&globals->irC_out.irCon );

		if (status == IR_STATUS_SUCCESS || 
		    status == IR_STATUS_PENDING) {
		    globals->ir_state = IR_STATE_DISCOVERY_SENT;
		} else {
		    XP_STATUSF( "discov failed: %d", status );
		    globals->ir_state = IR_STATE_REDO_DISCOVERY;
		}
	    }
	    break;

	case IR_STATE_DISCOVERY_SENT:
	    break;

	case IR_STATE_DOLAP:
	    /* better be a message to send! */
	    XP_ASSERT( !!getSendQueueHead( globals ) );
 	    XP_STATUSF( "calling IrConnectIrLap" ); 
	    status = IrConnectIrLap( globals->irLibRefNum, globals->irDev );
	    if (status != IR_STATUS_SUCCESS && status != IR_STATUS_PENDING) {
		XP_STATUSF( "IrConnectIrLap call failed: %d", status );
	    } else {
		globals->ir_state = IR_STATE_LAP_SENT;
	    }
	    break;

	case IR_STATE_LAP_SENT:
/* 	    XP_DEBUGF( "state still IR_STATE_LAP_SENT" ); */
	    break;

	case IR_STATE_LAP_ESTAB:
	    if ( !globals->conPacketInUse ) {
		/* not true if from other side */
/* 		XP_ASSERT( !!globals->conPacket.buff ); */
		XP_ASSERT( IrIsIrLapConnected(globals->irLibRefNum) );
		/* not sure what this means anyway.... */
/*  		XP_ASSERT(globals->irC_out.irCon.rLsap== */
/* 			  globals->save_rLsap);  */
		status = IrConnectReq( globals->irLibRefNum, 
				       &globals->irC_out.irCon,
				       &globals->conPacket, 0 );
		if ( status == IR_STATUS_PENDING ) {

		    if ( globals->ir_state == IR_STATE_LAP_ESTAB ) {
			globals->ir_state = IR_STATE_LMPREQ_SENT;
		    } else {
			globals->ir_state = IR_STATE_LMPRCV_REQ_SENT;
		    }

		    globals->conPacketInUse = true;
		    XP_STATUSF( "IrConnectReq succeeded" );

		} else {
		    XP_STATUSF( "IrConnectReq returned %d; will try again",
			      status );
		}
	    } else {
		XP_WARNF( "Can't call IrConnectReq b/c packet_in_use" );
	    }
	    break;

	case IR_STATE_LMP_ESTAB:
	    packetPtr = getSendQueueHead( globals );
	    XP_ASSERT( !!packetPtr );
	    if ( !!packetPtr ) {
		XP_ASSERT( !!packetPtr->packet.buff );
		XP_ASSERT( packetPtr->packet.len > 0 );
		status = IrDataReq( globals->irLibRefNum, 
				    &globals->irC_out.irCon,
				    &packetPtr->packet );
		if ( status == IR_STATUS_PENDING ) {
		    packetPtr->in_use = true;
		    globals->ir_state = IR_STATE_SEND_DONE;
		} else {
		    XP_WARNF( "IrDataReq returned %d", status );
		}
	    }
	    break;

	case IR_STATE_MESSAGE_RECD:
	    XP_ASSERT( !!globals->rcvBuffSize );

	    instream = mem_stream_make( MEMPOOL globals, 0, NULL );
	    stream_open( instream );
	    stream_putBytes( instream, globals->rcvBuff, 
			     globals->rcvBuffSize );
	    globals->rcvBuffSize = 0;

	    if ( comms_checkIncommingStream( globals->game.comms, instream,
					     &instream, 1 ) ) { /* FIXME!!! */
		result = server_receiveMessage( globals->game.server,
						instream );
	    }
	    stream_destroy( instream );

	    palm_util_requestTime( &globals->util );	    

	    globals->ir_state = IR_STATE_CAN_DISCONNECT;
	    break;		/* comment this out? */

#if 1
	case IR_STATE_CAN_DISCONNECT:
	    /* send the disconnect message so receiver will know the
	       message is finished */

	    /* if the other side disconnects, it'll already be down?? */
	    if ( IrIsIrLapConnected( globals->irLibRefNum ) ) {
		status = IrDisconnectIrLap( globals->irLibRefNum );
		XP_ASSERT( status == IR_STATUS_PENDING );
	    }
	    globals->ir_state = IR_STATE_NONE;

	    break;
#endif
	default:
	    break;
	}
    }
    return result;
} /* ir_do_work */

void
ir_show_status( PalmAppGlobals* globals )
{
    if ( !!globals->mainForm ) {
	XP_U16 x, y;
	WinHandle oldHand = WinSetDrawWindow( (WinHandle)globals->mainForm );

	x = globals->isLefty?1:154;
	y = 160 - TRAY_HEIGHT - IR_STATUS_HEIGHT;

	if ( globals->ir_state > IR_STATE_NOTHING_TO_DO ) {
	    char buf[2] = { 0, 0 };
	    if ( globals->ir_state <= 9 ) {
		buf[0] = '0' + globals->ir_state;
	    } else {
		buf[0] = 'A' + globals->ir_state-10;
	    }
	    WinDrawChars( buf, 1, x, y );
	} else {
	    RectangleType r = { {x, y}, {8, 10} };
	    WinEraseRectangle( &r, 0);
	}

	WinSetDrawWindow( oldHand );
    }
} /* ir_show_status */

/* Free any memory associated with message queues, etc.
 */
void
ir_cleanup( PalmAppGlobals* globals )
{
    MyIrPacket* packet;
    MyIrPacket* next;

    for ( packet = globals->packetListHead; !!packet; packet = next ) {
	next = packet->next;
	XP_FREE( globals->mpool,  packet );
    }
    globals->packetListHead = NULL;
} /* ir_cleanup */
#endif

/* We're passed an address as we've previously defined it and a buffer
 * containing a message to send.  Prepend any palm/ir specific headers to the
 * message, save the buffer somewhere, and fire up the state machine that
 * will eventually get it sent to the address.
 *
 * Note that the caller will queue the message for possible resend, but
 * won't automatically schedule that resend whatever results we return.
 *
 * NOTE also that simply stuffing the buf ptr into the packet won't work
 * if there's any ir-specific packet header I need to prepend to what's
 * outgoing.
 */
XP_S16
palm_ir_send( const XP_U8* buf, XP_U16 len, PalmAppGlobals* globals )
{
#ifdef IR_EXCHMGR
    UInt32 sent = 0;
    Err err;
    ExgSocketType exgSocket;
    XP_MEMSET( &exgSocket, 0, sizeof(exgSocket) );
    exgSocket.description = "Crosswords data"; 
    exgSocket.length = len;
    exgSocket.target = APPID;

    if ( globals->romVersion >= 40 ) {
        exgSocket.name = exgBeamPrefix;
    }

    err = ExgPut( &exgSocket );
    while ( !err && sent < len ) {
        sent += ExgSend( &exgSocket, buf+sent, len-sent, &err );
        XP_ASSERT( sent < 0x7FFF );
    }
    err = ExgDisconnect( &exgSocket, err );

    return err==0? sent : 0;
#else
    MyIrPacket* packet = getFreeSendPacket( globals );

    packet->packet.buff = buf;
    packet->packet.len = len;
    XP_ASSERT( !packet->in_use );

    addToSendQueue( globals, packet );

    return len;
#endif
} /* palm_ir_send */

#ifdef IR_EXCHMGR
void
palm_ir_receiveMove( PalmAppGlobals* globals, ExgSocketPtr socket )
{
    UInt32 nBytesReceived = -1;
    Err err;

    err = ExgAccept( socket );
    if ( err == 0 ) {
        XWStreamCtxt* instream;

        instream = mem_stream_make( MEMPOOL globals->vtMgr, globals, 
                                    CHANNEL_NONE, NULL );
        stream_open( instream );

        for ( ; ; ) {
            UInt8 buf[128];
            nBytesReceived = ExgReceive( socket, buf, sizeof(buf), &err );
            if ( nBytesReceived == 0 || err != 0 ) {
                break;
            }

            stream_putBytes( instream, buf, nBytesReceived );
        }
        (void)ExgDisconnect( socket, err );

        if ( nBytesReceived == 0 ) { /* successful loop exit */
            checkAndDeliver( globals, NULL, instream );
        }
    }
} /* palm_ir_receiveMove */
#else
static void
receiveData( PalmAppGlobals* globals, UInt8* buf, UInt16 len )
{
    XP_ASSERT( !!len );
    XP_ASSERT( !globals->conPacketInUse );
    
    XP_ASSERT( !globals->rcvBuffSize ); /* else messages coming in several
					   parts; old code handled this */
    XP_MEMCPY( globals->rcvBuff, buf, len );
    globals->rcvBuffSize = len;

    globals->ir_timeout = IR_NO_TIMEOUT;
} /* receiveData */

/* return the first packet ready to be sent, i.e. whose buf ptr is non-null
 * and whose in_use flag is not set.  To make searching faster, keep track of
 * whether there are actually any on the queue. */
MyIrPacket*
getSendQueueHead( PalmAppGlobals* globals )
{
    MyIrPacket* packet = NULL;

    if ( globals->irSendQueueLen > 0 ) {

	packet = (MyIrPacket*)globals->packetListHead;
	for ( ; !!packet; packet = packet->next ) {
	    if ( !!packet->packet.buff && !packet->in_use ) {
		break;
	    }
	}
    }
    return packet;
} /* getSendQueueHead */
#endif

/* The ptr's already on the list, but we need to move it to the end, behind
 * anything that's already there waiting to be sent.  That's because messages
 * need to get sent in order.
 */
#ifndef IR_EXCHMGR
static void
addToSendQueue( PalmAppGlobals* globals, MyIrPacket* packet )
{
    MyIrPacket* end = globals->packetListHead;

    packet->next = NULL;

    if ( !end ) {
	globals->packetListHead = packet;
    } else {

	for ( ; !!end->next; end = end->next ) {
	    
	}
	end->next = packet;
    }
    ++globals->irSendQueueLen;
    RESET_TIMER(globals);
} /* addToSendQueue */
#endif /* ifndef IR_EXCHMGR */

#ifndef IR_EXCHMGR
static void
clearSendQueue( PalmAppGlobals* globals )
{
    MyIrPacket* packet;
    MyIrPacket* next;

    for ( packet = globals->packetListHead; !!packet; packet = next ) {
	next = packet->next;
	if ( packet->packet.buff != NULL ) {
	    packet->packet.buff = NULL;
	    packet->packet.len = 0;
	    --globals->irSendQueueLen;
	}
    }

    XP_ASSERT( globals->irSendQueueLen == 0 ) ;
} /* clearSendQueue */

static MyIrPacket*
getFreeSendPacket( PalmAppGlobals* globals )
{
    MyIrPacket* packet = globals->packetListHead;
    MyIrPacket* prev;

    for ( prev = NULL; !!packet; prev = packet, packet = packet->next ) {
	if ( !packet->packet.buff ) {
	    XP_ASSERT( packet->packet.len == 0 );

	    /* cut out of list before returning */
	    if ( !!prev ) {
		prev->next = packet->next;
	    } else {
		XP_ASSERT( globals->packetListHead == packet );
		globals->packetListHead = NULL;
	    }

	    return packet;
	}
    }
    packet = XP_MALLOC( globals->mpool, sizeof(*packet) );
    XP_MEMSET( packet, 0, sizeof(*packet) );

    return packet;
} /* getFreeSendPacket */
#endif

#ifdef DEBUG
#ifndef IR_EXCHMGR
static char*
getStateName( IR_STATE state )
{
    switch ( state ) {

    case IR_STATE_NONE: return "NONE";
    case IR_STATE_DISCOVERY_COMPLETE: return "DISCOVERY_COMPLETE";


    case IR_STATE_NOTHING_TO_DO: return "NOTHING_TO_DO";
    case IR_STATE_NO_OTHER_FOUND: return "NO_OTHER_FOUND";
    case IR_STATE_DO_DISCOVERY: return "DO_DISCOVERY";
    case IR_STATE_REDO_DISCOVERY: return "REDO_DISCOVERY";
    case IR_STATE_DISCOVERY_SENT: return "DISCOVERY_SENT";
    case IR_STATE_DOLAP: return "DOLAP";


    case IR_STATE_LAP_SENT: return "LAP_SENT";
    case IR_STATE_LAP_ESTAB: return "LAP_ESTAB";


    case IR_STATE_LMPREQ_SENT: return "LMPREQ_SENT";
    case IR_STATE_LMP_ESTAB: return "LMP_ESTAB";


    case IR_STATE_SEND_DONE: return "SEND_DONE";
    case IR_STATE_CAN_DISCONNECT: return "CAN_DISCONNECT";

    case IR_STATE_CONN_RECD: return "CONN_RECD";
    case IR_STATE_LAP_RCV: return "LAP_RCV";
    case IR_STATE_LMPRCV_REQ_SENT: return "LMPRCV_REQ_SENT";
    case IR_STATE_CONN_INCOMMING: return "CONN_INCOMMING";
    case IR_STATE_MESSAGE_RECD: return "MESSAGE_RECD";

    default:
	return "unknown";
    }

} /* getStateName */

static void
assert_state1( PalmAppGlobals* globals, short line, IR_STATE assertState )
{
    if ( globals->ir_state != assertState ) {
	XP_WARNF( "Line %d: sought %s; found %s", line,
		  getStateName(assertState), getStateName(globals->ir_state));
    }
} /* assert_state1 */

static void
assert_state2( PalmAppGlobals* globals, short line, IR_STATE assertState1,
	       IR_STATE assertState2 )
{
    if ( globals->ir_state != assertState1 
	 && globals->ir_state != assertState2){
	XP_WARNF( "Line %d: sought %s or %s; found %s", line,
		  getStateName(assertState1), getStateName(assertState2),
		  getStateName( globals->ir_state ) );
    }

} /* assertState2 */

static void
printStateTransition( PalmAppGlobals* globals )
{
    if ( globals->ir_state != globals->ir_state_prev ) {
        char* oldState = getStateName( globals->ir_state_prev );
        char* newState = getStateName( globals->ir_state );

        XP_STATUSF( "ir_st:%s->%s", oldState, newState );

        globals->ir_state_prev = globals->ir_state;
    }
} /* printStateTransition */
# endif /* IR_EXCHMGR */
#endif /* DEBUG */

#endif /* IR_SUPPORT */
