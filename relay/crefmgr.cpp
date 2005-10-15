/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */

/* 
 * Copyright 2005 by Eric House (fixin@peak.org).  All rights reserved.
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

/* static */ CRefMgr*
CRefMgr::Get() 
{
    static CRefMgr* instance = NULL;
    if ( instance == NULL ) {
        instance = new CRefMgr();
    }
    return instance;
} /* Get */

CRefMgr::CRefMgr()
    : m_nextCID(0)
{
    /* should be using pthread_once() here */
    pthread_mutex_init( &m_guard, NULL );
    pthread_rwlock_init( &m_cookieMapRWLock, NULL );
}

CRefMgr::~CRefMgr()
{
}

CookieRef*
CRefMgr::FindOpenGameFor( const char* cORn, int isCookie,
                          HostID hid, int nPlayersH, int nPlayersT )
{
    logf( XW_LOGINFO, "FindOpenGameFor with %s", cORn );
    CookieRef* cref = NULL;
    RWReadLock rwl( &m_cookieMapRWLock );

    CookieMap::iterator iter = m_cookieMap.begin();
    while ( iter != m_cookieMap.end() ) {
        cref = iter->second;
        if ( isCookie ) {
            if ( 0 == strcmp( cref->Name(), cORn ) ) {
                if ( cref->NeverFullyConnected() ) {
                    break;
                }
            }
        } else {
            if ( 0 == strcmp( cref->ConnName(), cORn ) ) {
                if ( cref->AcceptingReconnections( hid, 
                                                   nPlayersH, nPlayersH ) ) {
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

CookieRef*
CRefMgr::getMakeCookieRef_locked( const char* cORn, int isCookie, HostID hid,
                                  int nPlayersH, int nPlayersT )
{
    CookieRef* cref;

    pthread_mutex_lock( &m_guard );

    /* We have a cookie from a new connection.  This may be the first time
       it's been seen, or there may be a game currently in the
       XW_ST_CONNECTING state.  So we need to look up the cookie first, then
       generate new connName and cookieIDs if it's not found. */

    cref = FindOpenGameFor( cORn, isCookie, hid, nPlayersH, nPlayersT );
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

    if ( cref == NULL ) {
        pthread_mutex_unlock( &m_guard );
    }

    return cref;
} /* getMakeCookieRef_locked */

void
CRefMgr::Associate( int socket, CookieRef* cref )
{
    MutexLock ml( &m_SocketStuffMutex );
    SocketMap::iterator iter = m_SocketStuff.find( socket );
    if ( iter != m_SocketStuff.end() ) {
        logf( XW_LOGINFO, "replacing existing cref/threadID pair for socket %d", socket );
    }
    
    SocketStuff* stuff = new SocketStuff( cref );
    m_SocketStuff.insert( pair< int, SocketStuff* >( socket, stuff ) );
}

void 
CRefMgr::Disassociate( int socket, CookieRef* cref )
{
    MutexLock ml( &m_SocketStuffMutex );
    SocketMap::iterator iter = m_SocketStuff.find( socket );
    if ( iter == m_SocketStuff.end() ) {
        logf( XW_LOGERROR, "can't find cref/threadID pair for socket %d", socket );
    } else {
        SocketStuff* stuff = iter->second;
        assert( stuff->m_cref == cref );
        delete stuff;
        m_SocketStuff.erase( iter );
    }
}

pthread_mutex_t* 
CRefMgr::GetWriteMutexForSocket( int socket )
{
    MutexLock ml( &m_SocketStuffMutex );
    SocketMap::iterator iter = m_SocketStuff.find( socket );
    if ( iter != m_SocketStuff.end() ) {
        SocketStuff* stuff = iter->second;
        return &stuff->m_writeMutex;
    }
    logf( XW_LOGERROR, "GetWriteMutexForSocket: not found" );
    return NULL;
} /* GetWriteMutexForSocket */

void 
CRefMgr::RemoveSocketRefs( int socket )
{
    SafeCref scr( socket );
    scr.Remove( socket );
}

void
CRefMgr::PrintSocketInfo( int socket, string& out )
{
    SafeCref scr( socket );
    const char* name = scr.Name();
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
CRefMgr::getCookieRef_locked( CookieID cookieID )
{
    pthread_mutex_lock( &m_guard );

    CookieRef* cref = getCookieRef_impl( cookieID );

    if ( cref == NULL ) {
        pthread_mutex_unlock( &m_guard );
    }

    return cref;
} /* getCookieRef_locked */

CookieRef* 
CRefMgr::getCookieRef_locked( int socket )
{
    pthread_mutex_lock( &m_guard );
    CookieRef* cref = NULL;

    CookieMap::iterator iter = m_cookieMap.begin();
    while ( iter != m_cookieMap.end() ) {
        CookieRef* tmp = iter->second;
        if ( tmp->HasSocket( socket ) ) {
            cref = tmp;
            break;
        }
        ++iter;
    }

    if ( cref == NULL ) {
        pthread_mutex_unlock( &m_guard );
    }

    return cref;
} /* getCookieRef_locked */

int
CRefMgr::checkCookieRef_locked( CookieRef* cref )
{
    int exists = 1;
    assert( cref != NULL );
    pthread_mutex_lock( &m_guard );

    pthread_mutex_t* cref_mutex = m_crefMutexes[cref];
    logf( XW_LOGINFO, "checkCookieRef_locked: cref_mutex=%p", cref_mutex );

    if ( cref_mutex == NULL ) {
        pthread_mutex_unlock( &m_guard );
        exists = 0;
    }

    return exists;
} /* checkCookieRef_locked */

int
CRefMgr::LockCref( CookieRef* cref )
{
    /* assertion: m_guard is locked */

    pthread_mutex_t* cref_mutex = m_crefMutexes[cref];
    if ( cref_mutex == NULL ) {
        cref_mutex = (pthread_mutex_t*)malloc( sizeof( *cref_mutex ) );
        pthread_mutex_init( cref_mutex, NULL );
        m_crefMutexes[cref] = cref_mutex;
    }
    pthread_mutex_lock( cref_mutex );

    pthread_mutex_unlock( &m_guard );
    return 1;
} /* LockCref */

void
CRefMgr::UnlockCref( CookieRef* cref )
{
    pthread_mutex_t* cref_mutex = m_crefMutexes[cref];
    pthread_mutex_unlock( cref_mutex );
}

/* static */ void
CRefMgr::heartbeatProc( void* closure )
{
    CRefMgr* self = (CRefMgr*)closure;
    self->checkHeartbeats( now() );
} /* heartbeatProc */

CookieRef*
CRefMgr::AddNew( const char* cookie, const char* connName, CookieID id )
{
    CookieRef* exists = getCookieRef_impl( id );
    assert( exists == NULL );

    RWWriteLock rwl( &m_cookieMapRWLock );
    logf( XW_LOGINFO, "making new cref: %d", id );
    CookieRef* ref = new CookieRef( cookie, connName, id );
    m_cookieMap.insert( pair<CookieID, CookieRef*>(ref->GetCookieID(), ref ) );
    logf( XW_LOGINFO, "paired cookie %s/connName %s with id %d", 
          (cookie?cookie:"NULL"), connName, ref->GetCookieID() );

    if ( m_cookieMap.size() == 1 ) {
        RelayConfigs* cfg = RelayConfigs::GetConfigs();
        short heartbeat = cfg->GetHeartbeatInterval();
        TimerMgr::GetTimerMgr()->SetTimer( heartbeat, heartbeatProc, this,
                                           heartbeat );
    }

    return ref;
} /* AddNew */

void
CRefMgr::Delete( CookieRef* cref )
{
    RWWriteLock rwl( &m_cookieMapRWLock );

    CookieMap::iterator iter = m_cookieMap.begin();
    while ( iter != m_cookieMap.end() ) {
        CookieRef* ref = iter->second;
        if ( ref == cref ) {
            logf( XW_LOGINFO, "erasing cref" );
            m_cookieMap.erase( iter );
            break;
        }
        ++iter;
    }

    pthread_mutex_t* cref_mutex = m_crefMutexes[cref];
    pthread_mutex_unlock( cref_mutex );
    pthread_mutex_destroy( cref_mutex );
    free( cref_mutex );

    map<CookieRef*,pthread_mutex_t*>::iterator iter2;
    iter2 = m_crefMutexes.find(cref);
    m_crefMutexes.erase( iter2 );

    delete cref;

    if ( m_cookieMap.size() == 0 ) {
        TimerMgr::GetTimerMgr()->ClearTimer( heartbeatProc, this );
    }

    logf( XW_LOGINFO, "CRefMgr::Delete done" );
}

void
CRefMgr::Delete( CookieID id )
{
    CookieRef* cref = getCookieRef_impl( id );
    if ( cref != NULL ) {
        Delete( cref );
    }
} /* Delete */

void
CRefMgr::Delete( const char* connName )
{
    CookieID id = cookieIDForConnName( connName );
    Delete( id );
} /* Delete */

CookieRef* 
CRefMgr::getCookieRef_impl( CookieID cookieID )
{
    CookieRef* ref = NULL;
    RWReadLock rwl( &m_cookieMapRWLock );

    CookieMap::iterator iter = m_cookieMap.find( cookieID );
    while ( iter != m_cookieMap.end() ) {
        CookieRef* sec = iter->second;
        if ( sec->GetCookieID() == cookieID ) {
            ref = sec;
            break;
        }
        ++iter;
    }
    return ref;
}

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

    unsigned int i;
    for ( i = 0; i < crefs.size(); ++i ) {
        SafeCref scr( crefs[i] );
        scr.CheckHeartbeats( now );
    }
} /* checkHeartbeats */

/* static */ CookieMapIterator
CRefMgr::GetCookieIterator()
{
    CookieMapIterator iter;
    return iter;
}


CookieMapIterator::CookieMapIterator()
     : _iter( CRefMgr::Get()->m_cookieMap.begin() )
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

SafeCref::SafeCref( const char* cORn, int isCookie, HostID hid, 
                    int nPlayersH, int nPlayersT )
     : m_cref( NULL )
     , m_mgr( CRefMgr::Get() )
{
    CookieRef* cref;

    cref = m_mgr->getMakeCookieRef_locked( cORn, isCookie, hid, 
                                           nPlayersH, nPlayersT );
    if ( cref != NULL ) {
        if ( m_mgr->LockCref( cref ) ) {
            m_cref = cref;
        }
    }
}

SafeCref::SafeCref( CookieID connID )
     : m_cref( NULL )
     , m_mgr( CRefMgr::Get() )
{
    CookieRef* cref = m_mgr->getCookieRef_locked( connID );
    if ( cref != NULL ) {
        if ( m_mgr->LockCref( cref ) ) {
            m_cref = cref;
        }
    }
}

SafeCref::SafeCref( int socket )
     : m_cref( NULL )
     , m_mgr( CRefMgr::Get() )
{
    CookieRef* cref = m_mgr->getCookieRef_locked( socket );
    if ( cref != NULL ) {
        if ( m_mgr->LockCref( cref ) ) {
            m_cref = cref;
        }
    }
}

SafeCref::SafeCref( CookieRef* cref )
     : m_cref( NULL )
     , m_mgr( CRefMgr::Get() )
{
    if ( m_mgr->checkCookieRef_locked( cref ) ) {
        if ( m_mgr->LockCref( cref ) ) {
            m_cref = cref;
        }
    }
}

SafeCref::~SafeCref()
{
    if ( m_cref != NULL ) {
        if ( m_cref->ShouldDie() ) {
            m_mgr->Delete( m_cref );
        } else {
            m_mgr->UnlockCref( m_cref );
        }
    }
}
