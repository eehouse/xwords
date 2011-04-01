/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */

/* 
 * Copyright 2005-2009 by Eric House (xwords@eehouse.org).  All rights
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
#include "permid.h"

using namespace std;

/*****************************************************************************
 * SocketsIterator class
 *****************************************************************************/

SocketsIterator::SocketsIterator( SocketMap::iterator iter,
                                  SocketMap::iterator end,
                                  pthread_mutex_t* mutex )
    : m_iter( iter )
    , m_end( end )
    , m_mutex( mutex )
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

#define ASSERT_LOCKED() \
    assert( m_locking_thread == pthread_self() )

void
CookieRef::ReInit( const char* cookie, const char* connName, CookieID id,
                   int langCode, int nPlayers, int nAlreadyHere )
{
    m_cookie = cookie==NULL?"":cookie;
    m_connName = connName==NULL?"":connName;
    m_cookieID = id;
    m_curState = XWS_INITED;
    m_nPlayersSought = nPlayers;
    m_nPlayersHere = nAlreadyHere;
    m_locking_thread = 0;
    m_starttime = uptime();
    m_in_handleEvents = false;
    m_langCode = langCode;
    m_nPendingAcks = 0;

    if ( RelayConfigs::GetConfigs()->GetValueFor( "SEND_DELAY_MILLIS", 
                                                   &m_delayMicros ) ) {
        m_delayMicros *= 1000;  /* millis->micros */
    } else {
        m_delayMicros = 0;
    }
    RelayConfigs::GetConfigs()->GetValueFor( "HEARTBEAT", &m_heatbeat );
    logf( XW_LOGINFO, "initing cref for cookie %s, connName %s",
          m_cookie.c_str(), m_connName.c_str() );
}


CookieRef::CookieRef( const char* cookie, const char* connName, CookieID id,
                      int langCode, int nPlayersT, int nAlreadyHere )
{
    pthread_mutex_init( &m_mutex, NULL );
    ReInit( cookie, connName, id, langCode, nPlayersT, nAlreadyHere );
}

CookieRef::~CookieRef()
{
    cancelAllConnectedTimer();

    /* get rid of any sockets still contained */
    XWThreadPool* tPool = XWThreadPool::GetTPool();

    ASSERT_LOCKED();
    vector<HostRec>::iterator iter;
    for ( iter = m_sockets.begin(); iter != m_sockets.end(); ++iter ) {
        int socket = iter->m_socket;
        tPool->CloseSocket( socket );
        m_sockets.erase( iter );
    }

    printSeeds(__func__);

    logf( XW_LOGINFO, "CookieRef for %d being deleted", m_cookieID );
} /* ~CookieRef */

void
CookieRef::Clear(void)
{
    m_cookie = "";
    m_connName = "";
    m_cookieID = 0;
    m_eventQueue.clear();
} /* Clear */

bool
CookieRef::Lock( void ) 
{
    bool success = true;

    pthread_mutex_lock( &m_mutex );

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
        pthread_mutex_unlock( &m_mutex );
    }

    return success;
} /* CookieRef::Lock */

void
CookieRef::Unlock() { 
    assert( m_locking_thread == pthread_self() );
    m_locking_thread = 0;
    pthread_mutex_unlock( &m_mutex ); 
}

bool
CookieRef::_Connect( int socket, int nPlayersH, int nPlayersS, int seed )
{
    bool connected = false;
    if ( AlreadyHere( seed, socket ) ) {
        connected = true;       /* but drop the packet */
    /* } else if ( AlreadyHere( seed, -1 ) ) { */
        /* dupe packet on different socket; need host record */
    } else if ( CRefMgr::Get()->Associate( socket, this ) ) {
        pushConnectEvent( socket, nPlayersH, nPlayersS, seed );
        handleEvents();
        connected = HasSocket_locked( socket );
    } else {
        logf( XW_LOGINFO, "dropping connect event; already connected" );
    }
    return connected;
}

void
CookieRef::_Reconnect( int socket, HostID hid, int nPlayersH, int nPlayersS,
                       int seed, bool gameDead )
{
    if ( AlreadyHere( hid, seed, socket ) ) {
        logf( XW_LOGINFO, "dropping reconnection because already here" );
    } else {
        (void)CRefMgr::Get()->Associate( socket, this );
        pushReconnectEvent( socket, hid, nPlayersH, nPlayersS, seed );
    }
    if ( gameDead ) {
        pushGameDead( socket );
    }
    handleEvents();
}

