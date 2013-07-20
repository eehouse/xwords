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
#include <stdbool.h>

#include "relaycon.h"
#include "comtypes.h"

typedef struct _RelayConStorage {
    int socket;
    RelayConnProcs procs;
    void* procsClosure;
    struct sockaddr_in saddr;
    uint32_t nextID;
} RelayConStorage;

typedef struct _MsgHeader {
    XWRelayReg cmd;
    uint32_t packetID;
} MsgHeader;

static RelayConStorage* getStorage( LaunchParams* params );
static XP_U32 hostNameToIP( const XP_UCHAR* name );
static void relaycon_receive( void* closure, int socket );
static ssize_t sendIt( RelayConStorage* storage, const XP_U8* msgbuf, XP_U16 len );
static size_t addStrWithLength( XP_U8* buf, XP_U8* end, const XP_UCHAR* str );
static void getNetString( const XP_U8** ptr, XP_U16 len, XP_UCHAR* buf );
static XP_U16 getNetShort( const XP_U8** ptr );
static XP_U32 getNetLong( const XP_U8** ptr );
static int writeHeader( RelayConStorage* storage, XP_U8* dest, XWRelayReg cmd );
static bool readHeader( const XP_U8** buf, MsgHeader* header );
static size_t writeDevID( XP_U8* buf, size_t len, const XP_UCHAR* str );


void
relaycon_init( LaunchParams* params, const RelayConnProcs* procs, 
               void* procsClosure, const char* host, int port )
{
    XP_ASSERT( !params->relayConStorage );
    RelayConStorage* storage = getStorage( params );
    XP_MEMCPY( &storage->procs, procs, sizeof(storage->procs) );
    storage->procsClosure = procsClosure;

    storage->socket = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
    (*procs->socketChanged)( procsClosure, storage->socket, -1,
                             relaycon_receive, params );

    XP_MEMSET( &storage->saddr, 0, sizeof(storage->saddr) );
    storage->saddr.sin_family = PF_INET;
    storage->saddr.sin_addr.s_addr = htonl( hostNameToIP(host) );
    storage->saddr.sin_port = htons(port);
}

void
relaycon_reg( LaunchParams* params, const XP_UCHAR* devID, DevIDType typ )
{
    LOG_FUNC();
    XP_U8 tmpbuf[32];
    int indx = 0;
    
    RelayConStorage* storage = getStorage( params );
    XP_ASSERT( !!devID || typ == ID_TYPE_ANON );
    indx += writeHeader( storage, tmpbuf, XWPDEV_REG );
    tmpbuf[indx++] = typ;
    indx += writeDevID( &tmpbuf[indx], sizeof(tmpbuf) - indx, devID );

    sendIt( storage, tmpbuf, indx );
}

XP_S16
relaycon_send( LaunchParams* params, const XP_U8* buf, XP_U16 buflen, 
               XP_U32 gameToken, const CommsAddrRec* XP_UNUSED(addrRec) )
{
    XP_ASSERT( 0 != gameToken );
    ssize_t nSent = -1;
    RelayConStorage* storage = getStorage( params );

    XP_U8 tmpbuf[1 + 4 + 1 + sizeof(gameToken) + buflen];
    int indx = 0;
    indx += writeHeader( storage, tmpbuf, XWPDEV_MSG );
    XP_U32 inNBO = htonl(gameToken);
    XP_MEMCPY( &tmpbuf[indx], &inNBO, sizeof(inNBO) );
    indx += sizeof(inNBO);
    XP_MEMCPY( &tmpbuf[indx], buf, buflen );
    indx += buflen;
    nSent = sendIt( storage, tmpbuf, indx );
    if ( nSent > buflen ) {
        nSent = buflen;
    }
    LOG_RETURNF( "%d", nSent );
    return nSent;
}

