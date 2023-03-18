/* -*- Compile-command: "make MEMDEBUG=TRUE -j3";-*- */ 
/* 
 * Copyright 2006 - 2012 by Eric House (xwords@eehouse.org).  All rights
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

/*
  http://www.btessentials.com/examples/examples.html is good for some of this
  stuff.  Copyright allows free use.
*/

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#if defined BT_USE_L2CAP
# include <bluetooth/l2cap.h>
#elif defined BT_USE_RFCOMM
# include <bluetooth/rfcomm.h>
#endif
#include <bluetooth/sdp.h>
#include <bluetooth/sdp_lib.h>

#include "linuxbt.h"
#include "comms.h"
#include "strutils.h"
#include "uuidhack.h"
#include "gsrcwrap.h"

#define MAX_CLIENTS 1

#if defined BT_USE_L2CAP
# define L2_RF_ADDR struct sockaddr_l2
#elif defined BT_USE_RFCOMM
# define L2_RF_ADDR struct sockaddr_rc
#endif

typedef struct LinBtStuff {
    CommonGlobals* globals;
    union {
        struct {
            int listener;               /* socket */
            void* listenerStorage;
            XP_Bool threadDie;
            sdp_session_t* session;
        } master;
    } u;

    /* A single socket's fine as long as there's only one client allowed. */
    int socket;
    XP_Bool amMaster;
} LinBtStuff;

static gboolean bt_socket_proc( GIOChannel* source, GIOCondition condition, 
                                gpointer data );

static LinBtStuff*
lbt_make( MPFORMAL XP_Bool amMaster )
{
    LinBtStuff* btStuff = (LinBtStuff*)XP_MALLOC( mpool, sizeof(*btStuff) );
    XP_MEMSET( btStuff, 0, sizeof(*btStuff) );

    btStuff->amMaster = amMaster;
    btStuff->socket = -1;

    return btStuff;
} /* lbt_make */

static L2_RF_ADDR*
getL2Addr( const CommsAddrRec* addrP, L2_RF_ADDR* const saddr )
{
    LOG_FUNC();
    L2_RF_ADDR* result = NULL;

    int status;
    bdaddr_t target;
    uuid_t svc_uuid;
    sdp_list_t *response_list, *search_list, *attrid_list;
    sdp_session_t *session = 0;
    uint32_t range = 0x0000ffff;

    XP_MEMCPY( &target, &addrP->u.bt.btAddr, sizeof(target) );

    // connect to the SDP server running on the remote machine
    session = sdp_connect( BDADDR_ANY, &target, 0 );
    if ( NULL == session ) {
        XP_LOGF( "%s: sdp_connect->%s", __func__, strerror(errno) );
    } else {
        str2uuid( XW_BT_UUID, &svc_uuid.value.uuid128, 
                  sizeof(svc_uuid.value.uuid128) );
        svc_uuid.type = SDP_UINT128;
        search_list = sdp_list_append( 0, &svc_uuid );
        attrid_list = sdp_list_append( 0, &range );

        response_list = NULL;
        status = sdp_service_search_attr_req( session, search_list,
                                              SDP_ATTR_REQ_RANGE, attrid_list, 
                                              &response_list );

        if( status == 0 ) {
            sdp_list_t *r;
        
            // go through each of the service records
            for ( r = response_list; r && !result; r = r->next ) {
                sdp_list_t *proto_list = NULL;
                sdp_record_t *rec = (sdp_record_t*) r->data;
            
                // get a list of the protocol sequences
                if( sdp_get_access_protos( rec, &proto_list ) == 0 ) {
#if defined BT_USE_L2CAP
                    unsigned short psm = sdp_get_proto_port( proto_list, 
                                                             L2CAP_UUID );
                    if ( psm > 0 ) {
                        sdp_list_free( proto_list, 0 );
                        saddr->l2_family = AF_BLUETOOTH;
                        saddr->l2_psm = htobs( psm );
                        XP_MEMCPY( &saddr->l2_bdaddr, &addrP->u.bt.btAddr, 
                                   sizeof(saddr->l2_bdaddr) );
                        result = saddr;
                    }
#elif defined BT_USE_RFCOMM
                    int channel = sdp_get_proto_port( proto_list,
                                                      RFCOMM_UUID );
                    if ( channel > 0 ) {
                        XP_LOGF( "got channel: %d", channel );
                        saddr->rc_channel = (uint8_t)channel;
                        saddr->rc_family = AF_BLUETOOTH;
                        XP_MEMCPY( &saddr->rc_bdaddr, &addrP->u.bt.btAddr, 
                                   sizeof(saddr->rc_bdaddr) );
                        result = saddr;
                    }
#endif
                }
                sdp_record_free( rec );
            }
        }
        sdp_list_free( response_list, 0 );
        sdp_list_free( search_list, 0 );
        sdp_list_free( attrid_list, 0 );
        sdp_close( session );
    }

    LOG_RETURNF( "%p", result );
    return result;
} /* getL2Addr */

