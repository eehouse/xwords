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

#include <string>
#include <map>
#include <assert.h>
#include <pthread.h>

#include "cref.h"
#include "xwrelay.h"
#include "mlock.h"

using namespace std;

static CookieMap gCookieMap;
pthread_mutex_t gCookieMapMutex = PTHREAD_MUTEX_INITIALIZER;

CookieID CookieRef::ms_nextConnectionID = 1000;
static pthread_mutex_t gNextConnIDMutex = PTHREAD_MUTEX_INITIALIZER;


CookieRef* 
get_make_cookieRef( char* cookie )
{
    CookieRef* ref;

    MutexLock ml( &gCookieMapMutex );

    string s(cookie);
    CookieMap::iterator iter = gCookieMap.begin();
    while ( iter != gCookieMap.end() ) {
        ref = iter->second;
        if ( ref->Name() == s ) {
            break;
        }
        ++iter;
    }

    if ( iter != gCookieMap.end() ) {
        logf( "ref found for cookie %s", cookie );
        ref = iter->second;
    } else {
        ref = new CookieRef(s);
        gCookieMap.insert( pair<CookieID, CookieRef*>(ref->GetConnID(), ref ) );
    }

    return ref;
}

CookieRef* 
get_cookieRef( CookieID cookieID )
{
    CookieRef* ref = NULL;
    MutexLock ml( &gCookieMapMutex );

    CookieMap::iterator iter = gCookieMap.find( cookieID);
    while ( iter != gCookieMap.end() ) {
        CookieRef* sec = iter->second;
        if ( sec->GetConnID() == cookieID ) {
            ref = sec;
            break;
        }
        ++iter;
    }
    return ref;
} /* get_cookieRef */

static void
ForgetCref( CookieRef* cref )
{
    MutexLock ml( &gCookieMapMutex );

    CookieMap::iterator iter = gCookieMap.begin();
    while ( iter != gCookieMap.end() ) {
        CookieRef* ref = iter->second;
        if ( ref == cref ) {
            logf( "erasing cref" );
            gCookieMap.erase( iter );
            break;
        }
        ++iter;
    }
    assert( iter != gCookieMap.end() ); /* didn't find it */
}

class SocketStuff {
 public:
    SocketStuff( pthread_t id, CookieRef* cref )
        : m_threadID(id), 
        m_cref(cref)
        {        
            pthread_mutex_init( &m_writeMutex, NULL );
        }
    ~SocketStuff() { pthread_mutex_destroy( &m_writeMutex ); }
    pthread_t m_threadID;
    CookieRef* m_cref;
    pthread_mutex_t m_writeMutex; /* so only one thread writes at a time */
};

typedef map< int, SocketStuff* > SocketMap;
static SocketMap gSocketStuff;
static pthread_mutex_t gSocketStuffMutex = PTHREAD_MUTEX_INITIALIZER;

void
Associate( int socket, CookieRef* cref )
{
    pthread_mutex_lock( &gSocketStuffMutex );
    SocketMap::iterator iter = gSocketStuff.find( socket );
    if ( iter == gSocketStuff.end() ) {
        logf( "replacing existing cref/threadID pair for socket %d", socket );
    }
    
    pthread_t self = pthread_self();
    
    SocketStuff* stuff = new SocketStuff( self, cref );
    gSocketStuff.insert( pair< int, SocketStuff* >( socket, stuff ) );
    pthread_mutex_unlock( &gSocketStuffMutex );
} /* Associate */

static CookieRef*
getCookieRefForSocket( int socket )
{
    MutexLock ml( &gSocketStuffMutex );

    SocketMap::iterator iter = gSocketStuff.find( socket );
    if ( iter != gSocketStuff.end() ) {
        SocketStuff* stuff = iter->second;
        return stuff->m_cref;
    }
    return NULL;
}

pthread_mutex_t*
GetWriteMutexForSocket( int socket )
{
    MutexLock ml( &gSocketStuffMutex );
    SocketMap::iterator iter = gSocketStuff.find( socket );
    if ( iter != gSocketStuff.end() ) {
        SocketStuff* stuff = iter->second;
        return &stuff->m_writeMutex;
    }
    assert( 0 );
}

void
RemoveSocketRefs( int socket )
{
    CookieRef* cref = getCookieRefForSocket( socket );
    if ( cref != NULL ) {

        MutexLock ml( &gSocketStuffMutex );
        SocketMap::iterator iter = gSocketStuff.find( socket );
        assert( iter != gSocketStuff.end() );
        delete iter->second;
        gSocketStuff.erase( iter );

        cref->Remove( socket );
    } else {
        logf( "socket already dead" );
    }
}

/*****************************************************************************
 * CookieRef class
 *****************************************************************************/

CookieRef::CookieRef(string s)
    : m_name(s)
{
    pthread_mutex_init( &m_mutex, NULL );
    MutexLock ml( &gNextConnIDMutex );
    m_connectionID = ms_nextConnectionID++; /* needs a mutex!!! */
}

CookieRef::~CookieRef()
{
    pthread_mutex_destroy( &m_mutex );
    logf( "CookieRef for %d being deleted", m_connectionID );
}

void
CookieRef::Associate( int socket, HostID srcID )
{
    assert( srcID != HOST_ID_NONE );
    logf( "remembering pair: hostid=%x, socket=%d", srcID, socket );
    m_hostSockets.insert( pair<HostID,int>(srcID,socket) );
}

int
CookieRef::SocketForHost( HostID dest )
{
    int socket;
    map<HostID,int>::iterator iter = m_hostSockets.find( dest );
    if ( iter == m_hostSockets.end() ) {
        socket = -1;
    } else {
        socket = iter->second;
        logf( "socketForHost(%x) => %d", dest, socket );
    }
    logf( "returning socket=%d for hostid=%x", socket, dest );
    return socket;
}

void
CookieRef::Remove( int socket )
{
    pthread_mutex_t mutexCopy = m_mutex; /* in case we call delete */
    MutexLock ml( &mutexCopy );

    int count = CountSockets();
    map<HostID,int>::iterator iter = m_hostSockets.begin();
    while ( iter != m_hostSockets.end() ) {
        if ( iter->second == socket ) {
            m_hostSockets.erase(iter);
            --count;
            break;
        }
        ++iter;
    }

    if ( count == 0 ) {
        ForgetCref( this );
        delete this;
    }
}

/* static */ CookieMapIterator
CookieRef::GetCookieNameIterator()
{
    CookieMapIterator iter;
    return iter;
}


CookieMapIterator:: CookieMapIterator()
     : _iter( gCookieMap.begin() )
{
}

const char* 
CookieMapIterator::Next()
{
    const char* str = NULL;
    if ( _iter != gCookieMap.end() ) {
        CookieRef* cref = _iter->second;
        str = cref->Name().c_str();
        ++_iter;
    }
    return str;
}
