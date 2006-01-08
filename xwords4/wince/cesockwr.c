/* -*-mode: C; fill-column: 77; c-basic-offset: 4; -*- */
/* 
 * Copyright 2005 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#include "cesockwr.h"
#include "cemain.h"

#include <winsock.h>

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
    CE_IP_NONE,                 /* shouldn't be used */
    CE_IP_UNCONNECTED,
    CE_IP_CONNECTED
} CE_CONNSTATE;

#define MAX_QUEUE_SIZE 3

 struct CeSocketWrapper {
    DataRecvProc dataProc;
    void* dataClosure;

    XP_U8* packets[MAX_QUEUE_SIZE];
    XP_U16 lens[MAX_QUEUE_SIZE];
    XP_U16 nPackets;

    CommsAddrRec addrRec;

    SOCKET socket;
    CE_CONNSTATE connState;

    HANDLE queueAddEvent;
    HANDLE socketConnEvent;

    HANDLE queueMutex;
    HANDLE threads[N_THREADS];

#ifdef DEBUG
    XP_U16 nSent;
#endif

    MPSLOT
};

/* queue_packet: Place packet on queue using semaphore.  Return false
 * if no room or fail for some other reason.
 */
static XP_Bool
queue_packet( CeSocketWrapper* self, XP_U8* packet, XP_U16 len )
{
    DWORD wres;
    XP_Bool success = XP_FALSE;

    // 2/5 second time-out interval.  This is called from the UI thread, so
    // long pauses are unacceptable.  comms will have to try again if for
    // some reason the queue is locked for that long.
    wres = WaitForSingleObject( self->queueMutex, 200L );

    if ( wres == WAIT_OBJECT_0 ) {
        if ( self->nPackets < MAX_QUEUE_SIZE - 1 ) {
            /* add it to the queue */
            self->packets[self->nPackets] = packet;
            self->lens[self->nPackets] = len;
            ++self->nPackets;
            XP_LOGF( "there are now %d packets on send queue", self->nPackets );

            /* signal the writer thread */
            SetEvent( self->queueAddEvent );
            success = XP_TRUE;
        }

        if ( !ReleaseMutex( self->queueMutex ) ) {
            logLastError( "ReleaseMutex" );
        }
    } else {
        XP_LOGF( "timed out" );
    }

    return success;
}

static XP_Bool
get_packet( CeSocketWrapper* self, XP_U8** packet, XP_U16* len )
{
    DWORD wres = WaitForSingleObject( self->queueMutex, INFINITE );
    XP_Bool success = wres == WAIT_OBJECT_0;
    
    if ( success ) {
        success = self->nPackets > 0;
        if ( success ) {
            *packet = self->packets[0];
            *len = self->lens[0];
        }
        if ( !ReleaseMutex( self->queueMutex ) ) {
            logLastError( "ReleaseMutex" );
        }
    }

    return success;
} /* get_packet */

static void
remove_packet( CeSocketWrapper* self )
{
    DWORD wres = WaitForSingleObject( self->queueMutex, INFINITE );
    if ( wres == WAIT_OBJECT_0 ) {
        XP_ASSERT( self->nPackets > 0 );
        if ( --self->nPackets > 0 ) {
            XP_MEMCPY( &self->packets[0], &self->packets[1],
                       self->nPackets * sizeof(self->packets[0]) );
            XP_MEMCPY( &self->lens[0], &self->lens[1],
                       self->nPackets * sizeof(self->lens[0]) );
        } else {
            XP_ASSERT( self->nPackets == 0 );
        }
        if ( !ReleaseMutex( self->queueMutex ) ) {
            logLastError( "ReleaseMutex" );
        }
    }
    XP_LOGF( "%d packets left on queue", self->nPackets );
} /* remove_packet */