static void
lbt_connectSocket( LinBtStuff* btStuff, const CommsAddrRec* addrP )
{
    int sock;
    LOG_FUNC();

    // allocate a socket
    sock = socket( AF_BLUETOOTH, 
#if defined BT_USE_L2CAP
                   SOCK_SEQPACKET, BTPROTO_L2CAP
#elif defined BT_USE_RFCOMM
                   SOCK_STREAM, BTPROTO_RFCOMM
#endif
                   );
    if ( sock < 0 ) {
        XP_LOGF( "%s: socket->%s", __func__, strerror(errno) );
    } else {
        L2_RF_ADDR saddr;
        XP_MEMSET( &saddr, 0, sizeof(saddr) );
        if ( (NULL != getL2Addr( addrP, &saddr ) )
             // set the connection parameters (who to connect to)
             // connect to server
             && (0 == connect( sock, (struct sockaddr *)&saddr, sizeof(saddr) )) ) {
            CommonGlobals* globals = btStuff->globals;
            ADD_SOCKET( globals->socketAddedClosure, sock, bt_socket_proc );
            btStuff->socket = sock;
        } else {
            XP_LOGF( "%s: connect->%s; closing socket %d", __func__, strerror(errno), sock );
            close( sock );
        }
    }
} /* lbt_connectSocket */

static XP_Bool
lbt_accept( int listener, void* ctxt )
{
    CommonGlobals* globals = (CommonGlobals*)ctxt;
    LinBtStuff* btStuff = globals->btStuff;
    int sock = -1;
    L2_RF_ADDR inaddr;
    socklen_t slen;
    XP_Bool success;

    LOG_FUNC();

    XP_LOGF( "%s: calling accept", __func__ );
    slen = sizeof( inaddr );
    XP_ASSERT( listener == btStuff->u.master.listener );
    sock = accept( listener, (struct sockaddr *)&inaddr, &slen );
    XP_LOGF( "%s: accept returned; socket = %d", __func__, sock );
    
    success = sock >= 0;
    if ( success ) {
        ADD_SOCKET( globals->socketAddedClosure, sock, bt_socket_proc );
        XP_ASSERT( btStuff->socket == -1 );
        btStuff->socket = sock;
    } else {
        XP_LOGF( "%s: accept->%s", __func__, strerror(errno) );
    }
    return success;
} /* lbt_accept */

