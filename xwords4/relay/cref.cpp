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
#include "tpool.h"

using namespace std;

static CookieMap gCookieMap;
pthread_rwlock_t gCookieMapRWLock = PTHREAD_RWLOCK_INITIALIZER;


CookieID CookieRef::ms_nextConnectionID = 1000;

/* static */ CookieRef*
CookieRef::AddNew( string s )
{
    RWWriteLock rwl( &gCookieMapRWLock );
    CookieRef* ref = new CookieRef( s );
    gCookieMap.insert( pair<CookieID, CookieRef*>(ref->GetCookieID(), ref ) );
    logf( "paired cookie %s with id %d", s.c_str(), ref->GetCookieID() );
    return ref;
}

/* static */ void
CookieRef::Delete( CookieID id )
{
    CookieRef* cref = get_cookieRef( id );
    if ( cref != NULL ) {
        delete cref;
    }
}

/* static */ void
CookieRef::Delete( const char* name )
{
    CookieID id = CookieIdForName( name );
    Delete( id );
} /* Delete */

CookieID
CookieIdForName( const char* name )
{
    CookieRef* ref = NULL;
    string s(name);

    RWReadLock rwl( &gCookieMapRWLock );

    CookieMap::iterator iter = gCookieMap.begin();
    while ( iter != gCookieMap.end() ) {
        ref = iter->second;
        if ( ref->Name() == s ) {
            ref = iter->second;
            return ref->GetCookieID();
        }
        ++iter;
    }
    return 0;
} /* CookieIdForName */

void
CheckHeartbeats( time_t now, vector<int>* sockets )
{
    logf( "CheckHeartbeats" );
    RWReadLock rwl( &gCookieMapRWLock );
    CookieMap::iterator iter = gCookieMap.begin();
    while ( iter != gCookieMap.end() ) {
        CookieRef* ref = iter->second;
        ref->CheckHeartbeats( now, sockets );
        ++iter;
    }
    logf( "CheckHeartbeats done" );
} /* CheckHeartbeats */

CookieRef* 
get_make_cookieRef( const char* cookie, 
                    CookieID connID ) /* connID ignored for now */
{
    CookieID id = CookieIdForName( cookie );
    CookieRef* cref;
    
    if ( id == 0 ) {
        cref = CookieRef::AddNew( string(cookie) );
    } else {
        cref = get_cookieRef( id );
    }

    return cref;
}

