/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */

/* 
 * Copyright 2005-2009 by Eric House (xwords@eehouse.org).  All rights
 * reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option.
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

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>

#include "crefmgr.h"
#include "cref.h"
#include "mlock.h"
#include "permid.h"
#include "configs.h"
#include "timermgr.h"

class SocketStuff {
 public:
    SocketStuff( CookieRef* cref )
        : m_cref(cref)
        {        
            pthread_mutex_init( &m_writeMutex, NULL );
        }
    ~SocketStuff() { pthread_mutex_destroy( &m_writeMutex ); }
    CookieRef* m_cref;
    pthread_mutex_t m_writeMutex; /* so only one thread writes at a time */
};

static CRefMgr* s_instance = NULL;

/* static */ CRefMgr*
CRefMgr::Get() 
{
    if ( s_instance == NULL ) {
        s_instance = new CRefMgr();
    }
    return s_instance;
} /* Get */

CRefMgr::CRefMgr()
    : m_nextCID(0)
    , m_startTime(time(NULL))
{
    /* should be using pthread_once() here */
    pthread_mutex_init( &m_SocketStuffMutex, NULL );
    pthread_mutex_init( &m_freeList_mutex, NULL );
    pthread_rwlock_init( &m_cookieMapRWLock, NULL );
}

CRefMgr::~CRefMgr()
{
    assert( this == s_instance );

    pthread_mutex_destroy( &m_freeList_mutex );
    pthread_rwlock_destroy( &m_cookieMapRWLock );

    SocketMap::iterator iter;
    for ( iter = m_SocketStuff.begin(); iter != m_SocketStuff.end(); ++iter ) {
        SocketStuff* stuff = iter->second;
        delete stuff;
    }

    s_instance = NULL;
}

void
CRefMgr::CloseAll()
{
    /* Get every cref instance, shut it down */

    for ( ; ; ) {
        CookieRef* cref = NULL;
        {
            RWWriteLock rwl( &m_cookieMapRWLock );
            CookieMap::iterator iter = m_cookieMap.begin();
            if ( iter == m_cookieMap.end() ) {
                break;
            }
            cref = iter->second; 
            {
                SafeCref scr( cref ); /* cref */
                scr.Shutdown();
            }
        }
    }
} /* CloseAll */

CookieRef*
CRefMgr::FindOpenGameFor( const char* cORn, bool isCookie,
                          HostID hid, int socket, int nPlayersH, int nPlayersT )
{
    logf( XW_LOGINFO, "%s(cORn=%s,hid=%d,socket=%d)", __func__, cORn, hid, 
          socket );
    CookieRef* cref = NULL;
    RWReadLock rwl( &m_cookieMapRWLock );

    CookieMap::iterator iter = m_cookieMap.begin();
    while ( iter != m_cookieMap.end() ) {
        cref = iter->second;
        if ( isCookie ) {
            if ( 0 == strcmp( cref->Cookie(), cORn ) ) {
                if ( cref->NeverFullyConnected() ) {
                    break;
                } else if ( cref->HasSocket(socket) ) {
                    logf( XW_LOGINFO, "%s: HasSocket case", __func__ );
                    break;
                }
            }
        } else {
            if ( 0 == strcmp( cref->ConnName(), cORn ) ) {
                bool found = false;
                if ( cref->Lock() ) {
                    found = cref->AcceptingReconnections( hid, nPlayersH, 
                                                          nPlayersH );
                    cref->Unlock();
                }
                if ( found ) {
                    break;
                }
            }
        }
        ++iter;
    }

    return (iter == m_cookieMap.end()) ? NULL : cref;
} /* FindOpenGameFor */

CookieID
CRefMgr::nextCID( const char* connName )
{
    /* Later may want to guarantee that wrap-around doesn't cause an overlap.
       But that's really only a theoretical possibility. */
    return ++m_nextCID;
} /* nextCID */

int 
CRefMgr::GetNumGamesSeen( void )
{
    return m_nextCID;
}


int 
CRefMgr::GetSize( void )
{
    return m_cookieMap.size();
}