static void
lbt_register( LinBtStuff* btStuff, unsigned short l2_psm, 
              uint8_t XP_UNUSED_RFCOMM(rc_channel) )
{
    LOG_FUNC();
    if ( NULL == btStuff->u.master.session ) {
        const char *service_name = XW_BT_NAME;
        const char *svc_dsc = "An open source word game";
        const char *service_prov = "xwords.sf.net";

        uuid_t svc_uuid;
        sdp_list_t 
            *root_list = 0,
            *proto_list = 0, 
            *access_proto_list = 0,
            *svc_class_list = 0,
            *profile_list = 0;
        sdp_record_t record = { 0 };
        sdp_session_t *session = NULL;

        sdp_list_t *l2cap_list = 0;
        sdp_data_t *psm = 0;
#if defined BT_USE_L2CAP
#elif defined BT_USE_RFCOMM
        sdp_list_t *rfcomm_list = 0;
        sdp_data_t *channel = 0;
#endif

        // set the general service ID
        str2uuid( XW_BT_UUID, &svc_uuid.value.uuid128, 
                  sizeof(svc_uuid.value.uuid128) );
        svc_uuid.type = SDP_UINT128;
        sdp_set_service_id( &record, svc_uuid );

        // set l2cap information
        uuid_t l2cap_uuid;
        sdp_uuid16_create( &l2cap_uuid, L2CAP_UUID );
        l2cap_list = sdp_list_append( 0, &l2cap_uuid );
        /* from pybluez source */
        unsigned short l2cap_psm = l2_psm;
        psm = sdp_data_alloc( SDP_UINT16, &l2cap_psm );
        sdp_list_append( l2cap_list, psm );
        proto_list = sdp_list_append( 0, l2cap_list );

#if defined BT_USE_RFCOMM
        uuid_t rfcomm_uuid;
        uint8_t rfcomm_channel = rc_channel;
        sdp_uuid16_create( &rfcomm_uuid, RFCOMM_UUID );
        channel = sdp_data_alloc( SDP_UINT8, &rfcomm_channel );
        rfcomm_list = sdp_list_append( 0, &rfcomm_uuid );
        sdp_list_append( rfcomm_list, channel );
        sdp_list_append( proto_list, rfcomm_list );
#endif

        access_proto_list = sdp_list_append( 0, proto_list );
        sdp_set_access_protos( &record, access_proto_list );

        // set the name, provider, and description
        sdp_set_info_attr(&record, service_name, service_prov, svc_dsc);

        // connect to the local SDP server, register the service record, 
        // and disconnect
        session = sdp_connect( BDADDR_ANY, BDADDR_LOCAL, SDP_RETRY_IF_BUSY );
        if ( NULL == session ) {
            XP_LOGFF( "sdp_connect->%s", strerror(errno) );
        } else {
            sdp_record_register( session, &record, 0 );
        }

        // cleanup
        sdp_data_free( psm );
        sdp_list_free( root_list, 0 );
        sdp_list_free( access_proto_list, 0 );
        sdp_list_free( svc_class_list, 0 );
        sdp_list_free( profile_list, 0 );
#if defined BT_USE_L2CAP
        sdp_list_free( l2cap_list, 0 );
#elif defined BT_USE_RFCOMM
        sdp_list_free( rfcomm_list, 0 );
        sdp_data_free( channel );
#endif

        btStuff->u.master.session = session;
    }
} /* lbt_register */

static void
lbt_listenerSetup( CommonGlobals* globals )
{
    LinBtStuff* btStuff = globals->btStuff;
    L2_RF_ADDR saddr;
    int listener;
    uint8_t rc_channel = 0;

    listener = socket( AF_BLUETOOTH, 
#if defined BT_USE_L2CAP
                   SOCK_SEQPACKET, BTPROTO_L2CAP
#elif defined BT_USE_RFCOMM
                   SOCK_STREAM, BTPROTO_RFCOMM
#endif
                   );
    btStuff->u.master.listener = listener;

    XP_MEMSET( &saddr, 0, sizeof(saddr) );
#if defined BT_USE_L2CAP
    saddr.l2_family = AF_BLUETOOTH;
    saddr.l2_bdaddr = *BDADDR_ANY;
    saddr.l2_psm = htobs( XW_PSM ); /* need to associate uuid with this before opening? */
    if ( 0 != bind( listener, (struct sockaddr *)&saddr, sizeof(saddr) ) ) {
        XP_LOGF( "%s: bind->%s", __func__, strerror(errno) ); 
    }
#elif defined BT_USE_RFCOMM
    saddr.rc_family = AF_BLUETOOTH;
    saddr.rc_bdaddr = *BDADDR_ANY;
    for ( rc_channel = 1; rc_channel < 30; ++rc_channel ) {
        saddr.rc_channel = rc_channel;
        XP_LOGF( "setting channel: %d", saddr.rc_channel );
        if ( 0 == bind( listener, (struct sockaddr *)&saddr, sizeof(saddr) ) ) {
            break;
        }
    }
#endif
    
    XP_LOGF( "%s: calling listen on socket %d", __func__, listener );
    listen( listener, MAX_CLIENTS );

    lbt_register( btStuff, htobs( XW_PSM ), rc_channel );

    (*globals->addAcceptor)( listener, lbt_accept, globals, 
                             &btStuff->u.master.listenerStorage );
} /* lbt_listenerSetup */

void
linux_bt_open( CommonGlobals* globals, XP_Bool amMaster )
{
    LinBtStuff* btStuff = globals->btStuff;
    if ( !btStuff ) {
        btStuff = globals->btStuff
            = lbt_make( MPPARM(globals->util->mpool) amMaster );
        btStuff->globals = globals;
        btStuff->socket = -1;

        globals->btStuff = btStuff;

        if ( amMaster ) {
            lbt_listenerSetup( globals );
        } else {
            if ( btStuff->socket < 0 ) {
                XP_ASSERT(0);   /* don't know if this works */
                CommsAddrRec addr;
                comms_getSelfAddr( globals->game.comms, &addr );
                lbt_connectSocket( btStuff, &addr );
            }
        }
    }
} /* linux_bt_open */

