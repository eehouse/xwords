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
#include "palmutil.h"

# include <BtLib.h>
# include <BtLibTypes.h>

#if defined BT_USE_L2CAP
# define SEL_PROTO btLibL2CapProtocol
# define TRUE_IF_RFCOMM XP_FALSE
#elif defined BT_USE_RFCOMM
# define SEL_PROTO btLibRfCommProtocol
# define TRUE_IF_RFCOMM XP_TRUE
# define INITIAL_CREDIT 50
#endif

#define L2CAPSOCKETMTU 500
#define SOCK_INVAL ((BtLibSocketRef)-1)

#define DO_SERVICE_RECORD 1
#define ACL_WAIT_INTERVAL 4

typedef enum { PBT_UNINIT = 0, PBT_MASTER, PBT_SLAVE } PBT_PicoRole;

typedef enum {
    PBT_ACT_NONE
    , PBT_ACT_MASTER_RESET
    , PBT_ACT_SLAVE_RESET
    , PBT_ACT_SETUP_LISTEN
    , PBT_ACT_CONNECT_ACL
    , PBT_ACT_GETSDP            /* slave only */
    , PBT_ACT_CONNECT_DATA      /* l2cap or rfcomm */
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
    , PBTST_DATA_CONNECTING      /* slave; l2cap or rfcomm */
    , PBTST_DATA_CONNECTED       /* slave; l2cap or rfcomm */ 
} PBT_STATE;

#define PBT_MAX_ACTS 8          /* six wasn't enough */
#define HASWORK(s)  ((s)->queueLen > 0)
#define MAX_PACKETS 4

typedef struct PBT_queue {
    XP_U16 lens[MAX_PACKETS];
    XP_U8 bufs[L2CAPSOCKETMTU*2];       /* what's the mmu? */
} PBT_queue;

typedef struct PalmBTStuff {
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
#if defined BT_USE_L2CAP
            BtLibL2CapPsmType remotePsm;
#elif defined BT_USE_RFCOMM
            BtLibRfCommServerIdType remoteService;
#endif
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
        XP_U16 maxQueueLen;
    } stats;
#endif
} PalmBTStuff;

#ifdef DEBUG
static void palm_bt_log( const char* btfunc, const char* func, Err err );
#define LOG_ERR(f,e) palm_bt_log( #f, __func__, e )
#define CALL_ERR(e,f,...) \
    XP_LOGF( "%s: calling %s", __func__, #f ); \
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
static Err pbd_discover( PalmBTStuff* btStuff, BtLibDeviceAddressType* addr );
static void pbt_setup_slave( PalmBTStuff* btStuff, const CommsAddrRec* addr );
static void pbt_takedown_slave( PalmBTStuff* btStuff );
static void pbt_setup_master( PalmBTStuff* btStuff );
static void pbt_takedown_master( PalmBTStuff* btStuff );
static void pbt_do_work( PalmBTStuff* btStuff, BtCbEvtProc proc );
static void pbt_postpone( PalmBTStuff* btStuff, PBT_ACTION act );
static XP_S16 pbt_enqueue( PBT_queue* queue, const XP_U8* data, XP_S16 len, 
                           XP_Bool addLen, XP_Bool append );
static void pbt_handoffIncoming( PalmBTStuff* btStuff, BtCbEvtProc proc );

static void waitACL( PalmBTStuff* btStuff );
static void pbt_reset_buffers( PalmBTStuff* btStuff );
static void pbt_killLinks( PalmBTStuff* btStuff );
static XP_Bool pbt_checkAddress( PalmBTStuff* btStuff, const CommsAddrRec* addr );
static Err pbt_nameForAddr( PalmBTStuff* btStuff, 
                            const BtLibDeviceAddressType* addr,
                            char* const out, XP_U16 outlen );

#ifdef DEBUG
static void pbt_setstate( PalmBTStuff* btStuff, PBT_STATE newState,
                          const char* whence );
# define SET_STATE(b,s)  pbt_setstate((b),(s),__func__)
#else
# define SET_STATE(b,s)  (b)->p_connState = (s)
#endif
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
palm_bt_init( PalmAppGlobals* globals, XP_Bool* userCancelled )
{
    XP_Bool inited;
    PalmBTStuff* btStuff;

    LOG_FUNC();

    btStuff = globals->btStuff;
    if ( !btStuff ) {
        btStuff = pbt_checkInit( globals, userCancelled );
    } else {
        pbt_reset_buffers( btStuff );
        pbt_killLinks( btStuff );
    }

    /* Don't try starting master or slave: we don't know which we are yet.
       Wait for the first send attempt.*/

    inited = !!btStuff;
    if ( inited ) {
        btStuff->picoRole = PBT_UNINIT;
    }
    LOG_RETURNF( "%d", (XP_U16)inited );
    return inited;
} /* palm_bt_init */

void
palm_bt_reset( PalmAppGlobals* globals )
{
    PalmBTStuff* btStuff = globals->btStuff;

    if ( !!btStuff ) {
        if ( btStuff->picoRole == PBT_MASTER ) {
            pbt_takedown_master( btStuff );
            pbt_postpone( btStuff, PBT_ACT_MASTER_RESET );
        } else if ( btStuff->picoRole == PBT_SLAVE ) {
            pbt_takedown_slave( btStuff );
            pbt_postpone( btStuff, PBT_ACT_SLAVE_RESET );
        }
    }
}

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
        XP_LOGF( "%s: btStuff null", __func__ );
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
palm_bt_doWork( PalmAppGlobals* globals, BtCbEvtProc proc, BtUIState* btUIStateP )
{
    PalmBTStuff* btStuff = globals->btStuff;
    XP_Bool haveWork = !!btStuff && HASWORK(btStuff);

    if ( haveWork ) {
        pbt_do_work( btStuff, proc );
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
        case PBTST_DATA_CONNECTING:
            btUIState = BTUI_CONNECTING; 
            break;
        case PBTST_DATA_CONNECTED:
            btUIState = btStuff->picoRole == PBT_MASTER?
                BTUI_SERVING : BTUI_CONNECTED; 
            break;
        default:
            XP_ASSERT(0);       /* Don't add new states without handling here */
            break;
        }
        *btUIStateP = btUIState;
    }
    return haveWork;
} /* palm_bt_doWork */

