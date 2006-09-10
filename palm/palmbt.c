/* -*-mode: C; fill-column: 77; c-basic-offset: 4; compile-command: "make ARCH=68K_ONLY MEMDEBUG=TRUE"; -*- */
/* 
 * Copyright 2006 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#ifdef XWFEATURE_BLUETOOTH

#include "xptypes.h"
#include "palmbt.h"
#include "strutils.h"

# include <BtLib.h>
# include <BtLibTypes.h>

#define L2CAPSOCKETMTU 500
#define SOCK_INVAL 0XFFFF

typedef enum { PBT_UNINIT = 0, PBT_MASTER, PBT_SLAVE } PBT_SetState;

typedef enum {
    PBT_EVENT_NONE
    , PBT_EVENT_CONNECT_ACL
    , PBT_EVENT_CONNECT_L2C
    , PBT_EVENT_GOTDATA
    , PBT_EVENT_TRYSEND
} PBT_EVENT;

typedef enum {
    PBTST_NONE
    , PBTST_ACL_CONNECTING
    , PBTST_ACL_CONNECTED
    , PBTST_L2C_CONNECTING
    , PBTST_L2C_CONNECTED
} PBT_STATE;

#define PBT_MAX_EVTS 4
#define HASWORK(s)  ((s)->vol.queueCur != (s)->vol.queueNext)
#define MAX_INCOMING 4

typedef struct PalmBTStuff {
    DataCb cb;
    PalmAppGlobals* globals;

    XP_U16 btLibRefNum;

    struct {
        XP_U16 lenOut;
        XP_UCHAR bufOut[L2CAPSOCKETMTU];       /* what's the mmu? */
        XP_U16 lens[MAX_INCOMING];
        XP_U8 bufIn[L2CAPSOCKETMTU*2];       /* what's the mmu? */

        XP_U16 queueCur;
        XP_U16 queueNext;
        PBT_EVENT evtQueue[PBT_MAX_EVTS];

        XP_Bool sendInProgress;
        XP_Bool sendPending;
    } vol;

    BtLibSocketRef dataSocket;
    PBT_SetState setState;

    PBT_STATE connState;

    union {
        struct {
            BtLibDeviceAddressType masterAddr;
            XP_Bool addrSet;
        } slave;
        struct {
            BtLibSocketRef listenSocket;
        } master;
    } u;

    BtLibSdpRecordHandle sdpRecordH;

} PalmBTStuff;

