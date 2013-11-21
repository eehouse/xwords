/* -*- compile-command: "make -j3"; -*- */

/* 
 * Copyright 2005 - 2012 by Eric House (xwords@eehouse.org).  All rights
 * reserved.
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
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "cref.h"
#include "xwrelay.h"
#include "mlock.h"
#include "tpool.h"
#include "states.h"
#include "timermgr.h"
#include "configs.h"
#include "crefmgr.h"
#include "devmgr.h"
#include "permid.h"
#include "udpack.h"

using namespace std;

/*****************************************************************************
 * SocketsIterator class
 *****************************************************************************/

/* SocketsIterator::SocketsIterator( SocketMap::iterator iter, */
/*                                   SocketMap::iterator end, */
/*                                   pthread_mutex_t* mutex ) */
/*     : m_iter( iter ) */
/*     , m_end( end ) */
/*     , m_mutex( mutex ) */
/* { */
/* } */

/* SocketsIterator::~SocketsIterator() */
/* { */
/*     pthread_mutex_unlock( m_mutex ); */
/* } */

/* int */
/* SocketsIterator::Next() */
/* { */
/*     int socket = 0;  */
/*     if ( m_iter != m_end ) { */
/*         socket = m_iter->first; */
/*         ++m_iter; */
/*     } */
/*     return socket; */
/* } */

/*****************************************************************************
 * CookieRef class
 *****************************************************************************/

#define ASSERT_LOCKED() \
    assert( m_locking_thread == pthread_self() )

void
CookieRef::ReInit( const char* cookie, const char* connName, CookieID cid,
                   int langCode, int nPlayers, int nAlreadyHere )
{
    m_cookie = cookie==NULL?"":cookie;
    m_connName = connName==NULL?"":connName;
    m_cid = cid;
    m_curState = XWS_EMPTY;
    m_nPlayersSought = nPlayers;
    m_nPlayersHere = nAlreadyHere;
    m_locking_thread = 0;
    m_starttime = uptime();
    m_in_handleEvents = false;
    m_langCode = langCode;
    memset( &m_sockets, 0, sizeof(m_sockets) );

    if ( RelayConfigs::GetConfigs()->GetValueFor( "SEND_DELAY_MILLIS", 
                                                   &m_delayMicros ) ) {
        m_delayMicros *= 1000;  /* millis->micros */
    } else {
        m_delayMicros = 0;
    }
    RelayConfigs::GetConfigs()->GetValueFor( "HEARTBEAT", &m_heartbeat );
    logf( XW_LOGINFO, "initing cref for cookie %s, connName %s",
          m_cookie.c_str(), m_connName.c_str() );

    unsigned int ii;
    for ( ii = 0; ii < sizeof(m_timers)/sizeof(m_timers[0]); ++ii ) {
        m_timers[ii].m_this = NULL;
        m_timers[ii].m_hid = ii + 1;
    }
}

CookieRef::CookieRef( const char* cookie, const char* connName, CookieID cid,
                      int langCode, int nPlayersT, int nAlreadyHere )
{
    pthread_rwlock_init( &m_socketsRWLock, NULL );
    ReInit( cookie, connName, cid, langCode, nPlayersT, nAlreadyHere );
}

CookieRef::~CookieRef()
{
    logf( XW_LOGINFO, "CookieRef for %d being deleted", m_cid );
    cancelAllConnectedTimer();

    /* get rid of any sockets still contained */
    XWThreadPool* tPool = XWThreadPool::GetTPool();

    ASSERT_LOCKED();
    vector<HostRec>::iterator iter;
    {
        RWWriteLock rwl( &m_socketsRWLock );
        for ( unsigned int ii = 0; ii < VSIZE(m_sockets); ++ii ) {
            HostRec* hr = m_sockets[ii];
            if ( NULL != hr ) {
                const AddrInfo* addr = &hr->m_addr;
                if ( addr->isTCP() ) {
                    tPool->CloseSocket( addr );
                }
                delete hr;
                m_sockets[ii] = NULL;
            }
        }
    }
    printSeeds(__func__);

    pthread_rwlock_destroy( &m_socketsRWLock );
} /* ~CookieRef */

void
CookieRef::Clear(void)
{
    m_cookie = "";
    m_connName = "";
    m_cid = 0;
    m_eventQueue.clear();
} /* Clear */

bool
CookieRef::Lock( void ) 
{
    bool success = true;

    /* We get here possibly after having been blocked on the mutex for a
       while.  This cref may no longer be live.  If it's not, unlock and
       return. */

    assert( m_locking_thread == 0 );
    m_locking_thread = pthread_self();

    if ( notInUse() ) {
        logf( XW_LOGINFO, "%s: not locking %p because not in use", __func__, 
              this );
        success = false;
        m_locking_thread = 0;
    }

    return success;
} /* CookieRef::Lock */

void
CookieRef::Unlock() { 
    assert( m_locking_thread == pthread_self() );
    m_locking_thread = 0;
}

bool
CookieRef::_Connect( int clientVersion, DevID* devID, 
                     int nPlayersH, int nPlayersS, int seed, 
                     bool seenSeed, const AddrInfo* addr )
{
    bool connected = false;
    HostID prevHostID = HOST_ID_NONE;
    bool alreadyHere = AlreadyHere( seed, addr, &prevHostID );

    if ( alreadyHere ) {
        if ( seenSeed ) {   /* we need to get rid of the current entry, then
                               proceed as if this were a new connection */
            assert( HOST_ID_NONE != prevHostID );
            postDropDevice( prevHostID );
        } else {
            connected = true;   /* but drop the packet */
        }
    }

    if ( !connected ) {
        bool socketOK = addr->isUDP();
        if ( !socketOK ) {
            socketOK = true;
            vector<AddrInfo> addrs = GetAddrs();
            vector<AddrInfo>::const_iterator iter;
            for ( iter = addrs.begin(); iter != addrs.end(); ++iter ) {
                if ( iter->equals( *addr ) ) {
                    socketOK = false;
                    break;
                }
            }
        }
        if ( socketOK ) {
            pushConnectEvent( clientVersion, devID, nPlayersH, nPlayersS, 
                              seed, addr );
            handleEvents();
            connected = HasSocket_locked( addr );
        } else {
            logf( XW_LOGINFO, "dropping connect event; already connected" );
        }
    }
    return connected;
}

