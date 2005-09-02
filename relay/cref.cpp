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
#include <netinet/in.h>

#include "cref.h"
#include "xwrelay.h"
#include "mlock.h"
#include "tpool.h"
#include "states.h"
#include "timermgr.h"
#include "configs.h"
#include "crefmgr.h"

using namespace std;

pthread_mutex_t g_IdsMutex = PTHREAD_MUTEX_INITIALIZER;
CookieID CookieRef::ms_nextConnectionID = 1000;

/*****************************************************************************
 * SocketsIterator class
 *****************************************************************************/

SocketsIterator::SocketsIterator( SocketMap::iterator iter,
                                  SocketMap::iterator end,
                                  pthread_mutex_t* rwlock )
    : m_iter( iter )
    , m_end( end )
    , m_mutex( rwlock )
{
}

SocketsIterator::~SocketsIterator()
{
    pthread_mutex_unlock( m_mutex );
}

int
SocketsIterator::Next()
{
    int socket = 0; 
    if ( m_iter != m_end ) {
        socket = m_iter->first;
        ++m_iter;
    }
    return socket;
}

/*****************************************************************************
 * CookieRef class
 *****************************************************************************/

CookieRef::CookieRef( string s, CookieID id )
    : m_heatbeat(RelayConfigs::GetConfigs()->GetHeartbeatInterval())
    , m_name(s)
    , m_totalSent(0)
    , m_curState(XW_ST_INITED)
{
    pthread_rwlock_init( &m_sockets_rwlock, NULL );
    pthread_mutex_init( &m_EventsMutex, NULL );

    if ( id == 0 ) {
        MutexLock ml( &g_IdsMutex );
        m_connectionID = ms_nextConnectionID++; /* needs a mutex!!! */
    } else {
        m_connectionID = id;
    }
}

CookieRef::~CookieRef()
{
    cancelAllConnectedTimer();

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

    pthread_mutex_destroy( &m_EventsMutex );
    pthread_rwlock_destroy( &m_sockets_rwlock );
} /* ~CookieRef */

void
CookieRef::_Connect( int socket, HostID srcID )
{
    CRefMgr::Get()->Associate( socket, this );
    MutexLock ml( &m_EventsMutex );
    pushConnectEvent( socket, srcID );
    handleEvents();
}