CookieID
CRefMgr::cookieIDForConnName( const char* connName )
{
    CookieID cid = 0;
    /* for now, just walk the existing data structure and see if the thing's
       in use.  If it isn't, return a new id. */

    RWReadLock rwl( &m_cookieMapRWLock );

    CookieMap::iterator iter = m_cookieMap.begin();
    while ( iter != m_cookieMap.end() ) {
        CookieRef* cref = iter->second;
        if ( 0 == strcmp( cref->ConnName(), connName ) ) {
            cid = iter->first;
            break;
        }
        ++iter;
    }

    return cid;
} /* cookieIDForConnName */

void
CRefMgr::addToFreeList( CookieRef* cref )
{
    MutexLock ml( &m_freeList_mutex );
    m_freeList.push_back( cref );
}

CookieRef*
CRefMgr::getFromFreeList( void )
{
    CookieRef* cref = NULL;
    MutexLock ml( &m_freeList_mutex );
    if ( m_freeList.size() > 0 ) {
        cref = m_freeList.front();
        m_freeList.pop_front();
    }
    return cref;
}


CookieRef*
CRefMgr::getMakeCookieRef_locked( const char* cORn, bool isCookie, HostID hid,
                                  int socket, int nPlayersH, int nPlayersT )
{
    CookieRef* cref;

    /* We have a cookie from a new connection.  This may be the first time
       it's been seen, or there may be a game currently in the
       XW_ST_CONNECTING state, or it may be a dupe of a connect packet.  So we
       need to look up the cookie first, then generate new connName and
       cookieIDs if it's not found. */

    cref = FindOpenGameFor( cORn, isCookie, hid, socket, nPlayersH, nPlayersT );
    if ( cref == NULL ) {
        string s;
        const char* connName;
        const char* cookie = NULL;
        if ( isCookie ) {
            cookie = cORn;
            s = PermID::GetNextUniqueID();
            connName = s.c_str();
        } else {
            connName = cORn;
        }

        CookieID cid = cookieIDForConnName( connName );
        if ( cid == 0 ) {
            cid = nextCID( connName );
        }

        cref = AddNew( cookie, connName, cid );
    }

    return cref;
} /* getMakeCookieRef_locked */

bool
CRefMgr::Associate( int socket, CookieRef* cref )
{
    bool isNew = false;
    MutexLock ml( &m_SocketStuffMutex );
    SocketMap::iterator iter = m_SocketStuff.find( socket );
    /* This isn't enough.  Must provide a way to reuse sockets should a
       genuinely different connection appear.  Now maybe we already remove
       this reference when a socket is closed.  Test this!  Or assert
       something here.  Bottom line: need to swallow repeated/duplicate
       connect messages from same host. */
    if ( iter == m_SocketStuff.end() ) {
        SocketStuff* stuff = new SocketStuff( cref );
        m_SocketStuff.insert( pair< int, SocketStuff* >( socket, stuff ) );
        isNew = true;
    } else {
        logf( XW_LOGERROR, "Already have cref/threadID pair for socket %d; "
              "error???", socket );
    }
    return isNew;
}

void 
CRefMgr::Disassociate( int socket, CookieRef* cref )
{
    MutexLock ml( &m_SocketStuffMutex );
    SocketMap::iterator iter = m_SocketStuff.find( socket );
    if ( iter == m_SocketStuff.end() ) {
        logf( XW_LOGERROR, "can't find SocketStuff for socket %d", socket );
    } else {
        SocketStuff* stuff = iter->second;
        assert( cref == NULL || stuff->m_cref == cref );
        delete stuff;
        m_SocketStuff.erase( iter );
    }
}

#if 0
pthread_mutex_t* 
CRefMgr::GetWriteMutexForSocket( int socket )
{
    pthread_mutex_t* mutex = NULL;
    MutexLock ml( &m_SocketStuffMutex );
    SocketMap::iterator iter = m_SocketStuff.find( socket );
    if ( iter != m_SocketStuff.end() ) {
        SocketStuff* stuff = iter->second;
        /* this is dangerous!  What if we want to nuke SocketStuff while this
           is locked?  And shouldn't it be the case that only one thread at a
           time can be trying to write to one of a given cref's sockets since
           only one thread at a time is handling a cref? */
        mutex = &stuff->m_writeMutex;
    }
    logf( XW_LOGERROR, "GetWriteMutexForSocket: not found" );
    return mutex;
} /* GetWriteMutexForSocket */
#endif

void 
CRefMgr::RemoveSocketRefs( int socket )
{
    {    
        SafeCref scr( socket );
        scr.Remove( socket );
    }

    Disassociate( socket, NULL );
}

