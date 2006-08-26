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

typedef struct PalmBTStuff {
    DataCb cb;
    PalmAppGlobals* globals;

    XP_U16 btLibRefNum;
    XP_U16 lenOut;
    XP_UCHAR bufOut[L2CAPSOCKETMTU];       /* what's the mmu? */
    BtLibSocketRef dataSocket;
    XP_Bool sendInProgress;
    XP_Bool sendPending;
    XP_Bool amMaster;

    union {
        struct {
            BtLibDeviceAddressType masterAddr;
            BtLibSocketRef spdSocket;
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
        e = f(__VA_ARGS__); \
        LOG_ERR(f,e)

/* WHAT SHOULD THIS BE?  Copied from Whiteboard....  PENDING */
static const BtLibSdpUuidType XWORDS_UUID = {
    btLibUuidSize128, 
    { 0x83, 0xe0, 0x87, 0xae, 0x4e, 0x18, 0x46, 0xbe, 
      0x83, 0xe0, 0x7b, 0x3d, 0xe6, 0xa1, 0xc3, 0x3b } };

static Err initBTStuff( PalmAppGlobals* globals, DataCb cb, XP_Bool amMaster );
static void palm_bt_log( const char* btfunc, const char* func, Err err );
static void pbt_connect_slave( PalmBTStuff* btStuff, BtLibL2CapPsmType psm );
static Err bpd_discover( PalmBTStuff* btStuff, BtLibDeviceAddressType* addr );
static void pbt_setup_slave( PalmBTStuff* btStuff, const CommsAddrRec* addr );

#ifdef DEBUG
static const char* btErrToStr( Err err );
static const char* btEvtToStr( BtLibSocketEventEnum evt );
static const char* mgmtEvtToStr( BtLibManagementEventEnum event );
#else
# define btErrToStr( err ) ""
# define btEvtToStr( evt ) ""
# define mgmtEvtToStr( evt ) ""
#endif

/* callbacks */
static void libMgmtCallback( BtLibManagementEventType* mEvent, UInt32 refCon );
static void spdSocketCallback( BtLibSocketEventType* sEvent, UInt32 refCon );
static void l2SocketCallback( BtLibSocketEventType* sEvent, UInt32 refCon );

Err
palm_bt_init( PalmAppGlobals* globals, DataCb cb, XP_Bool amMaster )
{
    XP_LOGF( "%s(amMaster=%d)", __FUNCTION__, (XP_U16)amMaster );

    return initBTStuff( globals, cb, amMaster );
}

void
palm_bt_close( PalmAppGlobals* globals )
{
    PalmBTStuff* btStuff = globals->btStuff;

    if ( !!btStuff ) {
        XP_U16 btLibRefNum = btStuff->btLibRefNum;
        if ( btLibRefNum != 0 ) {
            Err err;

            /* Need to unregister callbacks */
            (void)BtLibUnregisterManagementNotification( btLibRefNum,
                                                         libMgmtCallback );
            if ( btStuff->amMaster
                 && btStuff->u.master.listenSocket != SOCK_INVAL ) {
                CALL_ERR( err, BtLibSocketClose, btLibRefNum,  
                          btStuff->u.master.listenSocket );
/*                 btStuff->u.master.listenSocket = SOCK_INVAL; */
            }
            if ( btStuff->dataSocket != SOCK_INVAL ) {
                CALL_ERR( err, BtLibSocketClose, btLibRefNum,  
                          btStuff->u.master.listenSocket );
/*                 btStuff->dataSocket = SOCK_INVAL; */
            }

            if ( !!btStuff->sdpRecordH ) {
                CALL_ERR( err, BtLibSdpServiceRecordDestroy, btLibRefNum, 
                          btStuff->sdpRecordH );
            }

            CALL_ERR( err, BtLibClose, btLibRefNum );
            XP_ASSERT( errNone == err );
        }
        XP_FREE( globals->mpool, btStuff );
        globals->btStuff = NULL;
    }
} /* palm_bt_close */

XP_Bool
palm_bt_browse_device( PalmAppGlobals* globals, XP_BtAddr* btAddr,
                       XP_UCHAR* out, XP_U16 len )
{
    Err err;
    PalmBTStuff* btStuff;

    err = initBTStuff( globals, NULL, XP_FALSE );
    if ( errNone == err ) {
        UInt16 index;
        BtLibDeviceAddressType addr;
        btStuff = globals->btStuff;

        err = bpd_discover( btStuff, &addr );
        CALL_ERR( err, BtLibSecurityFindTrustedDeviceRecord, 
                  btStuff->btLibRefNum, &addr, &index );
        CALL_ERR( err, BtLibSecurityGetTrustedDeviceRecordInfo, 
                  btStuff->btLibRefNum, index, NULL, out, len, 
                  NULL, NULL, NULL );
        XP_ASSERT( sizeof(*btAddr) >= sizeof(addr) );
        XP_MEMCPY( btAddr, &addr, sizeof(addr) );
        
        LOG_HEX( &addr, sizeof(addr), __FUNCTION__ );

/*         err = BtLibGetRemoteDeviceName( btStuff->btLibRefNum,   */
/*                                         BtLibDeviceAddressTypePtr remoteDeviceP,  */
/*                                         BtLibFriendlyNameType* nameP,  */
/*                                         BtLibGetNameEnum retrievalMethod ); */
/*         err = BtLibAddrBtdToA( btStuff->btLibRefNum, &btStuff->u.slave.masterAddr,  */
/*                                out, len ); */
    } else {
        XP_WARNF( "%s: err = %s", __FUNCTION__, btErrToStr(err) );
    }
    return errNone == err;
} /* palm_bt_browse_device */

XP_Bool
btSocketIsOpen( PalmAppGlobals* globals )
{
    return (NULL != globals->btStuff)
        && (globals->btStuff->dataSocket != 0);
}

static void
pbt_send_pending( PalmBTStuff* btStuff, const CommsAddrRec* addr )
{
    Err err;
    LOG_FUNC();
    if ( btStuff->sendPending && !btStuff->sendInProgress ) {
        if ( btStuff->dataSocket != SOCK_INVAL ) {
            if ( btStuff->lenOut > 0 ) { /* hack: zero-len send to cause connect */
                CALL_ERR( err, BtLibSocketSend, btStuff->btLibRefNum, 
                          btStuff->dataSocket, 
                          btStuff->bufOut, btStuff->lenOut );
                if ( err == errNone ) {
                    // clear on receipt of btLibSocketEventSendComplete 
                    btStuff->sendInProgress = XP_TRUE;
                }
            } else {
                btStuff->sendPending = XP_FALSE;
            }
        } else {
            /* No data socket? */
            if ( !btStuff->amMaster ) {
                pbt_setup_slave( btStuff, addr );
            }
        }
    }
} /* pbt_send_pending */

static void
pbt_check_socket( PalmBTStuff* btStuff, const CommsAddrRec* addr )
{
    LOG_FUNC();
    if ( btStuff->dataSocket == SOCK_INVAL ) {
        if ( btStuff->amMaster ) {
            XP_ASSERT(0);
/*             pbt_setup_master( btStuff ); */
        } else {
            pbt_setup_slave( btStuff, addr );
        }
    }
}

XP_S16
palm_bt_send( const XP_U8* buf, XP_U16 len, const CommsAddrRec* addr,
              PalmAppGlobals* globals )
{
    XP_S16 nSent = -1;
    PalmBTStuff* btStuff;
    CommsAddrRec remoteAddr;
    XP_LOGF( "%s(len=%d)", __FUNCTION__, len );

    if ( !addr ) {
        comms_getAddr( globals->game.comms, &remoteAddr );
        addr = &remoteAddr;
    }
    XP_ASSERT( !!addr );

    btStuff = globals->btStuff;
    if ( !!btStuff ) {
        pbt_check_socket( btStuff, addr );

        if ( !!btStuff ) {
            if ( !btStuff->sendInProgress ) {
                if ( len > sizeof( btStuff->bufOut ) ) {
                    len = sizeof( btStuff->bufOut );
                }
                XP_MEMCPY( btStuff->bufOut, buf, len );
                btStuff->lenOut = len;
                btStuff->sendPending = XP_TRUE;            

                pbt_send_pending( btStuff, addr );
                nSent = len;
            } else {
                XP_LOGF( "%s: send ALREADY in progress", __FUNCTION__ );
            }
        }
    } else {
        XP_LOGF( "%s: btStuff null", __FUNCTION__ );
    }
    LOG_RETURNF( "%d", nSent );
    return nSent;
} /* palm_bt_send */

static void
pbt_find_psm( PalmBTStuff* btStuff )
{
    Err err;
    LOG_FUNC();

    XP_ASSERT( !btStuff->amMaster );

    CALL_ERR( err, BtLibSocketCreate, btStuff->btLibRefNum, 
              &btStuff->u.slave.spdSocket, 
              spdSocketCallback, (UInt32)btStuff, btLibSdpProtocol );

    /* sends btLibSocketEventSdpGetPsmByUuid */
    CALL_ERR( err, BtLibSdpGetPsmByUuid, btStuff->btLibRefNum, 
              btStuff->u.slave.spdSocket,
              &btStuff->u.slave.masterAddr,
              (BtLibSdpUuidType*)&XWORDS_UUID, 1 );
}

static void
pbt_connect_slave( PalmBTStuff* btStuff, BtLibL2CapPsmType psm )
{
    Err err;
    XP_ASSERT( !btStuff->amMaster );

    XP_LOGF( "%s(psm=%x)", __FUNCTION__, psm );

    CALL_ERR( err, BtLibSocketCreate, btStuff->btLibRefNum, 
              &btStuff->dataSocket, 
              l2SocketCallback, (UInt32)btStuff, 
              btLibL2CapProtocol );

    if ( btLibErrNoError == err ) {
        BtLibSocketConnectInfoType connInfo;
        connInfo.data.L2Cap.remotePsm = psm;
        connInfo.data.L2Cap.localMtu = L2CAPSOCKETMTU; 
        connInfo.data.L2Cap.minRemoteMtu = L2CAPSOCKETMTU;
        connInfo.remoteDeviceP = &btStuff->u.slave.masterAddr;
	
        /* sends btLibManagementEventACLConnectOutbound */
        CALL_ERR( err, BtLibSocketConnect, btStuff->btLibRefNum, 
                  btStuff->dataSocket, &connInfo );
    }
}

static void
pbt_setup_master( PalmBTStuff* btStuff )
{
    /* Will eventually want to create a piconet here for more than two
       devices to play.... */
    Err err;
    BtLibSocketListenInfoType listenInfo;

    btStuff->u.master.listenSocket = SOCK_INVAL;

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

    /*    4. BtLibSdpServiceRecordSetAttributesForSocket: initialize an SDP
         memory record so it can represent the newly-created L2CAP listener
         socket as a service */
    CALL_ERR( err, BtLibSdpServiceRecordSetAttributesForSocket,
              btStuff->btLibRefNum, btStuff->u.master.listenSocket, 
              (BtLibSdpUuidType*)&XWORDS_UUID, 1, APPNAME, 
              StrLen(APPNAME), btStuff->sdpRecordH );

/*    5. BtLibSdpServiceRecordStartAdvertising: make an SDP memory record
         representing a local SDP service record visible to remote
         devices.  */
    CALL_ERR( err, BtLibSdpServiceRecordStartAdvertising, btStuff->btLibRefNum,
              btStuff->sdpRecordH );
} /* pbt_setup_master */

static Err
bpd_discover( PalmBTStuff* btStuff, BtLibDeviceAddressType* addr )
{
    Err err;
    static const BtLibClassOfDeviceType deviceFilter
        = btLibCOD_ServiceAny
        | btLibCOD_Major_Any // btLibCOD_Major_Computer
        | btLibCOD_Minor_Comp_Any; //btLibCOD_Minor_Comp_Palm;

    CALL_ERR( err, BtLibDiscoverSingleDevice, btStuff->btLibRefNum, "Crosswords host",
              (BtLibClassOfDeviceType*)&deviceFilter, 1,
              addr, false, false );
    LOG_RETURNF( "%s", btErrToStr(err) );
    return err;
} /* bpd_discover */

static void
pbt_setup_slave( PalmBTStuff* btStuff, const CommsAddrRec* addr )
{
    LOG_FUNC();
    XP_ASSERT( !btStuff->amMaster );

    if ( !!addr ) {
        char buf[64];
        if ( errNone == BtLibAddrBtdToA( btStuff->btLibRefNum, 
                                         (BtLibDeviceAddressType*)&addr->u.bt.btAddr,
                                         buf, sizeof(buf) ) ) {
            XP_LOGF( "%s(%s)", __FUNCTION__, buf );
        }
    } else {
        XP_LOGF( "null addr" );
    }

    if ( !btStuff->u.slave.addrSet && !!addr ) {
        Err err;

        /* Our xp type better be big enough */
        XP_ASSERT( sizeof(addr->u.bt.btAddr)
                   >= sizeof(btStuff->u.slave.masterAddr) );
        XP_MEMCPY( &btStuff->u.slave.masterAddr, addr->u.bt.btAddr, 
                   sizeof(btStuff->u.slave.masterAddr) );

        btStuff->u.slave.addrSet = XP_TRUE;

        CALL_ERR( err, BtLibLinkConnect, btStuff->btLibRefNum, 
                  &btStuff->u.slave.masterAddr );
        XP_ASSERT( err == btLibErrPending );
    } else {
        XP_LOGF( "%s: doing nothing", __FUNCTION__ );
    }
} /* pbt_setup_slave */

static Err
initBTStuff( PalmAppGlobals* globals, DataCb cb, XP_Bool amMaster )
{
    PalmBTStuff* btStuff;
    Err err;

    LOG_FUNC();

    btStuff = globals->btStuff;
    if ( btStuff != NULL ) {
        if ( NULL != cb ) {
            btStuff->cb = cb;
        }
        if ( btStuff->amMaster == amMaster ) {
            /* nothing to do */
        } else {
            /* role change.  Adapt... */
            XP_ASSERT( 0 );
        }
        err = errNone;
    } else {
        XP_U16 btLibRefNum;

        CALL_ERR( err, SysLibFind, btLibName, &btLibRefNum );
        if ( errNone == err ) {

            btStuff = XP_MALLOC( globals->mpool, sizeof(*btStuff) );
            XP_ASSERT( !!btStuff );
            XP_MEMSET( btStuff, 0, sizeof(*btStuff) );
            globals->btStuff = btStuff;

            btStuff->globals = globals;
            btStuff->cb = cb;
            btStuff->amMaster = amMaster;
            btStuff->dataSocket = SOCK_INVAL;

            btStuff->btLibRefNum = btLibRefNum;

            CALL_ERR( err, BtLibOpen, btLibRefNum, false );
            XP_ASSERT( errNone == err );

            CALL_ERR( err, BtLibRegisterManagementNotification, btLibRefNum, 
                      libMgmtCallback, (UInt32)btStuff );

            if ( btStuff->amMaster ) {
                pbt_setup_master( btStuff );
            } else {
                /* Can't set up b/c don't have address yet. */
 /*                pbt_setup_slave( btStuff, addr ); */
            }
        }
    }
    LOG_RETURNF( "%s", btErrToStr(err) );
    return err;
} /* initBTStuff */

static void
l2SocketCallback( BtLibSocketEventType* sEvent, UInt32 refCon )
{
    PalmBTStuff* btStuff = (PalmBTStuff*)refCon;
    BtLibSocketEventEnum event = sEvent->event;
    Err err;

    XP_LOGF( "%s(%s)", __FUNCTION__, btEvtToStr(event) );

    switch( event ) {
    case btLibSocketEventConnectRequest: 
        CALL_ERR( err, BtLibSocketRespondToConnection, btStuff->btLibRefNum,  
                  sEvent->socket, true );
        break;
    case btLibSocketEventConnectedInbound:
        if ( sEvent->status == errNone ) {
            btStuff->dataSocket = sEvent->eventData.newSocket;
            XP_LOGF( "we have a data socket!!!" );
            pbt_send_pending( btStuff, NULL );
        } else {
            XP_LOGF( "%s: status = %d(%s)", __FUNCTION__, 
                     sEvent->status, btErrToStr(sEvent->status) );
        }
        break;
    case btLibSocketEventConnectedOutbound:
        pbt_send_pending( btStuff, NULL );
        break;
    case btLibSocketEventData:
        XP_ASSERT( !!btStuff->cb );
        (*btStuff->cb)( btStuff->globals, sEvent->eventData.data.data,
                        sEvent->eventData.data.dataLen );
        break;
    default:
        break;
    }
} /* l2SocketCallback */

/***********************************************************************
 * Callbacks
 ***********************************************************************/
static void
spdSocketCallback( BtLibSocketEventType* sEvent, UInt32 refCon )
{
    Err err;
    PalmBTStuff* btStuff = (PalmBTStuff*)refCon;
    BtLibSocketEventEnum event = sEvent->event;

    XP_LOGF( "%s(%s)", __FUNCTION__, btEvtToStr(event) );
    XP_ASSERT( sEvent->socket == btStuff->u.slave.spdSocket );
    XP_ASSERT( !btStuff->amMaster );

    switch( event ) {
    case btLibSocketEventSdpGetPsmByUuid:
        if ( btLibErrNoError == sEvent->status ) {
            CALL_ERR( err, BtLibSocketClose, btStuff->btLibRefNum, 
                      sEvent->socket );
            btStuff->u.slave.spdSocket = SOCK_INVAL;
            pbt_connect_slave( btStuff, 
                               sEvent->eventData.sdpByUuid.param.psm );
        }
        break;
    default:                    /* happy now, compiler? */
        break;
    }
} /* spdSocketCallback */

static void
libMgmtCallback( BtLibManagementEventType* mEvent, UInt32 refCon )
{
    PalmBTStuff* btStuff = (PalmBTStuff*)refCon;
    BtLibManagementEventEnum event = mEvent->event;
    XP_LOGF( "%s(%s)", __FUNCTION__, mgmtEvtToStr(event) );

    switch( event ) {
    case btLibManagementEventACLConnectOutbound:
        if ( btLibErrNoError == mEvent->status ) {
            XP_LOGF( "successful ACL connection to master!" );
            pbt_find_psm( btStuff );
        }
        break;

    case btLibManagementEventACLConnectInbound:
        if ( btLibErrNoError == mEvent->status ) {
            XP_LOGF( "successful ACL connection!" );
        }
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
        CASESTR(btLibErrNoError);
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
        XP_LOGF( "%s from %s called in %s", btErrToStr(err), btfunc, func );
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