XP_S16 
relaycon_sendnoconn( LaunchParams* params, const XP_U8* buf, XP_U16 buflen, 
                     const XP_UCHAR* relayID, XP_U32 gameToken )
{
    XP_LOGF( "%s(relayID=%s)", __func__, relayID );
    XP_ASSERT( 0 != gameToken );
    XP_U16 indx = 0;
    ssize_t nSent = -1;
    RelayConStorage* storage = getStorage( params );

    XP_U16 idLen = XP_STRLEN( relayID );
    XP_U8 tmpbuf[1 + 4 + 1 +
                 1 + idLen +
                 sizeof(gameToken) + buflen];
    indx += writeHeader( storage, tmpbuf, XWPDEV_MSGNOCONN );
    gameToken = htonl( gameToken );
    XP_MEMCPY( &tmpbuf[indx], &gameToken, sizeof(gameToken) );
    indx += sizeof(gameToken);
    XP_MEMCPY( &tmpbuf[indx], relayID, idLen );
    indx += idLen;
    tmpbuf[indx++] = '\n';
    XP_MEMCPY( &tmpbuf[indx], buf, buflen );
    nSent = sendIt( storage, tmpbuf, sizeof(tmpbuf) );
    if ( nSent > buflen ) {
        nSent = buflen;
    }
    LOG_RETURNF( "%d", nSent );
    return nSent;
}

void
relaycon_requestMsgs( LaunchParams* params, const XP_UCHAR* devID )
{
    XP_LOGF( "%s(devID=%s)", __func__, devID );
    RelayConStorage* storage = getStorage( params );

    XP_U8 tmpbuf[128];
    int indx = 0;
    indx += writeHeader( storage, tmpbuf, XWPDEV_RQSTMSGS );
    indx += addStrWithLength( &tmpbuf[indx], tmpbuf + sizeof(tmpbuf), devID );

    sendIt( storage, tmpbuf, indx );
}

void
relaycon_deleted( LaunchParams* params, const XP_UCHAR* devID, 
                  XP_U32 gameToken )
{
    LOG_FUNC();
    RelayConStorage* storage = getStorage( params );
    XP_U8 tmpbuf[128];
    int indx = 0;
    indx += writeHeader( storage, tmpbuf, XWPDEV_DELGAME );
    indx += writeDevID( &tmpbuf[indx], sizeof(tmpbuf) - indx, devID );
    gameToken = htonl( gameToken );
    memcpy( &tmpbuf[indx], &gameToken, sizeof(gameToken) );
    indx += sizeof( gameToken );

    sendIt( storage, tmpbuf, indx );
}