bool
CookieRef::_Reconnect( int clientVersion, DevID* devID, HostID hid, 
                       int nPlayersH, int nPlayersS, int seed, 
                       const AddrInfo* addr, bool gameDead )
{
    bool spotTaken = false;
    bool alreadyHere = AlreadyHere( hid, seed, addr, &spotTaken );
    if ( spotTaken ) {
        logf( XW_LOGINFO, "%s: failing because spot taken", __func__ );
    } else {
        if ( alreadyHere ) {
            logf( XW_LOGINFO, "%s: dropping because already here",
                  __func__ );
        } else {
            pushReconnectEvent( clientVersion, devID, hid, nPlayersH, 
                                nPlayersS, seed, addr );
        }
        if ( gameDead ) {
            pushGameDead( addr );
        }
        handleEvents();
    }
    return !spotTaken;
}

void
CookieRef::_HandleAck( HostID hostID )
{
    CRefEvent evt( XWE_GOTONEACK );
    evt.u.ack.srcID = hostID;
    m_eventQueue.push_back( evt );
    handleEvents();
}

void
CookieRef::_PutMsg( HostID srcID, const AddrInfo* addr, HostID destID, 
                    const uint8_t* buf, int buflen )
{
    CRefEvent evt( XWE_PROXYMSG, addr );
    evt.u.fwd.src = srcID;
    evt.u.fwd.dest = destID;
    evt.u.fwd.buf = buf;
    evt.u.fwd.buflen = buflen;

    m_eventQueue.push_back( evt );
    handleEvents();
}

void
CookieRef::_Disconnect( const AddrInfo* addr, HostID hostID )
{
    logf( XW_LOGINFO, "%s(hostID=%d)", __func__, hostID );

    CRefEvent evt( XWE_DISCONN, addr );
    evt.u.discon.srcID = hostID;
    m_eventQueue.push_back( evt );

    handleEvents();
}

void
CookieRef::_DeviceGone( HostID hostID, int seed )
{
    CRefEvent evt( XWE_DEVGONE );
    evt.u.devgone.hid = hostID;
    evt.u.devgone.seed = seed;
    m_eventQueue.push_back( evt );

    handleEvents();
}

void
CookieRef::_Shutdown()
{
    CRefEvent evt( XWE_SHUTDOWN );
    m_eventQueue.push_back( evt );

    handleEvents();
} /* _Shutdown */

HostID
CookieRef::HostForSocket( const AddrInfo* addr )
{
    HostID hid = HOST_ID_NONE;
    ASSERT_LOCKED();
    RWReadLock rrl( &m_socketsRWLock );
    for ( unsigned int ii = 0; ii < VSIZE(m_sockets); ++ii ) {
        HostRec* hr = m_sockets[ii];
        if ( !!hr && hr->m_addr.equals( *addr ) ) {
            hid = ii + 1;
            assert( HOST_ID_NONE != hid );
            break;
        }
    }
    return hid;
}

const AddrInfo*
CookieRef::SocketForHost( HostID dest )
{
    const AddrInfo* result = NULL;
    ASSERT_LOCKED();
    assert( dest != 0 );        /* don't use as lookup before assigned */

    RWReadLock rrl( &m_socketsRWLock );
    HostRec* hr = m_sockets[dest - 1];
    if ( !!hr ) {
        result = &hr->m_addr;
    }

    // logf( XW_LOGVERBOSE0, "returning socket=%d for hostid=%x", socket, dest );
    return result;
}

bool 
CookieRef::AlreadyHere( unsigned short seed, const AddrInfo* addr, 
                        HostID* prevHostID )
{
    logf( XW_LOGINFO, "%s(seed=%x(%d))", __func__, seed, seed );
    bool here = false;

    RWReadLock rrl( &m_socketsRWLock );
    for ( unsigned int ii = 0; ii < VSIZE(m_sockets); ++ii ) {
        HostRec* hr = m_sockets[ii];
        here = !!hr && hr->m_seed == seed; /* client already registered */
        if ( here ) {
            if ( !addr->equals(hr->m_addr) ) { /* not just a dupe packet */
                logf( XW_LOGINFO, "%s: seeds match; socket assumed closed",
                      __func__ );
                *prevHostID = ii + 1;
            }
            break;
        }
    }
    
    logf( XW_LOGINFO, "%s=>%d", __func__, here );
    return here;
}

bool 
CookieRef::AlreadyHere( HostID hid, unsigned short seed, const AddrInfo* addr, 
                        bool* spotTaken )
{
    logf( XW_LOGINFO, "%s(hid=%d,seed=%x(%d),socket=%d)", __func__, 
          hid, seed, seed, addr->socket() );
    bool here = false;

    if ( HOST_ID_NONE != hid ) {
        RWWriteLock rwl( &m_socketsRWLock );
        HostRec* hr = m_sockets[hid-1];
        if ( !!hr ) {
            if ( seed != hr->m_seed ) {
                *spotTaken = true;
            } else if ( addr->equals( hr->m_addr ) ) {
                here = true;    /* dup packet */
            } else {
                logf( XW_LOGINFO, "%s: hids match; nuking existing record "
                      "for socket b/c assumed closed", __func__ );
                delete hr;
                m_sockets[hid-1] = NULL;
            }
        }
    }
    
    logf( XW_LOGINFO, "%s=>%d", __func__, here );
    return here;
}

void
CookieRef::notifyDisconn( const CRefEvent* evt )
{
    uint8_t buf[] = { 
        XWRELAY_DISCONNECT_YOU,
        evt->u.disnote.why 
    };

    send_with_length( &evt->addr, HOST_ID_NONE, buf, sizeof(buf), true );
} /* notifyDisconn */

void
CookieRef::removeSocket( const AddrInfo* addr )
{
    logf( XW_LOGINFO, "%s(socket=%d)", __func__, addr->socket() );
    bool found = false;
    ASSERT_LOCKED();

    {
        RWWriteLock rwl( &m_socketsRWLock );

        for ( unsigned int ii = 0; !found && ii < VSIZE(m_sockets); ++ii ) {
            HostRec* hr = m_sockets[ii];
            if ( !!hr && hr->m_addr.equals( *addr ) ) {
                if ( hr->m_ackPending ) {
                    HostID hid = ii + 1;
                    logf( XW_LOGINFO,
                          "%s: Never got ack; removing hid %d from DB",
                          __func__, hid );
                    DBMgr::Get()->RmDeviceByHid( ConnName(), hid );
                    m_nPlayersHere -= hr->m_nPlayersH;
                    cancelAckTimer( hid );
                }
                m_sockets[ii] = NULL;
                delete hr;
                found = true;
            }
        }
    }
    if ( !found ) {
        logf( XW_LOGINFO, "%s: socket not found", __func__ );
    }

    printSeeds(__func__);

    if ( 0 == CountSockets() ) {
        pushLastSocketGoneEvent();
    }
} /* removeSocket */

