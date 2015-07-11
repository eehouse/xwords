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
#include "linuxmain.h"
#include "comtypes.h"

typedef struct _RelayConStorage {
    int socket;
    RelayConnProcs procs;
    void* procsClosure;
    struct sockaddr_in saddr;
    uint32_t nextID;
    XWPDevProto proto;
    LaunchParams* params;
} RelayConStorage;

typedef struct _MsgHeader {
    XWRelayReg cmd;
    uint32_t packetID;
} MsgHeader;

static RelayConStorage* getStorage( LaunchParams* params );
static XP_U32 hostNameToIP( const XP_UCHAR* name );
static gboolean relaycon_receive( GIOChannel *source, GIOCondition condition, 
                                  gpointer data );
static ssize_t sendIt( RelayConStorage* storage, const XP_U8* msgbuf, XP_U16 len );
static size_t addVLIStr( XP_U8* buf, size_t len, const XP_UCHAR* str );
static void getNetString( const XP_U8** ptr, XP_U16 len, XP_UCHAR* buf );
static XP_U16 getNetShort( const XP_U8** ptr );
static XP_U32 getNetLong( const XP_U8** ptr );
static int writeHeader( RelayConStorage* storage, XP_U8* dest, XWRelayReg cmd );
static bool readHeader( const XP_U8** buf, MsgHeader* header );
static size_t writeDevID( XP_U8* buf, size_t len, const XP_UCHAR* str );
static size_t writeShort( XP_U8* buf, size_t len, XP_U16 shrt );
static size_t writeLong( XP_U8* buf, size_t len, XP_U32 lng );
static size_t writeBytes( XP_U8* buf, size_t len, const XP_U8* bytes, 
                          size_t nBytes );
static size_t writeVLI( XP_U8* out, uint32_t nn );
static size_t un2vli( int nn, uint8_t* buf );
static bool vli2un( const uint8_t** inp, uint32_t* outp );


void
relaycon_init( LaunchParams* params, const RelayConnProcs* procs, 
               void* procsClosure, const char* host, int port )
{
    XP_ASSERT( !params->relayConStorage );
    RelayConStorage* storage = getStorage( params );
    XP_MEMCPY( &storage->procs, procs, sizeof(storage->procs) );
    storage->procsClosure = procsClosure;

    storage->socket = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
    (*procs->socketAdded)( storage, storage->socket, relaycon_receive );

    XP_MEMSET( &storage->saddr, 0, sizeof(storage->saddr) );
    storage->saddr.sin_family = PF_INET;
    storage->saddr.sin_addr.s_addr = htonl( hostNameToIP(host) );
    storage->saddr.sin_port = htons(port);

    storage->params = params;

    storage->proto = XWPDEV_PROTO_VERSION_1;
}

/* Send existing relay-assigned rDevID to relay, or empty string if we have
   none.  Send local devID and type, ID_TYPE_NONE, if we aren't providing an
   update.  It's an error for neither to be provided. */
void
relaycon_reg( LaunchParams* params, const XP_UCHAR* rDevID, 
              DevIDType typ, const XP_UCHAR* devID )
{
    XP_LOGF( "%s(typ=%d)", __func__, typ );
    XP_U8 tmpbuf[256];
    int indx = 0;

    RelayConStorage* storage = getStorage( params );
    indx += writeHeader( storage, tmpbuf, XWPDEV_REG );
    indx += addVLIStr( &tmpbuf[indx], sizeof(tmpbuf) - indx, rDevID );

    assert( ID_TYPE_RELAY != typ );
    tmpbuf[indx++] = typ;
    if ( ID_TYPE_NONE != typ ) {
        indx += writeDevID( &tmpbuf[indx], sizeof(tmpbuf) - indx, devID );
    }
    indx += writeShort( &tmpbuf[indx], sizeof(tmpbuf) - indx, 
                        INITIAL_CLIENT_VERS );
    indx += addVLIStr( &tmpbuf[indx], sizeof(tmpbuf) - indx, SVN_REV );
    indx += addVLIStr( &tmpbuf[indx], sizeof(tmpbuf) - indx, "linux box" );
    indx += addVLIStr( &tmpbuf[indx], sizeof(tmpbuf) - indx, "linux version" );

    sendIt( storage, tmpbuf, indx );
}