void
linux_bt_reset( CommonGlobals* globals )
{
    XP_Bool amMaster = globals->btStuff->amMaster;
    LOG_FUNC();
    linux_bt_close( globals );
    linux_bt_open( globals, amMaster );
    LOG_RETURN_VOID();
}

void
linux_bt_close( CommonGlobals* globals )
{
    LinBtStuff* btStuff = globals->btStuff;

    if ( !!btStuff ) {
        if ( btStuff->amMaster ) {
            XP_LOGF( "%s: closing listener socket %d", __func__, btStuff->u.master.listener );
            /* Remove from main event loop */
            (*globals->addAcceptor)( -1, NULL, globals,
                                     &btStuff->u.master.listenerStorage );
            close( btStuff->u.master.listener );
            btStuff->u.master.listener = -1;

            sdp_close( btStuff->u.master.session );
            XP_LOGF( "sleeping for Palm's sake..." );
            sleep( 2 );         /* see if this gives palm a chance to not hang */
        }

        XP_FREE( globals->util->mpool, btStuff );
        globals->btStuff = NULL;
    }
} /* linux_bt_close */

static XP_S16
linux_bt_send_impl( const XP_U8* buf, XP_U16 buflen,
                    const CommsAddrRec* addrP,
                    CommonGlobals* globals )
{
    XP_S16 nSent = -1;
    LinBtStuff* btStuff;

    XP_LOGF( "%s(len=%d)", __func__, buflen );
    LOG_HEX( buf, buflen, __func__ );

    btStuff = globals->btStuff;
    if ( !!btStuff ) {
        CommsAddrRec addr;
        if ( !addrP ) {
            comms_getSelfAddr( globals->game.comms, &addr );
            addrP = &addr;
        }

        if ( btStuff->socket < 0  && !btStuff->amMaster ) {
            lbt_connectSocket( btStuff, addrP );
        }

        if ( btStuff->socket >= 0 ) {
#if defined BT_USE_RFCOMM
            unsigned short len = htons(buflen);
            nSent = write( btStuff->socket, &len, sizeof(len) );
            assert( nSent == sizeof(len) );
#endif
            nSent = write( btStuff->socket, buf, buflen );
            if ( nSent < 0 ) {
                XP_LOGF( "%s: send->%s", __func__, strerror(errno) );
            } else if ( nSent < buflen ) {
                XP_LOGF( "%s: sent only %d bytes of %d", __func__, nSent, 
                         buflen );
                /* Need to loop until sent if this is happening */
                XP_ASSERT( 0 );
            }
        } else {
            XP_LOGF( "%s: socket still not set", __func__ );
        }
    }
    LOG_RETURNF( "%d", nSent );
    return nSent;
} /* linux_bt_send_impl */

XP_S16
linux_bt_send( XP_U16 count, SendMsgsPacket msgs[],
               const CommsAddrRec* addrRec, CommonGlobals* globals )
{
    XP_S16 result = 0;
    for ( int ii = 0; ii < count; ++ii ) {
        const SendMsgsPacket* packet = &msgs[ii];
        XP_S16 tmp = linux_bt_send_impl( packet->buf, packet->len,
                                         addrRec, globals );
        if ( tmp > 0 ) {
            result += tmp;
        } else {
            result = -1;
            break;
        }
    }
    return result;
}

#if defined BT_USE_RFCOMM
static void
read_all( int sock, unsigned char* buf, const int len )
{
    int totalRead = 0;
    while ( totalRead < len ) {
        int nRead = read( sock, buf+totalRead, len-totalRead );
        if ( nRead < 0 ) {
            XP_LOGF( "%s: read->%s", __func__, strerror(errno) );
            break;
        }
        totalRead += nRead;
        XP_ASSERT( totalRead <= len );
    }
}
#endif