void
palm_bt_addrString( PalmAppGlobals* globals, const XP_BtAddr* btAddr, 
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
        Err err = pbd_discover( btStuff, &addr );

        if ( errNone == err ) {
            XP_MEMCPY( btAddr, &addr, sizeof(addr) );
            LOG_HEX( &btAddr, sizeof(btAddr), __func__ );

            err = pbt_nameForAddr( btStuff, &addr, out, len );
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
        stream_catString( stream, "bt not initialized" );
    } else {
        char buf[64];
        XP_U16 cur;

        XP_SNPRINTF( buf, sizeof(buf), "Role: %s\n", 
                     btStuff->picoRole == PBT_MASTER? "master":
                     (btStuff->picoRole == PBT_SLAVE? "slave":"unknown") );
        stream_catString( stream, buf );
        XP_SNPRINTF( buf, sizeof(buf), "State: %s\n", 
                     stateToStr( GET_STATE(btStuff)) );
        stream_catString( stream, buf );

        XP_SNPRINTF( buf, sizeof(buf), "%d actions queued:\n", 
                     btStuff->queueLen );
        stream_catString( stream, buf );
        for ( cur = 0; cur < btStuff->queueLen; ++cur ) {
            XP_SNPRINTF( buf, sizeof(buf), " - %s\n",
                         actToStr( btStuff->actQueue[cur] ) );
            stream_catString( stream, buf );
        }

        XP_SNPRINTF( buf, sizeof(buf), "total sent: %ld\n",
                     btStuff->stats.totalSent );
        stream_catString( stream, buf );
        XP_SNPRINTF( buf, sizeof(buf), "total rcvd: %ld\n",
                     btStuff->stats.totalRcvd );
        stream_catString( stream, buf );
        XP_SNPRINTF( buf, sizeof(buf), "max act queue len seen: %d\n",
                     btStuff->stats.maxQueueLen );
        stream_catString( stream, buf );
    }
}
#endif

static Err
pbt_nameForAddr( PalmBTStuff* btStuff, const BtLibDeviceAddressType* addr,
                 char* const out, XP_U16 outlen )
{
    Err err;
    UInt8 name[PALM_BT_NAME_LEN];
    BtLibFriendlyNameType nameType = {
        .name = name, 
        .nameLength = sizeof(name) 
    };

    CALL_ERR( err, BtLibGetRemoteDeviceName, btStuff->btLibRefNum,
              (BtLibDeviceAddressType*)addr, &nameType,
              btLibCachedThenRemote );
    if ( errNone == err ) {
        XP_LOGF( "%s: got name %s", __func__, nameType.name );
    
        XP_ASSERT( outlen >= nameType.nameLength );
        XP_MEMCPY( out, nameType.name, nameType.nameLength );
    }
    return err;
}