static XP_Bool
sendAll( CeSocketWrapper* self, XP_U8* buf, XP_U16 len )
{
    for ( ; ; ) {
        int nSent = send( self->socket, buf, len, 0 ); /* flags? */
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
sendLenAndData( CeSocketWrapper* self, XP_U8* packet, XP_U16 len )
{
    XP_Bool success = XP_FALSE;
    XP_U16 lenData;
    XP_ASSERT( self->socket != -1 );

    lenData = XP_HTONS( len );
    if ( sendAll( self, (XP_U8*)&lenData, sizeof(lenData) ) ) {
        success = sendAll( self, packet, len );
    }
    return success;
} /* sendLenAndData */

static XP_Bool
connectSocket( CeSocketWrapper* self )
{
    SOCKET sock;

    /* first look up the ip address */
    if ( self->addrRec.u.ip_relay.ipAddr == 0 ) {
        struct hostent* ent;
        ent = gethostbyname( self->addrRec.u.ip_relay.hostName );
        if ( ent != NULL ) {
            XP_U32 tmp;
            XP_MEMCPY( &tmp, &ent->h_addr_list[0][0], 
                       sizeof(self->addrRec.u.ip_relay.ipAddr) );
            self->addrRec.u.ip_relay.ipAddr = XP_NTOHL( tmp );
        } else {
            logLastError( "gethostbyname" );
        }
    }

    if ( self->addrRec.u.ip_relay.ipAddr != 0 ) {
        sock = socket( AF_INET, SOCK_STREAM, IPPROTO_IP );
        XP_LOGF( "got socket %d", sock );

        if ( sock != INVALID_SOCKET ) {
            struct sockaddr_in name;

            name.sin_family = AF_INET;
            name.sin_port = XP_HTONS( self->addrRec.u.ip_relay.port );
            name.sin_addr.S_un.S_addr = XP_HTONL(self->addrRec.u.ip_relay.ipAddr);

            if ( SOCKET_ERROR != connect( sock, (struct sockaddr *)&name, 
                                          sizeof(name) ) ) {
                self->connState = CE_IP_CONNECTED;
                self->socket = sock;

                /* Let the reader thread know there's now a socket to listen on */
                SetEvent( self->socketConnEvent );

            } else {
                logLastError( "connect" );
            }
        } else {
            logLastError( "socket" );
        }
    }

    return self->connState == CE_IP_CONNECTED;
} /* connectSocket */

static XP_Bool
connectIfNot( CeSocketWrapper* self )
{
    XP_Bool success = self->connState == CE_IP_CONNECTED;

    if ( !success ) {
        success = connectSocket( self );
    }
    return success;
} /* connectIfNot */

static void
closeConnection( CeSocketWrapper* self )
{
    if ( self->connState >= CE_IP_UNCONNECTED ) {

        if ( self->socket != -1 ) {
            closesocket( self->socket );
        }

        self->socket = -1;
        self->connState = CE_IP_UNCONNECTED;
    }
} /* closeConnection */

static DWORD
WriterThreadProc( LPVOID lpParameter )
{
    CeSocketWrapper* self = (CeSocketWrapper*)lpParameter;

    connectSocket( self );

    /* Then loop waiting for packets to write to it. */
    for ( ; ; ) { 
        XP_U8* packet;
        XP_U16 len;

        WaitForSingleObject( self->queueAddEvent, INFINITE );

        if ( get_packet( self, &packet, &len ) && connectIfNot( self ) ) {
            if ( sendLenAndData( self, packet, len ) ) {

                /* successful send.  Remove our copy */
                remove_packet( self );
                XP_FREE( self->mpool, packet );
            }
        }

        /* Should this happen sooner?  What if other thread signals in the
           meantime? */
        ResetEvent( self->queueAddEvent );
    }

    ExitThread(0);              /* docs say to exit this way */
    return 0;
} /* WriterThreadProc */

/* Read until we get the number of bytes sought or until an error's
   received. */
static XP_Bool
read_bytes_blocking( CeSocketWrapper* self, XP_U8* buf, XP_U16 len )
{
    while ( len > 0 ) {
        fd_set readSet;
        int sres;

        FD_ZERO( &readSet );
        /* There also needs to be a pipe in here for interrupting */
        FD_SET( self->socket, &readSet );

        sres = select( 0,   /* nFds is ignored on wince */
                       &readSet, NULL, NULL, /* others not interesting */
                       NULL ); /* no timeout */
        XP_LOGF( "back from select: got %d", sres );
        if ( sres == 0 ) {
            break;
        } else if ( sres == 1 && FD_ISSET( self->socket, &readSet ) ) {
            int nRead = recv( self->socket, buf, len, 0 );
            if ( nRead > 0 ) {
                XP_LOGF( "read %d bytes", nRead );
                XP_ASSERT( nRead <= len );
                buf += nRead;
                len -= nRead;
            } else {
                break;
            }
        } else {
            XP_ASSERT(0);
            break;
        }
    }

    /* We probably want to close the socket if something's wrong here.  Once
       we get out of sync somehow we'll never get the framing right again. */
    XP_ASSERT( len == 0 );
    return len == 0;
} /* read_bytes_blocking */

static DWORD
ReaderThreadProc( LPVOID lpParameter )
{
    XP_U8 buf[MAX_MSG_LEN];
    CeSocketWrapper* self = (CeSocketWrapper*)lpParameter;

    for ( ; ; ) {
        WaitForSingleObject( self->socketConnEvent, INFINITE );

        for ( ; ; ) {
            XP_U16 len;
            XP_LOGF( "ReaderThreadProc running" );

            /* This will block in select */
            if ( !read_bytes_blocking( self, (XP_U8*)&len, sizeof(len) ) ) {
                break;          /* bad socket.  Go back to waiting new
                                   one. */
            }
            len = XP_NTOHS( len );
            if ( !read_bytes_blocking( self, buf, len ) ) {
                break;          /* bad socket */
            }

            (*self->dataProc)( buf, len, self->dataClosure );
        }
    }

    ExitThread(0);              /* docs say to exit this way */
    return 0;
} /* ReaderThreadProc */


CeSocketWrapper* 
ce_sockwrap_new( MPFORMAL DataRecvProc proc, void* closure )
{
    CeSocketWrapper* self = XP_MALLOC( mpool, sizeof(*self) );
    XP_MEMSET( self, 0, sizeof(*self) );

    self->dataProc = proc;
    self->dataClosure = closure;
    MPASSIGN(self->mpool, mpool );
    self->socket = -1;

    self->queueMutex = CreateMutex( NULL, FALSE, NULL );
    XP_ASSERT( self->queueMutex != NULL );

    self->queueAddEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
    self->socketConnEvent = CreateEvent( NULL, FALSE, FALSE, NULL );
    

    self->threads[WRITER_THREAD] = CreateThread( NULL, 0, WriterThreadProc, 
                                                 self, 0, NULL );
    self->threads[READER_THREAD] = CreateThread( NULL, 0, ReaderThreadProc,
                                                 self, 0, NULL );
    return self;
} /* ce_sockwrap_new */

void
ce_sockwrap_delete( CeSocketWrapper* self )
{
    /* This isn't a good thing to do.  Better to signal them to exit
       some other way */
    TerminateThread( self->threads[WRITER_THREAD], 0 );
    TerminateThread( self->threads[READER_THREAD], 0 );
    
    WaitForMultipleObjects( N_THREADS, self->threads, TRUE, INFINITE );

    closeConnection( self );

    CloseHandle( self->threads[WRITER_THREAD] );
    CloseHandle( self->threads[READER_THREAD] );
    CloseHandle( self->queueMutex );

    CloseHandle( self->queueAddEvent );
    CloseHandle( self->socketConnEvent );

    XP_FREE( self->mpool, self );
} /* ce_sockwrap_delete */

XP_U16
ce_sockwrap_send( CeSocketWrapper* self, XP_U8* buf, XP_U16 len, 
                  const CommsAddrRec* addr )
{
    XP_U8* packet;

    /* If the address has changed, we need to close the connection.  Send
       thread will take care of opening it again. */
    XP_ASSERT( addr->conType == COMMS_CONN_RELAY );
    if ( 0 != XP_STRCMP( addr->u.ip_relay.hostName, self->addrRec.u.ip_relay.hostName )
         || 0 != XP_STRCMP( addr->u.ip_relay.cookie, self->addrRec.u.ip_relay.cookie )
         || addr->u.ip_relay.port != self->addrRec.u.ip_relay.port ) {
        closeConnection( self );
        XP_MEMCPY( &self->addrRec, addr, sizeof(self->addrRec) );
    }

    packet = XP_MALLOC( self->mpool, len );
    XP_MEMCPY( packet, buf, len );
    if ( !queue_packet( self, packet, len ) ) {
        len = 0;                /* error */
    }

    return len;
} /* ce_sockwrap_send */

#endif