bool
CookieRef::HaveRoom( int nPlayers )
{
    int total = m_nPlayersSought;
    int soFar = m_nPlayersHere;
    bool haveRoom = nPlayers <= total - soFar;
    logf( XW_LOGINFO, "%s(%d): total %d - soFar %d >= new %d => %d", __func__,
          nPlayers, total, soFar, nPlayers, haveRoom );
    return haveRoom;
}

int
CookieRef::CountSockets()
{
    RWReadLock rrl( &m_socketsRWLock );
    return CountSockets_locked();
}

int
CookieRef::CountSockets_locked()
{
    int count = 0;
    for ( unsigned int ii = 0; ii < VSIZE(m_sockets); ++ii ) {
        HostRec* hr = m_sockets[ii];
        if ( !!hr ) {
            ++count;
        }
    }
    return count;
}

bool
CookieRef::HasSocket( const AddrInfo* addr )
{
    bool result = Lock();
    if ( result ) {
        result = HasSocket_locked( addr );
        Unlock();
    }
    return result;
}

vector<AddrInfo>
CookieRef::GetAddrs()
{
    vector<AddrInfo> result;
    RWReadLock rrl( &m_socketsRWLock );
    for ( unsigned int ii = 0; ii < VSIZE(m_sockets); ++ii ) {
        HostRec* hr = m_sockets[ii];
        if ( !!hr ) {
            result.push_back( hr->m_addr );
        }
    }
    return result;
}

bool
CookieRef::HasSocket_locked( const AddrInfo* addr )
{
    ASSERT_LOCKED();
    RWReadLock rrl( &m_socketsRWLock );
    bool found = false;
    for ( unsigned int ii = 0; !found && ii < VSIZE(m_sockets); ++ii ) {
        HostRec* hr = m_sockets[ii];
        found = !!hr && hr->m_addr.equals( *addr );
    }

    logf( XW_LOGINFO, "%s=>%d", __func__, found );
    return found;
} /* HasSocket_locked */

#ifdef RELAY_HEARTBEAT
void
CookieRef::_HandleHeartbeat( HostID id, int socket )
{
    pushHeartbeatEvent( id, socket );
    handleEvents();
} /* HandleHeartbeat */

void
CookieRef::_CheckHeartbeats( time_t now )
{
    ASSERT_LOCKED();
    {
        RWReadLock rrl( &m_socketsRWLock );
        map<int,HostRec>::const_iterator iter;
        for ( iter = m_sockets.begin(); iter != m_sockets.end(); ++iter ) {
            time_t last = iter->second.m_lastHeartbeat;
            if ( (now - last) > GetHeartbeat() ) {
                pushHeartFailedEvent( iter->first );
            }
        }
    }

    handleEvents();
} /* CheckHeartbeats */
#endif

void
CookieRef::_Forward( HostID src, const AddrInfo* addr, 
                     HostID dest, const uint8_t* buf, int buflen )
{
    pushForwardEvent( src, addr, dest, buf, buflen );
    handleEvents();
} /* Forward */

void
CookieRef::_Remove( const AddrInfo* addr )
{
    pushRemoveSocketEvent( addr );
    handleEvents();
} /* Forward */

void 
CookieRef::pushConnectEvent( int clientVersion, DevID* devID,
                             int nPlayersH, int nPlayersS,
                             int seed, const AddrInfo* addr )
{
    CRefEvent evt( XWE_DEVCONNECT, addr );
    evt.u.con.clientVersion = clientVersion;
    evt.u.con.devID = devID;
    evt.u.con.srcID = HOST_ID_NONE;
    evt.u.con.nPlayersH = nPlayersH;
    evt.u.con.nPlayersS = nPlayersS;
    evt.u.con.seed = seed;
    m_eventQueue.push_back( evt );
} /* pushConnectEvent */

void 
CookieRef::pushReconnectEvent( int clientVersion, DevID* devID,
                               HostID srcID, int nPlayersH, int nPlayersS, 
                               int seed, const AddrInfo* addr )
{
    CRefEvent evt( XWE_RECONNECT, addr );
    evt.u.con.clientVersion = clientVersion;
    evt.u.con.devID = devID;
    evt.u.con.srcID = srcID;
    evt.u.con.nPlayersH = nPlayersH;
    evt.u.con.nPlayersS = nPlayersS;
    evt.u.con.seed = seed;
    m_eventQueue.push_back( evt );
} /* pushReconnectEvent */

#ifdef RELAY_HEARTBEAT
void
CookieRef::pushHeartbeatEvent( HostID id, int socket )
{
    CRefEvent evt( XWE_HEARTRCVD );
    evt.u.heart.id = id;
    evt.u.heart.socket = socket;
    m_eventQueue.push_back( evt );
}

void
CookieRef::pushHeartFailedEvent( int socket )
{
    logf( XW_LOGINFO, "%s", __func__ );
    CRefEvent evt( XWE_HEARTFAILED );
    evt.u.heart.socket = socket;
    m_eventQueue.push_back( evt );
}
#endif

void
CookieRef::pushForwardEvent( HostID src, const AddrInfo* addr, HostID dest, 
                             const uint8_t* buf, int buflen )
{
    logf( XW_LOGVERBOSE1, "%s: %d -> %d", __func__, src, dest );
    CRefEvent evt( XWE_FORWARDMSG, addr );
    evt.u.fwd.src = src;
    evt.u.fwd.dest = dest;
    evt.u.fwd.buf = buf;
    evt.u.fwd.buflen = buflen;
    m_eventQueue.push_back( evt );
}

void
CookieRef::pushRemoveSocketEvent( const AddrInfo* addr )
{
    CRefEvent evt( XWE_REMOVESOCKET, addr );
    m_eventQueue.push_back( evt );
}

void
CookieRef::pushNotifyDisconEvent( const AddrInfo* addr, XWREASON why )
{
    CRefEvent evt( XWE_NOTIFYDISCON, addr );
    evt.u.disnote.why = why;
    m_eventQueue.push_back( evt );
}

void
CookieRef::pushLastSocketGoneEvent()
{
    CRefEvent evt( XWE_NOMORESOCKETS );
    m_eventQueue.push_back( evt );
}

void
CookieRef::pushGameDead( const AddrInfo* addr )
{
    CRefEvent evt( XWE_GAMEDEAD, addr );
    m_eventQueue.push_back( evt );
}

