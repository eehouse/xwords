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
// The extreme limitations it has now are meant to be fixed by using stl-based
// structs rather than arrays to track stuff, and by adding mutexes on a
// per-cookie basis.
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
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>


typedef unsigned short HostID;

typedef struct ThreadData {
    int socket;
    HostID srcID;
} ThreadData;

typedef struct SavedBuf {
    unsigned char* buf;
    int   bufLen;
    HostID srcID;
} SavedBuf;

#define MAX_BUFS_SAVES 10
#define ILLEGAL_ID (HostID)0
/* One of these puppies needed per cookie, or one, roughly, per two
   threads. */
#define MAX_HOSTS_PER_COOKIE 4
typedef struct CookieData {
    int nSavedBufs;
    SavedBuf savedBufs[MAX_BUFS_SAVES];

    int nHosts;
    struct {
        HostID id;
        int socket;
    } hostSockets[MAX_HOSTS_PER_COOKIE];
} CookieData;

/* This is too gross in scope!  Need per-cookie mutexes once there are more
   than one cookie on a system a once so removing data for a dying socket in
   one game doesn't lock other games out.*/
pthread_mutex_t gCookieDataMutex = PTHREAD_MUTEX_INITIALIZER;

#define MAX_COOKIES 10
typedef struct CookieHash {
    char* cookie;
    CookieData* ref;
} CookieHash;
static CookieHash gCookieRefs[MAX_COOKIES];
static int gNCookies = 0;

static CookieData*
getCookieRef( const char* cookie )
{
    int i;
    for ( i = 0; i < gNCookies; ++i ) {
        if ( 0 == strcmp( gCookieRefs[i].cookie, cookie ) ) {
            return gCookieRefs[i].ref;
        }
    }
    assert( gNCookies < MAX_COOKIES + 1 );
    CookieData* newRef = new CookieData;
    char* c = new char[strlen(cookie)+1];
    strcpy( c, cookie );
    gCookieRefs[gNCookies].cookie = c;
    gCookieRefs[gNCookies].ref = newRef;
    ++gNCookies;

    newRef->nSavedBufs = 0;
    return newRef;
} /* getCookieRef */

static void
deleteCookieRef( CookieData* cref )
{
    int i;
    int found = 0;

    for ( i = 0; i < cref->nSavedBufs; ++i ) {
        delete [] cref->savedBufs[i].buf;
    }
    
    for ( i = 0; i < gNCookies; ++i ) {
        if ( gCookieRefs[i].ref == cref ) {
            found = 1;

            fprintf( stderr, "removing ref for cookie %s\n", 
                     gCookieRefs[i].cookie );
            delete [] gCookieRefs[i].cookie;

            int nToMove = gNCookies - i - 1;
            if ( nToMove > 0 ) {
                memmove( &gCookieRefs[i].ref, gCookieRefs[i+1].ref,
                         nToMove * sizeof(gCookieRefs[0].ref) );
            }

            break;
        }
    }
    assert( found );

    delete cref;

    --gNCookies;
} /* deleteCookieRef */

static void
setSocketForId( CookieData* cref, HostID id, int socket )
{
    int i;
    for ( i = 0; i < cref->nHosts; ++i ) {
        if ( cref->hostSockets[i].id == id ) {
            cref->hostSockets[i].socket == socket;
            return;
        }
    }

    /* Not found; need to add */
    fprintf( stderr, "adding slot for hostID %x\n", id );
    assert( cref->nHosts < MAX_HOSTS_PER_COOKIE - 1 );
    cref->hostSockets[cref->nHosts].id = id;
    cref->hostSockets[cref->nHosts].socket = socket;
    ++cref->nHosts;
} /* setSocketForId */

static const SavedBuf*
getSavedBufs( const CookieData* cref, int* nBufs )
{
    *nBufs = cref->nSavedBufs;
    return cref->savedBufs;
}

static int
getIds( const CookieData* cref, HostID* ids )
{
    int i;
    for ( i = 0; i < cref->nHosts; ++i ) {
        *ids++ = cref->hostSockets[i].id;
    }
    return cref->nHosts;
} /* getIds */

