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
#include <getopt.h>
#include <sys/time.h>

#include "xwrelay.h"
#include "crefmgr.h"
#include "ctrl.h"
#include "mlock.h"
#include "tpool.h"
#include "configs.h"
#include "timermgr.h"
#include "permid.h"

#define N_WORKER_THREADS 5
#define MILLIS 1000

void
logf( XW_LogLevel level, const char* format, ... )
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

static int
getNetShort( unsigned char** bufpp, unsigned char* end, unsigned short* out )
{
    int ok = *bufpp + 2 <= end;
    if ( ok ) {
        unsigned short tmp;
        memcpy( &tmp, *bufpp, 2 );
        *bufpp += 2;
        *out = ntohs( tmp );
    }
    return ok;
} /* getNetShort */

static int
getNetByte( unsigned char** bufpp, unsigned char* end, unsigned char* out )
{
    int ok = *bufpp < end;
    if ( ok ) {
        *out = **bufpp;
        ++*bufpp;
    }
    return ok;
} /* getNetByte */

static int
processHeartbeat( unsigned char* buf, int bufLen, int socket )
{
    unsigned char* end = buf + bufLen;
    CookieID cookieID; 
    HostID hostID;
    int success = 0;

    if ( getNetShort( &buf, end, &cookieID )
         && getNetByte( &buf, end, &hostID ) ) {
        logf( XW_LOGINFO, "processHeartbeat: cookieID 0x%lx, hostID 0x%x", 
              cookieID, hostID );

        SafeCref scr( cookieID );
        success = scr.HandleHeartbeat( hostID, socket );
        if ( !success ) {
            killSocket( socket, "no cref for socket" );
        }
    }
    return success;
} /* processHeartbeat */

static int
readStr( unsigned char** bufp, const unsigned char* end, 
         char* outBuf, int bufLen )
{
    unsigned char clen = **bufp;
    ++*bufp;
    if ( ((*bufp + clen) <= end) && (clen < bufLen) ) {
        memcpy( outBuf, *bufp, clen );
        outBuf[clen] = '\0';
        *bufp += clen;
        return 1;
    }
    return 0;
} /* readStr */

static XWREASON
flagsOK( unsigned char flags )
{
    return flags == XWRELAY_PROTO_VERSION ?
        XWRELAY_ERROR_NONE : XWRELAY_ERROR_OLDFLAGS;
} /* flagsOK */

static void
denyConnection( int socket, XWREASON err )
{
    unsigned char buf[2];

    buf[0] = XWRELAY_CONNECTDENIED;
    buf[1] = err;

    send_with_length_unsafe( socket, buf, sizeof(buf) );
}

/* No mutex here.  Caller better be ensuring no other thread can access this
 * socket. */
int
send_with_length_unsafe( int socket, unsigned char* buf, int bufLen )
{
    int ok = 0;
    unsigned short len = htons( bufLen );
    ssize_t nSent = send( socket, &len, 2, 0 );
    if ( nSent == 2 ) {
        nSent = send( socket, buf, bufLen, 0 );
        if ( nSent == bufLen ) {
            logf( XW_LOGINFO, "sent %d bytes on socket %d", nSent, socket );
            ok = 1;
        }
    }
    return ok;
} /* send_with_length_unsafe */


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
static int
processConnect( unsigned char* bufp, int bufLen, int socket )
{
    char cookie[MAX_COOKIE_LEN+1];
    unsigned char* end = bufp + bufLen;
    int success = 0;

    logf( XW_LOGINFO, "processConnect" );

    cookie[0] = '\0';

    unsigned char flags = *bufp++;
    XWREASON err = flagsOK( flags );
    if ( err != XWRELAY_ERROR_NONE ) {
        denyConnection( socket, err );
    } else {
        HostID srcID;
        unsigned char nPlayersH;
        unsigned char nPlayersT;
        if ( readStr( &bufp, end, cookie, sizeof(cookie) ) 
             && getNetByte( &bufp, end, &srcID )
             && getNetByte( &bufp, end, &nPlayersH )
             && getNetByte( &bufp, end, &nPlayersT ) ) {

            SafeCref scr( cookie, 1, srcID, nPlayersH, nPlayersT );
            success = scr.Connect( socket, srcID, nPlayersH, nPlayersT );
        }

        if ( !success ) {
            denyConnection( socket, XWRELAY_ERROR_BADPROTO );
        }
    }
    return success;
} /* processConnect */