void
CookieRef::handleEvents()
{
    assert( !m_in_handleEvents );
    m_in_handleEvents = true;

    /* Assumption: has mutex!!!! */
    while ( m_eventQueue.size () > 0 ) {
        XW_RELAY_STATE nextState;
        DevIDRelay devID;
        CRefEvent evt = m_eventQueue.front();
        m_eventQueue.pop_front();

        XW_RELAY_ACTION takeAction;
        if ( getFromTable( m_curState, evt.type, &takeAction, &nextState ) ) {

            logf( XW_LOGINFO, "%s: %s -> %s on evt %s, act=%s", __func__,
                  stateString(m_curState), stateString(nextState),
                  eventString(evt.type), actString(takeAction) );

            switch( takeAction ) {

            case XWA_SEND_CONNRSP: 
                {
                    HostID hid;
                    if ( increasePlayerCounts( &evt, false, &hid, &devID ) ) {
                        setAllConnectedTimer();
                        sendResponse( &evt, true, &devID );
                        setAckTimer( hid );
                    }
                }
                break;

            case XWA_NOTEACK:
                updateAck( evt.u.ack.srcID, true );
                postCheckAllHere();
                break;

            case XWA_DROPDEVICE:
                updateAck( evt.u.ack.srcID, false );
                break;

            /* case XWA_SEND_1ST_RERSP: */
            /*     if ( increasePlayerCounts( &evt, false ) ) { */
            /*         setAllConnectedTimer(); */
            /*         sendResponse( &evt, takeAction != XWA_SEND_1ST_RERSP ); */
            /*     } */
            /*     break; */

            case XWA_SEND_RERSP:
                increasePlayerCounts( &evt, true, NULL, &devID );
                sendResponse( &evt, false, &devID );
                sendAnyStored( &evt );
                postCheckAllHere();
                break;

            case XWA_SEND_NO_ROOM:
                send_denied( &evt, XWRELAY_ERROR_NO_ROOM );
                break;
            case XWA_SEND_DUP_ROOM:
                send_denied( &evt, XWRELAY_ERROR_DUP_ROOM );
                removeSocket( &evt.addr );
                break;
            case XWA_SEND_TOO_MANY:
                send_denied( &evt, XWRELAY_ERROR_TOO_MANY );
                removeSocket( &evt.addr );
                break;

            case XWA_FWD:
                forward_or_store( &evt );
                break;

            case XWA_PROXYMSG:
                forward_or_store/*_proxy*/( &evt );
                break;

            case XWA_TRYTELL:
                send_havemsgs( &evt.addr );
                break;

            case XWA_TIMERDISCONN:
                disconnectSockets( XWRELAY_ERROR_TIMEOUT );
                break;

            case XWA_SHUTDOWN:
                disconnectSockets( XWRELAY_ERROR_SHUTDOWN );
                break;

            case XWA_HEARTDISCONN:
                notifyOthers( &evt.addr, XWRELAY_DISCONNECT_OTHER, 
                              XWRELAY_ERROR_HEART_OTHER );
                setAllConnectedTimer();
                // reducePlayerCounts( evt.u.discon.socket );
                disconnectSocket( &evt.addr, XWRELAY_ERROR_HEART_YOU );
                break;

            case XWA_DISCONNECT:
                setAllConnectedTimer();
                // reducePlayerCounts( evt.u.discon.socket );
                notifyOthers( &evt.addr, XWRELAY_DISCONNECT_OTHER,
                              XWRELAY_ERROR_OTHER_DISCON );
                removeSocket( &evt.addr );
                /* Don't notify.  This is a normal part of a game ending. */
                break;

            case XWA_RMDEV:
                removeDevice( &evt );
                break;

            case XWA_TELLGAMEDEAD:
                notifyGameDead( &evt.addr );
                break;

            case XWA_NOTEHEART:
                noteHeartbeat( &evt );
                break;

            case XWA_NOTIFYDISCON:
                notifyDisconn( &evt );
                break;

            case XWA_REMOVESOCK_2:
                setAllConnectedTimer();
                /* fallthru */
            case XWA_REMOVESOCK_1:
                // reducePlayerCounts( evt.u.rmsock.socket );
                if ( XWA_REMOVESOCK_2 == takeAction ) {
                    notifyOthers( &evt.addr, XWRELAY_DISCONNECT_OTHER,
                                  XWRELAY_ERROR_LOST_OTHER );
                }
                removeSocket( &evt.addr );
                break;

            case XWA_SENDALLHERE:
                CRefMgr::Get()->IncrementFullCount();
                cancelAllConnectedTimer();
                sendAllHere( true );
                break;

            case XWA_SNDALLHERE_2:
                sendAllHere( false );
                break;

            case XWA_NOTE_EMPTY:
                //cancelAllConnectedTimer();
                if ( 0 == DBMgr::Get()->CountStoredMessages( ConnName() ) ) {
                    CRefEvent evt( XWE_NOMOREMSGS );
                    m_eventQueue.push_back( evt );
                }
                break;

            case XWA_NONE: 
                /* nothing to do for these */
                break;

            default:
                assert(0); 
                break;
            }

            m_curState = nextState;

#ifdef DEBUG
            if ( XWS_EMPTY == m_curState ) {
                assert( 0 == m_sockets.size() );

                int nTotal, nHere;
                GetPlayerCounts( ConnName(), &nTotal, &nHere );
                assert( 0 == nHere );
            }
#endif
        } else {
            logf( XW_LOGERROR, "Killing cref b/c unable to find transition "
                  "from %s on event %s", stateString(m_curState),
                  eventString(evt.type) );
            CRefEvent shutevt( XWE_SHUTDOWN );
            m_eventQueue.push_back( shutevt );
        }
    }
    m_in_handleEvents = false;
} /* handleEvents */

bool
CookieRef::send_with_length( const AddrInfo* addr, HostID dest, 
                             const uint8_t* buf, int bufLen, bool cascade,
                             uint32_t* packetIDP )
{
    bool failed = false;
    if ( send_with_length_unsafe( addr, buf, bufLen, packetIDP ) ) {
        if ( HOST_ID_NONE == dest ) {
            dest = HostForSocket(addr);
        }
        if ( HOST_ID_NONE != dest ) {
            DBMgr::Get()->RecordSent( ConnName(), dest, bufLen );
        } else {
            logf( XW_LOGERROR, "%s: no hid for addr", __func__ );
        }
    } else {
        failed = true;
    }

    if ( failed && cascade ) {
        pushRemoveSocketEvent( addr );
        XWThreadPool::GetTPool()->CloseSocket( addr );
    }
    return !failed;
} /* send_with_length */

