/* -*- compile-command: "make -j TARGET_OS=win32 DEBUG=TRUE" -*- */
/* 
 * Copyright 2005-2009 by Eric House (xwords@eehouse.org).  All rights
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
#ifndef XWFEATURE_STANDALONE_ONLY

#include <winsock2.h>
#include <stdio.h>

#include "bufqueue.h"

#include "cesockwr.h"
#include "cemain.h"
#include "cedebug.h"
#include "debhacks.h"


/* This object owns all network activity: sending and receiving packets.  It
   maintains two threads, one to send and the other to listen.  Incoming
   packets are passed out via a proc passed into the "constructor".  Outgoing
   packets are passed in directly.  Uses TCP, and the relay framing protocol
   wherein each packet is proceeded by its length in two bytes, network byte
   order.
*/

enum { WRITER_THREAD, 
       READER_THREAD,
       N_THREADS };

typedef enum {
    CE_IPST_START
    ,CE_IPST_RESOLVINGHOST
    ,CE_IPST_HOSTRESOLVED
    ,CE_IPST_CONNECTING
    ,CE_IPST_CONNECTED
} CeConnState;

#define MAX_QUEUE_SIZE 6

struct CeSocketWrapper {
    DataRecvProc dataProc;
    CEAppGlobals* globals;

    union {
        HOSTENT hent;
        XP_U8 hostNameBuf[MAXGETHOSTSTRUCT];
    } hostNameUnion;
    HANDLE getHostTask;

    /* PENDING rewrite this as one sliding buffer */
    /* Outgoing queue */
    XP_U8 bufOut[512];
    BufQueue queueOut;

    /* Incoming */
    char in_buf[512];           /* char is what WSARecv wants */
    XP_U16 in_offset;

    CommsAddrRec addrRec;

    SOCKET socket;
    CeConnState connState;

#ifdef DEBUG
    XP_U16 nSent;
#endif

    MPSLOT
};

#ifdef DEBUG
static const char*
ConnState2Str( CeConnState connState )
{
#define CASESTR(s)   case (s): return #s
    switch( connState ) {
        CASESTR( CE_IPST_START );
        CASESTR( CE_IPST_RESOLVINGHOST );
        CASESTR( CE_IPST_HOSTRESOLVED );
        CASESTR( CE_IPST_CONNECTING );
        CASESTR( CE_IPST_CONNECTED );
    }
#undef CASESTR
    return "<unknown>";
}
#else
# define ConnState2Str(s)
#endif

static XP_Bool connectIfNot( CeSocketWrapper* self );

static XP_Bool
sendAll( CeSocketWrapper* self, const XP_U8* buf, XP_U16 len )
{
    for ( ; ; ) {
        int nSent = send( self->socket, (char*)buf, len, 0 ); /* flags? */
        if ( nSent == SOCKET_ERROR ) {
            return XP_FALSE;
        } else if ( nSent == len ) {
            XP_LOGF( "sent %d bytes", nSent );
            return XP_TRUE;
        } else {
            XP_LOGF( "sent %d bytes", nSent );
            XP_ASSERT( nSent < len );
            len -= nSent;
            buf += nSent;
        }
    }
} /* sendAll */

static XP_Bool
sendLenAndData( CeSocketWrapper* self, const XP_U8* packet, XP_U16 len )
{
    XP_Bool success;
    XP_U16 lenData;
    XP_ASSERT( self->socket != -1 );

    lenData = XP_HTONS( len );
    success = sendAll( self, (XP_U8*)&lenData, sizeof(lenData) )
        && sendAll( self, packet, len );
    return success;
} /* sendLenAndData */

static void
send_packet_if( CeSocketWrapper* self )
{
    const XP_U8* packet;
    XP_U16 len;
    if ( self->socket != -1 && bqGet( &self->queueOut, &packet, &len ) ) {
        if ( sendLenAndData( self, packet, len ) ) {
            /* successful send.  Remove our copy */
            bqRemoveOne( &self->queueOut );
        }
    }
}