static int
processReconnect( unsigned char* bufp, int bufLen, int socket )
{
    unsigned char* end = bufp + bufLen;
    int success = 0;

    logf( XW_LOGINFO, "processReconnect" );

    unsigned char flags = *bufp++;
    XWREASON err = flagsOK( flags );
    if ( err != XWRELAY_ERROR_NONE ) {
        denyConnection( socket, err );
    } else {
        char connName[MAX_CONNNAME_LEN+1];
        HostID srcID;
        unsigned char nPlayersH;
        unsigned char nPlayersT;

        connName[0] = '\0';
        if ( getNetByte( &bufp, end, &srcID )
             && getNetByte( &bufp, end, &nPlayersH )
             && getNetByte( &bufp, end, &nPlayersT )
             && readStr( &bufp, end, connName, sizeof(connName) ) ) {

            SafeCref scr( connName, 0, srcID, nPlayersH, nPlayersT );
            success = scr.Reconnect( socket, srcID, nPlayersH, nPlayersT );
        }

        if ( !success ) {
            denyConnection( socket, XWRELAY_ERROR_BADPROTO );
        }
    }
    return success;
} /* processReconnect */

static int
processDisconnect( unsigned char* bufp, int bufLen, int socket )
{
    unsigned char* end = bufp + bufLen;
    CookieID cookieID;
    HostID hostID;
    int success = 0;

    if ( getNetShort( &bufp, end, &cookieID ) 
         && getNetByte( &bufp, end, &hostID ) ) {

        SafeCref scr( cookieID );
        scr.Disconnect( socket, hostID );
        success = 1;
    } else {
        logf( XW_LOGERROR, "dropping XWRELAY_GAME_DISCONNECT; wrong length" );
    }
    return success;
} /* processDisconnect */

void
killSocket( int socket, char* why )
{
    logf( XW_LOGERROR, "killSocket(%d): %s", socket, why );
    CRefMgr::Get()->RemoveSocketRefs( socket );
    /* Might want to kill the thread it belongs to if we're not in it,
       e.g. when unable to write to another socket. */
    logf( XW_LOGINFO,  "killSocket done" );
    XWThreadPool::GetTPool()->CloseSocket( socket );
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
    unsigned char* end = buf + buflen;
    CookieID cookieID;
    HostID src;
    HostID dest;

    if ( getNetShort( &bufp, end, &cookieID )
         && getNetByte( &bufp, end, &src ) 
         && getNetByte( &bufp, end, &dest ) ) {
        logf( XW_LOGINFO, "cookieID = %d", cookieID );

        SafeCref scr( cookieID );
        success = scr.Forward( src, dest, buf, buflen );
    }
    return success;
} /* forwardMessage */

static int
processMessage( unsigned char* buf, int bufLen, int socket )
{
    int success = 0;            /* default is failure */
    XWRELAY_Cmd cmd = *buf;
    switch( cmd ) {
    case XWRELAY_GAME_CONNECT: 
        logf( XW_LOGINFO, "processMessage got XWRELAY_CONNECT" );
        success = processConnect( buf+1, bufLen-1, socket );
        break;
    case XWRELAY_GAME_RECONNECT: 
        logf( XW_LOGINFO, "processMessage got XWRELAY_RECONNECT" );
        success = processReconnect( buf+1, bufLen-1, socket );
        break;
    case XWRELAY_GAME_DISCONNECT:
        success = processDisconnect( buf+1, bufLen-1, socket );
        break;
    case XWRELAY_HEARTBEAT:
        logf( XW_LOGINFO, "processMessage got XWRELAY_HEARTBEAT" );
        success = processHeartbeat( buf + 1, bufLen - 1, socket );
        break;
    case XWRELAY_MSG_TORELAY:
        logf( XW_LOGINFO, "processMessage got XWRELAY_MSG_TORELAY" );
        success = forwardMessage( buf, bufLen, socket );
        break;
    default:
        logf( XW_LOGINFO, "processMessage bad: %d", cmd );
        break;
        /* just drop it */
    }

    if ( !success ) {
        killSocket( socket, "couldn't forward message" );
    }

    return success;        /* caller defines non-0 as failure */
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
        logf( XW_LOGERROR, "exiting: unable to bind port %d: %d, errno = %d\n", 
              port, result, errno );
        return -1;
    }
    logf( XW_LOGINFO, "bound socket %d on port %d", sock, port );

    result = listen( sock, 5 );
    if ( result != 0 ) {
        logf( XW_LOGERROR, "exiting: unable to listen: %d, errno = %d\n", result, errno );
        return -1;
    }
    return sock;
} /* make_socket */