static void
putNetShort( uint8_t** bufpp, unsigned short s )
{
    s = htons( s );
    memcpy( *bufpp, &s, sizeof(s) );
    *bufpp += sizeof(s);
}

void
CookieRef::store_message( HostID dest, const uint8_t* buf, 
                          unsigned int len )
{
    logf( XW_LOGVERBOSE0, "%s: storing msg size %d for dest %d", __func__,
          len, dest );
    DBMgr::Get()->StoreMessage( ConnName(), dest, buf, len );
}

void
CookieRef::send_stored_messages( HostID dest, const AddrInfo* addr )
{
    logf( XW_LOGVERBOSE0, "%s(dest=%d)", __func__, dest );
    assert( dest > 0 && dest <= 4 );

    if ( addr->isCurrent() ) {
        DBMgr* dbmgr = DBMgr::Get();
        const char* cname = ConnName();
        vector<DBMgr::MsgInfo> msgs;
        dbmgr->GetStoredMessages( cname, dest, msgs );

        vector<int> sentIDs;
        vector<DBMgr::MsgInfo>::const_iterator iter;
        for ( iter = msgs.begin(); addr->isCurrent() && msgs.end() != iter;
              ++iter ) {
            DBMgr::MsgInfo msg = *iter;
            uint32_t packetID;
            if ( !send_with_length( addr, dest, msg.msg.data(), 
                                    msg.msg.size(), true, &packetID ) ) {
                break;
            }
            if ( !UDPAckTrack::setOnAck( onMsgAcked, packetID, 
                                         (void*)msg.msgID() ) ) {
                sentIDs.push_back( msg.msgID() );
            }
        }
        dbmgr->RemoveStoredMessages( sentIDs );
    }
} /* send_stored_messages */

bool
CookieRef::increasePlayerCounts( CRefEvent* evt, bool reconn, HostID* hidp, 
                                 DevIDRelay* devIDp )
{
    DevIDRelay devID = DBMgr::DEVID_NONE;
    int nPlayersH = evt->u.con.nPlayersH;
    int seed = evt->u.con.seed;

    assert( m_nPlayersSought > 0 );
    assert( m_nPlayersSought == evt->u.con.nPlayersS );

    logf( XW_LOGINFO, "%s: nPlayersH=%d", __func__, nPlayersH );

    ASSERT_LOCKED();

    /* Add the players provided by this [re]connect event to the cref after
       performing sanity checks.  If this is an initial connect, then the host
       should be added first.  If it's a recon, any order is possible.  In no
       circumstances should the number of players present exceed the number
       sought (if known.)  Currently some of this stuff is asserted.  Instead
       when bad values are seen the sender should be notified then
       disconnencted.  On the host side the error message should probably
       recommend a new game as things must be pretty f*cked up.  Or somebody's
       mucking with me. */

    if ( !reconn ) {
        m_nPlayersHere += nPlayersH;
        assert( m_nPlayersHere <= m_nPlayersSought );
    }

    const AddrInfo* addr = &evt->addr;
    if ( !!devIDp ) {
        DevIDType devIDType = evt->u.con.devID->m_devIDType;
        // does client support devID
        if ( ID_TYPE_RELAY == devIDType ) { 
            devID = evt->u.con.devID->asRelayID();
        } else if ( ID_TYPE_NONE != devIDType ) { 
            if ( reconn     // should have a relay id; can we look it up?
                 && DBMgr::Get()->FindRelayIDFor( ConnName(), evt->u.con.srcID, 
                                                  seed, evt->u.con.devID, 
                                                  &devID ) ) {
                // do nothing; we have our relayID!!
            } else {
                devID = DBMgr::Get()->RegisterDevice( evt->u.con.devID );
            }
        }
        if ( addr->isUDP() ) {
            DevMgr::Get()->rememberDevice( devID, addr );
        }
        *devIDp = devID;
    }

    HostID hostid =
        DBMgr::Get()->AddToGame( ConnName(), evt->u.con.srcID, 
                                 evt->u.con.clientVersion, nPlayersH, seed, 
                                 addr, devID, reconn );
    evt->u.con.srcID = hostid;
    if ( NULL != hidp ) {
        *hidp = hostid;
    }

    /* first add the rec here, whether it'll get ack'd or not */
    int count = CountSockets();
    logf( XW_LOGINFO, "%s: remembering pair: hostid=%x, "
          "(size=%d)", __func__, hostid, count );

    assert( count < 4 );

    {
        RWWriteLock rwl( &m_socketsRWLock );
        HostRec* hr = m_sockets[hostid-1];
        if ( NULL == hr ) {
            hr = new HostRec( hostid, addr, nPlayersH, seed, !reconn );
            m_sockets[hostid-1] = hr;
        } else {
            hr->update( addr, nPlayersH, seed, !reconn );
        }
        logf( XW_LOGINFO, "%s: adding socket rec with ts %lx", __func__, 
              addr->created() );
    }

    printSeeds(__func__);

    logf( XW_LOGVERBOSE1, "%s: here=%d; total=%d", __func__,
          m_nPlayersHere, m_nPlayersSought );

    return true;
} /* increasePlayerCounts */

void
CookieRef::updateAck( HostID hostID, bool keep )
{
    assert( hostID >= HOST_ID_SERVER );
    assert( hostID <= 4 );
    const AddrInfo* nonKeeper = NULL;

    cancelAckTimer( hostID );

    {
        RWWriteLock rwl( &m_socketsRWLock );
        HostRec* hr = m_sockets[hostID-1];
        if ( !!hr && hr->m_ackPending ) {
            if ( keep ) {
                hr->m_ackPending = false;
                DBMgr::Get()->NoteAckd( ConnName(), hostID );
            } else {
                nonKeeper = &hr->m_addr;
            }
        }
    }

    if ( NULL != nonKeeper ) {
        removeSocket( nonKeeper );
    }

    printSeeds(__func__);
}

void
CookieRef::postCheckAllHere()
{
    if ( DBMgr::Get()->AllDevsAckd( ConnName() ) ) {
        CRefEvent evt( XWE_ALLHERE );
        m_eventQueue.push_back( evt );
    }
}

void
CookieRef::postDropDevice( HostID hostID )
{
    CRefEvent evt( XWE_ACKTIMEOUT );
    evt.u.ack.srcID = hostID;
    m_eventQueue.push_back( evt );
    handleEvents();
}

void
CookieRef::postTellHaveMsgs( const AddrInfo* addr )
{
    CRefEvent evt( XWE_TRYTELL, addr );
    m_eventQueue.push_back( evt );
    assert( m_in_handleEvents );
}

