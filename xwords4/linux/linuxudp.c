/* -*-mode: C; fill-column: 78; c-basic-offset: 4; compile-command: "make MEMDEBUG=TRUE";-*- */ 
/* 
 * Copyright 2007-2009 by Eric House (xwords@eehouse.org).  All rights
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

#ifdef XWFEATURE_IP_DIRECT

#include <stdio.h>
#include <unistd.h>
#include <errno.h>       
#include <netdb.h>
#include <sys/socket.h>
#ifdef XWFEATURE_BLUETOOTH
# include <bluetooth/bluetooth.h>
# if defined BT_USE_L2CAP
#  include <bluetooth/l2cap.h>
# elif defined BT_USE_RFCOMM
#  include <bluetooth/rfcomm.h>
# endif
# include <bluetooth/sdp.h>
# include <bluetooth/sdp_lib.h>
#endif

#include "linuxudp.h"
#include "comms.h"
#include "strutils.h"

/* Conecting: Expectation is that the client initiates the connection to a
 * known port on the server and that the server uses return addresses to reach
 * the client.  This works for games started from scratch.  But when we start
 * from saved games it doesn't: the server knows whatever ephemeral port the
 * client was on before, but that's all.  The client needs to ping the server
 * immediately, perhaps in a way analogous to btStartup.
 */


typedef struct LinUDPStuff {
    CommonGlobals* globals;
    CommsAddrRec addr;
    XP_Bool isServer;
    void* storage;
    int knownSocks[MAX_NUM_PLAYERS];
    int nKnownSocks;
    int socket;                 /* host opens to receive; guest opens to send */
} LinUDPStuff; 

void
linux_udp_open( CommonGlobals* globals, const CommsAddrRec* newAddr )
{
    LOG_FUNC();
    LinUDPStuff* stuff = globals->udpStuff;
    if ( !stuff ) {
        struct sockaddr_in saddr;
        int err;

        stuff = XP_MALLOC( globals->params->util->mpool, sizeof(*stuff) );
        XP_MEMSET( stuff, 0, sizeof(*stuff) );
        XP_MEMCPY( &stuff->addr, newAddr, sizeof(stuff->addr) );

        globals->udpStuff = stuff;
        stuff->globals = globals;
        stuff->socket = -1;
        
        stuff->isServer = comms_getIsServer( globals->game.comms );

        if ( stuff->isServer ) {
            int listenSock;
            listenSock = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
            saddr.sin_family = PF_INET;
            saddr.sin_addr.s_addr = htonl(INADDR_ANY);
            saddr.sin_port = htons(stuff->addr.u.ip.port_ip);
            XP_LOGF( "binding listen socket" );
            err = bind( listenSock, (struct sockaddr *)&saddr, sizeof(saddr) );
            if ( 0 != err ) {
                XP_LOGF( "bind()=>%s", strerror(errno) );
                listenSock = -1;
            }
            stuff->socket = listenSock;
            (*globals->socketChanged)( globals->socketChangedClosure, -1, 
                                       listenSock, &stuff->storage );
        }

    }
    LOG_RETURN_VOID();
} /* linux_udp_open */

void
linux_udp_reset( CommonGlobals* globals )
{
    CommsAddrRec addr;
    LinUDPStuff* stuff = globals->udpStuff;
    LOG_FUNC();
    XP_ASSERT( stuff );
    if ( !!stuff ) {
        XP_MEMCPY( &addr, &stuff->addr, sizeof(addr) );
        linux_udp_close( globals );
    }
    sleep( 1 );
    linux_udp_open( globals, &addr );
    LOG_RETURN_VOID();
}

void
linux_udp_close( CommonGlobals* globals )
{
    LinUDPStuff* stuff = globals->udpStuff;
    LOG_FUNC();
    if ( !!stuff ) {
        if ( stuff->socket != -1 ) {
            XP_LOGF( "closed socket %d", stuff->socket );
            (*globals->socketChanged)( globals->socketChangedClosure, stuff->socket, -1,
                                       &stuff->storage );
            close( stuff->socket );
            stuff->socket = -1;
        } else {
            XP_LOGF( "no socket to close" );
        }
        XP_FREE( globals->params->util->mpool, stuff );
        globals->udpStuff = NULL;
    }
    LOG_RETURN_VOID();
}

static XP_U32
addrForHost( const CommsAddrRec* addr )
{
    struct hostent* host;
    XP_U32 ip = addr->u.ip.ipAddr_ip;

    if ( 0L == ip ) {
        host = gethostbyname( addr->u.ip.hostName_ip );
        if ( NULL == host ) {
            XP_WARNF( "gethostbyname returned -1\n" );
        } else {
            XP_MEMCPY( &ip, host->h_addr_list[0], sizeof(ip) );
            ip = ntohl(ip);
        }
        XP_LOGF( "%s found %lx for %s", __func__, ip, addr->u.ip.hostName_ip );
    }
    return ip;
} /* addrForHost */

