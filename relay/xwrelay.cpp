/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */

/* 
 * Copyright 2005 by Eric House (fixin@peak.org).  All rights reserved.
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

//////////////////////////////////////////////////////////////////////////////
//
// This program is a *very rough* cut at a message forwarding server that's
// meant to sit somewhere that cellphones can reach and forward packets across
// connections so that they can communicate.  It exists to work around the
// fact that many cellular carriers prevent direct incoming connections from
// reaching devices on their networks.  It's meant for Crosswords, but might
// be useful for other things.  It also needs a lot of work, and I hacked it
// up before making an exhaustive search for other alternatives.
//
//////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <unistd.h>
#include <netdb.h>		/* gethostbyname */
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>
#include <sys/select.h>
#include <stdarg.h>
#include <sys/time.h>

#include "xwrelay.h"
#include "cref.h"
#include "ctrl.h"
#include "mlock.h"
#include "tpool.h"

#define N_WORKER_THREADS 5

void
logf( const char* format, ... )
{
    FILE* where = stderr;
    struct tm* timp;
    struct timeval tv;
    struct timezone tz;
    gettimeofday( &tv, &tz );
    timp = localtime( &tv.tv_sec );

    pthread_t me = pthread_self();

    fprintf( where, "<%lx>%d:%d:%d: ", me, timp->tm_hour, timp->tm_min, 
             timp->tm_sec );

    va_list ap;
    va_start( ap, format );
    vfprintf( where, format, ap );
    va_end(ap);
    fprintf( where, "\n" );
} /* logf */

static unsigned short
getNetShort( unsigned char** bufpp )
{
    unsigned short tmp;
    memcpy( &tmp, *bufpp, 2 );
    *bufpp += 2;
    return ntohs( tmp );
}

static void
putNetShort( unsigned char** bufpp, unsigned short s )
{
    s = htons( s );
    memcpy( *bufpp, &s, sizeof(s) );
    *bufpp += sizeof(s);
}

static void
processHeartbeat( const unsigned char* buf, int bufLen )
{
} /* processHeartbeat */

/* A CONNECT message from a device gives us the hostID and socket we'll
 * associate with one participant in a relayed session.  We'll store this
 * information with the cookie where other participants can find it when they
 * arrive.
 *
 * What to do if we already have a game going?  In that case the connection ID
 * passed in will be non-zero.  If the device can be associated with an
 * ongoing game, with its new socket, associate it and forward any messages
 * outstanding.  Otherwise close down the socket.  And maybe the others in the
 * game?
 */
static CookieRef*
processConnect( unsigned char* bufp, int bufLen, int socket )
{
    logf( "processConnect" );
    CookieRef* cref = NULL;
    unsigned char* end = bufp + bufLen;
    unsigned char clen = *bufp++;
    if ( bufp < end && clen < MAX_COOKIE_LEN ) {
        char cookie[MAX_COOKIE_LEN+1];
        memcpy( cookie, bufp, clen );
        cookie[clen] = '\0';
        logf( "got cookie: %s", cookie );
        bufp += clen;

        if ( bufp < end ) {
            HostID srcID = getNetShort( &bufp );
            CookieID connID = getNetShort( &bufp );
            if ( bufp == end ) {
                cref = get_make_cookieRef( cookie, connID );
                assert( cref != NULL );
                cref->Associate( socket, srcID );
                SocketMgr::Associate( socket, cref );
            }
        }
    }
    return cref;
} /* processConnect */

void
killSocket( int socket, char* why )
{
    logf( "killSocket(%d): %s", socket, why );
    SocketMgr::RemoveSocketRefs( socket );
    /* Might want to kill the thread it belongs to if we're not in it,
       e.g. when unable to write to another socket. */
}

static void
send_with_length( int socket, unsigned char* buf, int bufLen )
{
    SocketWriteLock slock( socket );
    int ok = 0;
    unsigned short len = htons( bufLen );
    ssize_t nSent = send( socket, &len, 2, 0 );
    if ( nSent == 2 ) {
        nSent = send( socket, buf, bufLen, 0 );
        if ( nSent == bufLen ) {
            logf( "sent %d bytes on socket %d", nSent, socket );
            ok = 1;
        }
    }
    if ( !ok ) {
        killSocket( socket, "couldn't send" );
    }
}

static void
sendConnResp( CookieRef* cref, int socket )
{
    /* send cmd, heartbeat, connid */
    unsigned char buf[5];
    unsigned char* bufp = buf;

    *bufp++ = XWRELAY_CONNECTRESP;
    putNetShort( &bufp, cref->GetHeartbeat() );
    putNetShort( &bufp, cref->GetCookieID() );

    send_with_length( socket, buf, sizeof(buf) );
    cref->RecordSent( sizeof(buf), socket );
    logf( "sent CONNECTIONRSP" );
}