void
CookieRef::setAllConnectedTimer()
{
    time_t inHowLong;
    if ( RelayConfigs::GetConfigs()->GetValueFor( "ALLCONN", &inHowLong ) ) {
        TimerMgr::GetTimerMgr()->SetTimer( inHowLong,
                                           s_checkAllConnected, this, 0 );
    }
}

void
CookieRef::setAckTimer( HostID hid )
{
    ASSERT_LOCKED();
    logf( XW_LOGINFO, "%s(hid=%d)", __func__, hid );

    assert( hid >= HOST_ID_SERVER );
    assert( hid <= 4 );
    --hid;

    assert( NULL == m_timers[hid].m_this );
    m_timers[hid].m_this = this;

    time_t inHowLong;
    if ( RelayConfigs::GetConfigs()->GetValueFor( "DEVACK", &inHowLong ) ) {
        TimerMgr::GetTimerMgr()->SetTimer( inHowLong,
                                           s_checkAck, &m_timers[hid], 0 );
    } else {
        logf( XW_LOGINFO, "not setting timer" );
    }
}

void
CookieRef::cancelAckTimer( HostID hid )
{
    ASSERT_LOCKED();
    logf( XW_LOGINFO, "%s(hid=%d)", __func__, hid );

    assert( hid >= HOST_ID_SERVER );
    assert( hid <= 4 );
    m_timers[hid-1].m_this = NULL;
    
    // TimerMgr::GetTimerMgr()->ClearTimer( s_checkAck, this );
}

void
CookieRef::cancelAllConnectedTimer()
{
    TimerMgr::GetTimerMgr()->ClearTimer( s_checkAllConnected, this );
}

void
CookieRef::sendResponse( const CRefEvent* evt, bool initial, 
                         const DevIDRelay* devID )
{
    /* Now send the response */
    uint8_t buf[1       /* cmd */
                      + sizeof(uint8_t) /* hostID */
                      + sizeof(short) /* cookidID */
                      + sizeof(short) /* heartbeat */
                      + sizeof(uint8_t) /* total here */
                      + sizeof(uint8_t) /* total expected */
                      + 1 + MAX_CONNNAME_LEN
                      + 1 + 1 + MAX_DEVID_LEN
    ];

    uint8_t* bufp = buf;

    *bufp++ = initial ? XWRELAY_CONNECT_RESP : XWRELAY_RECONNECT_RESP;
    *bufp++ = evt->u.con.srcID;
    putNetShort( &bufp, GetCid() );
    putNetShort( &bufp, GetHeartbeat() );
    int nTotal, nHere;
    DBMgr::Get()->GetPlayerCounts( ConnName(), &nTotal, &nHere );
    *bufp++ = nTotal;
    *bufp++ = nHere;

    const char* connName = ConnName();
    assert( !!connName && connName[0] );
    size_t len = strlen( connName );
    assert( len < MAX_CONNNAME_LEN );
    *bufp++ = (char)len;
    memcpy( bufp, connName, len );
    bufp += len;

    // we always write at least empty string

    // If client supports devid, and we have one (response case), write it as
    // 8-byte hex string plus a length byte -- but only if we didn't already
    // receive it.

    // there are three possibilities: it sent us a platform-specific ID and we
    // need to return the relay version; or it sent us a valid relay version;
    // or it sent us an invalid one (for whatever reason, e.g. we've wiped the
    // devices table entry for a problematic GCM id to force reregistration.)
    // In the first case, we return the new relay version.  In the second, we
    // return that the type is ID_TYPE_RELAY but don't bother with the version
    // string; and in the third, we return ID_TYPE_NONE.

    if ( DBMgr::DEVID_NONE == *devID ) { // first case
        *bufp++ = ID_TYPE_NONE;
    } else {
        *bufp++ = ID_TYPE_RELAY;

        // Write an empty string if the client passed the ID to us, or the id
        // if it's new to the client.
        char idbuf[MAX_DEVID_LEN + 1];
        if ( !!ID_TYPE_RELAY < evt->u.con.devID->m_devIDType ) { 
            len = snprintf( idbuf, sizeof(idbuf), "%.8X", *devID );
            assert( len < sizeof(idbuf) );
        } else {
            len = 0;
        }
        *bufp++ = (char)len;
        if ( 0 < len ) {
            memcpy( bufp, idbuf, len );
            bufp += len;
        }
    }

    send_with_length( &evt->addr, evt->u.con.srcID, buf, bufp - buf, true );
    logf( XW_LOGVERBOSE0, "sent %s", cmdToStr( XWRELAY_Cmd(buf[0]) ) );
} /* sendResponse */

void
CookieRef::sendAnyStored( const CRefEvent* evt )
{
    HostID dest = evt->u.con.srcID;
    if ( HOST_ID_NONE != dest ) {
        send_stored_messages( dest, &evt->addr );
    }
}

typedef struct _StoreData {
    string connName;
    HostID dest;
    uint8_t* buf;
    int buflen;
} StoreData;

void
CookieRef::storeNoAck( bool acked, uint32_t packetID, void* data )
{
    StoreData* sdata = (StoreData*)data;
    if ( !acked ) {
        DBMgr::Get()->StoreMessage( sdata->connName.c_str(), sdata->dest, 
                                    sdata->buf, sdata->buflen );
    }
    free( sdata->buf );
    delete sdata;
}