void
relaycon_invite( LaunchParams* params, XP_U32 destDevID, 
                 const XP_UCHAR* relayID, NetLaunchInfo* invit )
{
    XP_U8 tmpbuf[256];
    int indx = 0;

    RelayConStorage* storage = getStorage( params );
    indx += writeHeader( storage, tmpbuf, XWPDEV_INVITE );
    XP_U32 me = linux_getDevIDRelay( params );
    indx += writeLong( &tmpbuf[indx], sizeof(tmpbuf) - indx, me );

    /* write relayID <connname>/<hid>, or if we have an actual devID write a
       null byte plus it. */
    if ( 0 == destDevID ) {
        XP_ASSERT( '\0' != relayID[0] );
        indx += writeBytes( &tmpbuf[indx], sizeof(tmpbuf) - indx, 
                            (XP_U8*)relayID, 1 + XP_STRLEN( relayID ) );
    } else {
        tmpbuf[indx++] = '\0';  /* null byte: zero-len str */
        indx += writeLong( &tmpbuf[indx], sizeof(tmpbuf) - indx, destDevID );
    }

    XWStreamCtxt* stream = mem_stream_make( MPPARM(params->mpool) 
                                            params->vtMgr, params, 
                                            CHANNEL_NONE, NULL );
    nli_saveToStream( invit, stream );
    XP_U16 len = stream_getSize( stream );
    indx += writeShort( &tmpbuf[indx], sizeof(tmpbuf) - indx, len );
    XP_ASSERT( indx + len < sizeof(tmpbuf) );
    const XP_U8* ptr = stream_getPtr( stream );
    indx += writeBytes( &tmpbuf[indx], sizeof(tmpbuf) - indx, ptr, len );
    stream_destroy( stream );

    sendIt( storage, tmpbuf, indx );
    LOG_RETURN_VOID();
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
    indx += writeLong( &tmpbuf[indx], sizeof(tmpbuf) - indx, gameToken );
    indx += writeBytes( &tmpbuf[indx], sizeof(tmpbuf) - indx, buf, buflen );
    nSent = sendIt( storage, tmpbuf, indx );
    if ( nSent > buflen ) {
        nSent = buflen;
    }
    LOG_RETURNF( "%zd", nSent );
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
    indx += writeLong( &tmpbuf[indx], sizeof(tmpbuf) - indx, gameToken );
    indx += writeBytes( &tmpbuf[indx], sizeof(tmpbuf) - indx, 
                        (const XP_U8*)relayID, idLen );
    tmpbuf[indx++] = '\n';
    indx += writeBytes( &tmpbuf[indx], sizeof(tmpbuf) - indx, buf, buflen );
    nSent = sendIt( storage, tmpbuf, indx );
    if ( nSent > buflen ) {
        nSent = buflen;
    }
    LOG_RETURNF( "%zd", nSent );
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
    indx += addVLIStr( &tmpbuf[indx], sizeof(tmpbuf) - indx, devID );

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
    indx += writeLong( &tmpbuf[indx], sizeof(tmpbuf) - indx, gameToken );

    sendIt( storage, tmpbuf, indx );
}

static void
sendAckIf( RelayConStorage* storage, const MsgHeader* header )
{
    if ( header->cmd != XWPDEV_ACK ) {
        XP_U8 tmpbuf[16];
        int indx = writeHeader( storage, tmpbuf, XWPDEV_ACK );
        indx += writeVLI( &tmpbuf[indx], header->packetID );
        sendIt( storage, tmpbuf, indx );
    }
}