void
CookieRef::_HandleAck( HostID hostID )
{
    assert( m_nPendingAcks > 0 && m_nPendingAcks <= 4 );
    CRefEvent evt( XWE_GOTONEACK );
    evt.u.ack.srcID = hostID;
    m_eventQueue.push_back( evt );
    handleEvents();
}

void
CookieRef::_Disconnect( int socket, HostID hostID )
{
    logf( XW_LOGINFO, "%s(socket=%d, hostID=%d)", __func__, socket, hostID );
    CRefMgr::Get()->Disassociate( socket, this );

    CRefEvent evt( XWE_DISCONN );
    evt.u.discon.socket = socket;
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
CookieRef::HostForSocket( int sock )
{
    HostID hid = -1;
    ASSERT_LOCKED();
    vector<HostRec>::const_iterator iter;
    for ( iter = m_sockets.begin(); iter != m_sockets.end(); ++iter ) {
        if ( iter->m_socket == sock ) {
            hid = iter->m_hostID;
            logf( XW_LOGINFO, "%s: assigning hid of %d", __func__, hid );
            break;
        }
    }
    return hid;
}

int
CookieRef::SocketForHost( HostID dest )
{
    int socket = -1;
    ASSERT_LOCKED();
    vector<HostRec>::const_iterator iter;
    assert( dest != 0 );        /* don't use as lookup before assigned */
    for ( iter = m_sockets.begin(); iter != m_sockets.end(); ++iter ) {
        if ( iter->m_hostID == dest ) {
            socket = iter->m_socket;
            break;
        }
    }

    logf( XW_LOGVERBOSE0, "returning socket=%d for hostid=%x", socket, dest );
    return socket;
}

bool 
CookieRef::AlreadyHere( unsigned short seed, int socket )
{
    logf( XW_LOGINFO, "%s(seed=%x,socket=%d)", __func__, seed, socket );
    bool here = false;

    vector<HostRec>::iterator iter;
    for ( iter = m_sockets.begin(); iter != m_sockets.end(); ++iter ) {
        if ( iter->m_seed == seed ) { /* client already registered */
            if ( iter->m_socket == socket ) {
                /* dup packet */
                here = true;
            } else {
                logf( XW_LOGINFO, "%s: seeds match; nuking existing record"
                      " for socket %d b/c assumed closed", __func__, 
                      iter->m_socket );
                m_sockets.erase( iter );
            }
            break;
        }
    }
    
    logf( XW_LOGINFO, "%s=>%d", __func__, here );
    return here;
}

bool 
CookieRef::AlreadyHere( HostID hid, unsigned short seed, int socket )
{
    logf( XW_LOGINFO, "%s(hid=%d,seed=%x,socket=%d)", __func__, 
          hid, seed, socket );
    bool here = false;

    vector<HostRec>::iterator iter;
    for ( iter = m_sockets.begin(); iter != m_sockets.end(); ++iter ) {
        if ( iter->m_hostID == hid ) {
            assert( seed == iter->m_seed );
            if ( socket == iter->m_socket ) {
                here = true;    /* dup packet */
            } else {
                logf( XW_LOGINFO, "%s: hids match; nuking existing record"
                      "for socket %d b/c assumed closed", __func__, 
                      iter->m_socket );
                m_sockets.erase( iter );
            }
            break;
        }
    }
    
    logf( XW_LOGINFO, "%s=>%d", __func__, here );
    return here;
}

void
CookieRef::notifyDisconn( const CRefEvent* evt )
{
    int socket = evt->u.disnote.socket;
    unsigned char buf[] = { 
        XWRELAY_DISCONNECT_YOU,
        evt->u.disnote.why 
    };

    send_with_length( socket, buf, sizeof(buf), true );
} /* notifyDisconn */

void
CookieRef::removeSocket( int socket )
{
    logf( XW_LOGINFO, "%s(socket=%d)", __func__, socket );
    int count;
    {
        bool found = false;
        ASSERT_LOCKED();

        count = m_sockets.size();
        assert( count <= 4 );
        if ( count > 0 ) {
            vector<HostRec>::iterator iter;
            for ( iter = m_sockets.begin(); 
                  !found && iter != m_sockets.end(); ++iter ) {
                if ( iter->m_socket == socket ) {
                    if ( iter->m_ackPending ) {
                        logf( XW_LOGINFO,
                              "Never got ack; removing %d players from DB",
                              iter->m_nPlayersH );
                        DBMgr::Get()->RmDevice( ConnName(), iter->m_hostID );
                        m_nPlayersHere -= iter->m_nPlayersH;
                        --m_nPendingAcks;
                    }
                    m_sockets.erase(iter);
                    --count;
                    found = true;
                }
            }
        } else {
            logf( XW_LOGERROR, "%s: no socket %d to remove", __func__, 
                  socket );
        }
        if ( !found ) {
            logf( XW_LOGINFO, "%s: socket %d not found", __func__, socket );
        }
    }

    printSeeds(__func__);

    if ( count == 0 ) {
        pushLastSocketGoneEvent();
    }
} /* removeSocket */

bool
CookieRef::HasSocket( int socket )
{
    bool result = Lock();
    if ( result ) {
        result = HasSocket_locked( socket );
        Unlock();
    }
    return result;
}

bool
CookieRef::HasSocket_locked( int socket )
{
    bool found = false;

    ASSERT_LOCKED();
    vector<HostRec>::iterator iter;
    for ( iter = m_sockets.begin(); iter != m_sockets.end(); ++iter ) {
        if ( iter->m_socket == socket ) {
            found = true;
            break;
        }
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
    vector<HostRec>::iterator iter;
    for ( iter = m_sockets.begin(); iter != m_sockets.end(); ++iter ) {
        time_t last = iter->m_lastHeartbeat;
        if ( (now - last) > GetHeartbeat() ) {
            pushHeartFailedEvent( iter->m_socket );
        }
    }

    handleEvents();
} /* CheckHeartbeats */
#endif

void
CookieRef::_Forward( HostID src, HostID dest, unsigned char* buf, int buflen )
{
    pushForwardEvent( src, dest, buf, buflen );
    handleEvents();
} /* Forward */

void
CookieRef::_Remove( int socket )
{
    pushRemoveSocketEvent( socket );
    handleEvents();
} /* Forward */

void 
CookieRef::pushConnectEvent( int socket, int nPlayersH, int nPlayersS,
                             int seed )
{
    CRefEvent evt;
    evt.type = XWE_DEVCONNECT;
    evt.u.con.socket = socket;
    evt.u.con.srcID = HOST_ID_NONE;
    evt.u.con.nPlayersH = nPlayersH;
    evt.u.con.nPlayersS = nPlayersS;
    evt.u.con.seed = seed;
    m_eventQueue.push_back( evt );
} /* pushConnectEvent */

void 
CookieRef::pushReconnectEvent( int socket, HostID srcID, int nPlayersH,
                               int nPlayersS, int seed )
{
    CRefEvent evt( XWE_RECONNECT );
    evt.u.con.socket = socket;
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
CookieRef::pushForwardEvent( HostID src, HostID dest, 
                             unsigned char* buf, int buflen )
{
    logf( XW_LOGVERBOSE1, "pushForwardEvent: %d -> %d", src, dest );
    CRefEvent evt( XWE_FORWARDMSG );
    evt.u.fwd.src = src;
    evt.u.fwd.dest = dest;
    evt.u.fwd.buf = buf;
    evt.u.fwd.buflen = buflen;
    m_eventQueue.push_back( evt );
}

void
CookieRef::pushRemoveSocketEvent( int socket )
{
    CRefEvent evt( XWE_REMOVESOCKET );
    evt.u.rmsock.socket = socket;
    m_eventQueue.push_back( evt );
}

void
CookieRef::pushNotifyDisconEvent( int socket, XWREASON why )
{
    CRefEvent evt( XWE_NOTIFYDISCON );
    evt.u.disnote.socket = socket;
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
CookieRef::pushGameDead( int socket )
{
    CRefEvent evt( XWE_GAMEDEAD );
    evt.u.discon.socket = socket;
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
        CRefEvent evt = m_eventQueue.front();
        m_eventQueue.pop_front();

        XW_RELAY_ACTION takeAction;
        if ( getFromTable( m_curState, evt.type, &takeAction, &nextState ) ) {

            logf( XW_LOGINFO, "%s: %s -> %s on evt %s, act=%s", __func__,
                  stateString(m_curState), stateString(nextState),
                  eventString(evt.type), actString(takeAction) );

            switch( takeAction ) {

            case XWA_SEND_CONNRSP:
                if ( increasePlayerCounts( &evt, false ) ) {
                    setAllConnectedTimer();
                    sendResponse( &evt, takeAction != XWA_SEND_1ST_RERSP );
                    setAckTimer();
                }
                break;

            case XWA_NOTEACKCHECK:
            case XWA_NOTEACK:
                modPending( &evt, true );
                postCheckAllHere();
                break;

            case XWA_DROPDEVICE:
                modPending( &evt, false );
                break;

            /* case XWA_SEND_1ST_RERSP: */
            /*     if ( increasePlayerCounts( &evt, false ) ) { */
            /*         setAllConnectedTimer(); */
            /*         sendResponse( &evt, takeAction != XWA_SEND_1ST_RERSP ); */
            /*     } */
            /*     break; */

            case XWA_SEND_RERSP:
                increasePlayerCounts( &evt, true );
                sendResponse( &evt, false );
                sendAnyStored( &evt );
                postCheckAllHere();
                break;

            case XWA_SEND_NO_ROOM:
                send_denied( &evt, XWRELAY_ERROR_NO_ROOM );
                break;
            case XWA_SEND_DUP_ROOM:
                send_denied( &evt, XWRELAY_ERROR_DUP_ROOM );
                removeSocket( evt.u.rmsock.socket );
                break;
            case XWA_SEND_TOO_MANY:
                send_denied( &evt, XWRELAY_ERROR_TOO_MANY );
                removeSocket( evt.u.rmsock.socket );
                break;

            case XWA_FWD:
                forward_or_store( &evt );
                break;

            case XWA_TIMERDISCONN:
                disconnectSockets( 0, XWRELAY_ERROR_TIMEOUT );
                break;

            case XWA_SHUTDOWN:
                disconnectSockets( 0, XWRELAY_ERROR_SHUTDOWN );
                break;

            case XWA_HEARTDISCONN:
                notifyOthers( evt.u.heart.socket, XWRELAY_DISCONNECT_OTHER, 
                              XWRELAY_ERROR_HEART_OTHER );
                setAllConnectedTimer();
                // reducePlayerCounts( evt.u.discon.socket );
                disconnectSockets( evt.u.heart.socket, 
                                   XWRELAY_ERROR_HEART_YOU );
                break;

            case XWA_DISCONNECT:
                setAllConnectedTimer();
                // reducePlayerCounts( evt.u.discon.socket );
                notifyOthers( evt.u.discon.socket, XWRELAY_DISCONNECT_OTHER,
                              XWRELAY_ERROR_OTHER_DISCON );
                removeSocket( evt.u.discon.socket );
                /* Don't notify.  This is a normal part of a game ending. */
                break;

            case XWA_RMDEV:
                removeDevice( &evt );
                break;

            case XWA_TELLGAMEDEAD:
                notifyGameDead( evt.u.discon.socket );
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
                    notifyOthers( evt.u.rmsock.socket, XWRELAY_DISCONNECT_OTHER,
                                  XWRELAY_ERROR_LOST_OTHER );
                }
                removeSocket( evt.u.rmsock.socket );
                break;

            case XWA_SENDALLHERE:
                CRefMgr::Get()->IncrementFullCount();
                cancelAllConnectedTimer();
                sendAllHere( true );
                /* checkSomeMissing(); */
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
        } else {
            logf( XW_LOGERROR, "Killing cref b/c unable to find transition "
                  "from %s on event %s", stateString(m_curState),
                  eventString(evt.type) );
            assert(0);
            CRefEvent shutevt( XWE_SHUTDOWN );
            m_eventQueue.push_back( shutevt );
        }
    }
    m_in_handleEvents = false;
} /* handleEvents */

bool
CookieRef::send_with_length( int socket, unsigned char* buf, int bufLen,
                             bool cascade )
{
    bool failed = false;
    if ( send_with_length_unsafe( socket, buf, bufLen ) ) {
        DBMgr::Get()->RecordSent( ConnName(), HostForSocket(socket), bufLen );
    } else {
        failed = true;
    }

    if ( failed && cascade ) {
        pushRemoveSocketEvent( socket );
        XWThreadPool::GetTPool()->CloseSocket( socket );
    }
    return !failed;
} /* send_with_length */

static void
putNetShort( unsigned char** bufpp, unsigned short s )
{
    s = htons( s );
    memcpy( *bufpp, &s, sizeof(s) );
    *bufpp += sizeof(s);
}

void
CookieRef::store_message( HostID dest, const unsigned char* buf, 
                          unsigned int len )
{
    logf( XW_LOGVERBOSE0, "%s: storing msg size %d for dest %d", __func__,
          len, dest );
    DBMgr::Get()->StoreMessage( ConnName(), dest, buf, len );
}

void
CookieRef::send_stored_messages( HostID dest, int socket )
{
    logf( XW_LOGVERBOSE0, "%s(dest=%d)", __func__, dest );

    assert( dest > 0 && dest <= 4 );
    assert( -1 != socket );

    for ( ; ; ) {
        unsigned char buf[MAX_MSG_LEN];
        size_t buflen = sizeof(buf);
        int msgID;
        if ( !DBMgr::Get()->GetStoredMessage( ConnName(), dest, 
                                              buf, &buflen, &msgID ) ) {
            break;
        }
        if ( ! send_with_length( socket, buf, buflen, true ) ) {
            break;
        }
        DBMgr::Get()->RemoveStoredMessages( &msgID, 1 );
    }
} /* send_stored_messages */

bool
CookieRef::increasePlayerCounts( CRefEvent* evt, bool reconn )
{
    int nPlayersH = evt->u.con.nPlayersH;
    int socket = evt->u.con.socket;
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

    evt->u.con.srcID = DBMgr::Get()->AddDevice( ConnName(), evt->u.con.srcID,
                                                nPlayersH, seed );

    HostID hostid = evt->u.con.srcID;

    /* first add the rec here, whether it'll get ack'd or not */
    logf( XW_LOGINFO, "%s: remembering pair: hostid=%x, "
          "socket=%d (size=%d)", 
          __func__, hostid, socket, m_sockets.size());

    assert( m_sockets.size() < 4 );

    HostRec hr( hostid, socket, nPlayersH, seed, !reconn );
    m_sockets.push_back( hr );

    printSeeds(__func__);

    logf( XW_LOGVERBOSE1, "%s: here=%d; total=%d", __func__,
          m_nPlayersHere, m_nPlayersSought );

    return true;
} /* increasePlayerCounts */

void
CookieRef::modPending( const CRefEvent* evt, bool keep )
{
    HostID hostID = evt->u.ack.srcID;
    vector<HostRec>::iterator iter;
    for ( iter = m_sockets.begin(); iter != m_sockets.end(); ++iter ) {
        if ( iter->m_ackPending && iter->m_hostID == hostID ) {
            --m_nPendingAcks;

            if ( keep ) {
                iter->m_ackPending = false;
            } else {
                DBMgr::Get()->RmDevice( ConnName(), iter->m_hostID );
                m_sockets.erase( iter );
            }
            break;
        }
    }
    printSeeds(__func__);
}

void
CookieRef::postCheckAllHere()
{
    if ( m_nPendingAcks == 0 && DBMgr::Get()->GameFull( ConnName() ) ) {
         /* && m_nPlayersHere == m_nPlayersSought ) { /\* complete! *\/ */
        CRefEvent evt( XWE_ALLHERE );
        m_eventQueue.push_back( evt );
    }
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
CookieRef::setAckTimer( void )
{
    logf( XW_LOGINFO, "%s()", __func__ );
    time_t inHowLong;
    if ( RelayConfigs::GetConfigs()->GetValueFor( "DEVACK", &inHowLong ) ) {
        TimerMgr::GetTimerMgr()->SetTimer( inHowLong,
                                           s_checkAck, this, 0 );
        ++m_nPendingAcks;
    } else {
        logf( XW_LOGINFO, "not setting timer" );
    }
}

void
CookieRef::cancelAckTimer( void )
{
    TimerMgr::GetTimerMgr()->ClearTimer( s_checkAck, this );
}

void
CookieRef::cancelAllConnectedTimer()
{
    TimerMgr::GetTimerMgr()->ClearTimer( s_checkAllConnected, this );
}

void
CookieRef::sendResponse( const CRefEvent* evt, bool initial )
{
    int socket = evt->u.con.socket;

    /* Now send the response */
    unsigned char buf[1       /* cmd */
                      + sizeof(unsigned char) /* hostID */
                      + sizeof(short) /* cookidID */
                      + sizeof(short) /* heartbeat */
                      + sizeof(unsigned char) /* total here */
                      + sizeof(unsigned char) /* total expected */
                      + 1 + MAX_CONNNAME_LEN
    ];

    unsigned char* bufp = buf;

    *bufp++ = initial ? XWRELAY_CONNECT_RESP : XWRELAY_RECONNECT_RESP;
    *bufp++ = evt->u.con.srcID;
    putNetShort( &bufp, GetCookieID() );
    putNetShort( &bufp, GetHeartbeat() );
    int nTotal, nHere;
    DBMgr::Get()->GetPlayerCounts( ConnName(), &nTotal, &nHere );
    *bufp++ = nTotal;
    *bufp++ = nHere;

    const char* connName = ConnName();
    assert( !!connName && connName[0] );
    int len = strlen( connName );
    assert( len < MAX_CONNNAME_LEN );
    *bufp++ = (char)len;
    memcpy( bufp, connName, len );
    bufp += len;

    send_with_length( socket, buf, bufp - buf, true );
    logf( XW_LOGVERBOSE0, "sent %s", cmdToStr( XWRELAY_Cmd(buf[0]) ) );
} /* sendResponse */

void
CookieRef::sendAnyStored( const CRefEvent* evt )
{
    HostID dest = evt->u.con.srcID;
    if ( HOST_ID_NONE != dest ) {
        send_stored_messages( dest, evt->u.con.socket );
    }
}

void
CookieRef::forward_or_store( const CRefEvent* evt )
{
    unsigned char* buf = evt->u.fwd.buf;
    int buflen = evt->u.fwd.buflen;
    HostID dest = evt->u.fwd.dest;

    int destSocket = SocketForHost( dest );

    /* This is an ugly hack!!!! */
    *buf = XWRELAY_MSG_FROMRELAY;

    if ( 0 < m_delayMicros && destSocket != -1 ) {
        usleep( m_delayMicros );
    }

    if ( (destSocket == -1)
         || !send_with_length( destSocket, buf, buflen, true ) ) {
        store_message( dest, buf, buflen );
    }

    /* also note that we've heard from src recently */
#ifdef RELAY_HEARTBEAT
    HostID src = evt->u.fwd.src;
    pushHeartbeatEvent( src, SocketForHost(src) );
#endif
} /* forward_or_store */

void
CookieRef::send_denied( const CRefEvent* evt, XWREASON why )
{
    denyConnection( evt->u.con.socket, why );
}

void
CookieRef::send_msg( int socket, HostID id, XWRelayMsg msg, XWREASON why,
                     bool cascade )
{
    unsigned char buf[10];
    short tmp;
    unsigned int len = 0;
    buf[len++] = msg;

    switch ( msg ) {
    case XWRELAY_DISCONNECT_OTHER:
        buf[len++] = why;
        tmp = htons( id );
        memcpy( &buf[len], &tmp, 2 );
        len += 2;
        break;
    default:
        logf( XW_LOGINFO, "not handling message %d", msg );
        assert(0);
    }

    assert( len <= sizeof(buf) );
    send_with_length( socket, buf, len, cascade );
} /* send_msg */

void
CookieRef::notifyOthers( int socket, XWRelayMsg msg, XWREASON why )
{
    assert( socket != 0 );

    ASSERT_LOCKED();
    vector<HostRec>::iterator iter;
    for ( iter = m_sockets.begin(); iter != m_sockets.end(); ++iter ) { 
        int other = iter->m_socket;
        if ( other != socket ) {
            send_msg( other, iter->m_hostID, msg, why, false );
        }
    }
} /* notifyOthers */

void
CookieRef::notifyGameDead( int socket )
{
    unsigned char buf[] = { 
        XWRELAY_MSG_STATUS
        ,XWRELAY_ERROR_DELETED
    };

    send_with_length( socket, buf, sizeof(buf), true );
}

void
CookieRef::moveSockets( void )
{
    ASSERT_LOCKED();

    vector<int> sockets;
    vector<HostRec>::iterator iter;
    for ( iter = m_sockets.begin(); iter != m_sockets.end(); ++iter ) { 
        sockets.push_back( iter->m_socket );
    }

    CRefMgr::Get()->MoveSockets( sockets, this );
}

void
CookieRef::sendAllHere( bool initial )
{
    unsigned char buf[1 + 1     /* hostID */
                      + 1 + MAX_CONNNAME_LEN];

    unsigned char* bufp = buf;
    unsigned char* idLoc;
    
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
    for ( dest = 1; dest <= m_nPlayersHere; ++dest ) {
        bool sent = false;
        *idLoc = dest;   /* write in this target's hostId */
        vector<HostRec>::iterator iter;
        for ( iter = m_sockets.begin(); iter != m_sockets.end(); ++iter ) { 
            if ( iter->m_hostID == dest ) {
                sent = send_with_length( iter->m_socket, buf, bufp-buf, 
                                         true );
                break;
            }
        }

        if ( !sent ) {
            store_message( dest, buf, bufp-buf );
        }
    }
} /* sendAllHere */

#define CONNNAME_DELIM ' '      /* ' ' so will wrap in browser */
/* Does my seed belong as part of existing connName */
bool 
CookieRef::SeedBelongs( int gameSeed )
{
    bool belongs = false;
    const char* ptr = ConnName();
    const char* end = ptr + strlen(ptr);
    assert( '\0' != ptr[0] );
    char buf[5];
    snprintf( buf, sizeof(buf), "%.4X", gameSeed );

    for ( ; *ptr != CONNNAME_DELIM && ptr < end; ptr += 4 ) {
        if ( 0 == strncmp( ptr, buf, 4 ) ) {
            belongs = true;
            break;
        }
    }

    return belongs;
} /* SeedBelongs */

/* does my connName provide a home for seeds already in this connName-less
   ref? */
bool
CookieRef::SeedsBelong( const char* connName )
{
    bool found = true;
    assert( !m_connName[0] );
    const char* delim = strchr( connName, CONNNAME_DELIM );
    assert( !!delim );

    vector<HostRec>::iterator iter;
    for ( iter = m_sockets.begin(); iter != m_sockets.end(); ++iter ) {
        char buf[5];
        snprintf( buf, sizeof(buf), "%.4X", iter->m_seed );
        const char* match = strstr( connName, buf );
        if ( !match || match > delim ) {
            found = false;
            break;
        }
    }

    return found;
} /* SeedsBelong */

void
CookieRef::assignConnName( void )
{
    if ( '\0' == ConnName()[0] ) {

        /* vector<HostRec>::iterator iter; */
        /* for ( iter = m_sockets.begin(); iter != m_sockets.end(); ++iter ) { */
        /*     char buf[5]; */
        /*     snprintf( buf, sizeof(buf), "%.4X", iter->m_seed ); */
        /*     m_connName += buf; */
        /* } */

        m_connName += /*CONNNAME_DELIM + */PermID::GetNextUniqueID();

        logf( XW_LOGINFO, "%s: assigning name: %s", __func__, ConnName() );
    } else {
        logf( XW_LOGINFO, "%s: already named: %s", __func__, ConnName() );
    }
}

void
CookieRef::disconnectSockets( int socket, XWREASON why )
{
    if ( socket == 0 ) {
        ASSERT_LOCKED();
        vector<HostRec>::iterator iter;
        for ( iter = m_sockets.begin(); iter != m_sockets.end(); ++iter ) {
            assert( iter->m_socket != 0 );
            disconnectSockets( iter->m_socket, why );
        }
    } else {
        pushNotifyDisconEvent( socket, why );
        pushRemoveSocketEvent( socket );
    }
} /* disconnectSockets */

void
CookieRef::removeDevice( const CRefEvent* const evt )
{
    DBMgr* dbmgr = DBMgr::Get();
    if ( dbmgr->HaveDevice( ConnName(), evt->u.devgone.hid,
                            evt->u.devgone.seed ) ) {
        dbmgr->KillGame( ConnName(), evt->u.devgone.hid );

        vector<HostRec>::iterator iter;
        for ( iter = m_sockets.begin(); iter != m_sockets.end(); ++iter ) {
            notifyGameDead( iter->m_socket );
        }
    }
}

void
CookieRef::noteHeartbeat( const CRefEvent* evt )
{
    int socket = evt->u.heart.socket;
    HostID id = evt->u.heart.id;

    ASSERT_LOCKED();
    vector<HostRec>::iterator iter;
    for ( iter = m_sockets.begin(); iter != m_sockets.end(); ++iter ) {
        if ( iter->m_hostID == id ) {
            break;
        }
    }

    if ( iter == m_sockets.end() ) {
        logf( XW_LOGERROR, "no socket for HostID %x", id );
    } else {

        int second_socket = iter->m_socket;
        if ( second_socket == socket ) {
            logf( XW_LOGVERBOSE1, "upping m_lastHeartbeat from %d to %d",
                  iter->m_lastHeartbeat, uptime() );
            iter->m_lastHeartbeat = uptime();
        } else {
            /* PENDING If the message came on an unexpected socket, kill the
               connection.  An attack is the most likely explanation.  But:
               now it's happening after a crash and clients reconnect. */
            logf( XW_LOGERROR, "wrong socket record for HostID %x; wanted %d, "
                  "found %d", id, socket, second_socket );
        }
    }
} /* noteHeartbeat */

/* timer callback */
/* static */ void
CookieRef::s_checkAllConnected( void* closure )
{
    /* Need to ensure */
    CookieRef* self = (CookieRef*)closure;
    SafeCref scr(self);
    scr.CheckAllConnected();
}

/* static */ void
CookieRef::s_checkAck( void* closure )
{
    CookieRef* self = (CookieRef*)closure;
    SafeCref scr(self);
    scr.CheckNotAcked();
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
CookieRef::_CheckNotAcked()
{
    logf( XW_LOGINFO, "%s", __func__ );
    if ( m_nPendingAcks > 0 ) {
        CRefEvent newEvt( XWE_ACKTIMEOUT );
        m_eventQueue.push_back( newEvt );
        handleEvents();
    }
}

void
CookieRef::printSeeds( const char* caller )
{
    int len = 0;
    char buf[64] = {0};
    vector<HostRec>::iterator iter;
    for ( iter = m_sockets.begin(); iter != m_sockets.end(); ++iter ) {
        len += snprintf( &buf[len], sizeof(buf)-len, "%.4x/%d ", 
                         iter->m_seed, iter->m_socket );
    }
    logf( XW_LOGINFO, "seeds/sockets after %s(): %s", caller, buf );
}

void
CookieRef::logf( XW_LogLevel level, const char* format, ... )
{
    char buf[256];
    int len;

    len = snprintf( buf, sizeof(buf), "cid:%d ", m_cookieID );

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

    snprintf( buf, sizeof(buf), "id=%d\n", GetCookieID() );
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
              m_sockets.size(), uptime() );
    out += buf;
    vector<HostRec>::iterator iter;
    for ( iter = m_sockets.begin(); iter != m_sockets.end(); ++iter ) {
        snprintf( buf, sizeof(buf), "  HostID=%d; socket=%d;last hbeat=%ld\n", 
                  iter->m_hostID, iter->m_socket, 
                  iter->m_lastHeartbeat );
        out += buf;
    }

} /* PrintCookieInfo */

void
CookieRef::_FormatHostInfo( string* hostIds, string* seeds, string* addrs )
{
    ASSERT_LOCKED();
    vector<HostRec>::iterator iter;
    for ( iter = m_sockets.begin(); iter != m_sockets.end(); ++iter ) {

        if ( !!hostIds ) {
            char buf[8];
            snprintf( buf, sizeof(buf), "%d ", iter->m_hostID );
            *hostIds += buf;
        }

        if ( !!seeds ) {
            char buf[6];
            snprintf( buf, sizeof(buf), "%.4X ", iter->m_seed );
            *seeds += buf;
        }

        if ( !!addrs ) {
            int s = iter->m_socket;
            struct sockaddr_in name;
            socklen_t siz = sizeof(name);
            if ( 0 == getpeername( s, (struct sockaddr*)&name, &siz) ) {
                char buf[32] = {0};
                snprintf( buf, sizeof(buf), "%s ", inet_ntoa(name.sin_addr) );
                *addrs += buf;
            }
        }
    }
}