XP_S16
linux_bt_receive( int sock, XP_U8* buf, XP_U16 buflen )
{
    XP_S16 nRead = 0;
    LOG_FUNC();
    XP_ASSERT( sock >= 0 );

#if defined BT_USE_RFCOMM
    read_all( sock, (unsigned char*)&nRead, sizeof(nRead) );
    nRead = ntohs(nRead);
    XP_LOGF( "nRead=%d", nRead );
    XP_ASSERT( nRead < buflen );

    read_all( sock, buf, nRead );
    LOG_HEX( buf, nRead, __func__ );
#else        
    nRead = read( sock, buf, buflen );
    if ( nRead < 0 ) {
        XP_LOGF( "%s: read->%s", __func__, strerror(errno) );
    }
#endif

    LOG_RETURNF( "%d", nRead );
    return nRead;
}

void
linux_bt_socketclosed( CommonGlobals* globals, int XP_UNUSED_DBG(sock) )
{
    LinBtStuff* btStuff = globals->btStuff;
    LOG_FUNC();
    XP_ASSERT( sock == btStuff->socket );
    btStuff->socket = -1;
}

#define RESPMAX 20

typedef gboolean (*bt_proc)( const bdaddr_t* bdaddr, int socket, void* closure );

static void
btDevsIterate( bt_proc proc, void* closure )
{
    LOG_FUNC();
    int id = hci_get_route( NULL );
    int socket = hci_open_dev( id );
    if ( id >= 0 && socket >= 0 ) {
        int flags = IREQ_CACHE_FLUSH;
        inquiry_info* iinfo = (inquiry_info*)
            malloc( RESPMAX * sizeof(inquiry_info) );
        int count = hci_inquiry( id, 8/*wait seconds*/,
                                 RESPMAX, NULL, &iinfo, flags );
        XP_LOGF( "%s: hci_inquiry=>%d", __func__, count );
        int ii;
        if ( 0 < count ) {
            for ( ii = 0; ii < count; ++ii ) {
                const bdaddr_t* bdaddr = &iinfo[ii].bdaddr;
                if ( !(*proc)( bdaddr, socket, closure ) ) {
                    break;
                }
            }
        } else {
            close( socket );
        }
    }
}

typedef struct _get_ba_data {
    const char* name;
    bdaddr_t* ba;
    gboolean success;
} get_ba_data;

static gboolean
get_ba_proc( const bdaddr_t* bdaddr, int socket, void* closure )
{
    get_ba_data* data = (get_ba_data*)closure;
    char buf[64];
    if ( 0 >= hci_read_remote_name( socket, bdaddr, sizeof(buf), buf, 0 ) ) {
        if ( 0 == strcmp( buf, data->name ) ) {
            XP_MEMCPY( data->ba, bdaddr, sizeof(*data->ba) );
            data->success = XP_TRUE;
            XP_LOGF( "%s: matched %s", __func__, data->name );
            char addrStr[32];
            ba2str( data->ba, addrStr );
            XP_LOGF( "bt_addr is %s", addrStr );
        }
    }
    return !data->success;
}

XP_Bool
nameToBtAddr( const char* name, bdaddr_t* ba )
{
    LOG_FUNC();
    get_ba_data data = { .name = name, .ba = ba, .success = FALSE };
    btDevsIterate( get_ba_proc, &data );
    return data.success;
} /* nameToBtAddr */

static gboolean
append_name_proc( const bdaddr_t* bdaddr, int socket, void* closure )
{
    GSList** list = (GSList**)closure;
    char buf[64];
    char addr[19] = { 0 };
    ba2str( bdaddr, addr );
    XP_LOGF( "%s: adding %s", __func__, addr );
    if ( 0 >= hci_read_remote_name( socket, bdaddr, sizeof(buf), buf, 0 ) ) {
        gchar* name = g_strdup( buf );
        *list = g_slist_append( *list, name );
    }
    return TRUE;
}

GSList*
linux_bt_scan()
{
    GSList* list = NULL;
    btDevsIterate( append_name_proc, &list );
    return list;
}

static gboolean
bt_socket_proc( GIOChannel* source, GIOCondition condition, gpointer data )
{
    XP_USE( data );
    int fd = g_io_channel_unix_get_fd( source );
    if ( 0 != (G_IO_IN & condition) ) {
        unsigned char buf[1024];
#ifdef DEBUG
        XP_S16 nBytes = 
#endif
            linux_bt_receive( fd, buf, sizeof(buf) );
        XP_ASSERT(nBytes==2 || XP_TRUE);

        XP_ASSERT(0);           /* not implemented beyond this point */
    } else {
        XP_ASSERT(0);           /* not implemented beyond this point */
    }
    return FALSE;
}

#endif /* XWFEATURE_BLUETOOTH */
