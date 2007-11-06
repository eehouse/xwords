/* -*-mode: C; fill-column: 77; c-basic-offset: 4; compile-command: "make ARCH=68K_ONLY MEMDEBUG=TRUE"; -*- */
/* 
 * Copyright 2006-2007 by Eric House (xwords@eehouse.org).  All rights
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

#ifdef XWFEATURE_BLUETOOTH

#include "xptypes.h"
#include "palmbt.h"
#include "strutils.h"

# include <BtLib.h>
# include <BtLibTypes.h>

#define L2CAPSOCKETMTU 500
#define SOCK_INVAL ((BtLibSocketRef)-1)

#define DO_SERVICE_RECORD 1
#define ACL_WAIT_INTERVAL 8

typedef enum { PBT_UNINIT = 0, PBT_MASTER, PBT_SLAVE } PBT_PicoRole;

typedef enum {
    PBT_ACT_NONE
    , PBT_ACT_SETUP_LISTEN
    , PBT_ACT_CONNECT_ACL
    , PBT_ACT_GETSDP            /* slave only */
    , PBT_ACT_CONNECT_L2C
    , PBT_ACT_TELLCONN
    , PBT_ACT_GOTDATA           /* can be duplicated */
    , PBT_ACT_TRYSEND
} PBT_ACTION;

#define DUPLICATES_OK(a)  ((a) >= PBT_ACT_GOTDATA)

typedef enum {
    PBTST_NONE
    , PBTST_LISTENING           /* master */
    , PBTST_ACL_CONNECTING      /* slave */
    , PBTST_ACL_CONNECTED       /* slave */
    , PBTST_SDP_QUERYING        /* slave */
    , PBTST_SDP_QUERIED         /* slave */
    , PBTST_L2C_CONNECTING      /* slave */
    , PBTST_L2C_CONNECTED       /* slave */
} PBT_STATE;

#define PBT_MAX_ACTS 8          /* six wasn't enough */
#define HASWORK(s)  ((s)->queueLen > 0)
#define MAX_PACKETS 4

typedef struct PBT_queue {
    XP_U16 lens[MAX_PACKETS];
    XP_U8 bufs[L2CAPSOCKETMTU*2];       /* what's the mmu? */
} PBT_queue;

typedef struct PalmBTStuff {
    DataCb dataCb;
    OnConnCb connCb;
    PalmAppGlobals* globals;

    XP_U16 btLibRefNum;

    struct {
        PBT_queue in;
        PBT_queue out;

        XP_Bool sendInProgress;
    } vol;

    /* peer's addr: passed in by UI in case of slave, received via connection
       in case of master.  Piconet master will need an array of these. */
    BtLibDeviceAddressType otherAddr;
    BtLibSocketRef dataSocket;
    PBT_PicoRole picoRole;
    PBT_STATE p_connState;
    BtLibAccessibleModeEnum accState;

    PBT_ACTION actQueue[PBT_MAX_ACTS];
    XP_U16 queueLen;

    struct /*union*/ {
        struct {
            BtLibL2CapPsmType remotePsm;
            BtLibSocketRef sdpSocket;
        } slave;
        struct {
            BtLibSocketRef listenSocket;
#ifdef DO_SERVICE_RECORD
            BtLibSdpRecordHandle sdpRecordH;
#endif
        } master;
    } u;

#ifdef DEBUG
    struct {
        XP_U32 totalSent;
        XP_U32 totalRcvd;
    } stats;
#endif
} PalmBTStuff;