static void
stateChanged( CeSocketWrapper* self, CeConnState newState )
{
    CeConnState curState = self->connState;
    self->connState = newState;

    XP_LOGF( "%s: %s -> %s", __func__, ConnState2Str( curState ), 
             ConnState2Str( newState ) );

    switch( newState ) {
    case CE_IPST_START:
        break;
    case CE_IPST_RESOLVINGHOST:
        break;
    case CE_IPST_HOSTRESOLVED:
        connectIfNot( self );
        break;
    case CE_IPST_CONNECTING:
        break;
    case CE_IPST_CONNECTED:
        send_packet_if( self );
        break;
    }
    
}

static XP_Bool
connectSocket( CeSocketWrapper* self )
{
    SOCKET sock;

    if ( self->addrRec.u.ip_relay.ipAddr != 0 ) {
        sock = WSASocket( AF_INET, SOCK_STREAM, IPPROTO_IP, 
                          NULL, 0, WSA_FLAG_OVERLAPPED );
        XP_LOGF( "got socket %d", sock );

        if ( sock != INVALID_SOCKET ) {
            struct sockaddr_in name = {0};

            /* Put socket in non-blocking mode */
            if ( 0 != WSAAsyncSelect( sock, self->globals->hWnd,
                                      XWWM_SOCKET_EVT,
                                      FD_READ | FD_WRITE | FD_CONNECT ) ) {
                XP_WARNF( "WSAAsyncSelect failed" );
            }

            name.sin_family = AF_INET;
            name.sin_port = XP_HTONS( self->addrRec.u.ip_relay.port );
            name.sin_addr.S_un.S_addr = XP_HTONL(self->addrRec.u.ip_relay.ipAddr);

            XP_LOGF( "%s: calling WSAConnect", __func__ );
            if ( SOCKET_ERROR != WSAConnect( sock, (struct sockaddr *)&name, 
                                             sizeof(name), NULL, NULL, 
                                             NULL, NULL ) ) {
                self->socket = sock;
                stateChanged( self, CE_IPST_CONNECTED );
            } else if ( WSAEWOULDBLOCK == WSAGetLastError() ) {
                stateChanged( self, CE_IPST_CONNECTING );
            } else {
                int err = WSAGetLastError();
                XP_LOGF( "%s:%d: WSAGetLastError=>%d", __func__, __LINE__, err );
            }
        } else {
            int err = WSAGetLastError();
            XP_LOGF( "%s:%d: WSAGetLastError=>%d", __func__, __LINE__, err );
        }
    }

    XP_LOGF( "%d", self->connState == CE_IPST_CONNECTED );
    return self->connState == CE_IPST_CONNECTED;
} /* connectSocket */

static XP_Bool
connectIfNot( CeSocketWrapper* self )
{
    LOG_FUNC();
    XP_Bool success = self->connState == CE_IPST_CONNECTED;

    if ( !success && CE_IPST_HOSTRESOLVED == self->connState ) {
        success = connectSocket( self );
    }
    return success;
} /* connectIfNot */

static void
closeConnection( CeSocketWrapper* self )
{
    if ( self->connState >= CE_IPST_CONNECTED ) {
        XP_ASSERT( self->socket != -1 );
        if ( self->socket != -1 ) {
            closesocket( self->socket );
            self->socket = -1;
        }

        stateChanged( self, CE_IPST_START );
    }
} /* closeConnection */

static void
getHostAddr( CeSocketWrapper* self )
{
    if ( self->addrRec.u.ip_relay.hostName[0] ) {
        XP_LOGF( "%s: calling WSAAsyncGetHostByName(%s)", 
                 __func__, self->addrRec.u.ip_relay.hostName );
        self->getHostTask
            = WSAAsyncGetHostByName( self->globals->hWnd,
                                     XWWM_HOSTNAME_ARRIVED,
                                     self->addrRec.u.ip_relay.hostName,
                                     (char*)&self->hostNameUnion,
                                     sizeof(self->hostNameUnion) );
        if ( NULL == self->getHostTask ) {
            int err = WSAGetLastError();
            XP_LOGF( "%s: WSAGetLastError=>%d", __func__, err );
        }

        stateChanged( self, CE_IPST_RESOLVINGHOST );
    }
}