void
CookieRef::forward_or_store( const CRefEvent* evt )
{
    const uint8_t* cbuf = evt->u.fwd.buf;
    do {
        int buflen = evt->u.fwd.buflen;
        uint8_t buf[buflen];
        if ( *cbuf == XWRELAY_MSG_TORELAY ) {
            buf[0] = XWRELAY_MSG_FROMRELAY;
        } else if ( *cbuf == XWRELAY_MSG_TORELAY_NOCONN ) {
            *buf = XWRELAY_MSG_FROMRELAY_NOCONN;
        } else {
            logf( XW_LOGERROR, "%s: got XWRELAY type of %d", __func__,
                  *buf );
            break;
        }

        memcpy( &buf[1], &cbuf[1], buflen-1 );

        HostID dest = evt->u.fwd.dest;
        const AddrInfo* destAddr = SocketForHost( dest );

        if ( 0 < m_delayMicros && NULL != destAddr ) {
            usleep( m_delayMicros );
        }

        uint32_t packetID = 0;
        if ( (NULL == destAddr)
             || !send_with_length( destAddr, dest, buf, buflen, true, 
                                   &packetID ) ) {
            store_message( dest, buf, buflen );
        } else if ( 0 != packetID ) { // sent via UDP
            StoreData* data = new StoreData;
            data->connName = m_connName;
            data->dest = dest;
            data->buf = (uint8_t*)malloc( buflen );
            memcpy( data->buf, buf, buflen );
            data->buflen = buflen;
            UDPAckTrack::setOnAck( storeNoAck, packetID, data );
        }

        // If recipient GAME isn't connected, see if owner device is and can
        // receive
        if ( NULL == destAddr) {
            DevIDRelay devid;
            AddrInfo::ClientToken token;
            if ( DBMgr::Get()->TokenFor( ConnName(), dest, &devid, &token ) ) {
                const AddrInfo::AddrUnion* saddr = DevMgr::Get()->get( devid );
                if ( !!saddr ) {
                    AddrInfo addr( token, saddr );
                    postTellHaveMsgs( &addr );
                }
            }
        }

        /* also note that we've heard from src recently */
        HostID src = evt->u.fwd.src;
        DBMgr::Get()->RecordAddress( ConnName(), src, &evt->addr );
#ifdef RELAY_HEARTBEAT
        pushHeartbeatEvent( src, SocketForHost(src) );
#endif
    } while ( 0 );
} /* forward_or_store */

void
CookieRef::send_denied( const CRefEvent* evt, XWREASON why )
{
    denyConnection( &evt->addr, why );
}

void
CookieRef::send_msg( const AddrInfo* addr, HostID hid, 
                     XWRelayMsg msg, XWREASON why, bool cascade )
{
    uint8_t buf[10];
    short tmp;
    unsigned int len = 0;
    buf[len++] = msg;

    switch ( msg ) {
    case XWRELAY_DISCONNECT_OTHER:
        buf[len++] = why;
        tmp = htons( hid );
        memcpy( &buf[len], &tmp, 2 );
        len += 2;
        break;
    default:
        logf( XW_LOGINFO, "not handling message %d", msg );
        assert(0);
    }

    assert( len <= sizeof(buf) );
    send_with_length( addr, HOST_ID_NONE, buf, len, cascade );
} /* send_msg */

void
CookieRef::notifyOthers( const AddrInfo* addr, XWRelayMsg msg, XWREASON why )
{
    assert( addr->socket() != 0 );

    ASSERT_LOCKED();
    RWReadLock rrl( &m_socketsRWLock );
    for ( unsigned int ii = 0; ii < VSIZE(m_sockets); ++ii ) {
        HostRec* hr = m_sockets[ii];
        if ( !!hr ) {
            const AddrInfo* other = &hr->m_addr;
            if ( !other->equals( *addr ) ) {
                send_msg( other, ii + 1, msg, why, false );
            }
        }
    }
} /* notifyOthers */

void
CookieRef::notifyGameDead( const AddrInfo* addr )
{
    uint8_t buf[] = { 
        XWRELAY_MSG_STATUS
        ,XWRELAY_ERROR_DELETED
    };

    send_with_length( addr, HOST_ID_NONE, buf, sizeof(buf), true );
}

/* void */
/* CookieRef::moveSockets( void ) */
/* { */
/*     ASSERT_LOCKED(); */

/*     vector<int> sockets; */
/*     map<int,HostRec>::iterator iter; */
/*     for ( iter = m_sockets.begin(); iter != m_sockets.end(); ++iter ) {  */
/*         sockets.push_back( iter->m_socket ); */
/*     } */

/*     CRefMgr::Get()->MoveSockets( sockets, this ); */
/* } */

void
CookieRef::sendAllHere( bool initial )
{
    uint8_t buf[1 + 1     /* hostID */
                      + 1 + MAX_CONNNAME_LEN];

    uint8_t* bufp = buf;
    uint8_t* idLoc;
    
    *bufp++ = XWRELAY_ALLHERE;
    idLoc = bufp++;                 /* space for hostId, remembering address */

    const char* connName = ConnName();
    assert( !!connName && connName[0] );
    int len = strlen( connName );
    assert( len < MAX_CONNNAME_LEN );
    *bufp++ = (char)len;
    memcpy( bufp, connName, len );
    bufp += len;

    ASSERT_LOCKED();

    /* Assuming destIds in range 1 .. nSought, for each find if it's here and
       if it is try sending to it.  If fail, or it's not here, store the
       message for it.  Would be better if could look up rather than run
       through the vector each time. */
    HostID dest;
    for ( dest = 1; dest <= m_nPlayersSought; ++dest ) {
        bool sent = false;
        *idLoc = dest;   /* write in this target's hostId */

        {
            RWReadLock rrl( &m_socketsRWLock );
            HostRec* hr = m_sockets[dest-1];
            if ( !!hr ) {
                sent = send_with_length( &hr->m_addr, dest, buf, 
                                         bufp-buf, true );
            }
        }
        if ( !sent ) {
            store_message( dest, buf, bufp-buf );
        }
    }
} /* sendAllHere */

#define CONNNAME_DELIM ' '      /* ' ' so will wrap in browser */

void
CookieRef::assignConnName( void )
{
    if ( '\0' == ConnName()[0] ) {
        m_connName += /*CONNNAME_DELIM + */PermID::GetNextUniqueID();

        logf( XW_LOGINFO, "%s: assigning name: %s", __func__, ConnName() );
    } else {
        logf( XW_LOGINFO, "%s: already named: %s", __func__, ConnName() );
    }
}

void
CookieRef::disconnectSockets( XWREASON why )
{
    ASSERT_LOCKED();
    RWReadLock rrl( &m_socketsRWLock );
    for ( unsigned int ii = 0; ii < VSIZE(m_sockets); ++ii ) {
        HostRec* hr = m_sockets[ii];
        if ( !!hr ) {
            const AddrInfo* addr = &hr->m_addr;
            if ( addr->socket() != 0 ) {
                disconnectSocket( addr, why );
            } else {
                assert( 0 );
            }
        }
    }
}

void
CookieRef::disconnectSocket( const AddrInfo* addr, XWREASON why )
{
    ASSERT_LOCKED();
    pushNotifyDisconEvent( addr, why );
    pushRemoveSocketEvent( addr );
} /* disconnectSocket */