CookieRef* 
get_cookieRef( CookieID cookieID )
{
    CookieRef* ref = NULL;
    RWReadLock rwl( &gCookieMapRWLock );

    CookieMap::iterator iter = gCookieMap.find( cookieID);
    while ( iter != gCookieMap.end() ) {
        CookieRef* sec = iter->second;
        if ( sec->GetCookieID() == cookieID ) {
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
    RWWriteLock ml( &gCookieMapRWLock );

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

SocketMap SocketMgr::ms_SocketStuff;
pthread_mutex_t SocketMgr::ms_SocketStuffMutex = PTHREAD_MUTEX_INITIALIZER;

/* static */ void
SocketMgr::Associate( int socket, CookieRef* cref )
{
    logf( "ms_SocketStuffMutex=%x", &ms_SocketStuffMutex );
    MutexLock ml( &ms_SocketStuffMutex );
    SocketMap::iterator iter = ms_SocketStuff.find( socket );
    if ( iter == ms_SocketStuff.end() ) {
        logf( "replacing existing cref/threadID pair for socket %d", socket );
    }
    
    pthread_t self = pthread_self();
    
    SocketStuff* stuff = new SocketStuff( self, cref );
    ms_SocketStuff.insert( pair< int, SocketStuff* >( socket, stuff ) );
} /* Associate */

/*static*/ CookieRef*
SocketMgr::CookieRefForSocket( int socket )
{
    MutexLock ml( &ms_SocketStuffMutex );

    SocketMap::iterator iter = ms_SocketStuff.find( socket );
    if ( iter != ms_SocketStuff.end() ) {
        SocketStuff* stuff = iter->second;
        return stuff->m_cref;
    }
    return NULL;
}

/* static */ pthread_mutex_t*
SocketMgr::GetWriteMutexForSocket( int socket )
{
    MutexLock ml( &ms_SocketStuffMutex );
    SocketMap::iterator iter = ms_SocketStuff.find( socket );
    if ( iter != ms_SocketStuff.end() ) {
        SocketStuff* stuff = iter->second;
        return &stuff->m_writeMutex;
    }
    assert( 0 );
}

/* static */ void
SocketMgr::RemoveSocketRefs( int socket )
{
    CookieRef* cref = CookieRefForSocket( socket );
    if ( cref != NULL ) {

        MutexLock ml( &ms_SocketStuffMutex );
        SocketMap::iterator iter = ms_SocketStuff.find( socket );
        assert( iter != ms_SocketStuff.end() );
        delete iter->second;
        ms_SocketStuff.erase( iter );

        cref->Remove( socket );
    } else {
        logf( "socket already dead" );
    }
} /* RemoveSocketRefs */

/* static */ void
SocketMgr::PrintSocketInfo( int socket, string& out )
{
    CookieRef* me = SocketMgr::CookieRefForSocket( socket );
    assert( me );

    char buf[64];

    snprintf( buf, sizeof(buf), "* socket: %d\n", socket );
    out += buf;

    snprintf( buf, sizeof(buf), "  in cookie: %s\n", me->Name().c_str() );
    out += buf;
}


/* static */ SocketsIterator 
SocketMgr::MakeSocketsIterator()
{
    SocketsIterator iter( ms_SocketStuff.begin() );
    return iter;
}

/*****************************************************************************
 * SocketsIterator class
 *****************************************************************************/

SocketsIterator::SocketsIterator( SocketMap::iterator iter )
    : m_iter( iter )
{
}

int
SocketsIterator::Next()
{
    int socket = m_iter->first;
    ++m_iter;
    return socket;
}

/*****************************************************************************
 * CookieRef class
 *****************************************************************************/

CookieRef::CookieRef(string s)
    : m_name(s), 
     m_totalSent(0)
{
    pthread_rwlock_init( &m_sockets_rwlock, NULL );
    m_connectionID = ms_nextConnectionID++; /* needs a mutex!!! */
}

CookieRef::~CookieRef()
{
    /* get rid of any sockets still contained */
    XWThreadPool* tPool = XWThreadPool::GetTPool();

    for ( ; ; ) {
        RWWriteLock rwl( &m_sockets_rwlock );
        map<HostID,HostRec>::iterator iter = m_hostSockets.begin();

        if ( iter == m_hostSockets.end() ) {
            break;
        }

        int socket = iter->second.m_socket;
        tPool->CloseSocket( socket );
        m_hostSockets.erase( iter );
    }

    pthread_rwlock_destroy( &m_sockets_rwlock );
    logf( "CookieRef for %d being deleted", m_connectionID );
}

void
CookieRef::Associate( int socket, HostID srcID )
{
    assert( srcID != HOST_ID_NONE );
    logf( "remembering pair: hostid=%x, socket=%d", srcID, socket );
    RWWriteLock ml( &m_sockets_rwlock );
    HostRec hr(socket);
    m_hostSockets.insert( pair<HostID,HostRec>(srcID,hr) );
}

int
CookieRef::SocketForHost( HostID dest )
{
    int socket;
    map<HostID,HostRec>::iterator iter = m_hostSockets.find( dest );
    if ( iter == m_hostSockets.end() ) {
        socket = -1;
    } else {
        socket = iter->second.m_socket;
        logf( "socketForHost(%x) => %d", dest, socket );
    }
    logf( "returning socket=%d for hostid=%x", socket, dest );
    return socket;
}

void
CookieRef::Remove( int socket )
{
    int count;
    {
        RWWriteLock rwl( &m_sockets_rwlock );

        count = CountSockets();
        assert( count > 0 );
        map<HostID,HostRec>::iterator iter = m_hostSockets.begin();
        while ( iter != m_hostSockets.end() ) {
            if ( iter->second.m_socket == socket ) {
                m_hostSockets.erase(iter);
                --count;
                break;
            }
            ++iter;
        }
    }

    if ( count == 0 ) {
        ForgetCref( this );
        delete this;
    }
}

void
CookieRef::HandleHeartbeat( HostID id, int socket )
{
    RWWriteLock rwl( &m_sockets_rwlock );

    map<HostID,HostRec>::iterator iter = m_hostSockets.find(id);
    assert( iter != m_hostSockets.end() );

    /* PENDING If the message came on an unexpected socket, kill the
       connection.  An attack is the most likely explanation. */
    assert( iter->second.m_socket == socket );

    logf( "upping m_lastHeartbeat from %d to %d", 
          iter->second.m_lastHeartbeat, now() );
    iter->second.m_lastHeartbeat = now();
} /* HandleHeartbeat */

void
CookieRef::CheckHeartbeats( time_t now, vector<int>* victims )
{
    logf( "CookieRef::CheckHeartbeats" );

    RWWriteLock rwl( &m_sockets_rwlock );

    map<HostID,HostRec>::iterator iter = m_hostSockets.begin();
    while ( iter != m_hostSockets.end() ) {
        time_t last = iter->second.m_lastHeartbeat;
        if ( (now - last) > HEARTBEAT * 2 ) {
            victims->push_back( iter->second.m_socket );
        }
        ++iter;
    }
    logf( "CookieRef::CheckHeartbeats done" );
} /* CheckHeartbeats */

void
CookieRef::PrintCookieInfo( string& out )
{
    out += "Name: ";
    out += Name();
    out += "\n";
    out += "ID: ";
    char buf[64];
    snprintf( buf, sizeof(buf), "%d\n", GetCookieID() );
    out += buf;

    snprintf( buf, sizeof(buf), "Bytes sent: %d\n", m_totalSent );
    out += buf;

    /* n messages */
    /* n bytes */
    /* open since when */
    /* sockets */

} /* PrintCookieInfo */

/* static */ CookieMapIterator
CookieRef::GetCookieIterator()
{
    CookieMapIterator iter;
    return iter;
}


CookieMapIterator:: CookieMapIterator()
     : _iter( gCookieMap.begin() )
{
}

CookieID
CookieMapIterator::Next()
{
    CookieID id = 0;
    if ( _iter != gCookieMap.end() ) {
        CookieRef* cref = _iter->second;
        id = cref->GetCookieID();
        ++_iter;
    }
    return id;
}