CeSocketWrapper* 
ce_sockwrap_new( MPFORMAL DataRecvProc proc, CEAppGlobals* globals )
{
    CeSocketWrapper* self = NULL;

    self = XP_MALLOC( mpool, sizeof(*self) );
    XP_MEMSET( self, 0, sizeof(*self) );

    self->dataProc = proc;
    self->globals = globals;
    MPASSIGN(self->mpool, mpool );
    self->socket = -1;

    bqInit( &self->queueOut, self->bufOut, sizeof(self->bufOut) );

    getHostAddr( self );
    return self;
} /* ce_sockwrap_new */

void
ce_sockwrap_delete( CeSocketWrapper* self )
{
    /* This isn't a good thing to do.  Better to signal them to exit
       some other way */
    closeConnection( self );

    XP_FREE( self->mpool, self );
} /* ce_sockwrap_delete */

void
ce_sockwrap_hostname( CeSocketWrapper* self, WPARAM wParam, LPARAM lParam )
{
    LOG_FUNC();
    DWORD err = WSAGETASYNCERROR( lParam );

    XP_ASSERT( CE_IPST_RESOLVINGHOST == self->connState );

    if ( 0 == err ) {
        HANDLE comp = (HANDLE)wParam;
        XP_ASSERT( comp == self->getHostTask );

        XP_U32 tmp;
        XP_MEMCPY( &tmp, &self->hostNameUnion.hent.h_addr_list[0][0], 
                   sizeof(tmp) );
        self->addrRec.u.ip_relay.ipAddr = XP_NTOHL( tmp );

        XP_LOGF( "got address: %d.%d.%d.%d", 
                 (int)((tmp>>0) & 0xFF), 
                 (int)((tmp>>8) & 0xFF), 
                 (int)((tmp>>16) & 0xFF), 
                 (int)((tmp>>24) & 0xFF) );

        stateChanged( self, CE_IPST_HOSTRESOLVED );
    } else {
        XP_LOGF( "%s: async operation failed: %ld", __func__, err );
/* WSAENETDOWN */
/* WSAENOBUFS */
/* WSAEFAULT */
/* WSAHOST_NOT_FOUND */
/* WSATRY_AGAIN */
/* WSANO_RECOVERY */
/* WSANO_DATA */
    }

    LOG_RETURN_VOID();
} /* ce_sockwrap_hostname */

static XP_Bool
dispatch_msgs( CeSocketWrapper* self )
{
    XP_Bool draw = XP_FALSE;

    /* Repeat until we don't have a complete message in the buffer */
    for ( ; ; ) {
        XP_U16 lenInBuffer = self->in_offset;
        XP_U16 msgLen;
        XP_U16 lenUsed, lenLeft;

        XP_LOGF( "%s: have %d bytes", __func__, lenInBuffer );

        /* Do we even have the length header? */
        if ( lenInBuffer < sizeof(msgLen) ) {
            break;
        }

        XP_MEMCPY( &msgLen, self->in_buf, sizeof(msgLen) );
        msgLen = XP_NTOHS( msgLen );

        XP_LOGF( "%s: at least we have len: %d", __func__, msgLen );

        /* We know the length of the full buffer.  Do we have it? */
        if ( lenInBuffer < (msgLen + sizeof(msgLen)) ) {
            break;
        }

        /* first send */
        XP_LOGF( "%s: sending %d bytes to dataProc", __func__, msgLen );
        draw = (*self->dataProc)( (XP_U8*)&self->in_buf[sizeof(msgLen)], 
                                  msgLen, self->globals )
            || draw;

        /* then move down any additional bytes */
        lenUsed = msgLen + sizeof(msgLen);
        XP_ASSERT( lenInBuffer >= lenUsed );
        lenLeft = lenInBuffer - lenUsed;
        if ( lenLeft > 0 ) {
            XP_MEMCPY( self->in_buf, &self->in_buf[lenUsed], lenLeft );
        }

        self->in_offset = lenLeft;
    }

    return draw;
} /* dispatch_msgs */