void
CookieRef::_Reconnect( int socket, HostID srcID )
{
    CRefMgr::Get()->Associate( socket, this );
    MutexLock ml( &m_EventsMutex );
    pushReconnectEvent( socket, srcID );
    handleEvents();
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
CookieRef::removeSocket( const CRefEvent* evt )
{
    int socket = evt->u.rmsock.socket;
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

    /* Does this belong here or at a higher level? */
    XWThreadPool::GetTPool()->CloseSocket( socket );

    if ( count == 0 ) {
        pushLastSocketGoneEvent();
    }
} /* Remove */


int
CookieRef::HasSocket( int socket )
{
    int found = 0;
    logf( "CookieRef::HasSocket" );
    RWReadLock rwl( &m_sockets_rwlock );

    map<HostID,HostRec>::iterator iter = m_hostSockets.begin();
    while ( iter != m_hostSockets.end() ) {
        if ( iter->second.m_socket == socket ) {
            found = 1;
            break;
        }
        ++iter;
    }
    return found;
} /* HasSocket */

void
CookieRef::_HandleHeartbeat( HostID id, int socket )
{
    MutexLock ml( &m_EventsMutex );
    pushHeartbeatEvent( id, socket );
    handleEvents();
} /* HandleHeartbeat */

void
CookieRef::CheckHeartbeats( time_t now, vector<int>* victims )
{
    logf( "CookieRef::CheckHeartbeats" );
    MutexLock ml( &m_EventsMutex );
    pushHeartTimerEvent( now, victims );
    handleEvents();
} /* CheckHeartbeats */

void
CookieRef::_Forward( HostID src, HostID dest, unsigned char* buf, int buflen )
{
    MutexLock ml( &m_EventsMutex );
    pushForwardEvent( src, dest, buf, buflen );
    handleEvents();
} /* Forward */

void
CookieRef::_Remove( int socket )
{
    MutexLock ml( &m_EventsMutex );
    pushRemoveSocketEvent( socket );
    handleEvents();
} /* Forward */

void 
CookieRef::pushConnectEvent( int socket, HostID srcID )
{
    CRefEvent evt;
    evt.type = XW_EVENT_CONNECTMSG;
    evt.u.con.socket = socket;
    evt.u.con.srcID = srcID;
    m_eventQueue.push_front( evt );
} /* pushConnectEvent */

void 
CookieRef::pushReconnectEvent( int socket, HostID srcID )
{
    CRefEvent evt;
    evt.type = XW_EVENT_RECONNECTMSG;
    evt.u.recon.socket = socket;
    evt.u.recon.srcID = srcID;
    m_eventQueue.push_front( evt );
} /* pushConnectEvent */

void
CookieRef::pushHeartbeatEvent( HostID id, int socket )
{
    CRefEvent evt;
    evt.type = XW_EVENT_HEARTMSG;
    evt.u.heart.id = id;
    evt.u.heart.socket = socket;
    m_eventQueue.push_front( evt );
}

void
CookieRef::pushHeartTimerEvent( time_t now, vector<int>* victims )
{
    CRefEvent evt;
    evt.type = XW_EVENT_HEARTTIMER;
    evt.u.htime.now = now;
    evt.u.htime.victims = victims;
    m_eventQueue.push_front( evt );
}

void
CookieRef::pushForwardEvent( HostID src, HostID dest, 
                             unsigned char* buf, int buflen )
{
    CRefEvent evt;
    evt.type = XW_EVENT_FORWARDMSG;
    evt.u.fwd.src = src;
    evt.u.fwd.dest = dest;
    evt.u.fwd.buf = buf;
    evt.u.fwd.buflen = buflen;
    m_eventQueue.push_front( evt );
}

void
CookieRef::pushRemoveSocketEvent( int socket )
{
    CRefEvent evt;
    evt.type = XW_EVENT_REMOVESOCKET;
    evt.u.rmsock.socket = socket;
    m_eventQueue.push_front( evt );
}

void
CookieRef::pushDestBadEvent()
{
    CRefEvent evt;
    evt.type = XW_EVENT_DESTBAD;
    m_eventQueue.push_front( evt );
}

void
CookieRef::pushDestOkEvent( const CRefEvent* oldEvt )
{
    CRefEvent evt;
    memcpy( &evt, oldEvt, sizeof(evt) );
    evt.type = XW_EVENT_DESTOK;
    m_eventQueue.push_front( evt );
}

void
CookieRef::pushCanLockEvent( const CRefEvent* oldEvt )
{
    CRefEvent evt;
    memcpy( &evt, oldEvt, sizeof(evt) );
    evt.type = XW_EVENT_CAN_LOCK;
    m_eventQueue.push_front( evt );
}

void
CookieRef::pushCantLockEvent( const CRefEvent* oldEvt )
{
    CRefEvent evt;
    memcpy( &evt, oldEvt, sizeof(evt) );
    evt.type = XW_EVENT_CANT_LOCK;
    m_eventQueue.push_front( evt );
}

void
CookieRef::pushLastSocketGoneEvent()
{
    CRefEvent evt;
    evt.type = XW_EVENT_NOMORESOCKETS;
    m_eventQueue.push_front( evt );
}

void
CookieRef::handleEvents()
{
    /* Assumption: has mutex!!!! */
    while ( m_eventQueue.size () > 0 ) {
        CRefEvent evt = m_eventQueue.front();
        m_eventQueue.pop_front();

        XW_RELAY_ACTION takeAction;
        if ( getFromTable( m_curState, evt.type, &takeAction, &m_nextState ) ) {

            logf( "moving from state %s to state %s for event %s",
                  stateString(m_curState), stateString(m_nextState),
                  eventString(evt.type) );

            switch( takeAction ) {
            case XW_ACTION_SEND_1ST_RSP:
                setAllConnectedTimer();
                /* fallthru */
            case XW_ACTION_SENDRSP:
                sendResponse( &evt );
                break;

            case XW_ACTION_FWD:
                forward( &evt );
                break;

            case XW_ACTION_CHECKDEST:
                checkDest( &evt );
                break;

            case XW_ACTION_CHECK_CAN_LOCK:
                checkFromServer( &evt );
                break;

            case XW_ACTION_DISCONNECTALL:
                disconnectAll( &evt );
                break;

            case XW_ACTION_NOTEHEART:
                noteHeartbeat( &evt );
                break;

            case XW_ACTION_CHECKHEART:
                checkHeartbeats( &evt );
                break;

            case XW_ACTION_REMOVESOCKET:
                removeSocket( &evt );
                break;

            case XW_ACTION_HEARTOK:
            case XW_ACTION_NONE: 
                /* nothing to do for these */
                break;

            default:
                assert(0); 
                break;
            }

            m_curState = m_nextState;
        } else {
            assert(0);
        }
    }
} /* handleEvents */

void
CookieRef::send_with_length( int socket, unsigned char* buf, int bufLen )
{
    SocketWriteLock slock( socket );

    if ( send_with_length_unsafe( socket, buf, bufLen ) ) {
        RecordSent( bufLen, socket );
    } else {
        /* ok that the slock above is still in scope */
        killSocket( socket, "couldn't send" );
    }
}

static void
putNetShort( unsigned char** bufpp, unsigned short s )
{
    s = htons( s );
    memcpy( *bufpp, &s, sizeof(s) );
    *bufpp += sizeof(s);
}

static void
putNetLong( unsigned char** bufpp, unsigned long s )
{
    s = htonl( s );
    memcpy( *bufpp, &s, sizeof(s) );
    *bufpp += sizeof(s);
    assert( sizeof(s) == 4 );   /* otherwise need to hardcode */
}

void
CookieRef::setAllConnectedTimer()
{
    time_t inHowLong;
    inHowLong = RelayConfigs::GetConfigs()->GetAllConnectedInterval();
    TimerMgr::getTimerMgr()->setTimer( inHowLong,
                                       s_checkAllConnected, this, 0 );
}

void
CookieRef::cancelAllConnectedTimer()
{
    TimerMgr::getTimerMgr()->clearTimer( s_checkAllConnected, this );
}

void
CookieRef::sendResponse( const CRefEvent* evt )
{
    int socket = evt->u.con.socket;
    HostID id = evt->u.con.srcID;

    assert( id != HOST_ID_NONE );
    logf( "remembering pair: hostid=%x, socket=%d", id, socket );
    RWWriteLock ml( &m_sockets_rwlock );
    HostRec hr(socket);
    m_hostSockets.insert( pair<HostID,HostRec>(id,hr) );

    /* Now send the response */
    unsigned char buf[7];
    unsigned char* bufp = buf;

    *bufp++ = XWRELAY_CONNECTRESP;
    putNetShort( &bufp, GetHeartbeat() );
    putNetLong( &bufp, GetCookieID() );

    send_with_length( socket, buf, sizeof(buf) );
    logf( "sent XWRELAY_CONNECTRESP" );
} /* sendResponse */

void
CookieRef::forward( const CRefEvent* evt )
{
    unsigned char* buf = evt->u.fwd.buf;
    int buflen = evt->u.fwd.buflen;
    HostID src = evt->u.fwd.src;
    HostID dest = evt->u.fwd.dest;

    int destSocket = SocketForHost( dest );

    if ( destSocket != -1 ) {
        /* This is an ugly hack!!!! */
        *buf = XWRELAY_MSG_FROMRELAY;
        send_with_length( destSocket, buf, buflen );

        /* also note that we've heard from src recently */
        pushHeartbeatEvent( src, SocketForHost(src) );
    } else {
        /* We're not really connected yet! */
    }
} /* forward */

void
CookieRef::checkDest( const CRefEvent* evt )
{
    HostID dest = evt->u.fwd.dest;
    int destSocket = SocketForHost( dest );
    if ( destSocket == -1 ) {
        pushDestBadEvent();
    } else {
        pushDestOkEvent( evt );
    }
} /* checkDest */

void
CookieRef::checkFromServer( const CRefEvent* evt )
{
    HostID src = evt->u.fwd.src;
    if ( src == HOST_ID_SERVER ) {
        pushCanLockEvent( evt );
    } else {
        pushCantLockEvent( evt );
    }
}

void
CookieRef::disconnectAll( const CRefEvent* evt )
{
    logf( "disconnectAll" );
    map<HostID,HostRec>::iterator iter = m_hostSockets.begin();
    while ( iter != m_hostSockets.end() ) {
        pushRemoveSocketEvent( iter->second.m_socket );
        ++iter;
    }
    logf( "disconnectAll done" );
}

void
CookieRef::noteHeartbeat( const CRefEvent* evt )
{
    int socket = evt->u.heart.socket;
    HostID id = evt->u.heart.id;

    RWWriteLock rwl( &m_sockets_rwlock );

    map<HostID,HostRec>::iterator iter = m_hostSockets.find(id);
    if ( iter == m_hostSockets.end() ) {
        logf( "no socket for HostID %d", id );
    } else {

        /* PENDING If the message came on an unexpected socket, kill the
           connection.  An attack is the most likely explanation. */
        assert( iter->second.m_socket == socket );

        logf( "upping m_lastHeartbeat from %d to %d",
              iter->second.m_lastHeartbeat, now() );
        iter->second.m_lastHeartbeat = now();
    }
} /* noteHeartbeat */

void
CookieRef::checkHeartbeats( const CRefEvent* evt )
{
    int vcount = 0;
    vector<int>* victims = evt->u.htime.victims;
    time_t now = evt->u.htime.now;

    RWWriteLock rwl( &m_sockets_rwlock );

    map<HostID,HostRec>::iterator iter = m_hostSockets.begin();
    while ( iter != m_hostSockets.end() ) {
        time_t last = iter->second.m_lastHeartbeat;
        if ( (now - last) > GetHeartbeat() * 2 ) {
            victims->push_back( iter->second.m_socket );
            ++vcount;
        }
        ++iter;
    }
    logf( "CookieRef::CheckHeartbeats done" );

    /* Post an event */
    CRefEvent newEvt;
    newEvt.type = vcount > 0 ? XW_EVENT_HEARTFAILED : XW_EVENT_HEARTOK;
    m_eventQueue.push_front( newEvt );
} /* checkHeartbeats */

/* timer callback */
/* static */ void
CookieRef::s_checkAllConnected( void* closure )
{
    /* Need to ensure */
    CookieRef* self = (CookieRef*)closure;
    SafeCref scr(self);
    if ( scr.IsValid() ) {
        scr.CheckAllConnected();
    }
}

void
CookieRef::_CheckAllConnected()
{
    logf( "checkAllConnected" );
    MutexLock ml( &m_EventsMutex );
    CRefEvent newEvt;
    newEvt.type = XW_EVENT_CONNTIMER;
    m_eventQueue.push_front( newEvt );
    handleEvents();
}

void
CookieRef::_PrintCookieInfo( string& out )
{
    out += "Name: ";
    out += Name();
    out += "\n";
    out += "ID: ";
    char buf[64];
    snprintf( buf, sizeof(buf), "%ld\n", GetCookieID() );
    out += buf;

    snprintf( buf, sizeof(buf), "Bytes sent: %d\n", m_totalSent );
    out += buf;

    /* n messages */
    /* n bytes */
    /* open since when */
    /* sockets */

} /* PrintCookieInfo */