static void
peekData( unsigned char* buf, char** cookie, HostID* srcIDP, long* connIdP,
          unsigned short* channelNoP, HostID* destIDP )
{
    short cklen = (short)*buf++;
    *cookie = new char[cklen+1];
    memcpy( *cookie, buf, cklen );
    (*cookie)[cklen] = '\0';
    fprintf( stderr, "got cookie %s\n", *cookie );
    buf += cklen;

    long connId;
    HostID id;

    memcpy( &id, buf, 2 );
    *srcIDP = htons( id );
    buf += 2;

    memcpy( &connId, buf, 4 );
    *connIdP = htons( connId );
    buf += 4;

    memcpy( &id, buf, 2 );
    *channelNoP = htons( id );
    buf += 2;

    memcpy( &id, buf, 2 );
    *destIDP = htons( id );
    buf += 2;

    fprintf( stderr, "0x%x %ld %d 0x%x\n",
             *srcIDP, *connIdP, *channelNoP, *destIDP );
}

static int 
socketForHostID( const CookieData* cref, HostID id )
{
    assert( id != ILLEGAL_ID );
    int i;
    for ( i = 0; i < cref->nHosts; ++i ) {
        if ( cref->hostSockets[i].id == id ) {
            return cref->hostSockets[i].socket;
        }
    }
    assert( false );
}

/* Remove all data for a socket being closed down.  No point in remembering
 * its hostID or any buffers it originated.  If it's the last thread with this
 * cookie, remove the cookie ref as well */
static void
removeSocket( CookieData* cref, ThreadData* td )
{
    if ( cref != NULL ) {
        HostID dyingID = td->srcID;
        int i;

        fprintf( stderr, "removing socket %d for host 0x%x\n", 
                 td->socket, dyingID ); 

        pthread_mutex_lock( &gCookieDataMutex );

        /* If this is the last/only ref for this cookie, just nuke the
           cookie ref */
        if ( cref->nHosts == 1 ) {

            assert( cref->hostSockets[0].id == dyingID );
            deleteCookieRef( cref );

        } else {

            /* Remove all bufs */
            int nBufs = cref->nSavedBufs;
            for ( i = nBufs - 1; i >= 0; --i ) {
                SavedBuf* sbuf = &cref->savedBufs[i];
                if ( sbuf->srcID == dyingID ) {
                    delete [] sbuf->buf;
                
                    int nToMove = nBufs - i - 1;
                    assert( nToMove >= 0 );
                    if ( nToMove > 0 ) {
                        memcpy( sbuf, sbuf + 1, 
                                sizeof(*sbuf) * (nBufs - i - 1) );
                    }
                    --nBufs;
                }
            }
            cref->nSavedBufs = nBufs;

            /* remove ref to HostID */
            int nHosts = cref->nHosts;
            for ( i = nHosts - 1; i >= 0; --i ) {
                if ( cref->hostSockets[i].id == dyingID ) {
                    int nToMove = --nHosts - i;
                    if ( nToMove > 0 ) {
                        memmove( &cref->hostSockets[i],
                                 &cref->hostSockets[i+1],
                                 nToMove * sizeof( cref->hostSockets[0] ) );
                    }

                }
            }
            cref->nHosts = nHosts;
        }

        pthread_mutex_unlock( &gCookieDataMutex ) ;
    }
} /* removeSocket */

static void
saveBuffer( CookieData* cref, unsigned char* buf, int bufLen, HostID srcID )
{
    assert( cref->nSavedBufs < (MAX_BUFS_SAVES - 1) );
    SavedBuf* bufs = &cref->savedBufs[cref->nSavedBufs++];
    bufs->buf = buf;
    bufs->bufLen = bufLen;
    bufs->srcID = srcID;
    fprintf( stderr, "%d bufs now saved\n", cref->nSavedBufs );
}

static void
forwardMsg( const CookieData* cref, const void* buf, int bufLen, 
            HostID destID )
{
    int socket = socketForHostID( cref, destID );
    unsigned short len = htons( bufLen );
    ssize_t nSent = send( socket, &len, 2, 0 );
    assert( nSent == 2 );
    fprintf( stderr, "sent len %x (%x)\n", bufLen, len );
    nSent += send( socket, buf, bufLen, 0 );
    fprintf( stderr, "sent %d bytes to host %x on socket %d\n",
             nSent, destID, socket );
}

static void
forwardToAllBut( const CookieData* cref, void* buf, int bufLen, 
                 HostID thisID )
{
    fprintf( stderr, "forwardToAllBut(%x)\n", thisID );
    HostID ids[MAX_HOSTS_PER_COOKIE];
    int nIds = getIds( cref, ids );
    HostID destID;
    int i;
    for ( i = 0; i < nIds; ++i ) {
        if ( ids[i] != thisID ) {
            forwardMsg( cref, buf, bufLen, ids[i] );
        }
    }
}