static XP_Bool
read_from_socket( CeSocketWrapper* self )
{
    WSABUF wsabuf;
    DWORD flags = 0;
    DWORD nBytesRecvd = 0;

    wsabuf.buf = &self->in_buf[self->in_offset];
    wsabuf.len = sizeof(self->in_buf) - self->in_offset;

    int err = WSARecv( self->socket, &wsabuf, 1, &nBytesRecvd, 
                       &flags, NULL, NULL );
    XP_ASSERT( nBytesRecvd < 0xFFFF );

    if ( 0 == err ) {
        XP_LOGF( "%s: got %ld bytes", __func__, nBytesRecvd );
        self->in_offset += nBytesRecvd;
    } else {
        XP_ASSERT( err == SOCKET_ERROR );
        err = WSAGetLastError();
        XP_LOGF( "%s: WSARecv=>%d", __func__, err );
    }

    return nBytesRecvd > 0;
} /* read_from_socket */

/* MSDN: When one of the nominated network events occurs on the specified
   socket s, the application window hWnd receives message wMsg. The wParam
   parameter identifies the socket on which a network event has occurred. The
   low word of lParam specifies the network event that has occurred. The high
   word of lParam contains any error code. The error code be any error as
   defined in Winsock2.h.
 */
XP_Bool
ce_sockwrap_event( CeSocketWrapper* self, WPARAM wParam, LPARAM lParam )
{
    SOCKET socket = (SOCKET)wParam;
    long event = (long)LOWORD(lParam);
    XP_Bool draw = XP_FALSE;

    if ( 0 != (FD_WRITE & event) ) {
        send_packet_if( self );
        event &= ~FD_WRITE;
        XP_LOGF( "%s: got FD_WRITE", __func__ );
    }

    if ( 0 != (FD_READ & event) ) {
        XP_LOGF( "%s: got FD_READ", __func__ );
        if ( read_from_socket( self ) ) {
            draw = dispatch_msgs( self );
        }
        event &= ~FD_READ;
    }

    if ( 0 != (FD_CONNECT & event) ) {
        XP_LOGF( "%s: got FD_CONNECT", __func__ );
        event &= ~FD_CONNECT;
        self->socket = socket;
        stateChanged( self, CE_IPST_CONNECTED );
    }

    if ( 0 != event ) {
        XP_WARNF( "%s: unexpected bits left: 0x%lx", __func__, event );
    }
    return draw;
}

XP_S16
ce_sockwrap_send( CeSocketWrapper* self, const XP_U8* buf, XP_U16 len, 
                  const CommsAddrRec* addr )
{
    /* If the address has changed, we need to close the connection.  Send
       thread will take care of opening it again. */
    XP_ASSERT( addr->conType == COMMS_CONN_RELAY );
    if ( 0 != XP_STRCMP( addr->u.ip_relay.hostName, 
                         self->addrRec.u.ip_relay.hostName )
         || 0 != XP_STRCMP( addr->u.ip_relay.cookie, 
                            self->addrRec.u.ip_relay.cookie )
         || addr->u.ip_relay.port != self->addrRec.u.ip_relay.port ) {
        closeConnection( self );
        XP_MEMCPY( &self->addrRec, addr, sizeof(self->addrRec) );

        getHostAddr( self );
    }

    if ( bqAdd( &self->queueOut, buf, len ) ) {
        send_packet_if( self );
    } else {
        XP_WARNF( "dropping packet; queue full" );
        len = -1;                /* error */
    }

    return len;
} /* ce_sockwrap_send */

#endif