static void
addressToServer( struct sockaddr_in* to, const CommsAddrRec* addr )
{
    to->sin_family = PF_INET;
    to->sin_addr.s_addr = htonl( addrForHost(addr) );
    to->sin_port = htons(addr->u.ip.port_ip);
}

static void
remember( LinUDPStuff* stuff, int sock )
{
    XP_U16 i;
    XP_Bool known;
    for ( i = 0, known = XP_FALSE;
          !known && i < sizeof(stuff->knownSocks)/sizeof(stuff->knownSocks[0]);
          ++i ) {
        if ( stuff->knownSocks[i] == sock ) {
            known = XP_TRUE;
        } 
    }

    if ( !known ) {
        XP_ASSERT( stuff->nKnownSocks
                   < sizeof(stuff->knownSocks)/sizeof(stuff->knownSocks[0])-1 );
        XP_LOGF( "%s recording %d", __func__, sock );
        stuff->knownSocks[stuff->nKnownSocks++] = sock;
    }
}

static XP_Bool
remembered( const LinUDPStuff* stuff, int sock )
{
    XP_Bool known = XP_FALSE;
    XP_U16 i;
    for ( i = 0; i < stuff->nKnownSocks; ++i ) {
        XP_ASSERT( i < sizeof(stuff->knownSocks)/sizeof(stuff->knownSocks[0]) );
        if ( stuff->knownSocks[i] == sock ) {
            known = XP_TRUE;
        } 
    }
    LOG_RETURNF( "%d", (int)known );
    return known;
}

static XP_Bool
addressToClient( LinUDPStuff* stuff, struct sockaddr_in* to, const CommsAddrRec* addr )
{
    int port = addr->u.ip.port_ip;
    XP_Bool known = remembered( stuff, port );

    if ( known ) {
        to->sin_family = PF_INET;
        to->sin_addr.s_addr = htonl(addr->u.ip.ipAddr_ip);
        to->sin_port = htons(port);
    }
    return known;
}

XP_S16
linux_udp_send( const XP_U8* buf, XP_U16 buflen, const CommsAddrRec* addrp, 
                CommonGlobals* globals )
{
    LinUDPStuff* stuff = globals->udpStuff;
    ssize_t nSent = -1;

    LOG_FUNC();
    if ( NULL != stuff ) {
        XP_Bool haveAddress = XP_TRUE;
        CommsAddrRec addr;
        if ( !addrp ) {
            comms_getAddr( globals->game.comms, &addr );
            addrp = &addr;
        }

        struct sockaddr_in to;
        XP_MEMSET( &to, 0, sizeof(to) );

        if ( stuff->isServer ) {
            if ( !addressToClient( stuff, &to, addrp ) ) {
                haveAddress = XP_FALSE;
            }
        } else {
            if ( stuff->socket == -1 ) {
                stuff->socket = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
                XP_LOGF( "%s: client made socket = %d", __func__,
                         stuff->socket );
                (*stuff->globals->socketChanged)
                    ( stuff->globals->socketChangedClosure,
                      -1, stuff->socket, &stuff->storage );
            }
            addressToServer( &to, addrp );
        }

        if ( haveAddress ) {
            XP_LOGF( "calling sendto: sock=%d; port=%d; ipaddr=%x", 
                     stuff->socket, ntohs(to.sin_port), to.sin_addr.s_addr );
            nSent = sendto( stuff->socket, buf, buflen, 0, 
                            (struct sockaddr*)&to, sizeof(to) );
            if ( nSent != buflen ) {
                XP_LOGF( "sendto->%s", strerror(errno) );
            }
        }
    }

    LOG_RETURNF( "%d", nSent );
    return nSent;
} /* linux_udp_send */

XP_S16
linux_udp_receive( int sock, XP_U8* buf, XP_U16 buflen, CommsAddrRec* addr,
                   CommonGlobals* globals )
{
    struct sockaddr_in from;
    socklen_t fromlen = buflen;

    XP_LOGF( "%s: calling recvfrom on socket %d", __func__, sock );

    ssize_t nRead = recvfrom( sock, buf, buflen, 0, /* flags */
                              (struct sockaddr*)&from, &fromlen );
    XP_ASSERT( nRead > 0 );
    XP_LOGF( "%s read %d bytes", __func__, nRead );

    if ( nRead > 0 ) {
        int port;
        XP_MEMSET( addr, 0, sizeof(*addr) );
        addr->conType = COMMS_CONN_IP_DIRECT;
        port = ntohs(from.sin_port);
        addr->u.ip.port_ip = port;
        addr->u.ip.ipAddr_ip = ntohl(from.sin_addr.s_addr);

        if ( globals->udpStuff->isServer ) {
            remember( globals->udpStuff, port );
        }
    }

    return nRead;
}

void
linux_udp_socketclosed( CommonGlobals* globals, int sock )
{
    LinUDPStuff* stuff = globals->udpStuff;
    LOG_FUNC();
    XP_ASSERT( !!stuff );
    XP_ASSERT( stuff->socket == sock );
    stuff->socket = -1;
    LOG_RETURN_VOID();
}

#endif /* XWFEATURE_IP_DIRECT */