static void
sendAckIf( RelayConStorage* storage, const MsgHeader* header )
{
    if ( header->cmd != XWPDEV_ACK ) {
        XP_U8 tmpbuf[16];
        int indx = writeHeader( storage, tmpbuf, XWPDEV_ACK );
        uint32_t msgID = htonl( header->packetID );
        memcpy( &tmpbuf[indx], &msgID, sizeof(msgID) );
        indx += sizeof(msgID);
        sendIt( storage, tmpbuf, indx );
    }
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

    gchar* b64 = g_base64_encode( (const guchar*)buf,
                                  ((0 <= nRead)? nRead : 0) );
    XP_LOGF( "%s: read %d bytes ('%s')", __func__, nRead, b64 );
    g_free( b64 );
    if ( 0 <= nRead ) {
        const XP_U8* ptr = buf;
        const XP_U8* end = buf + nRead;
        MsgHeader header;
        if ( readHeader( &ptr, &header ) ) {
            sendAckIf( storage, &header );
            switch( header.cmd ) {
            case XWPDEV_REGRSP: {
                XP_U16 len = getNetShort( &ptr );
                XP_UCHAR devID[len+1];
                getNetString( &ptr, len, devID );
                (*storage->procs.devIDChanged)( storage->procsClosure, devID );
            }
                break;
            case XWPDEV_MSG:
                (*storage->procs.msgReceived)( storage->procsClosure, 
                                               ptr, end - ptr );
                break;
            case XWPDEV_BADREG:
                (*storage->procs.devIDChanged)( storage->procsClosure, NULL );
                break;
            case XWPDEV_HAVEMSGS: {
                (*storage->procs.msgNoticeReceived)( storage->procsClosure );
                break;
            }
            case XWPDEV_ALERT: {
                XP_U16 len = getNetShort( &ptr );
                XP_UCHAR buf[len+1];
                getNetString( &ptr, len, buf );
                (*storage->procs.msgErrorMsg)( storage->procsClosure, buf );
                break;
            }
            case XWPDEV_ACK: {
                XP_U32 packetID = getNetLong( &ptr );
                XP_USE( packetID );
                XP_LOGF( "got ack for packetID %ld", packetID );
                break;
            }
            default:
                XP_LOGF( "%s: Unexpected cmd %d", __func__, header.cmd );
                XP_ASSERT( 0 );
            }
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

static ssize_t
sendIt( RelayConStorage* storage, const XP_U8* msgbuf, XP_U16 len )
{
    ssize_t nSent =  sendto( storage->socket, msgbuf, len, 0, /* flags */
                             (struct sockaddr*)&storage->saddr, 
                             sizeof(storage->saddr) );
    XP_LOGF( "%s()=>%d", __func__, nSent );
    return nSent;
}

static size_t
addStrWithLength( XP_U8* buf, XP_U8* end, const XP_UCHAR* str )
{
    XP_U16 len = !!str? XP_STRLEN( str ) : 0;
    if ( buf + len + sizeof(len) <= end ) {
        XP_U16 lenNBO = htons( len );
        XP_MEMCPY( buf, &lenNBO, sizeof(lenNBO) );
        buf += sizeof(lenNBO);
        XP_MEMCPY( buf, str, len );
    }
    return len + sizeof(len);
}

static size_t
writeDevID( XP_U8* buf, size_t len, const XP_UCHAR* str )
{
    return addStrWithLength( buf, buf + len, str );
}

static XP_U16
getNetShort( const XP_U8** ptr )
{
    XP_U16 result;
    memcpy( &result, *ptr, sizeof(result) );
    *ptr += sizeof(result);
    return ntohs( result );
}

static XP_U32
getNetLong( const XP_U8** ptr )
{
    XP_U32 result;
    memcpy( &result, *ptr, sizeof(result) );
    *ptr += sizeof(result);
    return ntohl( result );
}

static void
getNetString( const XP_U8** ptr, XP_U16 len, XP_UCHAR* buf )
{
    memcpy( buf, *ptr, len );
    *ptr += len;
    buf[len] = '\0';
}

static int
writeHeader( RelayConStorage* storage, XP_U8* dest, XWRelayReg cmd )
{
    int indx = 0;
    dest[indx++] = XWPDEV_PROTO_VERSION;
    uint32_t packetNum = storage->nextID++;
    packetNum = htonl(packetNum);
    memcpy( &dest[indx], &packetNum, sizeof(packetNum) );
    indx += sizeof(packetNum);
    dest[indx++] = cmd;
    return indx;
}

static bool
readHeader( const XP_U8** buf, MsgHeader* header )
{
    const XP_U8* ptr = *buf;
    bool ok = XWPDEV_PROTO_VERSION == *ptr++;
    assert( ok );
    uint32_t packetID;
    memcpy( &packetID, ptr, sizeof(packetID) );
    ptr += sizeof(packetID);
    header->packetID = ntohl( packetID );
    XP_LOGF( "%s: got packet %d", __func__, header->packetID );
    header->cmd = *ptr++;
    *buf = ptr;
    return ok;
}

/* Utilities */
#define TOKEN_MULT 1000000
XP_U32 
makeClientToken( sqlite3_int64 rowid, XP_U16 seed )
{
    XP_ASSERT( rowid < 0x0000FFFF );
    XP_U32 result = rowid;
    result *= TOKEN_MULT;             /* so visible when displayed as base-10 */
    result += seed;
    return result;
}

void
rowidFromToken( XP_U32 clientToken, sqlite3_int64* rowid, XP_U16* seed )
{
    *rowid = clientToken / TOKEN_MULT;
    *seed = clientToken % TOKEN_MULT;
}