/* forward the message.  Need only change the command after looking up the
 * socket and it's ready to go. */
static int
forwardMessage( unsigned char* buf, int bufLen )
{
    int success = 0;
    unsigned char* bufp = buf + 1; /* skip cmd */
    unsigned short cookieID = getNetShort( &bufp );
    logf( "cookieID = %d", cookieID );
    CookieRef* cref = get_cookieRef( cookieID );
    if ( cref != NULL ) {
        HostID src = getNetShort( &bufp );
        HostID dest = getNetShort( &bufp );
        logf( "forwarding from %x to %x", src, dest );
        int socket = cref->SocketForHost( dest );

        logf( "got socket %d for dest %x", socket, dest );
        if ( socket != -1 ) {
            *buf = XWRELAY_MSG_FROMRELAY;
            send_with_length( socket, buf, bufLen );
            cref->RecordSent( bufLen, socket );
            success = 1;
        } else if ( dest == HOST_ID_SERVER ) {
            logf( "server not connected yet; fail silently" );
            success = 1;
        }
    }
    return success;
} /* forwardMessage */

static void
processMessage( unsigned char* buf, int bufLen, int socket )
{
    CookieRef* cref;

    XWRELAY_Cmd cmd = *buf;
    switch( cmd ) {
    case XWRELAY_CONNECT: 
        logf( "processMessage got XWRELAY_CONNECT" );
        cref = processConnect( buf+1, bufLen-1, socket );
        if ( cref != NULL ) {
            sendConnResp( cref, socket );
        } else {
            killSocket( socket, "no cref found" );
        }
        break;
    case XWRELAY_CONNECTRESP:
        logf( "bad: processMessage got XWRELAY_CONNECTRESP" );
        break;
    case XWRELAY_MSG_FROMRELAY:
        logf( "bad: processMessage got XWRELAY_MSG_FROMRELAY" );
        break;
    case XWRELAY_HEARTBEAT:
        logf( "processMessage got XWRELAY_HEARTBEAT" );
        processHeartbeat( buf + 1, bufLen - 1 );
        break;
    case XWRELAY_MSG_TORELAY:
        logf( "processMessage got XWRELAY_MSG_TORELAY" );
        if ( !forwardMessage( buf, bufLen ) ) {
            killSocket( socket, "couldn't forward message" );
        }
        break;
    }
} /* processMessage */

static int 
make_socket( unsigned long addr, unsigned short port )
{
    int sock = socket( AF_INET, SOCK_STREAM, 0 );

    struct sockaddr_in sockAddr;
    sockAddr.sin_family = AF_INET;
    sockAddr.sin_addr.s_addr = htonl(addr);
    sockAddr.sin_port = htons(port);

    int result = bind( sock, (struct sockaddr*)&sockAddr, sizeof(sockAddr) );
    if ( result != 0 ) {
        logf( "exiting: unable to bind port %d: %d, errno = %d\n", 
              port, result, errno );
        return -1;
    }
    logf( "bound socket %d on port %d", sock, port );

    result = listen( sock, 5 );
    if ( result != 0 ) {
        logf( "exiting: unable to listen: %d, errno = %d\n", result, errno );
        return -1;
    }
    return sock;
} /* make_socket */

int main( int argc, char** argv )
{
    int port = 10999;

    if ( argc > 1 ) {
        port = atoi( argv[1] );
    }
    /* Open a listening socket.  For each received message, fork a thread into
       which relevant stuff is passed. */

    int listener = make_socket( INADDR_ANY, port );
    if ( listener == -1 ) exit( 1 );
    int control = make_socket( INADDR_LOOPBACK, port + 1 );
    if ( control == -1 ) exit( 1 );

    XWThreadPool* tPool = XWThreadPool::GetTPool();
    tPool->Setup( N_WORKER_THREADS, processMessage );

    /* set up select call */
    fd_set rfds;
    for ( ; ; ) {
        FD_ZERO(&rfds);
        FD_SET( listener, &rfds );
        FD_SET( control, &rfds );
        int highest = listener;
        if ( control > listener ) {
            highest = control;
        }
        ++highest;

        int retval = select( highest, &rfds, NULL, NULL, NULL );
        assert( retval > 0 );
        
        if ( FD_ISSET( listener, &rfds ) ) {
            sockaddr newaddr;
            socklen_t siz = sizeof(newaddr);
            int newSock = accept( listener, &newaddr, &siz );
            tPool->AddSocket( newSock );
            --retval;
        }
        if ( FD_ISSET( control, &rfds ) ) {
            run_ctrl_thread( control );
            --retval;
        }
        assert( retval == 0 );
    }

    close( listener );
    close( control );
    return 0;
} // main