static void
forwardSavedTo( const CookieData* cref, HostID destID )
{
    fprintf( stderr, "forwardSavedTo(%x)\n", destID );
    int nBufs;
    const SavedBuf* bufs = getSavedBufs( cref, &nBufs );
    while ( nBufs-- ) {
        forwardMsg( cref, bufs->buf, bufs->bufLen, destID );
    }
} /* forwardSavedTo */

static CookieData*
processMessage( unsigned char* buf, int bufLen, ThreadData* ts )
{
    HostID srcID, destID;
    unsigned short channelNo;
    long connId;
    char* cookie;
    peekData( buf, &cookie, &srcID, &connId, &channelNo, &destID );
    ts->srcID = srcID;

    CookieData* cref = getCookieRef( cookie );

    setSocketForId( cref, srcID, ts->socket );

    if ( destID != ILLEGAL_ID ) {
        fprintf( stderr, "JUST FORWARDING: %x -> %x\n", srcID, destID );
        forwardMsg( cref, buf, bufLen, destID );
        /* I'm pretty sure that at this point we can nuke all buffers saved
           for this cookie: the device acting as server now has all the
           addresses it needs. */
    } else {
        pthread_mutex_lock( &gCookieDataMutex );
            
        /* comms not set up yet.  Need to get all existing messages */
        /* to this socket *and* get its message to all that came */
        /* before.  So we'll forward this first, then send it the */
        /* full set, and then add this message to the full set. */

        forwardToAllBut( cref, buf, bufLen, srcID );
        forwardSavedTo( cref, srcID );
        saveBuffer( cref, buf, bufLen, srcID );

        pthread_mutex_unlock( &gCookieDataMutex ) ;
    }
    return cref;
} /* processMessage */

static void*
thread_main( void* arg )
{
    ThreadData localStorage;
    memcpy( &localStorage, arg, sizeof(localStorage) );
    CookieData* cRef = NULL;

    for ( ; ; ) {
        short packetSize;
        assert( sizeof(packetSize) == 2 );

        ssize_t rcvd = recv( localStorage.socket, &packetSize, 
                             sizeof(packetSize), 0 );
        if ( rcvd < 2 ) break;
        packetSize = ntohs( packetSize );
        if ( packetSize < 0 ) break;
        assert( rcvd == 2 );

        unsigned char* buf = new unsigned char[packetSize];
        rcvd = recv( localStorage.socket, buf, packetSize, 0 );
        assert( rcvd == packetSize );
        fprintf( stderr, "read %d bytes\n", rcvd );

        cRef = processMessage( buf, packetSize, &localStorage );
    }
    close( localStorage.socket );

    removeSocket( cRef, &localStorage );

    fprintf( stderr, "exiting thread\n" );
    return NULL;
} /* thread_main */

int main( int argc, char** argv )
{
    int port = 12000;
    int result;

    if ( argc > 1 ) {
        port = atoi( argv[1] );
    }
    /* Open a listening socket.  For each received message, fork a thread into
       which relevant stuff is passed. */

    int listener = socket( AF_INET, SOCK_STREAM, 0 );

    struct sockaddr_in sockAddr;
    sockAddr.sin_family = AF_INET;
    sockAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    sockAddr.sin_port = htons(port);

    result = bind( listener, (struct sockaddr*)&sockAddr, sizeof(sockAddr) );
    if ( result != 0 ) {
        fprintf( stderr, "exiting: unable to bind: %d, errno = %d\n", result, errno );
        exit( 1 );
    }

    result = listen( listener, 5 );
    if ( result != 0 ) {
        fprintf( stderr, "exiting: unable to listen: %d, errno = %d\n", result, errno );
        exit( 1 );
    }

    fprintf( stderr, "listening on port %d, socket %d\n", port, listener );
    for ( ; ; ) {
        sockaddr newaddr;
        socklen_t siz = sizeof(newaddr);
        int newSock = accept( listener, &newaddr, &siz );
        fprintf( stderr, "got one\n" );

        ThreadData td;
        td.socket = newSock;

        pthread_t thread;
        int result = pthread_create( &thread, NULL, thread_main, &td );
    }

    close( listener );
    return 0;
} // main