#define LOG_ERR(f,e) palm_bt_log( #f, __FUNCTION__, e )
#define CALL_ERR(e,f,...) \
    XP_LOGF( "%s: calling %s", __FUNCTION__, #f ); \
    e = f(__VA_ARGS__); \
    LOG_ERR(f,e); \
    if ( e == btLibErrFailed ) { XP_WARNF( "%s=>btLibErrFailed", #f ); }

/* WHAT SHOULD THIS BE?  Copied from Whiteboard....  PENDING */
static const BtLibSdpUuidType XWORDS_UUID = {
    btLibUuidSize128, 
    { 0x83, 0xe0, 0x87, 0xae, 0x4e, 0x18, 0x46, 0xbe, 
      0x83, 0xe0, 0x7b, 0x3d, 0xe6, 0xa1, 0xc3, 0x3b } };

static PalmBTStuff* pbt_checkInit( PalmAppGlobals* globals );
static void palm_bt_log( const char* btfunc, const char* func, Err err );
static Err bpd_discover( PalmBTStuff* btStuff, BtLibDeviceAddressType* addr );
static void pbt_setup_slave( PalmBTStuff* btStuff, const CommsAddrRec* addr );
static void pbt_takedown_slave( PalmBTStuff* btStuff );
static void pbt_setup_master( PalmBTStuff* btStuff );
static void pbt_takedown_master( PalmBTStuff* btStuff );
static void pbt_do_work( PalmBTStuff* btStuff );
static void pbt_postpone( PalmBTStuff* btStuff, PBT_EVENT evt );
static void pbt_enqueIncoming( PalmBTStuff* btStuff, XP_U8* data, XP_U16 len );
static void pbt_processIncoming( PalmBTStuff* btStuff );
static void pbt_reset( PalmBTStuff* btStuff );

#ifdef DEBUG
static const char* btErrToStr( Err err );
static const char* btEvtToStr( BtLibSocketEventEnum evt );
static const char* mgmtEvtToStr( BtLibManagementEventEnum event );
static const char* evtToStr(PBT_EVENT evt);

#else
# define btErrToStr( err ) ""
# define btEvtToStr( evt ) ""
# define mgmtEvtToStr( evt ) ""
# define evtToStr(evt) ""
#endif

/* callbacks */
static void libMgmtCallback( BtLibManagementEventType* mEvent, UInt32 refCon );
static void l2SocketCallback( BtLibSocketEventType* sEvent, UInt32 refCon );

Err
palm_bt_init( PalmAppGlobals* globals, DataCb cb )
{
    PalmBTStuff* btStuff;

    btStuff = globals->btStuff;
    if ( !btStuff ) {
        btStuff = pbt_checkInit( globals );
    } else {
        pbt_reset( btStuff );
    }

    btStuff->cb = cb;
    return errNone;
} /* palm_bt_init */

void
palm_bt_close( PalmAppGlobals* globals )
{
    PalmBTStuff* btStuff = globals->btStuff;

    if ( !!btStuff ) {
        XP_U16 btLibRefNum = btStuff->btLibRefNum;
        if ( btLibRefNum != 0 ) {
            Err err;

            if ( btStuff->setState == PBT_MASTER ) {
                pbt_takedown_master( btStuff );
            } else if ( btStuff->setState == PBT_SLAVE ) {
                pbt_takedown_slave( btStuff );
            }

            /* Need to unregister callbacks */
            CALL_ERR( err, BtLibUnregisterManagementNotification, btLibRefNum,
                      libMgmtCallback );
            CALL_ERR( err, BtLibClose, btLibRefNum );
            XP_ASSERT( errNone == err );
        }
        XP_FREE( globals->mpool, btStuff );
        globals->btStuff = NULL;
    } else {
        XP_LOGF( "%s: btStuff null", __FUNCTION__ );
    }
} /* palm_bt_close */

void
palm_bt_amendWaitTicks( PalmAppGlobals* globals, Int32* result )
{
    PalmBTStuff* btStuff = globals->btStuff;
    if ( !!btStuff && HASWORK(btStuff) ) {
        *result = 0;
    }
}

XP_Bool
palm_bt_doWork( PalmAppGlobals* globals )
{
    PalmBTStuff* btStuff = globals->btStuff;
    XP_Bool haveWork = !!btStuff && HASWORK(btStuff);

    if ( haveWork ) {
        pbt_do_work( btStuff );
    }
    return haveWork;
}

void
palm_bt_addrString( PalmAppGlobals* globals, XP_BtAddr* btAddr, 
                    XP_BtAddrStr* str )
{
    PalmBTStuff* btStuff = pbt_checkInit( globals );
    *str[0] = '\0';
    if ( !!btStuff ) {
        Err err;
        CALL_ERR( err, BtLibAddrBtdToA, btStuff->btLibRefNum, 
                  (BtLibDeviceAddressType*)btAddr,
                  (char*)str, sizeof(*str) );
        XP_LOGF( "BtLibAddrBtdToA=>%s from:", str );
        LOG_HEX( btAddr, sizeof(*btAddr), "" );
    }
} /* palm_bt_addrString */

XP_Bool
palm_bt_browse_device( PalmAppGlobals* globals, XP_BtAddr* btAddr,
                       XP_UCHAR* out, XP_U16 len )
{
    Err err;
    PalmBTStuff* btStuff;

    btStuff = pbt_checkInit( globals );
    if ( NULL != btStuff ) {
        BtLibDeviceAddressType addr;
        err = bpd_discover( btStuff, &addr );

        if ( errNone == err ) {
            UInt16 index;
            CALL_ERR( err, BtLibSecurityFindTrustedDeviceRecord, 
                      btStuff->btLibRefNum, &addr, &index );
            CALL_ERR( err, BtLibSecurityGetTrustedDeviceRecordInfo, 
                      btStuff->btLibRefNum, index, NULL, out, len, 
                      NULL, NULL, NULL );
            XP_ASSERT( sizeof(*btAddr) >= sizeof(addr) );
            XP_MEMCPY( btAddr, &addr, sizeof(addr) );
        
            LOG_HEX( &addr, sizeof(addr), __FUNCTION__ );

/*             err = BtLibGetRemoteDeviceName( btStuff->btLibRefNum, */
/*                                             BtLibDeviceAddressTypePtr  */
/*                                             remoteDeviceP, */
/*                                             BtLibFriendlyNameType* nameP, */
/*                                             BtLibGetNameEnum retrievalMethod ); */
/*             err = BtLibAddrBtdToA( btStuff->btLibRefNum,  */
/*                                    &btStuff->u.slave.masterAddr, */
/*                                    out, len ); */
        }
    } else {
        XP_LOGF( "%s: err = %s", __FUNCTION__, btErrToStr(err) );
    }
    return errNone == err;
} /* palm_bt_browse_device */

static void
pbt_send_pending( PalmBTStuff* btStuff, const CommsAddrRec* addr )
{
    Err err;
    LOG_FUNC();
    if ( btStuff->vol.sendPending && !btStuff->vol.sendInProgress ) {
        if ( btStuff->dataSocket != SOCK_INVAL ) {
            /* hack: zero-len send to cause connect */
            if ( btStuff->vol.lenOut > 0 ) {
                CALL_ERR( err, BtLibSocketSend, btStuff->btLibRefNum, 
                          btStuff->dataSocket, 
                          btStuff->vol.bufOut, btStuff->vol.lenOut );
                if ( err == errNone ) {
                    // clear on receipt of btLibSocketEventSendComplete 
                    btStuff->vol.sendInProgress = XP_TRUE;
                }
            } else {
                btStuff->vol.sendPending = XP_FALSE;
            }
        } else {
            /* No data socket? */
            if ( btStuff->setState == PBT_SLAVE ) {
                pbt_setup_slave( btStuff, addr );
            }
        }
    }
} /* pbt_send_pending */

XP_S16
palm_bt_send( const XP_U8* buf, XP_U16 len, const CommsAddrRec* addr,
              DataCb cb, PalmAppGlobals* globals )
{
    XP_S16 nSent = -1;
    PalmBTStuff* btStuff;
    CommsAddrRec remoteAddr;
    PBT_SetState setState;
    XP_LOGF( "%s(len=%d)", __FUNCTION__, len );

    btStuff = pbt_checkInit( globals );
    if ( !btStuff->cb ) {
        btStuff->cb = cb;
    } else {
        XP_ASSERT( cb == btStuff->cb );
    }

    if ( !addr ) {
        comms_getAddr( globals->game.comms, &remoteAddr );
        addr = &remoteAddr;
    }
    XP_ASSERT( !!addr );

    setState = btStuff->setState;
    if ( setState == PBT_UNINIT ) {
        XP_Bool amMaster = comms_getIsServer( globals->game.comms );
        setState = amMaster? PBT_MASTER : PBT_SLAVE;
    }

    if ( !!btStuff ) {
        if ( setState == PBT_MASTER ) {
            pbt_setup_master( btStuff );
        } else {
            pbt_setup_slave( btStuff, addr );
        }

        if ( !btStuff->vol.sendInProgress ) {
            if ( len > sizeof( btStuff->vol.bufOut ) ) {
                len = sizeof( btStuff->vol.bufOut );
            }
            XP_MEMCPY( btStuff->vol.bufOut, buf, len );
            btStuff->vol.lenOut = len;
            btStuff->vol.sendPending = XP_TRUE;            

            pbt_send_pending( btStuff, addr );
            nSent = len;
        } else {
            XP_LOGF( "%s: send ALREADY in progress", __FUNCTION__ );
        }
    }
    LOG_RETURNF( "%d", nSent );
    return nSent;
} /* palm_bt_send */

static void
pbt_setup_master( PalmBTStuff* btStuff )
{
    if ( btStuff->setState == PBT_SLAVE ) {
        pbt_takedown_slave( btStuff );
    }
    btStuff->setState = PBT_MASTER;

    if ( btStuff->u.master.listenSocket == SOCK_INVAL ) {
        /* Will eventually want to create a piconet here for more than two
           devices to play.... */

        Err err;
        BtLibSocketListenInfoType listenInfo;

        /*    1. BtLibSocketCreate: create an L2CAP socket. */
        CALL_ERR( err, BtLibSocketCreate, btStuff->btLibRefNum, 
                  &btStuff->u.master.listenSocket, l2SocketCallback,
                  (UInt32)btStuff, btLibL2CapProtocol );

        /*    2. BtLibSocketListen: set up an L2CAP socket as a listener. */
        XP_MEMSET( &listenInfo, 0, sizeof(listenInfo) );
        listenInfo.data.L2Cap.localPsm = XW_PSM; // BT_L2CAP_RANDOM_PSM;
        listenInfo.data.L2Cap.localMtu = L2CAPSOCKETMTU; 
        listenInfo.data.L2Cap.minRemoteMtu = L2CAPSOCKETMTU;
        CALL_ERR( err, BtLibSocketListen, btStuff->btLibRefNum, 
                  btStuff->u.master.listenSocket, &listenInfo );

        /*    3. BtLibSdpServiceRecordCreate: allocate a memory chunk that
              represents an SDP service record. */
        CALL_ERR( err, BtLibSdpServiceRecordCreate,
                  btStuff->btLibRefNum, &btStuff->sdpRecordH );

        /*    4. BtLibSdpServiceRecordSetAttributesForSocket: initialize an
              SDP memory record so it can represent the newly-created L2CAP
              listener socket as a service */
        CALL_ERR( err, BtLibSdpServiceRecordSetAttributesForSocket,
                  btStuff->btLibRefNum, btStuff->u.master.listenSocket, 
                  (BtLibSdpUuidType*)&XWORDS_UUID, 1, APPNAME, 
                  StrLen(APPNAME), btStuff->sdpRecordH );

        /*    5. BtLibSdpServiceRecordStartAdvertising: make an SDP memory
              record representing a local SDP service record visible to
              remote devices.  */
        CALL_ERR( err, BtLibSdpServiceRecordStartAdvertising, 
                  btStuff->btLibRefNum, btStuff->sdpRecordH );
    }
} /* pbt_setup_master */

static void
pbt_takedown_master( PalmBTStuff* btStuff )
{
    XP_U16 btLibRefNum;
    Err err;

    LOG_FUNC();
    
    XP_ASSERT( btStuff->setState == PBT_MASTER );
    btLibRefNum = btStuff->btLibRefNum;

    if ( SOCK_INVAL != btStuff->dataSocket ) {
        CALL_ERR( err, BtLibSocketClose, btLibRefNum,  
                  btStuff->dataSocket );
    }

    if ( !!btStuff->sdpRecordH ) {
        CALL_ERR( err, BtLibSdpServiceRecordStopAdvertising,
                  btLibRefNum, btStuff->sdpRecordH );
        XP_ASSERT( errNone == err ); /* no errors if it was being advertised */

        CALL_ERR( err, BtLibSdpServiceRecordDestroy, btLibRefNum, 
                  btStuff->sdpRecordH );
        btStuff->sdpRecordH = NULL;
    }

    if ( SOCK_INVAL != btStuff->u.master.listenSocket ) {
        CALL_ERR( err, BtLibSocketClose, btLibRefNum,  
                  btStuff->u.master.listenSocket );
        btStuff->u.master.listenSocket = SOCK_INVAL;
    }

    btStuff->setState = PBT_UNINIT;
}

static void
pbt_do_work( PalmBTStuff* btStuff )
{
    PBT_EVENT evt;
    Err err;

    LOG_FUNC();

    evt = btStuff->vol.evtQueue[btStuff->vol.queueCur++];
    btStuff->vol.queueCur %= PBT_MAX_EVTS;

    switch( evt ) {
    case PBT_EVENT_CONNECT_ACL:
        if ( btStuff->connState == PBTST_NONE ) {
            /* sends btLibManagementEventACLConnectOutbound */
            CALL_ERR( err, BtLibLinkConnect, btStuff->btLibRefNum, 
                      &btStuff->u.slave.masterAddr );
            btStuff->connState = PBTST_ACL_CONNECTING;
        } else {
            err = btLibErrAlreadyConnected;
        }
        if ( btLibErrAlreadyConnected == err ) {
            pbt_postpone( btStuff, PBT_EVENT_CONNECT_L2C );
        }
        break;

    case PBT_EVENT_CONNECT_L2C:
        if ( btStuff->connState == PBTST_ACL_CONNECTED ) {
            XP_ASSERT( SOCK_INVAL == btStuff->dataSocket ); /* firing */
            CALL_ERR( err, BtLibSocketCreate, btStuff->btLibRefNum, 
                      &btStuff->dataSocket, 
                      l2SocketCallback, (UInt32)btStuff, 
                      btLibL2CapProtocol );

            if ( btLibErrNoError == err ) {
                BtLibSocketConnectInfoType connInfo;
                connInfo.data.L2Cap.remotePsm = XW_PSM;
                connInfo.data.L2Cap.localMtu = L2CAPSOCKETMTU; 
                connInfo.data.L2Cap.minRemoteMtu = L2CAPSOCKETMTU;
                connInfo.remoteDeviceP = &btStuff->u.slave.masterAddr;
	
                /* sends btLibSocketEventConnectedOutbound */
                CALL_ERR( err, BtLibSocketConnect, btStuff->btLibRefNum, 
                          btStuff->dataSocket, &connInfo );
                btStuff->connState = PBTST_L2C_CONNECTING;
            } else {
                btStuff->dataSocket = SOCK_INVAL;
            }
        }

        break;

    case PBT_EVENT_GOTDATA:
        pbt_processIncoming( btStuff );
        break;

    case PBT_EVENT_TRYSEND:
        pbt_send_pending( btStuff, NULL );
        break;

    default:
        XP_ASSERT( 0 );
    }
    LOG_RETURN_VOID();
} /* pbt_do_work */

static void
pbt_postpone( PalmBTStuff* btStuff, PBT_EVENT evt )
{
    EventType eventToPost = { .eType = nilEvent };

    XP_LOGF( "%s(%s)", __FUNCTION__, evtToStr(evt) );
    EvtAddEventToQueue( &eventToPost );

    btStuff->vol.evtQueue[ btStuff->vol.queueNext++ ] = evt;
    btStuff->vol.queueNext %= PBT_MAX_EVTS;
    XP_ASSERT( btStuff->vol.queueNext != btStuff->vol.queueCur );
}

static void
pbt_enqueIncoming( PalmBTStuff* btStuff, XP_U8* indata, XP_U16 inlen )
{
    XP_U16 i;
    XP_U16 total = 0;

    for ( i = 0; i < MAX_INCOMING; ++i ) {
        XP_U16 len = btStuff->vol.lens[i];
        if ( !len ) {
            break;
        }
        total += len;
    }

    if ( (i < MAX_INCOMING) && 
         ((total + inlen) < sizeof(btStuff->vol.bufIn)) ) {
        btStuff->vol.lens[i] = inlen;
        XP_MEMCPY( &btStuff->vol.bufIn[total], indata, inlen );
        pbt_postpone( btStuff, PBT_EVENT_GOTDATA );
    } else {
        XP_LOGF( "%s: dropping packet of len %d", __FUNCTION__, inlen );
    }
} /* pbt_enqueIncoming */

static void
pbt_processIncoming( PalmBTStuff* btStuff )
{
    XP_U16 len = btStuff->vol.lens[0];
    XP_ASSERT( !!btStuff->cb );
    if ( !!btStuff->cb ) {
        (*btStuff->cb)( btStuff->globals, btStuff->vol.bufIn, len );

        /* slide the remaining packets down */
        XP_MEMCPY( &btStuff->vol.lens[0], &btStuff->vol.lens[1], 
                   sizeof(btStuff->vol.lens) - sizeof(btStuff->vol.lens[0]) );
        btStuff->vol.lens[MAX_INCOMING-1] = 0; /* be safe */
        XP_MEMCPY( btStuff->vol.bufIn, btStuff->vol.bufIn + len, 
                   sizeof(btStuff->vol.bufIn) - len );
    }
} /* pbt_processIncoming */

static void
pbt_reset( PalmBTStuff* btStuff )
{
    LOG_FUNC();
    XP_MEMSET( &btStuff->vol, 0, sizeof(btStuff->vol) );
}

static Err
bpd_discover( PalmBTStuff* btStuff, BtLibDeviceAddressType* addr )
{
    Err err;
    const BtLibClassOfDeviceType deviceFilter
        = btLibCOD_ServiceAny
        | btLibCOD_Major_Any // btLibCOD_Major_Computer
        | btLibCOD_Minor_Comp_Any; //btLibCOD_Minor_Comp_Palm;

    CALL_ERR( err, BtLibDiscoverSingleDevice, btStuff->btLibRefNum, 
              "Crosswords host", (BtLibClassOfDeviceType*)&deviceFilter, 1,
              addr, false, false );
    LOG_RETURNF( "%s", btErrToStr(err) );
    return err;
} /* bpd_discover */

static void
pbt_setup_slave( PalmBTStuff* btStuff, const CommsAddrRec* addr )
{
    LOG_FUNC();

    if ( btStuff->setState == PBT_MASTER ) {
        pbt_takedown_master( btStuff );
    }
    btStuff->setState = PBT_SLAVE;

    if ( !!addr ) {
        char buf[64];
        if ( errNone == 
             BtLibAddrBtdToA( btStuff->btLibRefNum, 
                              (BtLibDeviceAddressType*)&addr->u.bt.btAddr,
                              buf, sizeof(buf) ) ) {
            XP_LOGF( "%s(%s)", __FUNCTION__, buf );
        }
    } else {
        XP_LOGF( "null addr" );
    }

    if ( btStuff->connState == PBTST_ACL_CONNECTED ) {
        pbt_postpone( btStuff, PBT_EVENT_CONNECT_L2C );
    } else if ( btStuff->connState == PBTST_L2C_CONNECTED ) {
        /* do nothing */
    } else if ( !!addr || btStuff->u.slave.addrSet ) {
        if ( !btStuff->u.slave.addrSet ) {
            /* Our xp type better be big enough */
            XP_ASSERT( sizeof(addr->u.bt.btAddr)
                       >= sizeof(btStuff->u.slave.masterAddr) );
            XP_MEMCPY( &btStuff->u.slave.masterAddr, addr->u.bt.btAddr, 
                       sizeof(btStuff->u.slave.masterAddr) );
            btStuff->u.slave.addrSet = XP_TRUE;
        }

        pbt_postpone( btStuff, PBT_EVENT_CONNECT_ACL );
    } else {
        XP_LOGF( "%s: doing nothing", __FUNCTION__ );
    }
    LOG_RETURN_VOID();
} /* pbt_setup_slave */

static void
pbt_takedown_slave( PalmBTStuff* btStuff )
{
    Err err;
    switch ( btStuff->connState ) {
    case PBTST_L2C_CONNECTED:
        XP_ASSERT ( SOCK_INVAL != btStuff->dataSocket );
        CALL_ERR( err, BtLibSocketClose, btStuff->btLibRefNum,  
                  btStuff->dataSocket );
        /* fallthru */
    case PBTST_L2C_CONNECTING:
    case PBTST_ACL_CONNECTED:
        if ( PBT_SLAVE == btStuff->setState ) {
            CALL_ERR( err, BtLibLinkDisconnect,
                      btStuff->btLibRefNum,
                      &btStuff->u.slave.masterAddr );
        }
        /* fallthru */
    case PBTST_ACL_CONNECTING:
    case PBTST_NONE:
        btStuff->connState = PBTST_NONE;
    }

    btStuff->setState = PBT_UNINIT;
}

static PalmBTStuff*
pbt_checkInit( PalmAppGlobals* globals )
{
    PalmBTStuff* btStuff = globals->btStuff;
    if ( !btStuff ) {
        Err err;
        XP_U16 btLibRefNum;

        CALL_ERR( err, SysLibFind, btLibName, &btLibRefNum );
        if ( errNone == err ) {
            btStuff = XP_MALLOC( globals->mpool, sizeof(*btStuff) );
            XP_ASSERT( !!btStuff );
            globals->btStuff = btStuff;

            XP_MEMSET( btStuff, 0, sizeof(*btStuff) );
            btStuff->globals = globals;
            btStuff->btLibRefNum = btLibRefNum;

            btStuff->dataSocket = SOCK_INVAL;
            btStuff->u.master.listenSocket = SOCK_INVAL;

            CALL_ERR( err, BtLibOpen, btLibRefNum, false );
            XP_ASSERT( errNone == err );

            CALL_ERR( err, BtLibRegisterManagementNotification, btLibRefNum, 
                      libMgmtCallback, (UInt32)btStuff );
        }
    }
    return btStuff;
} /* pbt_checkInit */

static void
l2SocketCallback( BtLibSocketEventType* sEvent, UInt32 refCon )
{
    PalmBTStuff* btStuff = (PalmBTStuff*)refCon;
    BtLibSocketEventEnum event = sEvent->event;
    Err err;

    XP_LOGF( "%s(%s); status:%s", __FUNCTION__, btEvtToStr(event),
             btErrToStr(sEvent->status) );

    switch( event ) {
    case btLibSocketEventConnectRequest: 
        /* sends btLibSocketEventConnectedInbound */
        CALL_ERR( err, BtLibSocketRespondToConnection, btStuff->btLibRefNum,  
                  sEvent->socket, true );
        break;
    case btLibSocketEventConnectedInbound:
        if ( sEvent->status == errNone ) {
            btStuff->dataSocket = sEvent->eventData.newSocket;
            XP_LOGF( "we have a data socket!!!" );
            pbt_postpone( btStuff, PBT_EVENT_TRYSEND );
        }
        break;
    case btLibSocketEventConnectedOutbound:
        if ( errNone == sEvent->status ) {
            btStuff->connState = PBTST_L2C_CONNECTED;
            pbt_postpone( btStuff, PBT_EVENT_TRYSEND );
        }
        break;
    case btLibSocketEventData:
        XP_ASSERT( sEvent->status == errNone );
        XP_ASSERT( sEvent->socket == btStuff->dataSocket );
        pbt_enqueIncoming( btStuff, sEvent->eventData.data.data, 
                           sEvent->eventData.data.dataLen );
        break;

    case btLibSocketEventSendComplete:
        btStuff->vol.sendInProgress = XP_FALSE;
        break;

    case btLibSocketEventDisconnected:
        XP_ASSERT( sEvent->socket == btStuff->dataSocket );
        btStuff->dataSocket = SOCK_INVAL;
        btStuff->connState = PBTST_ACL_CONNECTED;
        break;

    default:
        break;
    }
    LOG_RETURN_VOID();
} /* l2SocketCallback */

/***********************************************************************
 * Callbacks
 ***********************************************************************/
static void
libMgmtCallback( BtLibManagementEventType* mEvent, UInt32 refCon )
{
    PalmBTStuff* btStuff = (PalmBTStuff*)refCon;
    BtLibManagementEventEnum event = mEvent->event;
    XP_LOGF( "%s(%s)", __FUNCTION__, mgmtEvtToStr(event) );

    switch( event ) {
    case btLibManagementEventRadioState:
        XP_LOGF( "status: %s", btErrToStr(mEvent->status) );
        break;
    case btLibManagementEventACLConnectOutbound:
        if ( btLibErrNoError == mEvent->status ) {
            btStuff->connState = PBTST_ACL_CONNECTED;
            XP_LOGF( "successful ACL connection to master!" );
            pbt_postpone( btStuff, PBT_EVENT_CONNECT_L2C );
        } else {
            XP_LOGF( "bad ACL connection: %s", btErrToStr(mEvent->status) );
        }
        break;

    case btLibManagementEventACLConnectInbound:
        if ( btLibErrNoError == mEvent->status ) {
            btStuff->connState = PBTST_ACL_CONNECTED;
            XP_LOGF( "successful ACL connection!" );
        }
        break;
    case btLibManagementEventACLDisconnect:
        btStuff->dataSocket = SOCK_INVAL;
        btStuff->connState = PBTST_NONE;
        break;
    default:
        break;
    }
} /* libMgmtCallback */

/***********************************************************************
 * Debug helpers for verbose logging
 ***********************************************************************/
#ifdef DEBUG
# define CASESTR(e)    case(e): return #e

static const char*
evtToStr(PBT_EVENT evt)
{
    switch( evt ) {
        CASESTR(PBT_EVENT_NONE);
        CASESTR(PBT_EVENT_CONNECT_ACL);
        CASESTR(PBT_EVENT_CONNECT_L2C);
        CASESTR(PBT_EVENT_GOTDATA);
        CASESTR(PBT_EVENT_TRYSEND);
    default:
        XP_ASSERT(0);
        return "";
    }
} /* evtToStr */

static const char*
btEvtToStr( BtLibSocketEventEnum evt )
{
    switch( evt ) {
        CASESTR(btLibSocketEventConnectRequest);
        CASESTR(btLibSocketEventConnectedOutbound);
        CASESTR(btLibSocketEventConnectedInbound);	
        CASESTR(btLibSocketEventDisconnected);	
        CASESTR(btLibSocketEventData);
        CASESTR(btLibSocketEventSendComplete);
        CASESTR(btLibSocketEventSdpServiceRecordHandle);
        CASESTR(btLibSocketEventSdpGetAttribute);
        CASESTR(btLibSocketEventSdpGetStringLen);
        CASESTR(btLibSocketEventSdpGetNumListEntries);
        CASESTR(btLibSocketEventSdpGetNumLists);
        CASESTR(btLibSocketEventSdpGetRawAttribute);
        CASESTR(btLibSocketEventSdpGetRawAttributeSize);
        CASESTR(btLibSocketEventSdpGetServerChannelByUuid);
        CASESTR(btLibSocketEventSdpGetPsmByUuid);
        default:
    XP_ASSERT(0);
    return "";
    }
} /* btEvtToStr */

static const char*
mgmtEvtToStr( BtLibManagementEventEnum event )
{
    switch( event ) {
        CASESTR(btLibManagementEventRadioState);
        CASESTR(btLibManagementEventInquiryResult);
        CASESTR(btLibManagementEventInquiryComplete);
        CASESTR(btLibManagementEventInquiryCanceled);
        CASESTR(btLibManagementEventACLDisconnect);
        CASESTR(btLibManagementEventACLConnectInbound);
        CASESTR(btLibManagementEventACLConnectOutbound);
        CASESTR(btLibManagementEventPiconetCreated);
        CASESTR(btLibManagementEventPiconetDestroyed);
        CASESTR(btLibManagementEventModeChange);
        CASESTR(btLibManagementEventAccessibilityChange);
        CASESTR(btLibManagementEventEncryptionChange);
        CASESTR(btLibManagementEventRoleChange);
        CASESTR(btLibManagementEventNameResult);
        CASESTR(btLibManagementEventLocalNameChange);
        CASESTR(btLibManagementEventAuthenticationComplete);
        CASESTR(btLibManagementEventPasskeyRequest);
        CASESTR(btLibManagementEventPasskeyRequestComplete);
        CASESTR(btLibManagementEventPairingComplete);
    default:
        XP_ASSERT(0);
        return "unknown";
    }
} /* mgmtEvtToStr */

static const char*
btErrToStr( Err err )
{
    switch ( err ) {
        CASESTR(errNone);
        CASESTR(btLibErrError);
        CASESTR(btLibErrNotOpen);
        CASESTR(btLibErrBluetoothOff);
        CASESTR(btLibErrNoPrefs);
        CASESTR(btLibErrAlreadyOpen);
        CASESTR(btLibErrOutOfMemory);
        CASESTR(btLibErrFailed);
        CASESTR(btLibErrInProgress);
        CASESTR(btLibErrParamError);
        CASESTR(btLibErrTooMany);
        CASESTR(btLibErrPending);
        CASESTR(btLibErrNotInProgress);
        CASESTR(btLibErrRadioInitFailed);
        CASESTR(btLibErrRadioFatal);
        CASESTR(btLibErrRadioInitialized);
        CASESTR(btLibErrRadioSleepWake);
        CASESTR(btLibErrNoConnection);
        CASESTR(btLibErrAlreadyRegistered);
        CASESTR(btLibErrNoAclLink);
        CASESTR(btLibErrSdpRemoteRecord);
        CASESTR(btLibErrSdpAdvertised);
        CASESTR(btLibErrSdpFormat);
        CASESTR(btLibErrSdpNotAdvertised);
        CASESTR(btLibErrSdpQueryVersion);
        CASESTR(btLibErrSdpQueryHandle);
        CASESTR(btLibErrSdpQuerySyntax);
        CASESTR(btLibErrSdpQueryPduSize);
        CASESTR(btLibErrSdpQueryContinuation);
        CASESTR(btLibErrSdpQueryResources);
        CASESTR(btLibErrSdpQueryDisconnect);
        CASESTR(btLibErrSdpInvalidResponse);
        CASESTR(btLibErrSdpAttributeNotSet);
        CASESTR(btLibErrSdpMapped);
        CASESTR(btLibErrSocket);
        CASESTR(btLibErrSocketProtocol);
        CASESTR(btLibErrSocketRole);
        CASESTR(btLibErrSocketPsmUnavailable);
        CASESTR(btLibErrSocketChannelUnavailable);
        CASESTR(btLibErrSocketUserDisconnect);
        CASESTR(btLibErrCanceled);
        CASESTR(btLibErrBusy);
        CASESTR(btLibMeStatusUnknownHciCommand);
        CASESTR(btLibMeStatusNoConnection);
        CASESTR(btLibMeStatusHardwareFailure);
        CASESTR(btLibMeStatusPageTimeout);
        CASESTR(btLibMeStatusAuthenticateFailure);
        CASESTR(btLibMeStatusMissingKey);
        CASESTR(btLibMeStatusMemoryFull);
        CASESTR(btLibMeStatusConnnectionTimeout);
        CASESTR(btLibMeStatusMaxConnections);
        CASESTR(btLibMeStatusMaxScoConnections);
        CASESTR(btLibMeStatusMaxAclConnections);
        CASESTR(btLibMeStatusCommandDisallowed);
        CASESTR(btLibMeStatusLimitedResources);
        CASESTR(btLibMeStatusSecurityError);
        CASESTR(btLibMeStatusPersonalDevice);
        CASESTR(btLibMeStatusHostTimeout);
        CASESTR(btLibMeStatusUnsupportedFeature);
        CASESTR(btLibMeStatusInvalidHciParam);
        CASESTR(btLibMeStatusUserTerminated);
        CASESTR(btLibMeStatusLowResources);
        CASESTR(btLibMeStatusPowerOff);
        CASESTR(btLibMeStatusLocalTerminated);
        CASESTR(btLibMeStatusRepeatedAttempts);
        CASESTR(btLibMeStatusPairingNotAllowed);
        CASESTR(btLibMeStatusUnknownLmpPDU);
        CASESTR(btLibMeStatusUnsupportedRemote);
        CASESTR(btLibMeStatusScoOffsetRejected);
        CASESTR(btLibMeStatusScoIntervalRejected);
        CASESTR(btLibMeStatusScoAirModeRejected);
        CASESTR(btLibMeStatusInvalidLmpParam);
        CASESTR(btLibMeStatusUnspecifiedError);
        CASESTR(btLibMeStatusUnsupportedLmpParam);
        CASESTR(btLibMeStatusRoleChangeNotAllowed);
        CASESTR(btLibMeStatusLmpResponseTimeout);
        CASESTR(btLibMeStatusLmpTransdCollision);
        CASESTR(btLibMeStatusLmpPduNotAllowed);
        CASESTR(btLibL2DiscReasonUnknown);
        CASESTR(btLibL2DiscUserRequest);
        CASESTR(btLibL2DiscRequestTimeout);
        CASESTR(btLibL2DiscLinkDisc);
        CASESTR(btLibL2DiscQosViolation);
        CASESTR(btLibL2DiscSecurityBlock);
        CASESTR(btLibL2DiscConnPsmUnsupported);
        CASESTR(btLibL2DiscConnSecurityBlock);
        CASESTR(btLibL2DiscConnNoResources);
        CASESTR(btLibL2DiscConfigUnacceptable);
        CASESTR(btLibL2DiscConfigReject);
        CASESTR(btLibL2DiscConfigOptions);
        CASESTR(btLibServiceShutdownAppUse);
        CASESTR(btLibServiceShutdownPowerCycled);
        CASESTR(btLibServiceShutdownAclDrop);
        CASESTR(btLibServiceShutdownTimeout);
        CASESTR(btLibServiceShutdownDetached);
        CASESTR(btLibErrInUseByService);
        CASESTR(btLibErrNoPiconet);
        CASESTR(btLibErrRoleChange);
        CASESTR(btLibErrSdpNotMapped);
        CASESTR(btLibErrAlreadyConnected);
        CASESTR(btLibErrStackNotOpen);
        CASESTR(btLibErrBatteryTooLow);
        CASESTR(btLibErrNotFound);
        CASESTR(btLibNotYetSupported);
    default:
        return "unknown err";
    }
} /* btErrToStr */

static void
palm_bt_log( const char* btfunc, const char* func, Err err )
{
/*     if ( errNone != err ) { */
        XP_LOGF( "%s=>%s (in %s)", btfunc, btErrToStr(err), func );
/*     } */
}

#endif /* DEBUG */

/*
use piconet?  With that, HOST sets it up and clients join.  That establishes
ACL links that can then be used to open sockets.  I think.  How to get from
piconet advertising to clients connecting?

See http://www.palmos.com/dev/support/docs/palmos/BTCompanion.html

NOTE: I've read conflicting reports on whether a listening socket is good for
accepting more than one inbound connection.  Confirm.  Or just do a piconet.

*/
#endif /* #ifdef XWFEATURE_BLUETOOTH */