void
CookieRef::removeDevice( const CRefEvent* const evt )
{
    DBMgr* dbmgr = DBMgr::Get();
    if ( dbmgr->HaveDevice( ConnName(), evt->u.devgone.hid,
                            evt->u.devgone.seed ) ) {
        dbmgr->KillGame( ConnName(), evt->u.devgone.hid );

        RWReadLock rrl( &m_socketsRWLock );
        for ( unsigned int ii = 0; ii < VSIZE(m_sockets); ++ii ) {
            HostRec* hr = m_sockets[ii];
            if ( !!hr ) {
                notifyGameDead( &hr->m_addr );
            }
        }
    }
}

void
CookieRef::noteHeartbeat( const CRefEvent* evt )
{
    const AddrInfo& addr = evt->addr;
    HostID id = evt->u.heart.id;

    ASSERT_LOCKED();
    RWWriteLock rwl( &m_socketsRWLock );
    HostRec* hr = m_sockets[id-1];
    if ( !!hr ) {
        if ( hr->m_addr.equals(addr) ) {
            logf( XW_LOGVERBOSE1, "upping m_lastHeartbeat from %d to %d",
                  hr->m_lastHeartbeat, uptime() );
            hr->m_lastHeartbeat = uptime();
        } else {
            /* PENDING If the message came on an unexpected socket, kill the
               connection.  An attack is the most likely explanation.  But:
               now it's happening after a crash and clients reconnect. */
            logf( XW_LOGERROR, "wrong socket record for HostID %x; "
                  "wanted %d, found %d", id, addr.socket(), 
                  hr->m_addr.socket() );
        }
    } else {
        logf( XW_LOGERROR, "no socket for HostID %x", id );
    }
} /* noteHeartbeat */

/* timer callback */
/* static */ void
CookieRef::s_checkAllConnected( void* closure )
{
    /* Need to ensure */
    CookieRef* self = (CookieRef*)closure;
    SafeCref scr(self->GetCid(), false );
    scr.CheckAllConnected();
}

/* static */ void
CookieRef::s_checkAck( void* closure )
{
    AckTimer* at = (AckTimer*)closure;
    CookieRef* self = at->m_this;
    if ( NULL != self ) {
        at->m_this = NULL;
        SafeCref scr(self->GetCid(), false );
        scr.CheckNotAcked( at->m_hid );
    }
}

void
CookieRef::_CheckAllConnected()
{
    logf( XW_LOGVERBOSE0, "%s", __func__ );
/*     MutexLock ml( &m_EventsMutex ); */
    CRefEvent newEvt( XWE_CONNTIMER );
    m_eventQueue.push_back( newEvt );
    handleEvents();
}

void
CookieRef::_CheckNotAcked( HostID hid )
{
    logf( XW_LOGINFO, "%s(hid=%d)", __func__, hid );
    CRefEvent newEvt( XWE_ACKTIMEOUT );
    newEvt.u.ack.srcID = hid;
    m_eventQueue.push_back( newEvt );
    handleEvents();
}

void
CookieRef::printSeeds( const char* caller )
{
    int len = 0;
    char buf[64] = {0};

    RWReadLock rrl( &m_socketsRWLock );
    for ( unsigned int ii = 0; ii < VSIZE(m_sockets); ++ii ) {
        HostRec* hr = m_sockets[ii];
        if ( !!hr ) {
            len += snprintf( &buf[len], sizeof(buf)-len, "[%d]%.4x(%d)/%d/%c ", 
                             ii + 1, hr->m_seed, 
                             hr->m_seed, hr->m_addr.socket(), 
                             hr->m_ackPending?'a':'A' );
        }
    }
    logf( XW_LOGINFO, "%s: seeds/sockets/ack'd after %s(): %s", __func__, caller, buf );
}

void
CookieRef::logf( XW_LogLevel level, const char* format, ... )
{
    char buf[256];
    int len;

    len = snprintf( buf, sizeof(buf), "cid:%d(%s) ", m_cid, m_connName.c_str() );

    va_list ap;
    va_start( ap, format );
    vsnprintf( buf + len, sizeof(buf) - len, format, ap );
    va_end(ap);

    ::logf( level, buf );
}

void
CookieRef::_PrintCookieInfo( string& out )
{
    out += "Cookie=";
    out += Cookie();
    out += "\n";
    out += "connName=";
    char buf[MAX_CONNNAME_LEN+MAX_INVITE_LEN];

    snprintf( buf, sizeof(buf), "%s\n", ConnName() );
    out += buf;

    snprintf( buf, sizeof(buf), "id=%d\n", GetCid() );
    out += buf;

    snprintf( buf, sizeof(buf), "Total players=%d\n", m_nPlayersSought );
    out += buf;
    snprintf( buf, sizeof(buf), "Players here=%d\n", m_nPlayersHere );
    out += buf;

    snprintf( buf, sizeof(buf), "State=%s\n", stateString( m_curState ) );
    out += buf;

    /* Timer state: how long since last heartbeat; how long til disconn timer
       fires. */

    /* n messages */
    /* open since when */

    ASSERT_LOCKED();
    snprintf( buf, sizeof(buf), "Hosts connected=%d; cur time = %ld\n", 
              CountSockets(), uptime() );
    out += buf;

    RWReadLock rrl( &m_socketsRWLock );
    for ( unsigned int ii = 0; ii < VSIZE(m_sockets); ++ii ) {
        HostRec* hr = m_sockets[ii];
        if ( !!hr ) {
            snprintf( buf, sizeof(buf), "  HostID=%d; socket=%d;last hbeat=%ld\n", 
                      ii + 1, hr->m_addr.socket(), 
                      hr->m_lastHeartbeat );
            out += buf;
        }
    }

} /* PrintCookieInfo */

void
CookieRef::_FormatHostInfo( string* hostIds, string* seeds, string* addrs )
{
    ASSERT_LOCKED();
    RWReadLock rrl( &m_socketsRWLock );
    for ( unsigned int ii = 0; ii < VSIZE(m_sockets); ++ii ) {
        HostRec* hr = m_sockets[ii];
        if ( !!hr ) {
            if ( !!hostIds ) {
                char buf[8];
                snprintf( buf, sizeof(buf), "%d ", ii + 1 );
                *hostIds += buf;
            }

            if ( !!seeds ) {
                char buf[6];
                snprintf( buf, sizeof(buf), "%.4X ", hr->m_seed );
                *seeds += buf;
            }

            if ( !!addrs ) {
                int socket = hr->m_addr.socket();
                sockaddr_in name;
                socklen_t siz = sizeof(name);
                if ( 0 == getpeername( socket, (struct sockaddr*)&name, &siz) ) {
                    char buf[32] = {0};
                    snprintf( buf, sizeof(buf), "%s ", inet_ntoa(name.sin_addr) );
                    *addrs += buf;
                }
            }
        }
    }
}
