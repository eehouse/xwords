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
#include <signal.h>
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
#define MILLIS 1000000

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
} /* getNetShort */

static unsigned short
getNetLong( unsigned char** bufpp )
{
    unsigned long tmp;
    memcpy( &tmp, *bufpp, sizeof(tmp) );
    *bufpp += sizeof(tmp);
    assert( sizeof(tmp) == 4 );
    return ntohl( tmp );
} /* getNetLong */

static void
processHeartbeat( unsigned char* buf, int bufLen, int socket )
{
    CookieID cookieID = getNetLong( &buf );
    HostID hostID = getNetShort( &buf );
    logf( "processHeartbeat: cookieID 0x%lx, hostID 0x%x", cookieID, hostID );
    CookieRef* cref = get_cookieRef( cookieID );
    if ( cref != NULL ) {
        cref->HandleHeartbeat( hostID, socket );
    } else {
        killSocket( socket, "no cref for socket" );
    }
} /* processHeartbeat */

static int
readCookie( unsigned char** bufp, const unsigned char* end, 
            char* outBuf )
{
    unsigned char clen = **bufp;
    ++*bufp;
    if ( *bufp < end && clen > 0 && clen < MAX_COOKIE_LEN ) {
        memcpy( outBuf, *bufp, clen );
        outBuf[clen] = '\0';
        *bufp += clen;
        return 1;
    }
    return 0;
} /* readCookie */

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
static void
processConnect( unsigned char* bufp, int bufLen, int socket )
{
    char cookie[MAX_COOKIE_LEN+1];
    unsigned char* end = bufp + bufLen;

    logf( "processConnect" );

    if ( readCookie( &bufp, end, cookie ) ) {

        HostID srcID;
        CookieID cookieID;

        if ( bufp + sizeof(srcID) + sizeof(cookieID) == end ) {
            srcID = getNetShort( &bufp );
            cookieID = getNetLong( &bufp );

            CookieRef* cref = get_make_cookieRef( cookie, cookieID );
            assert( cref != NULL );
            cref->Connect( socket, srcID );
        }
    }
} /* processConnect */

void
killSocket( int socket, char* why )
{
    logf( "killSocket(%d): %s", socket, why );
    SocketMgr::RemoveSocketRefs( socket );
    /* Might want to kill the thread it belongs to if we're not in it,
       e.g. when unable to write to another socket. */
    logf( "killSocket done" );
}

time_t
now() 
{
    return (unsigned long)time(NULL);
}

/* forward the message.  Need only change the command after looking up the
 * socket and it's ready to go. */
static int
forwardMessage( unsigned char* buf, int buflen, int srcSocket )
{
    int success = 0;
    unsigned char* bufp = buf + 1; /* skip cmd */
    CookieID cookieID = getNetLong( &bufp );
    logf( "cookieID = %d", cookieID );
    CookieRef* cref = get_cookieRef( cookieID );
    if ( cref != NULL ) {
        HostID src = getNetShort( &bufp );
        HostID dest = getNetShort( &bufp );

        cref->Forward( src, dest, buf, buflen );
        success = 1;
    }
    return success;
} /* forwardMessage */

static void
processMessage( unsigned char* buf, int bufLen, int socket )
{
    XWRELAY_Cmd cmd = *buf;
    switch( cmd ) {
    case XWRELAY_CONNECT: 
        logf( "processMessage got XWRELAY_CONNECT" );
        processConnect( buf+1, bufLen-1, socket );
        break;
    case XWRELAY_CONNECTRESP:
        logf( "bad: processMessage got XWRELAY_CONNECTRESP" );
        break;
    case XWRELAY_MSG_FROMRELAY:
        logf( "bad: processMessage got XWRELAY_MSG_FROMRELAY" );
        break;
    case XWRELAY_HEARTBEAT:
        logf( "processMessage got XWRELAY_HEARTBEAT" );
        processHeartbeat( buf + 1, bufLen - 1, socket );
        break;
    case XWRELAY_MSG_TORELAY:
        logf( "processMessage got XWRELAY_MSG_TORELAY" );
        if ( !forwardMessage( buf, bufLen, socket ) ) {
            killSocket( socket, "couldn't forward message" );
        } else {
        }
        break;
    default:
        assert(0);
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

static void
sighandler( int signal )
{
    logf( "sighandler" );

    vector<int> victims;
    CheckHeartbeats( now(), &victims );

    unsigned int i;
    for ( i = 0; i < victims.size(); ++i ) {
        killSocket( victims[i], "heartbeat check failed" );
    }

    logf( "sighandler done" );
} /* sighandler */

int main( int argc, char** argv )
{
    int port = 10999;

    if ( argc > 1 ) {
        port = atoi( argv[1] );
    }
    /* Open a listening socket.  For each received message, fork a thread into
       which relevant stuff is passed. */

    int listener = make_socket( INADDR_ANY, port );
    if ( listener == -1 ) {
        exit( 1 );
    }
    int control = make_socket( INADDR_LOOPBACK, port + 1 );
    if ( control == -1 ) {
        exit( 1 );
    }

    /* generate a signal after n milliseconds, then every m milliseconds  */
    (void)signal( SIGALRM, sighandler );
    (void)ualarm( 2 * HEARTBEAT * MILLIS, 2 * HEARTBEAT* MILLIS );

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
        if ( retval < 0 ) {
            logf( "errno: %d", errno );
        } else {
            if ( FD_ISSET( listener, &rfds ) ) {
                struct sockaddr_in newaddr;
                socklen_t siz = sizeof(newaddr);
                int newSock = accept( listener, (sockaddr*)&newaddr, &siz );

                unsigned long remoteIP = newaddr.sin_addr.s_addr;
                logf( "accepting connection from 0x%lx", ntohl( remoteIP ) );

                tPool->AddSocket( newSock );
                --retval;
            }
            if ( FD_ISSET( control, &rfds ) ) {
                run_ctrl_thread( control );
                --retval;
            }
            assert( retval == 0 );
        }
    }

    close( listener );
    close( control );
    return 0;
} // main