enum { FLAG_HELP
       ,FLAG_CONFFILE
       ,FLAG_PORT
       ,FLAG_CPORT
       ,FLAG_NTHREADS
       ,FLAG_NAME
};

struct option longopts[] = {
    {
        "help",
        0,
        NULL,
        FLAG_HELP
    }
    ,{
        "conffile",
        1,
        NULL,
        FLAG_CONFFILE
    }
    ,{
        "port",
        1,
        NULL,
        FLAG_PORT
    }
    ,{
        "name",
        1,
        NULL,
        FLAG_NAME
    }
    ,{
        "ctrlport",
        1,
        NULL,
        FLAG_CPORT
    }
    ,{
        "nthreads",
        1,
        NULL,
        FLAG_NTHREADS
    }
};

static void
usage( char* arg0 )
{
    unsigned int i;
    fprintf( stderr, "usage: %s \\\n", arg0 );
    for ( i = 0; i < sizeof(longopts)/sizeof(longopts[0]); ++i ) {
        struct option* opt = &longopts[i];
        fprintf( stderr, "\t--%s", opt->name );
        if ( opt->has_arg ) {
            fprintf( stderr, " <%s>", opt->name );
        }
        fprintf( stderr, "\\\n" );
    }
}


int main( int argc, char** argv )
{
    int port = 0;
    int ctrlport = 0;
    int nWorkerThreads = 0;
    char* conffile = NULL;
    const char* serverName = NULL;

    /* Verify sizes here... */
    assert( sizeof(CookieID) == 2 );
                   

    /* Read options. Options trump config file values when they conflict, but
       the name of the config file is an option so we have to get that
       first. */

    for ( ; ; ) {
       int opt = getopt_long(argc, argv, "hc:p:l:n:",longopts, NULL);

       if ( opt == -1 ) {
           break;
       }

       switch( opt ) {
       case FLAG_HELP:
           usage( argv[0] );
           exit( 0 );
       case FLAG_CONFFILE:
           conffile = optarg;
           break;
       case FLAG_PORT:
           port = atoi( optarg );
           break;
       case FLAG_NAME:
           serverName = optarg;
           break;
       case FLAG_CPORT:
           ctrlport = atoi( optarg );
           break;
       case FLAG_NTHREADS:
           nWorkerThreads = atoi( optarg );
           break;
       default:
           usage( argv[0] );
           exit( 1 );
       }
    }

    RelayConfigs::InitConfigs( conffile );
    RelayConfigs* cfg = RelayConfigs::GetConfigs();

    if ( port == 0 ) {
        port = cfg->GetPort();
    }
    if ( ctrlport == 0 ) {
        ctrlport = cfg->GetCtrlPort();
    }
    if ( nWorkerThreads == 0 ) {
        nWorkerThreads = cfg->GetNWorkerThreads();
    }
    if ( serverName == NULL ) {
        serverName = cfg->GetServerName();
    }

    PermID::SetServerName( serverName );

    int listener = make_socket( INADDR_ANY, port );
    if ( listener == -1 ) {
        exit( 1 );
    }
    int control = make_socket( INADDR_LOOPBACK, ctrlport );
    if ( control == -1 ) {
        exit( 1 );
    }

    XWThreadPool* tPool = XWThreadPool::GetTPool();
    tPool->Setup( nWorkerThreads, processMessage );

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
            if ( errno != 4 ) { /* 4's what we get when signal interrupts */
                logf( XW_LOGINFO, "errno: %d", errno );
            }
        } else {
            if ( FD_ISSET( listener, &rfds ) ) {
                struct sockaddr_in newaddr;
                socklen_t siz = sizeof(newaddr);
                int newSock = accept( listener, (sockaddr*)&newaddr, &siz );

                logf( XW_LOGINFO, "accepting connection from %s", 
                      inet_ntoa(newaddr.sin_addr) );

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

    delete cfg;

    return 0;
} // main