#ifdef DEBUG
static void palm_bt_log( const char* btfunc, const char* func, Err err );
#define LOG_ERR(f,e) palm_bt_log( #f, __FUNCTION__, e )
#define CALL_ERR(e,f,...) \
    XP_LOGF( "%s: calling %s", __FUNCTION__, #f ); \
    e = f(__VA_ARGS__); \
    LOG_ERR(f,e); \
    if ( e == btLibErrFailed ) { XP_WARNF( "%s=>btLibErrFailed", #f ); }
#else
#define CALL_ERR(e,f,...)    e = f(__VA_ARGS__) 
#endif

static const BtLibSdpUuidType XWORDS_UUID = {
    btLibUuidSize128, 
    XW_BT_UUID
};

static PalmBTStuff* pbt_checkInit( PalmAppGlobals* globals, 
                                   XP_Bool* userCancelled );
static Err bpd_discover( PalmBTStuff* btStuff, BtLibDeviceAddressType* addr );
static void pbt_setup_slave( PalmBTStuff* btStuff, const CommsAddrRec* addr );
static void pbt_takedown_slave( PalmBTStuff* btStuff );
static void pbt_setup_master( PalmBTStuff* btStuff );
static void pbt_takedown_master( PalmBTStuff* btStuff );
static void pbt_do_work( PalmBTStuff* btStuff );
static void pbt_postpone( PalmBTStuff* btStuff, PBT_ACTION act );
static XP_S16 pbt_enque( PBT_queue* queue, const XP_U8* data, XP_S16 len );
static void pbt_processIncoming( PalmBTStuff* btStuff );

static void waitACL( PalmBTStuff* btStuff );
static void pbt_reset( PalmBTStuff* btStuff );
static void pbt_killL2C( PalmBTStuff* btStuff, BtLibSocketRef sock );
static void pbt_checkAddress( PalmBTStuff* btStuff, const CommsAddrRec* addr );
static void pbt_setstate( PalmBTStuff* btStuff, PBT_STATE newState,
                          const char* whence );
#define SET_STATE(b,s)  pbt_setstate((b),(s),__FUNCTION__)
#define GET_STATE(b)    ((b)->p_connState)

#ifdef DEBUG
static const char* btErrToStr( Err err );
static const char* btEvtToStr( BtLibSocketEventEnum evt );
static const char* mgmtEvtToStr( BtLibManagementEventEnum event );
static const char* actToStr(PBT_ACTION act);
static const char* stateToStr(PBT_STATE st);
static const char* connEnumToStr( BtLibAccessibleModeEnum mode );
static const char* proleToString( PBT_PicoRole r );

#else
# define btErrToStr( err ) ""
# define btEvtToStr( evt ) ""
# define mgmtEvtToStr( evt ) ""
# define actToStr(act) ""
# define stateToStr(st) ""
# define connEnumToStr(mode) ""
# define proleToString(r) ""
#endif

/* callbacks */
static void libMgmtCallback( BtLibManagementEventType* mEvent, UInt32 refCon );
static void socketCallback( BtLibSocketEventType* sEvent, UInt32 refCon );

XP_Bool
palm_bt_init( PalmAppGlobals* globals, DataCb dataCb, XP_Bool* userCancelled )
{
    XP_Bool inited;
    PalmBTStuff* btStuff;

    LOG_FUNC();

    btStuff = globals->btStuff;
    if ( !btStuff ) {
        btStuff = pbt_checkInit( globals, userCancelled );
        /* Should I start master/slave setup here?  If not, how? */
    } else {
        pbt_reset( btStuff );
    }

    /* If we're the master, and a new game is starting without shutting down,
       and the client has already sent its initial reg message, we'll have
       dropped it.  Best way to force it to resend is to kill the connection
       and force it to connect again.  */

    inited = !!btStuff;
    if ( inited ) {
        if ( comms_getIsServer( globals->game.comms ) ) {
            pbt_setup_master( btStuff );
        } else if ( btStuff->picoRole == PBT_MASTER ) {
            pbt_takedown_master( btStuff );
        }

        btStuff->dataCb = dataCb;
    }
    LOG_RETURNF( "%d", (XP_U16)inited );
    return inited;
} /* palm_bt_init */

void
palm_bt_close( PalmAppGlobals* globals )
{
    PalmBTStuff* btStuff = globals->btStuff;

    if ( !!btStuff ) {
        XP_U16 btLibRefNum = btStuff->btLibRefNum;
        if ( btLibRefNum != 0 ) {
            Err err;

            if ( btStuff->picoRole == PBT_MASTER ) {
                pbt_takedown_master( btStuff );
            } else if ( btStuff->picoRole == PBT_SLAVE ) {
                pbt_takedown_slave( btStuff );
            }

            /* Need to unregister callbacks */
            CALL_ERR( err, BtLibUnregisterManagementNotification, btLibRefNum,
                      libMgmtCallback );
            XP_ASSERT( errNone == err );
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
palm_bt_doWork( PalmAppGlobals* globals, BtUIState* btUIStateP )
{
    PalmBTStuff* btStuff = globals->btStuff;
    XP_Bool haveWork = !!btStuff && HASWORK(btStuff);

    if ( haveWork ) {
        pbt_do_work( btStuff );
    }
    if ( !!btStuff && !!btUIStateP ) {
        BtUIState btUIState = BTUI_NONE; /* default */
        switch( GET_STATE(btStuff) ) {
        case PBTST_NONE: 
            break;
        case PBTST_LISTENING:
            btUIState = BTUI_LISTENING; 
            break;
        case PBTST_ACL_CONNECTING:
        case PBTST_ACL_CONNECTED:
        case PBTST_SDP_QUERYING:
        case PBTST_SDP_QUERIED:
        case PBTST_L2C_CONNECTING:
            btUIState = BTUI_CONNECTING; 
            break;
        case PBTST_L2C_CONNECTED:
            btUIState = btStuff->picoRole == PBT_MASTER?
                BTUI_SERVING : BTUI_CONNECTED; 
            break;
        }
        *btUIStateP = btUIState;
    }
    return haveWork;
} /* palm_bt_doWork */

void
palm_bt_addrString( PalmAppGlobals* globals, XP_BtAddr* btAddr, 
                    XP_BtAddrStr* str )
{
    PalmBTStuff* btStuff = pbt_checkInit( globals, NULL );
    str->chars[0] = '\0';
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
    XP_Bool success = XP_FALSE;
    PalmBTStuff* btStuff;

    LOG_FUNC();

    btStuff = pbt_checkInit( globals, NULL );
    if ( NULL != btStuff ) {
        BtLibDeviceAddressType addr;
        Err err = bpd_discover( btStuff, &addr );

        if ( errNone == err ) {
            UInt16 index;
            UInt8 name[PALM_BT_NAME_LEN];
            BtLibFriendlyNameType nameType = {
                .name = name, 
                .nameLength = sizeof(name) 
            };

            CALL_ERR( err, BtLibSecurityFindTrustedDeviceRecord, 
                      btStuff->btLibRefNum, &addr, &index );
            XP_ASSERT( sizeof(*btAddr) >= sizeof(addr) );
            XP_MEMCPY( btAddr, &addr, sizeof(addr) );
        
            LOG_HEX( &addr, sizeof(addr), __FUNCTION__ );

            CALL_ERR( err, BtLibGetRemoteDeviceName, btStuff->btLibRefNum,
                      &addr, &nameType, btLibCachedThenRemote );
            XP_LOGF( "%s: got name %s", __func__, nameType.name );

            XP_ASSERT( len >= nameType.nameLength );
            XP_MEMCPY( out, nameType.name, nameType.nameLength );
        }
        success = errNone == err;
    }
    LOG_RETURNF( "%d", (XP_U16)success );
    return success;
} /* palm_bt_browse_device */

#ifdef DEBUG
void
palm_bt_getStats( PalmAppGlobals* globals, XWStreamCtxt* stream )
{
    PalmBTStuff* btStuff = globals->btStuff;
    if ( !btStuff ) {
        stream_putString( stream, "bt not initialized" );
    } else {
        char buf[64];
        XP_U16 cur;

        XP_SNPRINTF( buf, sizeof(buf), "Role: %s\n", 
                     btStuff->picoRole == PBT_MASTER? "master":
                     (btStuff->picoRole == PBT_SLAVE? "slave":"unknown") );
        stream_putString( stream, buf );
        XP_SNPRINTF( buf, sizeof(buf), "State: %s\n", 
                     stateToStr( GET_STATE(btStuff)) );
        stream_putString( stream, buf );

        XP_SNPRINTF( buf, sizeof(buf), "%d actions queued:\n", 
                     btStuff->queueLen );
        stream_putString( stream, buf );
        for ( cur = 0; cur < btStuff->queueLen; ++cur ) {
            XP_SNPRINTF( buf, sizeof(buf), " - %s\n",
                         actToStr( btStuff->actQueue[cur] ) );
            stream_putString( stream, buf );
        }

        XP_SNPRINTF( buf, sizeof(buf), "total sent: %ld\n",
                     btStuff->stats.totalSent );
        stream_putString( stream, buf );
        XP_SNPRINTF( buf, sizeof(buf), "total rcvd: %ld\n",
                     btStuff->stats.totalRcvd );
        stream_putString( stream, buf );
    }
}
#endif

static XP_U16
pbt_peekQueue( const PBT_queue* queue, const XP_U8** bufp )
{
    XP_U16 len = queue->lens[0];
    if ( len > 0 ) {
        *bufp = &queue->bufs[0];
    }
    return len;
}

static XP_U16
pbt_shiftQueue( PBT_queue* queue )
{
    XP_U16 len = queue->lens[0];
    XP_ASSERT( len != 0 );
    XP_MEMCPY( &queue->lens[0], &queue->lens[1], 
               sizeof(queue->lens) - sizeof(queue->lens[0]) );
    queue->lens[MAX_PACKETS-1] = 0; /* be safe */
    XP_MEMCPY( queue->bufs, queue->bufs + len, 
               sizeof(queue->bufs) - len );
    return len;
} /* pbt_shiftQueue */

static void
pbt_send_pending( PalmBTStuff* btStuff )
{
    Err err;
    LOG_FUNC();

    if ( !btStuff->vol.sendInProgress ) {
        const XP_U8* buf;
        XP_U16 len = pbt_peekQueue( &btStuff->vol.out, &buf );

        if ( SOCK_INVAL == btStuff->dataSocket ) {
/*             XP_LOGF( "%s: abort: inval socket", __func__ ); */
        } else if ( len <= 0 ) {
/*             XP_LOGF( "%s: abort: len is %d", __func__, len ); */
        } else {
            CALL_ERR( err, BtLibSocketSend, btStuff->btLibRefNum, 
                      btStuff->dataSocket, (char*)buf, len );
            if ( btLibErrPending == err ) {
                btStuff->vol.sendInProgress = XP_TRUE;
            }
        }
    }
    LOG_RETURN_VOID();
} /* pbt_send_pending */

XP_S16
palm_bt_send( const XP_U8* buf, XP_U16 len, const CommsAddrRec* addr,
              DataCb dataCb, OnConnCb connCb, PalmAppGlobals* globals,
              XP_Bool* userCancelled )
{
    XP_S16 nSent = -1;
    PalmBTStuff* btStuff;
    CommsAddrRec remoteAddr;
    PBT_PicoRole picoRole;
    XP_LOGF( "%s(len=%d)", __FUNCTION__, len);

    btStuff = pbt_checkInit( globals, userCancelled );
    if ( !!btStuff ) {
        if ( !btStuff->dataCb ) {
            btStuff->dataCb = dataCb;
        } else {
            XP_ASSERT( dataCb == btStuff->dataCb );
        }
        btStuff->connCb = connCb;

        if ( !addr ) {
            comms_getAddr( globals->game.comms, &remoteAddr );
            addr = &remoteAddr;
        }
        XP_ASSERT( !!addr );

        picoRole = btStuff->picoRole;
        XP_LOGF( "%s: role=%s", __FUNCTION__, proleToString(picoRole) );
        if ( picoRole == PBT_UNINIT ) {
            XP_Bool amMaster = comms_getIsServer( globals->game.comms );
            picoRole = amMaster? PBT_MASTER : PBT_SLAVE;
        }

        pbt_checkAddress( btStuff, addr );

        if ( picoRole == PBT_MASTER ) {
            pbt_setup_master( btStuff );
        } else {
            pbt_setup_slave( btStuff, addr );
        }

        nSent = pbt_enque( &btStuff->vol.out, buf, len );
        pbt_send_pending( btStuff );
    }
    LOG_RETURNF( "%d", nSent );
    return nSent;
} /* palm_bt_send */


#ifdef DO_SERVICE_RECORD
static XP_Bool
setupServiceRecord( PalmBTStuff* btStuff )
{
    Err err;
    /*    3. BtLibSdpServiceRecordCreate: allocate a memory chunk that
          represents an SDP service record. */
    CALL_ERR( err, BtLibSdpServiceRecordCreate,
              btStuff->btLibRefNum, &btStuff->u.master.sdpRecordH );

    /*    4. BtLibSdpServiceRecordSetAttributesForSocket: initialize an
          SDP memory record so it can represent the newly-created L2CAP
          listener socket as a service */
    if ( errNone == err ) {
        CALL_ERR( err, BtLibSdpServiceRecordSetAttributesForSocket,
                  btStuff->btLibRefNum, btStuff->u.master.listenSocket, 
                  (BtLibSdpUuidType*)&XWORDS_UUID, 1, XW_BT_NAME, 
                  StrLen(XW_BT_NAME), btStuff->u.master.sdpRecordH );

        /*    5. BtLibSdpServiceRecordStartAdvertising: make an SDP memory
              record representing a local SDP service record visible to
              remote devices.  */
        if ( errNone == err ) {
            CALL_ERR( err, BtLibSdpServiceRecordStartAdvertising, 
                      btStuff->btLibRefNum, btStuff->u.master.sdpRecordH );
        }
    }
    /* If this fails commonly, need to free the structure and try again */
    XP_ASSERT( errNone == err );
    return errNone == err;
} /* setupServiceRecord */
#else
# define setupServiceRecord(b) XP_TRUE
#endif

static void
pbt_setup_master( PalmBTStuff* btStuff )
{
    if ( btStuff->picoRole == PBT_SLAVE ) {
        pbt_takedown_slave( btStuff );
    }
    btStuff->picoRole = PBT_MASTER;

    if ( btStuff->u.master.listenSocket == SOCK_INVAL ) {
        /* Will eventually want to create a piconet here for more than two
           devices to play.... */

        Err err;
        BtLibSocketListenInfoType listenInfo;

        /*    1. BtLibSocketCreate: create an L2CAP socket. */
        CALL_ERR( err, BtLibSocketCreate, btStuff->btLibRefNum, 
                  &btStuff->u.master.listenSocket, socketCallback,
                  (UInt32)btStuff, btLibL2CapProtocol );
        XP_ASSERT( errNone == err );

        /*    2. BtLibSocketListen: set up an L2CAP socket as a listener. */
        XP_MEMSET( &listenInfo, 0, sizeof(listenInfo) );
        listenInfo.data.L2Cap.localPsm = BT_L2CAP_RANDOM_PSM;
        listenInfo.data.L2Cap.localMtu = L2CAPSOCKETMTU; 
        listenInfo.data.L2Cap.minRemoteMtu = L2CAPSOCKETMTU;
        /* Doesn't send events; returns errNone unless no resources avail. */
        CALL_ERR( err, BtLibSocketListen, btStuff->btLibRefNum, 
                  btStuff->u.master.listenSocket, &listenInfo );
        if ( (errNone == err) && setupServiceRecord( btStuff ) ) {
            /* Set state here to indicate I'm available, at least for
               debugging? */
            SET_STATE( btStuff, PBTST_LISTENING );
        } else {
            CALL_ERR( err, BtLibSocketClose, btStuff->btLibRefNum,
                      btStuff->u.master.listenSocket );
            btStuff->u.master.listenSocket = SOCK_INVAL;
            pbt_postpone( btStuff, PBT_ACT_SETUP_LISTEN );
        }
    }
    XP_ASSERT( NULL != btStuff->u.master.sdpRecordH );
} /* pbt_setup_master */

static void
pbt_close_datasocket( PalmBTStuff* btStuff )
{
    if ( SOCK_INVAL != btStuff->dataSocket ) {
        Err err;
        CALL_ERR( err, BtLibSocketClose, btStuff->btLibRefNum,
                  btStuff->dataSocket );
        XP_ASSERT( err == errNone );
        btStuff->dataSocket = SOCK_INVAL;
    }
}

static void
pbt_takedown_master( PalmBTStuff* btStuff )
{
    XP_U16 btLibRefNum;
    Err err;

    LOG_FUNC();
    
    XP_ASSERT( btStuff->picoRole == PBT_MASTER );
    btLibRefNum = btStuff->btLibRefNum;

    pbt_close_datasocket( btStuff );

#ifdef DO_SERVICE_RECORD
    if ( !!btStuff->u.master.sdpRecordH ) {
        CALL_ERR( err, BtLibSdpServiceRecordStopAdvertising,
                  btLibRefNum, btStuff->u.master.sdpRecordH );
        XP_ASSERT( errNone == err ); /* no errors if it was being advertised */

        CALL_ERR( err, BtLibSdpServiceRecordDestroy, btLibRefNum, 
                  btStuff->u.master.sdpRecordH );
        btStuff->u.master.sdpRecordH = NULL;
    }
#endif

    if ( SOCK_INVAL != btStuff->u.master.listenSocket ) {
        CALL_ERR( err, BtLibSocketClose, btLibRefNum,  
                  btStuff->u.master.listenSocket );
        btStuff->u.master.listenSocket = SOCK_INVAL;
        XP_ASSERT( err == errNone );
    }

    btStuff->picoRole = PBT_UNINIT;
    SET_STATE( btStuff, PBTST_NONE );
    LOG_RETURN_VOID();
} /* pbt_takedown_master */

#ifdef DEBUG
static void
debug_logQueue( const PalmBTStuff* const btStuff )
{
    XP_U16 i;
    XP_U16 len = btStuff->queueLen;
    XP_LOGF( "%s: queue len = %d", __func__, len );
    for ( i = 0; i < len; ++i ) {
        XP_LOGF( "\t%d: %s", i, actToStr( btStuff->actQueue[i] ) );
    }
}
#else
#define debug_logQueue( bts )
#endif

static void
pbt_do_work( PalmBTStuff* btStuff )
{
    PBT_ACTION act;
    Err err;
    XP_U16 btLibRefNum = btStuff->btLibRefNum;

    debug_logQueue( btStuff );

    act = btStuff->actQueue[0];
    --btStuff->queueLen;
    XP_MEMCPY( &btStuff->actQueue[0], &btStuff->actQueue[1], 
               btStuff->queueLen * sizeof(btStuff->actQueue[0]) );

    XP_LOGF( "%s: evt=%s; state=%s", __FUNCTION__, actToStr(act),
             stateToStr(GET_STATE(btStuff)) );

    switch( act ) {
    case PBT_ACT_SETUP_LISTEN:
        XP_ASSERT( btStuff->picoRole == PBT_MASTER );
        pbt_setup_master( btStuff );
        break;

    case PBT_ACT_CONNECT_ACL:
        if ( GET_STATE(btStuff) == PBTST_NONE ) {
            /* sends btLibManagementEventACLConnectOutbound */
            CALL_ERR( err, BtLibLinkConnect, btLibRefNum, 
                      &btStuff->otherAddr );
            if ( btLibErrPending == err ) {
                SET_STATE( btStuff, PBTST_ACL_CONNECTING );
            } else if ( btLibErrAlreadyConnected == err ) {
                SET_STATE( btStuff, PBTST_ACL_CONNECTED );
                pbt_postpone( btStuff, PBT_ACT_GETSDP );
            }
        }
        break;

    case PBT_ACT_GETSDP:
        XP_ASSERT( PBTST_ACL_CONNECTED == GET_STATE(btStuff) ); /* still firing */
        CALL_ERR( err, BtLibSocketCreate, btStuff->btLibRefNum,
                  &btStuff->u.slave.sdpSocket, socketCallback, (UInt32)btStuff,
                  btLibSdpProtocol );
        if ( err == errNone ) {
	    CALL_ERR( err, BtLibSdpGetPsmByUuid, btStuff->btLibRefNum, 
                      btStuff->u.slave.sdpSocket, &btStuff->otherAddr,
                      (BtLibSdpUuidType*)&XWORDS_UUID, 1 );
            if ( err == errNone ) {
                SET_STATE( btStuff, PBTST_SDP_QUERIED );
                pbt_postpone( btStuff, PBT_ACT_CONNECT_L2C );
            } else if ( err == btLibErrPending ) {
                SET_STATE( btStuff, PBTST_SDP_QUERYING );
            } else {
                XP_ASSERT(0);
            }
        }
        break;

    case PBT_ACT_CONNECT_L2C:
/*         XP_ASSERT( btStuff->picoRole == PBT_SLAVE ); */
        if ( GET_STATE(btStuff) == PBTST_SDP_QUERIED ) {
            pbt_close_datasocket( btStuff );
            CALL_ERR( err, BtLibSocketCreate, btLibRefNum, 
                      &btStuff->dataSocket, 
                      socketCallback, (UInt32)btStuff, 
                      btLibL2CapProtocol );

            if ( btLibErrNoError == err ) {
                BtLibSocketConnectInfoType connInfo;
                connInfo.data.L2Cap.remotePsm = btStuff->u.slave.remotePsm;
                connInfo.data.L2Cap.localMtu = L2CAPSOCKETMTU; 
                connInfo.data.L2Cap.minRemoteMtu = L2CAPSOCKETMTU;
                connInfo.remoteDeviceP = &btStuff->otherAddr;
	
                /* sends btLibSocketEventConnectedOutbound */
                CALL_ERR( err, BtLibSocketConnect, btLibRefNum, 
                          btStuff->dataSocket, &connInfo );
                if ( errNone == err ) {
                    SET_STATE( btStuff, PBTST_L2C_CONNECTED );
                } else if ( btLibErrPending == err ) {
                    SET_STATE( btStuff, PBTST_L2C_CONNECTING );
                } else {
                    SET_STATE( btStuff, PBTST_NONE );
                    waitACL( btStuff );
                }
            } else {
                btStuff->dataSocket = SOCK_INVAL;
            }
        }

        break;

    case PBT_ACT_GOTDATA:
        pbt_processIncoming( btStuff );
        break;

    case PBT_ACT_TRYSEND:
        pbt_send_pending( btStuff );
        break;

    case PBT_ACT_TELLCONN:
        if ( !!btStuff->connCb ) {
            (*btStuff->connCb)( btStuff->globals );
        } else {
            XP_LOGF( "no callback" );
        }
        break;

    default:
        XP_ASSERT( 0 );
    }
    LOG_RETURN_VOID();
} /* pbt_do_work */

static void
pbt_postpone( PalmBTStuff* btStuff, PBT_ACTION act )
{
    EventType eventToPost;
    eventToPost.eType = nilEvent;

    XP_LOGF( "%s(%s)", __FUNCTION__, actToStr(act) );
    EvtAddEventToQueue( &eventToPost );

    if ( DUPLICATES_OK(act)
         || (btStuff->queueLen == 0)
         || (act != btStuff->actQueue[btStuff->queueLen-1]) ) {
        btStuff->actQueue[ btStuff->queueLen++ ] = act;
        XP_ASSERT( btStuff->queueLen < PBT_MAX_ACTS );
    } else {
        XP_LOGF( "%s already at tail of queue; not adding", actToStr(act) );
    }
    debug_logQueue( btStuff );
}

static XP_S16
pbt_enque( PBT_queue* queue, const XP_U8* data, XP_S16 len )
{
    XP_U16 i;
    XP_U16 total = 0;

    for ( i = 0; i < MAX_PACKETS; ++i ) {
        XP_U16 curlen = queue->lens[i];
        if ( !curlen ) {
            break;
        }
        total += curlen;
    }

    if ( (i < MAX_PACKETS) && ((total + len) < sizeof(queue->bufs)) ) {
        queue->lens[i] = len;
        XP_MEMCPY( &queue->bufs[total], data, len );
/*         XP_LOGF( "%s: adding %d; total now %d (%d packets)",
           __FUNCTION__,  */
/*                  len, len+total, i+1 ); */
    } else {
        XP_LOGF( "%s: dropping packet of len %d", __FUNCTION__, len );
        len = -1;
    }
    return len;
} /* pbt_enque */

static void
pbt_processIncoming( PalmBTStuff* btStuff )
{
    const XP_U8* buf;
    
    XP_U16 len = pbt_peekQueue( &btStuff->vol.in, &buf );

    if ( len > 0 ) {
        XP_ASSERT( !!btStuff->dataCb );
        if ( !!btStuff->dataCb ) {
            CommsAddrRec fromAddr;
            fromAddr.conType = COMMS_CONN_BT;
            XP_MEMCPY( &fromAddr.u.bt.btAddr, &btStuff->otherAddr,
                       sizeof(fromAddr.u.bt.btAddr) );

            (*btStuff->dataCb)( btStuff->globals, &fromAddr, buf, len );
            pbt_shiftQueue( &btStuff->vol.in );
        }
    }
} /* pbt_processIncoming */

static void
pbt_reset( PalmBTStuff* btStuff )
{
    LOG_FUNC();
    XP_MEMSET( &btStuff->vol, 0, sizeof(btStuff->vol) );

    LOG_RETURN_VOID();
}

static void
btTimerProc( void* closure, XWTimerReason why )
{
    PalmBTStuff* btStuff;
    btStuff = (PalmBTStuff*)closure;
    XP_ASSERT( why == TIMER_ACL_BACKOFF );
    pbt_postpone( btStuff, PBT_ACT_CONNECT_ACL );
}

static void
waitACL( PalmBTStuff* btStuff )
{
    util_setTimer( &btStuff->globals->util, TIMER_ACL_BACKOFF, 
                   ACL_WAIT_INTERVAL, btTimerProc, btStuff );
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
    return err;
} /* bpd_discover */

static void
pbt_setup_slave( PalmBTStuff* btStuff, const CommsAddrRec* addr )
{
    XP_LOGF( "%s; state=%s", __FUNCTION__, stateToStr(GET_STATE(btStuff)));

    if ( btStuff->picoRole == PBT_MASTER ) {
        pbt_takedown_master( btStuff );
    }
    btStuff->picoRole = PBT_SLAVE;

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

    if ( GET_STATE(btStuff) == PBTST_NONE ) {
        pbt_postpone( btStuff, PBT_ACT_CONNECT_ACL );
    } else {
        XP_LOGF( "%s: doing nothing", __FUNCTION__ );
    }
    LOG_RETURN_VOID();
} /* pbt_setup_slave */

static void
pbt_takedown_slave( PalmBTStuff* btStuff )
{
    pbt_killL2C( btStuff, btStuff->dataSocket );
    btStuff->picoRole = PBT_UNINIT;
}

static PalmBTStuff*
pbt_checkInit( PalmAppGlobals* globals, XP_Bool* userCancelledP )
{
    PalmBTStuff* btStuff = globals->btStuff;
    XP_Bool userCancelled = XP_FALSE;
    if ( !btStuff ) {
        Err err;
        XP_U16 btLibRefNum;

        CALL_ERR( err, SysLibFind, btLibName, &btLibRefNum );
        if ( errNone == err ) {
            CALL_ERR( err, BtLibOpen, btLibRefNum, false );

            userCancelled = err == btLibErrBluetoothOff;

            /* BT is probably off if this fails */
            if ( errNone == err ) {
                btStuff = XP_MALLOC( globals->mpool, sizeof(*btStuff) );
                XP_ASSERT( !!btStuff );
                globals->btStuff = btStuff;

                XP_MEMSET( btStuff, 0, sizeof(*btStuff) );
                btStuff->globals = globals;
                btStuff->btLibRefNum = btLibRefNum;

                btStuff->dataSocket = SOCK_INVAL;
                btStuff->u.master.listenSocket = SOCK_INVAL;

                CALL_ERR( err, BtLibRegisterManagementNotification, 
                          btLibRefNum, libMgmtCallback, (UInt32)btStuff );
            }
        }
    }

    if ( !!userCancelledP ) {
        *userCancelledP = userCancelled;
    }

    return btStuff;
} /* pbt_checkInit */

static void
pbt_killL2C( PalmBTStuff* btStuff, BtLibSocketRef sock )
{
    Err err;

    XP_ASSERT( sock == btStuff->dataSocket );
    pbt_close_datasocket( btStuff );

    /* Harm in calling this when not connected? */
    if ( GET_STATE(btStuff) != PBTST_NONE ) {
        SET_STATE( btStuff, PBTST_NONE ); /* set first */
        /* sends btLibManagementEventACLDisconnect */
        CALL_ERR( err, BtLibLinkDisconnect, btStuff->btLibRefNum,
                  &btStuff->otherAddr );
    }
} /* pbt_killL2C */

static void
pbt_checkAddress( PalmBTStuff* btStuff, const CommsAddrRec* addr )
{
    LOG_FUNC();
    XP_ASSERT( !!addr );
   
    if ( 0 != XP_MEMCMP( &btStuff->otherAddr, &addr->u.bt.btAddr.bits, 
                         sizeof(addr->u.bt.btAddr.bits) ) ) {

        LOG_HEX( &btStuff->otherAddr, sizeof(addr->u.bt.btAddr.bits), 
                 "cur" );
        LOG_HEX( &addr->u.bt.btAddr.bits, sizeof(addr->u.bt.btAddr.bits), 
                 "new" );

        pbt_killL2C( btStuff, btStuff->dataSocket );

        XP_MEMCPY( &btStuff->otherAddr, &addr->u.bt.btAddr, 
                   sizeof(btStuff->otherAddr) );
    }
    LOG_RETURN_VOID();
} /* pbt_checkAddress */

static void
pbt_setstate( PalmBTStuff* btStuff, PBT_STATE newState, const char* whence )
{
    btStuff->p_connState = newState;
    XP_LOGF( "setting state to %s, from %s", stateToStr(newState), whence );
}

static void
socketCallback( BtLibSocketEventType* sEvent, UInt32 refCon )
{
    PalmBTStuff* btStuff = (PalmBTStuff*)refCon;
    BtLibSocketEventEnum event = sEvent->event;
    Err err;

    XP_LOGF( "%s(%s); status:%s", __FUNCTION__, btEvtToStr(event),
             btErrToStr(sEvent->status) );

    switch( event ) {
    case btLibSocketEventConnectRequest: 
        XP_ASSERT( btStuff->picoRole == PBT_MASTER );
        /* sends btLibSocketEventConnectedInbound */
        CALL_ERR( err, BtLibSocketRespondToConnection, btStuff->btLibRefNum,  
                  sEvent->socket, true );
        break;
    case btLibSocketEventConnectedInbound:
        XP_ASSERT( btStuff->picoRole == PBT_MASTER );
        if ( sEvent->status == errNone ) {
            btStuff->dataSocket = sEvent->eventData.newSocket;
            XP_LOGF( "we have a data socket!!!" );
            pbt_postpone( btStuff, PBT_ACT_TELLCONN );
            SET_STATE( btStuff, PBTST_L2C_CONNECTED );
        }
        break;
    case btLibSocketEventConnectedOutbound:
        if ( errNone == sEvent->status ) {
            SET_STATE( btStuff, PBTST_L2C_CONNECTED );
            pbt_postpone( btStuff, PBT_ACT_TELLCONN );
        }
        break;
    case btLibSocketEventData:
        XP_ASSERT( sEvent->status == errNone );
        XP_ASSERT( sEvent->socket == btStuff->dataSocket );

        if ( 0 < pbt_enque( &btStuff->vol.in, sEvent->eventData.data.data, 
                            sEvent->eventData.data.dataLen ) ) {
            pbt_postpone( btStuff, PBT_ACT_GOTDATA );
        }
#ifdef DEBUG
            btStuff->stats.totalRcvd += sEvent->eventData.data.dataLen;
#endif        
        break;

    case btLibSocketEventSendComplete:
        btStuff->vol.sendInProgress = XP_FALSE;
#ifdef DEBUG
        btStuff->stats.totalSent +=
#endif
            pbt_shiftQueue( &btStuff->vol.out );
        pbt_postpone( btStuff, PBT_ACT_TRYSEND ); /* in case there's more */
        break;

    case btLibSocketEventDisconnected:
        /* We'll see this as client if the host quits.  What to do?  I think
         * we need to start trying to reconnect hoping the host got
         * restarted.  Presumably users will not sit there forever running
         * the app once one of the players has taken his device and gone
         * home.  But there should probably be UI warning users that it's
         * trying to connect.... 
         */
        if ( PBT_SLAVE == btStuff->picoRole ) {
            pbt_killL2C( btStuff, sEvent->socket );
            waitACL( btStuff );
        } else if ( PBT_MASTER == btStuff->picoRole ) {
            pbt_close_datasocket( btStuff );
            SET_STATE( btStuff, PBTST_LISTENING );
        }
        break;

    case btLibSocketEventSdpGetPsmByUuid:
        XP_ASSERT( sEvent->socket == btStuff->u.slave.sdpSocket );
        CALL_ERR( err, BtLibSocketClose, btStuff->btLibRefNum, sEvent->socket );
        XP_ASSERT( err == errNone );
        btStuff->u.slave.sdpSocket = SOCK_INVAL;
        if ( sEvent->status == errNone ) {
            btStuff->u.slave.remotePsm = sEvent->eventData.sdpByUuid.param.psm;
            SET_STATE( btStuff, PBTST_SDP_QUERIED );
            pbt_postpone( btStuff, PBT_ACT_CONNECT_L2C );
        } else if ( sEvent->status == btLibErrSdpQueryDisconnect ) {
	    /* Maybe we can just ignore this... */
            XP_ASSERT( GET_STATE(btStuff) == PBTST_NONE );
/*             waitACL( btStuff ); */
        } else {
            if ( sEvent->status == btLibErrSdpAttributeNotSet ) {
                XP_LOGF( "**** Host not running!!! ****" );
            }
            /* try again???? */
            SET_STATE( btStuff, PBTST_ACL_CONNECTED );
            pbt_postpone( btStuff, PBT_ACT_GETSDP );
        }
        /* Do we want to try again in a few seconds?  Is this where the timer
           belongs? */
	break;

    case btLibL2DiscConnPsmUnsupported:
        /* Probably need to warn the user when this happens since not having
           established trust will be a common error.  Or: figure out if
           there's a way to fall back and establish trust programatically.
           For alpha just do the error message. :-) Also, no point in
           continuing to try to connect.  User will have to quit in order to
           establish trust.  So warn once per inited session. */
        XP_LOGF( "Crosswords not running on host or host not trusted." );
        break;

    default:
        break;
    }
    LOG_RETURN_VOID();
} /* socketCallback */

/***********************************************************************
 * Callbacks
 ***********************************************************************/
static void
libMgmtCallback( BtLibManagementEventType* mEvent, UInt32 refCon )
{
    Err err;
    PalmBTStuff* btStuff = (PalmBTStuff*)refCon;
    BtLibManagementEventEnum event = mEvent->event;
    XP_LOGF( "%s(%s); status=%s", __FUNCTION__, mgmtEvtToStr(event),
             btErrToStr(mEvent->status) );

    switch( event ) {
    case btLibManagementEventAccessibilityChange:
        XP_LOGF( "%s", connEnumToStr(mEvent->eventData.accessible) );
        btStuff->accState = mEvent->eventData.accessible;
        break;
    case btLibManagementEventRadioState:
/*         XP_LOGF( "status: %s", btErrToStr(mEvent->status) ); */
        break;
    case btLibManagementEventACLConnectOutbound:
        if ( btLibErrNoError == mEvent->status ) {
            SET_STATE( btStuff, PBTST_ACL_CONNECTED );
            XP_LOGF( "successful ACL connection to master!" );
            pbt_postpone( btStuff, PBT_ACT_GETSDP );
        } else {
            SET_STATE( btStuff, PBTST_NONE );
            waitACL( btStuff );
        }
        break;

    case btLibManagementEventACLConnectInbound:
        if ( btLibErrNoError == mEvent->status ) {
            XP_LOGF( "successful ACL connection!" );
            XP_MEMCPY( &btStuff->otherAddr, 
                       &mEvent->eventData.bdAddr,
                       sizeof(btStuff->otherAddr) );
            SET_STATE( btStuff, PBTST_ACL_CONNECTED );
        }
        break;
    case btLibManagementEventACLDisconnect:
        if ( mEvent->status == btLibMeStatusLocalTerminated ) {
            /* We caused this, probably switching roles.  Perhaps we've already
               done what's needed, e.g. opened socket to listen */
        } else {
            /* This is getting called from inside the BtLibLinkDisconnect
               call!!!! */
            XP_ASSERT( 0 == XP_MEMCMP( &mEvent->eventData.bdAddr,
                                       &btStuff->otherAddr, 6 ) );
            if ( SOCK_INVAL != btStuff->dataSocket ) {
                CALL_ERR( err, BtLibSocketClose, btStuff->btLibRefNum, 
                          btStuff->dataSocket );
                btStuff->dataSocket = SOCK_INVAL;
            }
            SET_STATE( btStuff, PBTST_NONE );
            /* See comment at btLibSocketEventDisconnected */
            if ( PBT_SLAVE == btStuff->picoRole ) {
                waitACL( btStuff );
            }
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
stateToStr(PBT_STATE st)
{
    switch( st ) {
        CASESTR(PBTST_NONE);
        CASESTR(PBTST_LISTENING);
        CASESTR(PBTST_ACL_CONNECTING);
        CASESTR(PBTST_SDP_QUERYING);
        CASESTR(PBTST_SDP_QUERIED);
        CASESTR(PBTST_ACL_CONNECTED);
        CASESTR(PBTST_L2C_CONNECTING);
        CASESTR(PBTST_L2C_CONNECTED);
    default:
        XP_ASSERT(0);
        return "";
    }
} /* stateToStr */

static const char*
actToStr(PBT_ACTION act)
{
    switch( act ) {
        CASESTR(PBT_ACT_NONE);
        CASESTR(PBT_ACT_SETUP_LISTEN);
        CASESTR(PBT_ACT_CONNECT_ACL);
        CASESTR(PBT_ACT_GETSDP);
        CASESTR(PBT_ACT_CONNECT_L2C);
        CASESTR(PBT_ACT_GOTDATA);
        CASESTR(PBT_ACT_TRYSEND);
        CASESTR(PBT_ACT_TELLCONN);
    default:
        XP_ASSERT(0);
        return "";
    }
} /* actToStr */

static const char*
connEnumToStr( BtLibAccessibleModeEnum mode )
{
    switch( mode ) {
        CASESTR(btLibNotAccessible);
        CASESTR(btLibConnectableOnly);
        CASESTR(btLibDiscoverableAndConnectable);
    case 0x0006:
        /* I've seen this on 68K even.  Seems to happen when the other
           device changes roles (temporarily). */
        return "undoc_06";
    case 0x00F8:                /* seen on ARM only */
        return "undoc_F8";
    default:
        XP_ASSERT(0);
        XP_LOGF( "%s: got 0x%x", __func__, mode );
        return "";
    }
}

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

static const char*
proleToString( PBT_PicoRole r )
{
    switch ( r ) {
        CASESTR(PBT_UNINIT);
        CASESTR(PBT_MASTER);
        CASESTR(PBT_SLAVE);
    default:
        XP_ASSERT(0);
        return "";
    }
}

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
