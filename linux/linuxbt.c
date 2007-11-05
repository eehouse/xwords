/* -*-mode: C; fill-column: 78; c-basic-offset: 4; compile-command: "make MEMDEBUG=TRUE";-*- */ 
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

/*
  http://www.btessentials.com/examples/ is good for some of this stuff.
  Copyright allows free use.
*/

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/l2cap.h>
#include <bluetooth/sdp.h>
#include <bluetooth/sdp_lib.h>

#include "linuxbt.h"
#include "comms.h"

#define MAX_CLIENTS 3

typedef struct BtaddrSockMap {
    bdaddr_t btaddr;
    int sock;
} BtaddrSockMap;

typedef struct LinBtStuff {
    CommonGlobals* globals;

    union {
        struct {
            BtaddrSockMap socks[MAX_CLIENTS];
            int listener;               /* socket */
            XP_U16 nSocks;
            XP_Bool threadDie;
            sdp_session_t* session;
        } master;
    } u;

    XP_Bool amMaster;
} LinBtStuff;

static void
lbt_addSock( LinBtStuff* btStuff, const bdaddr_t* btaddr, int sock )
{
    XP_U16 i;

    XP_ASSERT( btStuff->amMaster );
    XP_ASSERT( btStuff->u.master.nSocks < MAX_CLIENTS - 1 );

    for ( i = 0; i < MAX_CLIENTS; ++i ) {
        BtaddrSockMap* mp = &btStuff->u.master.socks[i];
        if ( mp->sock == -1 ) {
            XP_MEMCPY( &mp->btaddr, btaddr, sizeof(mp->btaddr) );
            mp->sock = sock;
            ++btStuff->u.master.nSocks;
            break;
        }
    }
    XP_ASSERT( i < MAX_CLIENTS );
} /* lbt_addSock */

static void
lbt_removeSock( LinBtStuff* btStuff, int sock )
{
    XP_U16 i;

    XP_ASSERT( btStuff->amMaster );
    XP_ASSERT( btStuff->u.master.nSocks > 0 );

    for ( i = 0; i < MAX_CLIENTS; ++i ) {
        BtaddrSockMap* mp = &btStuff->u.master.socks[i];
        if ( mp->sock == sock ) {
            mp->sock = -1;
            --btStuff->u.master.nSocks;
            break;
        }
    }
    XP_ASSERT( i < MAX_CLIENTS );
} /* lbt_removeSock */

static LinBtStuff*
lbt_make( MPFORMAL XP_Bool amMaster )
{
    LinBtStuff* btStuff = (LinBtStuff*)XP_MALLOC( mpool, sizeof(*btStuff) );
    XP_MEMSET( btStuff, 0, sizeof(*btStuff) );

    btStuff->amMaster = amMaster;

    if ( amMaster ) {
        XP_U16 i;
        for ( i = 0; i < MAX_CLIENTS; ++i ) {
            btStuff->u.master.socks[i].sock = -1;
        }
    }

    return btStuff;
} /* lbt_make */

static struct sockaddr_l2* 
getL2Addr( const CommsAddrRec const* addrP, struct sockaddr_l2* const saddr )
{
    LOG_FUNC();
    struct sockaddr_l2* result = NULL;

    uint8_t svc_uuid_int[] = XW_BT_UUID;

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
        XP_LOGF( "%s: sdp_connect->%s", __FUNCTION__, strerror(errno) );
    } else {
        sdp_uuid128_create( &svc_uuid, &svc_uuid_int );
        search_list = sdp_list_append( 0, &svc_uuid );
        attrid_list = sdp_list_append( 0, &range );

        response_list = NULL;
        status = sdp_service_search_attr_req( session, search_list,
                                              SDP_ATTR_REQ_RANGE, attrid_list, 
                                              &response_list );

        if( status == 0 ) {
            sdp_list_t *r;
        
            // go through each of the service records
            for ( r = response_list;; r; r = r->next ) {
                sdp_list_t *proto_list = NULL;
                sdp_record_t *rec = (sdp_record_t*) r->data;
            
                // get a list of the protocol sequences
                if( sdp_get_access_protos( rec, &proto_list ) == 0 ) {
                    unsigned short psm = sdp_get_proto_port( proto_list, 
                                                             L2CAP_UUID );
                    sdp_list_free( proto_list, 0 );

                    saddr->l2_family = AF_BLUETOOTH;
                    saddr->l2_psm = htobs( psm );
                    XP_MEMCPY( &saddr->l2_bdaddr, &addrP->u.bt.btAddr, 
                               sizeof(saddr->l2_bdaddr) );
                    result = saddr;
                }
                sdp_record_free( rec );
            }
        }
        sdp_list_free( response_list, 0 );
        sdp_list_free( search_list, 0 );
        sdp_list_free( attrid_list, 0 );
        sdp_close( session );
    }

    return result;
} /* getL2Addr */