static XP_U16
pbt_peekQueue( const PBT_queue* queue, const XP_U8** bufp )
{
    XP_U16 len = queue->lens[0];
    if ( len > 0 ) {
        *bufp = &queue->bufs[0];
    }
    LOG_RETURNF( "%d", len );
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

    if ( !btStuff->vol.sendInProgress && (SOCK_INVAL != btStuff->dataSocket)) {
        const XP_U8* buf;
        XP_U16 len = pbt_peekQueue( &btStuff->vol.out, &buf );
        if ( len > 0 ) {
#ifdef LOG_BTIO
            LOG_HEX( buf, len, "to BtLibSocketSend" );
#endif
            XP_LOGF( "sending on socket %d", btStuff->dataSocket );
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
              PalmAppGlobals* globals, XP_Bool* userCancelled )
{
    XP_S16 nSent = -1;
    PalmBTStuff* btStuff;
    CommsAddrRec remoteAddr;
    PBT_PicoRole picoRole;
    XP_LOGF( "%s(len=%d)", __func__, len);

    XP_ASSERT( !!globals->game.comms );

    btStuff = pbt_checkInit( globals, userCancelled );
    if ( !!btStuff ) {
        /* addr is NULL when client has not established connection to host */
        if ( !addr ) {
            comms_getAddr( globals->game.comms, &remoteAddr );
            addr = &remoteAddr;
        }
        XP_ASSERT( !!addr );

        picoRole = btStuff->picoRole;
        XP_LOGF( "%s: role=%s", __func__, proleToString(picoRole) );
        if ( picoRole == PBT_UNINIT ) {
            XP_Bool amMaster = comms_getIsServer( globals->game.comms );
            picoRole = amMaster? PBT_MASTER : PBT_SLAVE;
        }

        (void)pbt_checkAddress( btStuff, addr );

        if ( picoRole == PBT_MASTER ) {
            pbt_setup_master( btStuff );
        } else {
            pbt_setup_slave( btStuff, addr );
        }

        nSent = pbt_enqueue( &btStuff->vol.out, buf, len, TRUE_IF_RFCOMM, XP_FALSE );
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
                  (UInt32)btStuff, SEL_PROTO );
        XP_ASSERT( errNone == err );

        /*    2. BtLibSocketListen: set up an L2CAP socket as a listener. */
        XP_MEMSET( &listenInfo, 0, sizeof(listenInfo) );
#if defined BT_USE_L2CAP
        listenInfo.data.L2Cap.localPsm = BT_L2CAP_RANDOM_PSM;
        listenInfo.data.L2Cap.localMtu = L2CAPSOCKETMTU; 
        listenInfo.data.L2Cap.minRemoteMtu = L2CAPSOCKETMTU;
#elif defined BT_USE_RFCOMM
        // remoteService: assigned by rfcomm
        listenInfo.data.RfComm.maxFrameSize = BT_RF_DEFAULT_FRAMESIZE;
        listenInfo.data.RfComm.advancedCredit = INITIAL_CREDIT;
#endif
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
pbt_close_sdpsocket( PalmBTStuff* btStuff )
{
    XP_ASSERT( PBT_SLAVE == btStuff->picoRole );
    if ( SOCK_INVAL != btStuff->u.slave.sdpSocket ) {
        Err err;
        CALL_ERR( err, BtLibSocketClose, btStuff->btLibRefNum, btStuff->u.slave.sdpSocket );
        btStuff->u.slave.sdpSocket = SOCK_INVAL;
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

#if 0
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
pbt_do_work( PalmBTStuff* btStuff, BtCbEvtProc proc )
{
    PBT_ACTION act;
    Err err;
    XP_U16 btLibRefNum = btStuff->btLibRefNum;
    BtCbEvtInfo info;

    debug_logQueue( btStuff );

    act = btStuff->actQueue[0];
    --btStuff->queueLen;
    XP_MEMCPY( &btStuff->actQueue[0], &btStuff->actQueue[1], 
               btStuff->queueLen * sizeof(btStuff->actQueue[0]) );

    XP_LOGF( "%s: evt=%s; state=%s", __func__, actToStr(act),
             stateToStr(GET_STATE(btStuff)) );

    switch( act ) {
    case PBT_ACT_MASTER_RESET:
        pbt_setup_master( btStuff );
        break;
    case PBT_ACT_SLAVE_RESET:
        pbt_setup_slave( btStuff, NULL );
        break;
    case PBT_ACT_SETUP_LISTEN:
        pbt_setup_master( btStuff );
        break;

    case PBT_ACT_CONNECT_ACL:
        XP_ASSERT( PBT_SLAVE == btStuff->picoRole );
        if ( GET_STATE(btStuff) == PBTST_NONE ) {
            UInt8 name[PALM_BT_NAME_LEN];
            (void)pbt_nameForAddr( btStuff, &btStuff->otherAddr, 
                                   name, sizeof(name) );
            info.evt = BTCBEVT_CONFIRM;
            info.u.confirm.hostName = name;
            info.u.confirm.confirmed = XP_TRUE;
            (*proc)( btStuff->globals, &info );
            if ( !info.u.confirm.confirmed ) {
                break;
            }

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
        if ( PBTST_ACL_CONNECTED == GET_STATE(btStuff) ) {
            XP_ASSERT( SOCK_INVAL == btStuff->u.slave.sdpSocket );
            CALL_ERR( err, BtLibSocketCreate, btStuff->btLibRefNum,
                      &btStuff->u.slave.sdpSocket, socketCallback, (UInt32)btStuff,
                      btLibSdpProtocol );
            if ( err == errNone ) {
#if defined BT_USE_L2CAP
                XP_LOGF( "sending on sdpSocket socket %d", btStuff->u.slave.sdpSocket );
                CALL_ERR( err, BtLibSdpGetPsmByUuid, btStuff->btLibRefNum, 
                          btStuff->u.slave.sdpSocket, &btStuff->otherAddr,
                          (BtLibSdpUuidType*)&XWORDS_UUID, 1 );
#elif defined BT_USE_RFCOMM
                CALL_ERR( err, BtLibSdpGetServerChannelByUuid,
                          btStuff->btLibRefNum, btStuff->u.slave.sdpSocket,
                          &btStuff->otherAddr,
                          (BtLibSdpUuidType*)&XWORDS_UUID, 1 );
#endif
                if ( err == errNone ) {
                    SET_STATE( btStuff, PBTST_SDP_QUERIED );
                    pbt_postpone( btStuff, PBT_ACT_CONNECT_DATA );
                    break;
                } else if ( err == btLibErrPending ) {
                    SET_STATE( btStuff, PBTST_SDP_QUERYING );
                    break;
                } else if ( err == btLibErrNoAclLink ) {
                    /* fall through to waitACL below */
                } else {
                    XP_ASSERT(0);
                }
            }
        }
        /* Presumably state's been reset since PBT_ACT_GETSDP issued */
        XP_LOGF( "aborting b/c state wrong" );
        XP_ASSERT( PBT_SLAVE == btStuff->picoRole );
        pbt_close_sdpsocket( btStuff );
        SET_STATE( btStuff, PBTST_NONE );
        waitACL( btStuff );
        break;

    case PBT_ACT_CONNECT_DATA:
        XP_ASSERT( btStuff->picoRole == PBT_SLAVE );
        if ( GET_STATE(btStuff) == PBTST_SDP_QUERIED ) {
            pbt_close_datasocket( btStuff );
            CALL_ERR( err, BtLibSocketCreate, btLibRefNum, 
                      &btStuff->dataSocket, 
                      socketCallback, (UInt32)btStuff, SEL_PROTO );

            if ( btLibErrNoError == err ) {
                BtLibSocketConnectInfoType connInfo;
                connInfo.remoteDeviceP = &btStuff->otherAddr;
#if defined BT_USE_L2CAP
                connInfo.data.L2Cap.remotePsm = btStuff->u.slave.remotePsm;
                connInfo.data.L2Cap.localMtu = L2CAPSOCKETMTU; 
                connInfo.data.L2Cap.minRemoteMtu = L2CAPSOCKETMTU;
#elif defined BT_USE_RFCOMM
                connInfo.data.RfComm.remoteService
                    = btStuff->u.slave.remoteService;
                connInfo.data.RfComm.maxFrameSize = BT_RF_DEFAULT_FRAMESIZE;
                connInfo.data.RfComm.advancedCredit = INITIAL_CREDIT;
#else
                XP_ASSERT(0);
#endif
                /* sends btLibSocketEventConnectedOutbound */
                CALL_ERR( err, BtLibSocketConnect, btLibRefNum, 
                          btStuff->dataSocket, &connInfo );
                if ( errNone == err ) {
                    SET_STATE( btStuff, PBTST_DATA_CONNECTED );
                } else if ( btLibErrPending == err ) {
                    SET_STATE( btStuff, PBTST_DATA_CONNECTING );
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
#ifdef BT_USE_RFCOMM
        CALL_ERR( err, BtLibSocketAdvanceCredit, btLibRefNum, btStuff->dataSocket, 5 );
#endif
        pbt_handoffIncoming( btStuff, proc );
        break;

    case PBT_ACT_TRYSEND:
        pbt_send_pending( btStuff );
        break;

    case PBT_ACT_TELLCONN:
        XP_ASSERT( !!proc );
        info.evt = BTCBEVT_CONN;
        (*proc)( btStuff->globals, &info );
        break;

    default:
        XP_ASSERT( 0 );
    }
    LOG_RETURN_VOID();
} /* pbt_do_work */

static void
pbt_postpone( PalmBTStuff* btStuff, PBT_ACTION act )
{
    postEmptyEvent( noopEvent );

    XP_LOGF( "%s(%s)", __func__, actToStr(act) );

    if ( DUPLICATES_OK(act)
         || (btStuff->queueLen == 0)
         || (act != btStuff->actQueue[btStuff->queueLen-1]) ) {
        btStuff->actQueue[ btStuff->queueLen++ ] = act;
        XP_ASSERT( btStuff->queueLen < PBT_MAX_ACTS );
#ifdef DEBUG
        if ( btStuff->queueLen > btStuff->stats.maxQueueLen ) {
            btStuff->stats.maxQueueLen = btStuff->queueLen;
        }
#endif
    } else {
        XP_LOGF( "%s already at tail of queue; not adding", actToStr(act) );
    }
    debug_logQueue( btStuff );
}

static void
getSizeIndex( PBT_queue* queue, XP_U16* index, XP_U16* totalP )
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
    XP_LOGF( "%s=>index:%d, total: %d", __func__, i, total );
    *index = i;
    *totalP = total;
} /* getSizeIndex */

static XP_S16
pbt_enqueue( PBT_queue* queue, const XP_U8* data, const XP_S16 len, 
             XP_Bool addLen, XP_Bool append )
{
    XP_S16 result;
    XP_U16 index;
    XP_U16 total = 0;
    XP_U16 lensiz = 0;

    XP_ASSERT( len > 0 || !addLen );

    if ( addLen ) {
        lensiz = sizeof(len);
    }

    getSizeIndex( queue, &index, &total );

    if ( append ) {
        XP_ASSERT( index > 0 );
        --index;
    }

    if ( (index < MAX_PACKETS) && ((total + len + lensiz) < sizeof(queue->bufs)) ) {
        if ( !append ) {
            queue->lens[index] = 0;
        }

        queue->lens[index] += len + lensiz;
        if ( addLen ) {
            XP_U16 plen = XP_HTONS(len);
            XP_LOGF( "writing plen: %x", plen );
            XP_MEMCPY( &queue->bufs[total], &plen, sizeof(plen) );
            total += sizeof(plen);
        }
        XP_MEMCPY( &queue->bufs[total], data, len );
/*         XP_LOGF( "%s: adding %d; total now %d (%d packets)",
           __func__,  */
/*                  len, len+total, i+1 ); */
        result = len;
    } else {
        XP_LOGF( "%s: dropping packet of len %d", __func__, len );
        result = -1;
    }
    return result;
} /* pbt_enqueue */

static void
pbt_handoffIncoming( PalmBTStuff* btStuff, BtCbEvtProc proc )
{
    const XP_U8* buf;
    XP_U16 len;

    len = pbt_peekQueue( &btStuff->vol.in, &buf );

    if ( len > 0 ) {
        BtCbEvtInfo info;
        CommsAddrRec fromAddr;

        XP_ASSERT( !!proc );

        fromAddr.conType = COMMS_CONN_BT;
        XP_MEMCPY( &fromAddr.u.bt.btAddr, &btStuff->otherAddr,
                   sizeof(fromAddr.u.bt.btAddr) );

        info.evt = BTCBEVT_DATA;
        info.u.data.fromAddr = &fromAddr;
#ifdef BT_USE_RFCOMM
        XP_LOGF( "plen=%d; len=%d", *(XP_U16*)buf, len-2 );
        XP_ASSERT( *(XP_U16*)buf == len - sizeof(XP_U16) ); /* firing */
        info.u.data.len = len - sizeof(XP_U16);
        info.u.data.data = buf + sizeof(XP_U16);
#else
        info.u.data.len = len;
        info.u.data.data = buf;
#endif
        (*proc)( btStuff->globals, &info );

        pbt_shiftQueue( &btStuff->vol.in );
    }
} /* pbt_handoffIncoming */

static void
pbt_reset_buffers( PalmBTStuff* btStuff )
{
    LOG_FUNC();
    XP_MEMSET( &btStuff->vol, 0, sizeof(btStuff->vol) );

    LOG_RETURN_VOID();
}

static XP_Bool
btTimerProc( void* closure, XWTimerReason why )
{
    PalmBTStuff* btStuff;
    XP_ASSERT( why == TIMER_ACL_BACKOFF );
    btStuff = (PalmBTStuff*)closure;
    if ( GET_STATE(btStuff) != PBTST_NONE ) {
        XP_LOGF( "%s ignoring; have changed states", __func__ );
    } else if ( PBT_SLAVE != btStuff->picoRole ) {
        XP_LOGF( "%s ignoring; have changed roles", __func__ );
    } else {
        pbt_postpone( btStuff, PBT_ACT_CONNECT_ACL );
    }
    return XP_TRUE;
}

static void
waitACL( PalmBTStuff* btStuff )
{
    util_setTimer( &btStuff->globals->util, TIMER_ACL_BACKOFF, 
                   ACL_WAIT_INTERVAL, btTimerProc, btStuff );
}

static Err
pbd_discover( PalmBTStuff* btStuff, BtLibDeviceAddressType* addr )
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
} /* pbd_discover */

static void
pbt_setup_slave( PalmBTStuff* btStuff, const CommsAddrRec* addr )
{
    XP_LOGF( "%s; state=%s", __func__, stateToStr(GET_STATE(btStuff)));

    if ( btStuff->picoRole == PBT_MASTER ) {
        pbt_takedown_master( btStuff );
    }
    btStuff->picoRole = PBT_SLAVE;
    btStuff->u.slave.sdpSocket = SOCK_INVAL;

    if ( !!addr ) {
        char buf[64];
        if ( errNone == 
             BtLibAddrBtdToA( btStuff->btLibRefNum, 
                              (BtLibDeviceAddressType*)&addr->u.bt.btAddr,
                              buf, sizeof(buf) ) ) {
            XP_LOGF( "%s(%s)", __func__, buf );
        }
    } else {
        XP_LOGF( "null addr" );
    }

    if ( GET_STATE(btStuff) == PBTST_NONE ) {
        pbt_postpone( btStuff, PBT_ACT_CONNECT_ACL );
    } else {
        XP_LOGF( "%s: doing nothing", __func__ );
    }
    LOG_RETURN_VOID();
} /* pbt_setup_slave */

static void
pbt_takedown_slave( PalmBTStuff* btStuff )
{
    pbt_killLinks( btStuff );
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
                btStuff->u.slave.sdpSocket = SOCK_INVAL;

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
pbt_killLinks( PalmBTStuff* btStuff )
{
    Err err;

    pbt_close_datasocket( btStuff );

    if ( PBT_SLAVE == btStuff->picoRole ) {
        pbt_close_sdpsocket( btStuff );
    }

    /* Harm in calling this when not connected? */
    if ( GET_STATE(btStuff) != PBTST_NONE ) {
        SET_STATE( btStuff, PBTST_NONE ); /* set first */
        /* sends btLibManagementEventACLDisconnect */
        CALL_ERR( err, BtLibLinkDisconnect, btStuff->btLibRefNum,
                  &btStuff->otherAddr );
    }
} /* pbt_killLinks */

static XP_Bool
pbt_checkAddress( PalmBTStuff* btStuff, const CommsAddrRec* addr )
{
    XP_Bool addrOk;
    LOG_FUNC();
    XP_ASSERT( !!addr );
   
    addrOk = 0 == XP_MEMCMP( &btStuff->otherAddr, &addr->u.bt.btAddr.bits, 
                             sizeof(addr->u.bt.btAddr.bits) );
    if ( !addrOk ) {
        LOG_HEX( &btStuff->otherAddr, sizeof(addr->u.bt.btAddr.bits), 
                 "cur" );
        LOG_HEX( &addr->u.bt.btAddr.bits, sizeof(addr->u.bt.btAddr.bits), 
                 "new" );

        pbt_killLinks( btStuff );

        XP_MEMCPY( &btStuff->otherAddr, &addr->u.bt.btAddr, 
                   sizeof(btStuff->otherAddr) );
    }
    LOG_RETURNF( "%d", (int)addrOk );
    return addrOk;
} /* pbt_checkAddress */

#ifdef DEBUG
static void
pbt_setstate( PalmBTStuff* btStuff, PBT_STATE newState, const char* whence )
{
    btStuff->p_connState = newState;
    XP_LOGF( "setting state to %s, from %s", stateToStr(newState), whence );
}
#endif

#ifdef BT_USE_RFCOMM

static XP_U16
pbt_packetPending( PalmBTStuff* btStuff )
{
    XP_U16 pending;
    XP_U16 index, total;
    PBT_queue* queue = &btStuff->vol.in;

    /* Packet consists of two bytes of len plus len bytes of data.  An
       incomplete packet has len but less than len bytes of data.  When we
       write the len we add a packet but that's all.
       
       Total and index get us beyond the last packet written.  If index is 0,
       nothing's been written so packet is pending.  Otherwise we can back
       index off and look at the buffer there.  Buffer starts at total - lens[--index].
       Len will be written there.  If lens[index] is less, it's pending.
     */
    getSizeIndex( queue, &index, &total );
    if ( total < sizeof(XP_U16) ) {
        XP_ASSERT( total == 0 );
        pending = 0;
    } else {
        XP_U16 curLen, plen;
        XP_U8* curStart;
        --index;
        curLen = queue->lens[index];
        curStart = &queue->bufs[total-curLen];
        plen = *(XP_U16*)curStart;
        pending = plen - (curLen - sizeof(plen));
    }
    LOG_RETURNF( "%d", pending );
    return pending;
}

static void
pbt_assemble( PalmBTStuff* btStuff, unsigned char* data, XP_U16 len )
{
    XP_U16 bytesPending = pbt_packetPending( btStuff);
    LOG_FUNC();
    if ( bytesPending == 0 ) {
        XP_U16 plen;
        /* will need to handle case where len comes in separate packets!!!! */
        XP_ASSERT( len >= sizeof(plen) );
        plen = *(XP_U16*)data;
        data += sizeof(plen);
        len -= sizeof(plen);
        bytesPending = XP_NTOHS(plen);

        /* Start the packet */
        pbt_enqueue( &btStuff->vol.in, (XP_U8*)&bytesPending, sizeof(bytesPending), 
                     XP_FALSE, XP_FALSE );
    }

    /* if len is >= bytesPending, we have a packet.  Add bytesPending bytes,
       then recurse with remaining bytes.  If len is < bytesPending, just
       consume the bytes and return. */
    pbt_enqueue( &btStuff->vol.in, data, XP_MIN( len, bytesPending ), 
                 XP_FALSE, XP_TRUE );

    if ( len >= bytesPending ) {
        len -= bytesPending;
        data += bytesPending;
        pbt_postpone( btStuff, PBT_ACT_GOTDATA );
        if ( len > 0 ) {
            pbt_assemble( btStuff, data, len );
        }
    }
}
#endif

static void
socketCallback( BtLibSocketEventType* sEvent, UInt32 refCon )
{
    PalmBTStuff* btStuff = (PalmBTStuff*)refCon;
    BtLibSocketEventEnum event = sEvent->event;
    Err err;

    XP_LOGF( "%s(%s); status:%s", __func__, btEvtToStr(event),
             btErrToStr(sEvent->status) );

    switch( event ) {
    case btLibSocketEventConnectRequest: 
        if ( btStuff->picoRole == PBT_MASTER ) {
            /* sends btLibSocketEventConnectedInbound */
            CALL_ERR( err, BtLibSocketRespondToConnection, btStuff->btLibRefNum,  
                      sEvent->socket, true );
        } else {
            XP_LOGF( "ignoring b/c not master" );
        }
        break;
    case btLibSocketEventConnectedInbound:
        XP_ASSERT( btStuff->picoRole == PBT_MASTER );
        if ( sEvent->status == errNone ) {
            btStuff->dataSocket = sEvent->eventData.newSocket;
            XP_LOGF( "we have a data socket!!!" );
            pbt_postpone( btStuff, PBT_ACT_TELLCONN );
            SET_STATE( btStuff, PBTST_DATA_CONNECTED );
        }
        break;
    case btLibSocketEventConnectedOutbound:
        if ( errNone == sEvent->status ) {
            SET_STATE( btStuff, PBTST_DATA_CONNECTED );
            pbt_postpone( btStuff, PBT_ACT_TELLCONN );
        }
        break;
    case btLibSocketEventData:
        XP_ASSERT( sEvent->status == errNone );
        XP_ASSERT( sEvent->socket == btStuff->dataSocket );
        {
            XP_U8* data = sEvent->eventData.data.data;
            XP_U16 len = sEvent->eventData.data.dataLen;
#ifdef LOG_BTIO
            LOG_HEX( data, len, "btLibSocketEventData" );
#endif
#if defined BT_USE_RFCOMM
            pbt_assemble( btStuff, data, len );
#else
            if ( 0 < pbt_enqueue( &btStuff->vol.in, data, len, XP_FALSE, XP_FALSE ) ) {
                pbt_postpone( btStuff, PBT_ACT_GOTDATA );
            }
#endif
#ifdef DEBUG
            btStuff->stats.totalRcvd += sEvent->eventData.data.dataLen;
#endif        
        }
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
        if ( PBT_SLAVE == btStuff->picoRole ) {
            pbt_killLinks( btStuff );
            waitACL( btStuff );
        } else if ( PBT_MASTER == btStuff->picoRole ) {
            pbt_close_datasocket( btStuff );
            SET_STATE( btStuff, PBTST_LISTENING );
        }
        break;

#if defined BT_USE_L2CAP
    case btLibSocketEventSdpGetPsmByUuid:
#elif defined BT_USE_RFCOMM
    case btLibSocketEventSdpGetServerChannelByUuid:
#endif
        XP_ASSERT( sEvent->socket == btStuff->u.slave.sdpSocket );
        pbt_close_sdpsocket( btStuff );
        if ( sEvent->status == errNone ) {
#if defined BT_USE_L2CAP
            btStuff->u.slave.remotePsm = sEvent->eventData.sdpByUuid.param.psm;
#elif defined BT_USE_RFCOMM
            btStuff->u.slave.remoteService
                = sEvent->eventData.sdpByUuid.param.channel;
            XP_LOGF( "got remoteService of %d", 
                     btStuff->u.slave.remoteService );
#endif
            SET_STATE( btStuff, PBTST_SDP_QUERIED );
            pbt_postpone( btStuff, PBT_ACT_CONNECT_DATA );
        } else if ( sEvent->status == btLibErrSdpQueryDisconnect ) {
            /* Maybe we can just ignore this... */
            XP_ASSERT( GET_STATE(btStuff) == PBTST_NONE ); /* still still firing!!! */
            pbt_killLinks( btStuff );
            waitACL( btStuff );
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
        XP_ASSERT( 0 );         /* is this getting called?  It's an event *and* status??? */
        /* Probably need to warn the user when this happens since not having
           established trust will be a common error.  Or: figure out if
           there's a way to fall back and establish trust programatically.
           For alpha just do the error message. :-) Also, no point in
           continuing to try to connect.  User will have to quit in order to
           establish trust.  So warn once per inited session. */
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
    PalmBTStuff* btStuff = (PalmBTStuff*)refCon;
    BtLibManagementEventEnum event = mEvent->event;
    XP_LOGF( "%s(%s); status=%s", __func__, mgmtEvtToStr(event),
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
            pbt_close_datasocket( btStuff );
            SET_STATE( btStuff, PBTST_NONE );
            /* See comment at btLibSocketEventDisconnected */
            if ( PBT_SLAVE == btStuff->picoRole ) {
                waitACL( btStuff );
            }
        }
        break;
    default:
        XP_LOGF( "%s: %s not handled", __func__, mgmtEvtToStr(event));
        break;
    }
    LOG_RETURN_VOID();
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
        CASESTR(PBTST_DATA_CONNECTING);
        CASESTR(PBTST_DATA_CONNECTED);
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
        CASESTR(PBT_ACT_MASTER_RESET);
        CASESTR(PBT_ACT_SLAVE_RESET);
        CASESTR(PBT_ACT_SETUP_LISTEN);
        CASESTR(PBT_ACT_CONNECT_ACL);
        CASESTR(PBT_ACT_GETSDP);
        CASESTR(PBT_ACT_CONNECT_DATA);
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