static gboolean 
relaycon_receive( GIOChannel* source, GIOCondition XP_UNUSED_DBG(condition), gpointer data )
{
    XP_ASSERT( 0 != (G_IO_IN & condition) ); /* FIX ME */
    RelayConStorage* storage = (RelayConStorage*)data;
    XP_U8 buf[512];
    struct sockaddr_in from;
    socklen_t fromlen = sizeof(from);

    int socket = g_io_channel_unix_get_fd( source );
    XP_LOGF( "%s: calling recvfrom on socket %d", __func__, socket );

    ssize_t nRead = recvfrom( socket, buf, sizeof(buf), 0, /* flags */
                              (struct sockaddr*)&from, &fromlen );

    gchar* b64 = g_base64_encode( (const guchar*)buf,
                                  ((0 <= nRead)? nRead : 0) );
    XP_LOGF( "%s: read %zd bytes ('%s')", __func__, nRead, b64 );
    g_free( b64 );
#ifdef COMMS_CHECKSUM
    gchar* sum = g_compute_checksum_for_data( G_CHECKSUM_MD5, buf, nRead );
    XP_LOGF( "%s: read %zd bytes ('%s')(sum=%s)", __func__, nRead, b64, sum );
    g_free( sum );
#endif
    if ( 0 <= nRead ) {
        const XP_U8* ptr = buf;
        const XP_U8* end = buf + nRead;
        MsgHeader header;
        if ( readHeader( &ptr, &header ) ) {
            sendAckIf( storage, &header );
            switch( header.cmd ) {
            case XWPDEV_REGRSP: {
                uint32_t len;
                if ( !vli2un( &ptr, &len ) ) {
                    assert(0);
                }
                XP_UCHAR devID[len+1];
                getNetString( &ptr, len, devID );
                XP_U16 maxInterval = getNetShort( &ptr );
                XP_LOGF( "%s: maxInterval=%d", __func__, maxInterval );
                (*storage->procs.devIDReceived)( storage->procsClosure, devID,
                                                 maxInterval );
            }
                break;
            case XWPDEV_MSG: {
                CommsAddrRec addr = {0};
                addr_addType( &addr, COMMS_CONN_RELAY );
                (*storage->procs.msgReceived)( storage->procsClosure, &addr,
                                               ptr, end - ptr );
            }
                break;
            case XWPDEV_BADREG:
                (*storage->procs.devIDReceived)( storage->procsClosure, NULL, 0 );
                break;
            case XWPDEV_HAVEMSGS: {
                (*storage->procs.msgNoticeReceived)( storage->procsClosure );
                break;
            }
            case XWPDEV_UNAVAIL: {
#ifdef DEBUG
                XP_U32 unavail = getNetLong( &ptr );
                XP_LOGF( "%s: unavail = %u", __func__, unavail );
#endif
                uint32_t len;
                if ( !vli2un( &ptr, &len ) ) {
                    assert(0);
                }
                XP_UCHAR buf[len+1];
                getNetString( &ptr, len, buf );

                (*storage->procs.msgErrorMsg)( storage->procsClosure, buf );
                break;
            }
            case XWPDEV_ACK: {
                uint32_t packetID;
                if ( !vli2un( &ptr, &packetID ) ) {
                    assert( 0 );
                }
                XP_USE( packetID );
                XP_LOGF( "got ack for packetID %d", packetID );
                break;
            }
            case XWPDEV_ALERT: {
                uint32_t len;
                if ( !vli2un( &ptr, &len ) ) {
                    assert(0);
                }
                XP_UCHAR buf[len + 1];
                getNetString( &ptr, len, buf );
                XP_LOGF( "%s: got message: %s", __func__, buf );
                break;
            }
            case XWPDEV_GOTINVITE: {
                XP_LOGF( "%s(): got XWPDEV_GOTINVITE", __func__ );
#ifdef DEBUG
                XP_U32 sender = 
#endif
                    getNetLong( &ptr );
                XP_U16 len = getNetShort( &ptr );
                XWStreamCtxt* stream = mem_stream_make( MPPARM(storage->params->mpool) 
                                                        storage->params->vtMgr, storage,
                                                        CHANNEL_NONE, NULL );
                stream_putBytes( stream, ptr, len );
                NetLaunchInfo invit;
                XP_Bool success = nli_makeFromStream( &invit, stream );
                XP_LOGF( "sender: %d; invit.devID: %d", sender, invit.devID );
                XP_ASSERT( sender == invit.devID );
                stream_destroy( stream );

                if ( success ) {
                    (*storage->procs.inviteReceived)( storage->procsClosure, 
                                                      &invit );
                }
            }
                break;

            default:
                XP_LOGF( "%s: Unexpected cmd %d", __func__, header.cmd );
                XP_ASSERT( 0 );
            }
        }
    } else {
        XP_LOGF( "%s: error reading udp socket: %d (%s)", __func__, 
                 errno, strerror(errno) );
    }
    return TRUE;
}

void
relaycon_cleanup( LaunchParams* params )
{
    XP_FREEP( params->mpool, &params->relayConStorage );
}