void
CRefMgr::PrintSocketInfo( int socket, string& out )
{
    SafeCref scr( socket );
    const char* name = scr.Cookie();
    if ( name != NULL && name[0] != '\0' ) {
        char buf[64];

        snprintf( buf, sizeof(buf), "* socket: %d\n", socket );
        out += buf;

        snprintf( buf, sizeof(buf), "  in cookie: %s\n", name );
        out += buf;
    }
}

/* static */ SocketsIterator 
CRefMgr::MakeSocketsIterator()
{
    pthread_mutex_lock( &m_SocketStuffMutex );
    SocketsIterator iter( m_SocketStuff.begin(), m_SocketStuff.end(), 
                          &m_SocketStuffMutex );
    return iter;
}

CookieRef* 
CRefMgr::getCookieRef( CookieID cookieID )
{
    return getCookieRef_impl( cookieID );
} /* getCookieRef */

CookieRef* 
CRefMgr::getCookieRef( int socket )
{
    CookieRef* cref = NULL;
    MutexLock ml( &m_SocketStuffMutex );
    SocketMap::iterator iter = m_SocketStuff.find( socket );
    if ( iter != m_SocketStuff.end() ) {
        SocketStuff* stuff = iter->second;
        cref = stuff->m_cref;
    }

    return cref;
} /* getCookieRef */

#ifdef RELAY_HEARTBEAT
/* static */ void
CRefMgr::heartbeatProc( void* closure )
{
    CRefMgr* self = (CRefMgr*)closure;
    self->checkHeartbeats( ::uptime() );
} /* heartbeatProc */
#endif

CookieRef*
CRefMgr::AddNew( const char* cookie, const char* connName, CookieID id )
{
    logf( XW_LOGINFO, "%s( cookie=%s, connName=%s, id=%d", __func__,
          cookie, connName, id );
    CookieRef* exists = getCookieRef_impl( id );
    assert( exists == NULL );   /* failed once */

    CookieRef* ref = getFromFreeList();

    RWWriteLock rwl( &m_cookieMapRWLock );
    logf( XW_LOGINFO, "making new cref: %d", id );
    
    if ( !!ref ) {
        logf( XW_LOGINFO, "using from free list" );
        ref->ReInit( cookie, connName, id );
    } else {
        logf( XW_LOGINFO, "calling constructor" );
        ref = new CookieRef( cookie, connName, id );
    }

    m_cookieMap.insert( pair<CookieID, CookieRef*>(ref->GetCookieID(), ref ) );
    logf( XW_LOGINFO, "paired cookie %s/connName %s with id %d", 
          (cookie?cookie:"NULL"), connName, ref->GetCookieID() );

#ifdef RELAY_HEARTBEAT
    if ( m_cookieMap.size() == 1 ) {
        RelayConfigs* cfg = RelayConfigs::GetConfigs();
        int heartbeat;
        cfg->GetValueFor( "HEARTBEAT", &heartbeat );
        TimerMgr::GetTimerMgr()->SetTimer( heartbeat, heartbeatProc, this,
                                           heartbeat );
    }
#endif

    return ref;
} /* AddNew */

void
CRefMgr::Recycle_locked( CookieRef* cref )
{
    logf( XW_LOGINFO, "%s(cref=%p,cookie=%s)", __func__, cref, cref->Cookie() );
    CookieID id = cref->GetCookieID();
    cref->Clear();
    addToFreeList( cref );

    cref->Unlock();

    /* don't grab this lock until after releasing cref's lock; otherwise
       deadlock happens. */
    RWWriteLock rwl( &m_cookieMapRWLock );

    CookieMap::iterator iter = m_cookieMap.begin();
    while ( iter != m_cookieMap.end() ) {
        CookieRef* ref = iter->second;
        if ( ref == cref ) {
            logf( XW_LOGINFO, "%s: erasing cref cid %d", __func__, id );
            m_cookieMap.erase( iter );
            break;
        }
        ++iter;
    }


#ifdef RELAY_HEARTBEAT
    if ( m_cookieMap.size() == 0 ) {
        TimerMgr::GetTimerMgr()->ClearTimer( heartbeatProc, this );
    }
#endif
} /* CRefMgr::Recycle */