static void
lbt_connectSocket( LinBtStuff* btStuff, const CommsAddrRec* addrP )
{
    int sock;

    // allocate a socket
    sock = socket( AF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP );

    if ( sock < 0 ) {
        XP_LOGF( "%s: socket->%s", __FUNCTION__, strerror(errno) );
    } else {
        struct sockaddr_l2 saddr;
        XP_MEMSET( &saddr, 0, sizeof(saddr) );
        if ( (NULL != getL2Addr( addrP, &saddr ) )
        // set the connection parameters (who to connect to)
        // connect to server
             && (0 == connect( sock, (struct sockaddr *)&saddr, sizeof(saddr) )) ) {
            CommonGlobals* globals = btStuff->globals;
            (*globals->socketChanged)( globals->socketChangedClosure, 
                                       -1, sock );
        } else {
            XP_LOGF( "%s: connect->%s", __FUNCTION__, strerror(errno) );
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
    struct sockaddr_l2 inaddr;
    socklen_t slen;
    XP_Bool success;

    LOG_FUNC();

    XP_LOGF( "%s: calling accept", __FUNCTION__ );
    slen = sizeof( inaddr );
    XP_ASSERT( listener == btStuff->u.master.listener );
    sock = accept( listener, (struct sockaddr *)&inaddr, &slen );
    XP_LOGF( "%s: accept returned; sock = %d", __FUNCTION__, sock );
    
    success = sock >= 0;
    if ( success ) {
        lbt_addSock( btStuff, &inaddr.l2_bdaddr, sock );
        (*globals->socketChanged)( globals->socketChangedClosure, 
                                   -1, sock );
    } else {
        XP_LOGF( "%s: accept->%s", __FUNCTION__, strerror(errno) );
    }
    return success;
} /* lbt_accept */

static void
lbt_register( LinBtStuff* btStuff, const struct sockaddr_l2* const saddr )
{
    LOG_FUNC();
    if ( NULL == btStuff->u.master.session ) {
        uint8_t svc_uuid_int[] = XW_BT_UUID;
        const char *service_name = XW_BT_NAME;
        const char *svc_dsc = "An experimental plumbing router";
        const char *service_prov = "Roto-Rooter";

        uuid_t l2cap_uuid, svc_uuid;
        sdp_list_t *l2cap_list = 0, 
            *root_list = 0,
            *proto_list = 0, 
            *access_proto_list = 0,
            *svc_class_list = 0,
            *profile_list = 0;
        sdp_data_t *psm = 0;
        sdp_record_t record = { 0 };
        sdp_session_t *session = NULL;

        // set the general service ID
        sdp_uuid128_create( &svc_uuid, &svc_uuid_int );
        sdp_set_service_id( &record, svc_uuid );

        // set l2cap information
        sdp_uuid16_create( &l2cap_uuid, L2CAP_UUID );
        l2cap_list = sdp_list_append( 0, &l2cap_uuid );
        /* from pybluez source */
        unsigned short l2cap_psm = saddr->l2_psm;
        psm = sdp_data_alloc( SDP_UINT16, &l2cap_psm );
        sdp_list_append( l2cap_list, psm );
        proto_list = sdp_list_append( 0, l2cap_list );

        access_proto_list = sdp_list_append( 0, proto_list );
        sdp_set_access_protos( &record, access_proto_list );

        // set the name, provider, and description
        sdp_set_info_attr(&record, service_name, service_prov, svc_dsc);

        // connect to the local SDP server, register the service record, 
        // and disconnect
        session = sdp_connect( BDADDR_ANY, BDADDR_LOCAL, SDP_RETRY_IF_BUSY );
        if ( NULL == session ) {
            XP_LOGF( "%s: sdp_connect->%s", __FUNCTION__, strerror(errno) );
        }
        XP_ASSERT( NULL != session );
        sdp_record_register( session, &record, 0 );

        // cleanup
        sdp_data_free( psm );
        sdp_list_free( l2cap_list, 0 );
        sdp_list_free( root_list, 0 );
        sdp_list_free( access_proto_list, 0 );
        sdp_list_free( svc_class_list, 0 );
        sdp_list_free( profile_list, 0 );

        btStuff->u.master.session = session;
    }
} /* lbt_register */

static void
lbt_listenerSetup( CommonGlobals* globals )
{
    LinBtStuff* btStuff = globals->btStuff;
    struct sockaddr_l2 saddr;
    int listener;

    listener = socket( AF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP );
    btStuff->u.master.listener = listener;

    XP_MEMSET( &saddr, 0, sizeof(saddr) );
    saddr.l2_family = AF_BLUETOOTH;
    saddr.l2_bdaddr = *BDADDR_ANY;
    saddr.l2_psm = htobs( XW_PSM ); /* need to associate uuid with this before opening? */
    bind( listener, (struct sockaddr *)&saddr, sizeof(saddr) );

    listen( listener, MAX_CLIENTS );

    lbt_register( btStuff, &saddr );

    (*globals->addAcceptor)( listener, lbt_accept, globals );
} /* lbt_listenerSetup */

void
linux_bt_open( CommonGlobals* globals, XP_Bool amMaster )
{
    LinBtStuff* btStuff = globals->btStuff;
    if ( !btStuff ) {
        btStuff = globals->btStuff
            = lbt_make( MPPARM(globals->params->util->mpool) amMaster );
        btStuff->globals = globals;
        globals->btStuff = btStuff;

        if ( amMaster ) {
            lbt_listenerSetup( globals );
        } else {
            if ( globals->socket < 0 ) {
                CommsAddrRec addr;
                comms_getAddr( globals->game.comms, &addr );
                lbt_connectSocket( btStuff, &addr );
            }
        }
    }
} /* linux_bt_open */

void
linux_bt_close( CommonGlobals* globals )
{
    LinBtStuff* btStuff = globals->btStuff;

    if ( !!btStuff ) {
        if ( btStuff->amMaster ) {
            close( btStuff->u.master.listener );
            sdp_close( btStuff->u.master.session );
            XP_LOGF( "sleeping for Palm's sake..." );
            sleep( 2 );         /* see if this gives palm a chance to not hang */
        }
        XP_FREE( globals->params->util->mpool, btStuff );
        globals->btStuff = NULL;
    }
} /* linux_bt_close */

XP_S16
linux_bt_send( const XP_U8* buf, XP_U16 buflen, 
               const CommsAddrRec* addrP, 
               CommonGlobals* globals )
{
    XP_S16 nSent = -1;
    LinBtStuff* btStuff;

    XP_LOGF( "%s(len=%d)", __FUNCTION__, buflen );

    btStuff = globals->btStuff;
    if ( !!btStuff ) {
        CommsAddrRec addr;
        if ( !addrP ) {
            comms_getAddr( globals->game.comms, &addr );
            addrP = &addr;
        }

        if ( globals->socket < 0  && !btStuff->amMaster ) {
            lbt_connectSocket( btStuff, addrP );
        }

        if ( globals->socket >= 0 ) {
            nSent = write( globals->socket, buf, buflen );
            if ( nSent < 0 ) {
                XP_LOGF( "%s: send->%s", __FUNCTION__, strerror(errno) );
            } else if ( nSent < buflen ) {
                XP_LOGF( "%s: send only %d bytes of %d", __FUNCTION__, nSent, 
                         buflen );
            }
        } else {
            XP_LOGF( "%s: socket still not set", __FUNCTION__ );
        }
    }
    LOG_RETURNF( "%d", nSent );
    return nSent;
} /* linux_bt_send */

XP_S16
linux_bt_receive( int sock, XP_U8* buf, XP_U16 buflen )
{
    XP_S16 nRead = 0;
    LOG_FUNC();
    XP_ASSERT( sock >= 0 );

    nRead = read( sock, buf, buflen );
    if ( nRead < 0 ) {
        XP_LOGF( "%s: read->%s", __FUNCTION__, strerror(errno) );
    }

    LOG_RETURNF( "%d", nRead );
    return nRead;
}

void
linux_bt_socketclosed( CommonGlobals* globals, int sock )
{
    LinBtStuff* btStuff = globals->btStuff;
    if ( btStuff->amMaster ) {
        lbt_removeSock( btStuff, sock );
    }
}

#endif /* XWFEATURE_BLUETOOTH */

