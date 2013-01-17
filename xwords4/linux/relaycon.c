/* -*- compile-command: "make MEMDEBUG=TRUE -j3"; -*- */
/* 
 * Copyright 2013 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#include <netdb.h>
#include <errno.h>

#include "relaycon.h"
#include "comtypes.h"

typedef struct _RelayConStorage {
    int socket;
    RelayConnProcs procs;
    void* procsClosure;
} RelayConStorage;

static RelayConStorage* getStorage( LaunchParams* params );
static void addressToServer( struct sockaddr_in* to, const CommsAddrRec* addr );
static XP_U32 addrForHost( const CommsAddrRec* addr );
static XP_U32 hostNameToIP( const XP_UCHAR* name );
static void relaycon_receive( void* closure, int socket );

void
relaycon_init( LaunchParams* params, const RelayConnProcs* procs, 
               void* procsClosure, const char* host, int port, 
               const XP_UCHAR* devID, DevIDType typ )
{
    XP_ASSERT( !params->relayConStorage );
    RelayConStorage* storage = getStorage( params );
    XP_MEMCPY( &storage->procs, procs, sizeof(storage->procs) );
    storage->procsClosure = procsClosure;

    storage->socket = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
    (*params->socketChanged)( params->socketChangedClosure, storage->socket, -1,
                              relaycon_receive, params );

    XP_ASSERT( !!devID );
    XP_U16 idLen = XP_STRLEN( devID );
    XP_U16 lenNBO = XP_HTONS( idLen );
    XP_U8 tmpbuf[1 + 1 + 1 + sizeof(lenNBO) + idLen];
    tmpbuf[0] = XWREG_PROTO_VERSION;
    tmpbuf[1] = XWRREG_REG;
    tmpbuf[2] = typ;
    XP_MEMCPY( &tmpbuf[3], &lenNBO, sizeof(lenNBO) );
    XP_MEMCPY( &tmpbuf[5], devID, idLen );

    struct sockaddr_in to = {0};
    to.sin_family = PF_INET;
    to.sin_addr.s_addr = htonl( hostNameToIP(host) );
    to.sin_port = htons(port);

    (void)sendto( storage->socket, tmpbuf, sizeof(tmpbuf), 0, /* flags */
                  (struct sockaddr*)&to, sizeof(to) );
}

XP_S16
relaycon_send( LaunchParams* params, const XP_U8* buf, XP_U16 buflen, 
               XP_U32 gameToken, const CommsAddrRec* addrRec )
{
    ssize_t nSent = -1;
    RelayConStorage* storage = getStorage( params );

    struct sockaddr_in to = {0};
    addressToServer( &to, addrRec );

    XP_U8 tmpbuf[1 + 1 + sizeof(gameToken) + buflen];
    tmpbuf[0] = XWREG_PROTO_VERSION;
    tmpbuf[1] = XWRREG_MSG;
    XP_U32 inNBO = htonl(gameToken);
    XP_MEMCPY( &tmpbuf[2], &inNBO, sizeof(inNBO) );
    XP_MEMCPY( &tmpbuf[1 + 1 + sizeof(gameToken)], buf, buflen );
    nSent = sendto( storage->socket, tmpbuf, sizeof(tmpbuf), 0, /* flags */
                    (struct sockaddr*)&to, sizeof(to) );
    if ( 1 + 1 + sizeof(gameToken) < nSent ) {
        nSent -= 1 + 1 + sizeof(gameToken);
    }
    LOG_RETURNF( "%d", nSent );
    return nSent;
}

static void 
relaycon_receive( void* closure, int socket )
{
    LaunchParams* params = (LaunchParams*)closure;
    XP_ASSERT( !!params->relayConStorage );
    RelayConStorage* storage = getStorage( params );
    XP_U8 buf[512];
    struct sockaddr_in from;
    socklen_t fromlen = sizeof(from);

    XP_LOGF( "%s: calling recvfrom on socket %d", __func__, socket );

    ssize_t nRead = recvfrom( socket, buf, sizeof(buf), 0, /* flags */
                              (struct sockaddr*)&from, &fromlen );
    XP_LOGF( "%s: read %d bytes", __func__, nRead );
    if ( 0 <= nRead ) {
        XP_U8* ptr = buf;
        const XP_U8* end = buf + nRead;
        XP_ASSERT( XWREG_PROTO_VERSION == *ptr++ );
        XWRelayReg cmd = *ptr++;
        switch( cmd ) {
        case XWRREG_REGRSP: {
            XP_U16 len;
            XP_MEMCPY( &len, ptr, sizeof(len) );
            len = ntohs( len );
            ptr += sizeof( len );
            XP_UCHAR devID[len+1];
            XP_MEMCPY( devID, ptr, len );
            devID[len] = '\0';
            (*storage->procs.devIDChanged)( storage->procsClosure, devID );
        }
            break;
        case XWRREG_MSG:
            (*storage->procs.msgReceived)( storage->procsClosure, 
                                           ptr, end - ptr );
            break;
        default:
            XP_LOGF( "%s: Unexpected cmd %d", __func__, cmd );
            XP_ASSERT( 0 );
        }
    } else {
        XP_LOGF( "%s: error reading udp socket: %d (%s)", __func__, 
                 errno, strerror(errno) );
    }
}

void
relaycon_cleanup( LaunchParams* params )
{
    XP_FREEP( params->mpool, &params->relayConStorage );
}

static RelayConStorage* 
getStorage( LaunchParams* params )
{
    RelayConStorage* storage = (RelayConStorage*)params->relayConStorage;
    if ( NULL == storage ) {
        storage = XP_CALLOC( params->mpool, sizeof(*storage) );
        storage->socket = -1;
        params->relayConStorage = storage;
    }
    return storage;
}

static void
addressToServer( struct sockaddr_in* to, const CommsAddrRec* addr )
{
    to->sin_family = PF_INET;
    to->sin_addr.s_addr = htonl( addrForHost(addr) );
    to->sin_port = htons(addr->u.ip_relay.port);
}

static XP_U32
addrForHost( const CommsAddrRec* addr )
{
    XP_U32 ip = addr->u.ip_relay.ipAddr;
    if ( 0L == ip ) {
        ip = hostNameToIP( addr->u.ip_relay.hostName );
    }
    return ip;
} /* addrForHost */

static XP_U32
hostNameToIP( const XP_UCHAR* name )
{
    XP_U32 ip;
    struct hostent* host;
    XP_LOGF( "%s: looking up %s", __func__, name );
    host = gethostbyname( name );
    if ( NULL == host ) {
        XP_WARNF( "gethostbyname returned NULL\n" );
    } else {
        XP_MEMCPY( &ip, host->h_addr_list[0], sizeof(ip) );
        ip = ntohl(ip);
    }
    XP_LOGF( "%s found %lx for %s", __func__, ip, name );
    return ip;
}