static RelayConStorage* 
getStorage( LaunchParams* params )
{
    XP_ASSERT( params->useUdp );
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
    XP_LOGF( "%s found %x for %s", __func__, ip, name );
    return ip;
}

static ssize_t
sendIt( RelayConStorage* storage, const XP_U8* msgbuf, XP_U16 len )
{
    ssize_t nSent = sendto( storage->socket, msgbuf, len, 0, /* flags */
                            (struct sockaddr*)&storage->saddr, 
                            sizeof(storage->saddr) );
#ifdef COMMS_CHECKSUM
    gchar* sum = g_compute_checksum_for_data( G_CHECKSUM_MD5, msgbuf, len );
    XP_LOGF( "%s: sent %d bytes with sum %s", __func__, len, sum );
    g_free( sum );
#else
    XP_LOGF( "%s()=>%zd", __func__, nSent );
#endif
    return nSent;
}

static size_t
addVLIStr( XP_U8* buf, size_t buflen, const XP_UCHAR* str )
{
    uint32_t len = !!str? strlen( str ) : 0;
    uint8_t nbuf[5];
    size_t nsize = un2vli( len, nbuf );
    if ( nsize + len <= buflen ) {
        memcpy( buf, nbuf, nsize );
        buf += nsize;
        XP_MEMCPY( buf, str, len );
    }
    return nsize + len;
}

static size_t
writeDevID( XP_U8* buf, size_t len, const XP_UCHAR* str )
{
    return addVLIStr( buf, len, str );
}

static size_t
writeShort( XP_U8* buf, size_t len, XP_U16 shrt )
{
    shrt = htons( shrt );
    assert( sizeof( shrt ) <= len );
    XP_MEMCPY( buf, &shrt, sizeof(shrt) );
    return sizeof(shrt);
}

static size_t
writeLong( XP_U8* buf, size_t len, XP_U32 lng )
{
    lng = htonl( lng );
    assert( sizeof( lng ) <= len );
    memcpy( buf, &lng, sizeof(lng) );
    return sizeof(lng);
}

static size_t
writeBytes( XP_U8* buf, size_t len, const XP_U8* bytes, size_t nBytes )
{
    assert( nBytes <= len );
    XP_MEMCPY( buf, bytes, nBytes );
    return nBytes;
}

static size_t
writeVLI( XP_U8* out, uint32_t nn )
{
    uint8_t buf[5];
    size_t numSiz = un2vli( nn, buf );
    memcpy( out, buf, numSiz );
    return numSiz;
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
    dest[indx++] = storage->proto;
    XP_LOGF( "%s: wrote proto %d", __func__, storage->proto );
    uint32_t packetNum = 0;
    if ( XWPDEV_ACK != cmd ) {
        packetNum = storage->nextID++;
    }

    if ( XWPDEV_PROTO_VERSION_1 == storage->proto ) {
        indx += writeVLI( &dest[indx], packetNum );
    } else {
        assert( 0 );
    }

    dest[indx++] = cmd;
    return indx;
}

static bool
readHeader( const XP_U8** buf, MsgHeader* header )
{
    const XP_U8* ptr = *buf;
    bool ok = XWPDEV_PROTO_VERSION_1 == *ptr++;
    assert( ok );

    if ( !vli2un( &ptr, &header->packetID ) ) {
        assert( 0 );
    }
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

static size_t
un2vli( int nn, uint8_t* buf )
{
    int indx = 0;
    bool done = false;
    do {
        uint8_t byt = nn & 0x7F;
        nn >>= 7;
        done = 0 == nn;
        if ( done ) {
            byt |= 0x80;
        }
        buf[indx++] = byt;
    } while ( !done );

    return indx;
}

static bool
vli2un( const uint8_t** inp, uint32_t* outp )
{
    uint32_t result = 0;
    const uint8_t* in = *inp;
    const uint8_t* end = in + 5;

    int count;
    for ( count = 0; in < end; ++count ) {
        unsigned int byt = *in++;
        bool done = 0 != (byt & 0x80);
        if ( done ) {
            byt &= 0x7F;
        } 
        result |= byt << (7 * count);
        if ( done ) {
            break;
        }
    }

    bool success = in < end;
    if ( success ) {
        *inp = in;
        *outp = result;
    }
    return success;
}