void
CRefMgr::Recycle( CookieID id )
{
    CookieRef* cref = getCookieRef( id );
    if ( cref != NULL ) {
        cref->Lock();
        Recycle_locked( cref );
    }
} /* Delete */

void
CRefMgr::Recycle( const char* connName )
{
    CookieID id = cookieIDForConnName( connName );
    Recycle( id );
} /* Delete */

CookieRef* 
CRefMgr::getCookieRef_impl( CookieID cookieID )
{
    CookieRef* ref = NULL;
    RWReadLock rwl( &m_cookieMapRWLock );

    CookieMap::iterator iter = m_cookieMap.find( cookieID );
    while ( iter != m_cookieMap.end() ) {
        CookieRef* second = iter->second;
        if ( second->GetCookieID() == cookieID ) {
            ref = second;
            break;
        }
        ++iter;
    }
    return ref;
}

#ifdef RELAY_HEARTBEAT
void
CRefMgr::checkHeartbeats( time_t now )
{
    vector<CookieRef*> crefs;

    {
        RWReadLock rwl( &m_cookieMapRWLock );
        CookieMap::iterator iter = m_cookieMap.begin();
        while ( iter != m_cookieMap.end() ) {
            crefs.push_back(iter->second);
            ++iter;
        }
    }

    unsigned int ii;
    for ( ii = 0; ii < crefs.size(); ++ii ) {
        SafeCref scr( crefs[ii] );
        scr.CheckHeartbeats( now );
    }
} /* checkHeartbeats */
#endif

time_t
CRefMgr::uptime( void )
{
    return time(NULL) - m_startTime;
}

/* static */ CookieMapIterator
CRefMgr::GetCookieIterator()
{
    CookieMapIterator iter(&m_cookieMapRWLock);
    return iter;
}


CookieMapIterator::CookieMapIterator(pthread_rwlock_t* rwlock)
    : m_rwl( rwlock )
    ,_iter( CRefMgr::Get()->m_cookieMap.begin() )
{
}

CookieID
CookieMapIterator::Next()
{
    CookieID id = 0;
    if ( _iter != CRefMgr::Get()->m_cookieMap.end() ) {
        CookieRef* cref = _iter->second;
        id = cref->GetCookieID();
        ++_iter;
    }
    return id;
}

//////////////////////////////////////////////////////////////////////////////
// SafeCref
//////////////////////////////////////////////////////////////////////////////

SafeCref::SafeCref( const char* cORn, bool isCookie, HostID hid, int socket,
                    int nPlayersH, int nPlayersT )
    : m_cref( NULL )
    , m_mgr( CRefMgr::Get() )
    , m_isValid( false )
{
    CookieRef* cref;

    cref = m_mgr->getMakeCookieRef_locked( cORn, isCookie, hid, socket,
                                           nPlayersH, nPlayersT );
    if ( cref != NULL ) {
        m_locked = cref->Lock();
        m_cref = cref;
        m_isValid = true;
    }
}

SafeCref::SafeCref( CookieID connID, bool failOk )
    : m_cref( NULL )
    , m_mgr( CRefMgr::Get() )
    , m_isValid( false )
{
    CookieRef* cref = m_mgr->getCookieRef( connID );
    if ( cref != NULL ) {       /* known cookie? */
        m_locked = cref->Lock();
        m_isValid = m_locked && connID == cref->GetCookieID();
        m_cref = cref;
    }
}

SafeCref::SafeCref( int socket )
    : m_cref( NULL )
    , m_mgr( CRefMgr::Get() )
    , m_isValid( false )
{
    CookieRef* cref = m_mgr->getCookieRef( socket );
    if ( cref != NULL ) {       /* known socket? */
        m_locked = cref->Lock();
        m_isValid = m_locked && cref->HasSocket_locked( socket );
        m_cref = cref;
    }
}

SafeCref::SafeCref( CookieRef* cref )
    : m_cref( NULL )
    , m_mgr( CRefMgr::Get() )
    , m_isValid( false )
{
    m_locked = cref->Lock();
    m_isValid = m_locked;
    m_cref = cref;
}

SafeCref::~SafeCref()
{
    if ( m_cref != NULL && m_locked ) {
        if ( m_cref->ShouldDie() ) {
            m_mgr->Recycle_locked( m_cref );
        } else {
            m_cref->Unlock();
        }
    }
}
