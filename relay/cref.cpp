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
CookieRef::ReInit( const char* cookie, const char* connName, CookieID id )
{
    m_cookie = cookie==NULL?"":cookie;
    m_connName = connName==NULL?"":connName;
    m_cookieID = id;
    m_totalSent = 0;
    m_curState = XWS_INITED;
    m_nextState = XWS_INITED;
    m_nextHostID = HOST_ID_SERVER;
    m_nPlayersSought = 0;
    m_nPlayersHere = 0;
    m_locking_thread = 0;
    m_starttime = uptime();

    RelayConfigs::GetConfigs()->GetValueFor( "HEARTBEAT", &m_heatbeat );
    logf( XW_LOGINFO, "initing cref for cookie %s, connName %s",
          m_cookie.c_str(), m_connName.c_str() );
}


CookieRef::CookieRef( const char* cookie, const char* connName, CookieID id )
{
    pthread_mutex_init( &m_mutex, NULL );
    ReInit( cookie, connName, id );
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

    logf( XW_LOGINFO, 
          "CookieRef for %d being deleted; sent %d bytes over lifetime", 
          m_cookieID, m_totalSent );
} /* ~CookieRef */

void
CookieRef::Clear(void)
{
    m_cookie = "";
    m_connName = "";
    m_cookieID = 0;
    m_eventQueue.clear();
}

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

void
CookieRef::_Connect( int socket, HostID hid, int nPlayersH, int nPlayersT )
{
    if ( CRefMgr::Get()->Associate( socket, this ) ) {
        pushConnectEvent( socket, hid, nPlayersH, nPlayersT );
        handleEvents();
    } else {
        logf( XW_LOGINFO, "dropping connect event; already connected" );
    }
}

void
CookieRef::_Reconnect( int socket, HostID hid, int nPlayersH, int nPlayersT )
{
    (void)CRefMgr::Get()->Associate( socket, this );
    pushReconnectEvent( socket, hid, nPlayersH, nPlayersT );
    handleEvents();
}

void
CookieRef::_Disconnect( int socket, HostID hostID )
{
    logf( XW_LOGINFO, "%s(socket=%d, hostID=%d)", __func__, socket, hostID );
    CRefMgr::Get()->Disassociate( socket, this );

    CRefEvent evt;
    evt.type = XWE_DISCONNMSG;
    evt.u.discon.socket = socket;
    evt.u.discon.srcID = hostID;
    m_eventQueue.push_back( evt );

    handleEvents();
}

void
CookieRef::_Shutdown()
{
    CRefEvent evt;
    evt.type = XWE_SHUTDOWN;
    m_eventQueue.push_back( evt );

    handleEvents();
} /* _Shutdown */

int
CookieRef::SocketForHost( HostID dest )
{
    int socket = -1;
    ASSERT_LOCKED();
    vector<HostRec>::const_iterator iter;
    for ( iter = m_sockets.begin(); iter != m_sockets.end(); ++iter ) {
        if ( iter->m_hostID == dest ) {
            socket = iter->m_socket;
            break;
        }
    }

    logf( XW_LOGVERBOSE0, "returning socket=%d for hostid=%x", socket, dest );
    return socket;
}

/* The idea here is: have we never seen the XW_ST_ALLCONNECTED state.  This
   needs to include any states reachable from XW_ST_ALLCONNECTED from which
   recovery back to XW_ST_ALLCONNECTED is possible.  This is used to decide
   whether to admit a connection based on its cookie -- whether that cookie
   should join an existing cref or get a new one? */
bool
CookieRef::NeverFullyConnected()
{
    return m_curState != XWS_ALLCONND
        && m_curState != XWS_MISSING;
}

bool
CookieRef::AcceptingReconnections( HostID hid, const char* cookie, 
                                   int nPlayersH )
{
    bool accept = false;
    /* First, do we have room.  Second, are we missing this guy? */

    if ( m_curState != XWS_INITED
         && m_curState != XWS_CONNECTING
         && m_curState != XWS_MISSING ) {
        /* do nothing; reject */
        logf( XW_LOGINFO, "reject: bad state %s", stateString(m_curState) );
    } else if ( HostKnown( hid ) ) {
        logf( XW_LOGINFO, "reject: known hid" );
        /* do nothing: reject */
    } else {
        if ( m_nPlayersSought == 0 ) {
            accept = true;
        } else if ( nPlayersH + m_nPlayersHere <= m_nPlayersSought ) {
            accept = true;
        } else {
            logf( XW_LOGINFO, "reject: m_nPlayersSought=%d, m_nPlayersHere=%d",
                  m_nPlayersSought, m_nPlayersHere );
        }
    }

    /* Error to connect if cookie doesn't match. */
    if ( accept && !!cookie && 0 != strcmp( cookie, Cookie() ) ) {
        logf( XW_LOGERROR, "%s: not accepting b/c cookie mismatch: %s vs %s",
              __func__, cookie, Cookie() );
        accept = false;
    }

    return accept;
} /* AcceptingReconnections */

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
        ASSERT_LOCKED();

        count = m_sockets.size();
        assert( count > 0 );

        vector<HostRec>::iterator iter;
        for ( iter = m_sockets.begin(); iter != m_sockets.end(); ++iter ) {
            if ( iter->m_socket == socket ) {
                m_sockets.erase(iter);
                --count;
                break;
            }
        }
    }

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
    logf( XW_LOGINFO, "%s", __func__ );
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
CookieRef::pushConnectEvent( int socket, HostID srcID,
                             int nPlayersH, int nPlayersT )
{
    CRefEvent evt;
    evt.type = XWE_CONNECTMSG;
    evt.u.con.socket = socket;
    evt.u.con.srcID = srcID;
    evt.u.con.nPlayersH = nPlayersH;
    evt.u.con.nPlayersT = nPlayersT;
    m_eventQueue.push_back( evt );
} /* pushConnectEvent */

void 
CookieRef::pushReconnectEvent( int socket, HostID srcID,
                               int nPlayersH, int nPlayersT )
{
    CRefEvent evt;
    evt.type = XWE_RECONNECTMSG;
    evt.u.con.socket = socket;
    evt.u.con.srcID = srcID;
    evt.u.con.nPlayersH = nPlayersH;
    evt.u.con.nPlayersT = nPlayersT;
    m_eventQueue.push_back( evt );
} /* pushReconnectEvent */

#ifdef RELAY_HEARTBEAT
void
CookieRef::pushHeartbeatEvent( HostID id, int socket )
{
    CRefEvent evt;
    evt.type = XWE_HEARTRCVD;
    evt.u.heart.id = id;
    evt.u.heart.socket = socket;
    m_eventQueue.push_back( evt );
}

void
CookieRef::pushHeartFailedEvent( int socket )
{
    logf( XW_LOGINFO, "%s", __func__ );
    CRefEvent evt;
    evt.type = XWE_HEARTFAILED;
    evt.u.heart.socket = socket;
    m_eventQueue.push_back( evt );
}
#endif

void
CookieRef::pushForwardEvent( HostID src, HostID dest, 
                             unsigned char* buf, int buflen )
{
    logf( XW_LOGVERBOSE1, "pushForwardEvent: %d -> %d", src, dest );
    CRefEvent evt;
    evt.type = XWE_FORWARDMSG;
    evt.u.fwd.src = src;
    evt.u.fwd.dest = dest;
    evt.u.fwd.buf = buf;
    evt.u.fwd.buflen = buflen;
    m_eventQueue.push_back( evt );
}

void
CookieRef::pushRemoveSocketEvent( int socket )
{
    CRefEvent evt;
    evt.type = XWE_REMOVESOCKET;
    evt.u.rmsock.socket = socket;
    m_eventQueue.push_back( evt );
}

void
CookieRef::pushNotifyDisconEvent( int socket, XWREASON why )
{
    CRefEvent evt;
    evt.type = XWE_NOTIFYDISCON;
    evt.u.disnote.socket = socket;
    evt.u.disnote.why = why;
    m_eventQueue.push_back( evt );
}

void
CookieRef::pushLastSocketGoneEvent()
{
    CRefEvent evt;
    evt.type = XWE_NOMORESOCKETS;
    m_eventQueue.push_back( evt );
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

            logf( XW_LOGINFO, "%s: %s -> %s on evt %s, act=%s", __func__,
                  stateString(m_curState), stateString(m_nextState),
                  eventString(evt.type), actString(takeAction) );

            switch( takeAction ) {

            case XWA_CHKCOUNTS:
                checkCounts( &evt );
                break;

            case XWA_SEND_1ST_RSP:
            case XWA_SEND_1ST_RERSP:
                setAllConnectedTimer();
                increasePlayerCounts( &evt );
                sendResponse( &evt, takeAction == XWA_SEND_1ST_RSP );
                break;

            case XWA_SEND_RSP:
            case XWA_SEND_RERSP:
                increasePlayerCounts( &evt );
                sendResponse( &evt, takeAction == XWA_SEND_RSP );
                break;

            case XWA_FWD:
                forward( &evt );
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
                reducePlayerCounts( evt.u.discon.socket );
                disconnectSockets( evt.u.heart.socket, 
                                   XWRELAY_ERROR_HEART_YOU );
                break;

            case XWA_DISCONNECT:
                setAllConnectedTimer();
                reducePlayerCounts( evt.u.discon.socket );
                removeSocket( evt.u.discon.socket );
                /* Don't notify.  This is a normal part of a game ending. */
                break;

            case XWA_NOTEHEART:
                noteHeartbeat( &evt );
                break;

            case XWA_NOTIFYDISCON:
                notifyDisconn( &evt );
                break;

            case XWA_REMOVESOCKET:
                setAllConnectedTimer();
                reducePlayerCounts( evt.u.rmsock.socket );
                notifyOthers( evt.u.rmsock.socket, XWRELAY_DISCONNECT_OTHER,
                              XWRELAY_ERROR_LOST_OTHER );
                removeSocket( evt.u.rmsock.socket );
                break;

            case XWA_SENDALLHERE:
            case XWA_SNDALLHERE_2:
                cancelAllConnectedTimer();
                assignConnName();
                assignHostIds();
                sendAllHere( takeAction == XWA_SENDALLHERE );
                break;

            case XWA_REJECT:

            case XWA_NONE: 
                /* nothing to do for these */
                break;


            default:
                assert(0); 
                break;
            }

            m_curState = m_nextState;
        }
    }
} /* handleEvents */

void
CookieRef::send_with_length( int socket, unsigned char* buf, int bufLen,
                             bool cascade )
{
    bool failed = false;
    if ( send_with_length_unsafe( socket, buf, bufLen ) ) {
        RecordSent( bufLen, socket );
    } else {
        failed = true;
    }

    if ( failed && cascade ) {
        _Remove( socket );
        XWThreadPool::GetTPool()->CloseSocket( socket );
    }
} /* send_with_length */

static void
putNetShort( unsigned char** bufpp, unsigned short s )
{
    s = htons( s );
    memcpy( *bufpp, &s, sizeof(s) );
    *bufpp += sizeof(s);
}

void
CookieRef::increasePlayerCounts( const CRefEvent* evt )
{
    int nPlayersH = evt->u.con.nPlayersH;
    int nPlayersT = evt->u.con.nPlayersT;
    HostID hid = evt->u.con.srcID;
 
    logf( XW_LOGINFO, "increasePlayerCounts: hid=%d, nPlayersH=%d, "
          "nPlayersT=%d", hid, nPlayersH, nPlayersT );

    if ( hid == HOST_ID_SERVER ) {
        assert( m_nPlayersSought == 0 );
        m_nPlayersSought = nPlayersT;
    } else {
        assert( nPlayersT == 0 ); /* should catch this earlier!!! */
        assert( m_nPlayersSought == 0 || m_nPlayersHere <= m_nPlayersSought );
    }
    m_nPlayersHere += nPlayersH;

    logf( XW_LOGVERBOSE1, "increasePlayerCounts: here=%d; total=%d",
          m_nPlayersHere, m_nPlayersSought );

    CRefEvent newevt;
    newevt.type = (m_nPlayersHere == m_nPlayersSought) ? 
        XWE_ALLHERE : XWE_SOMEMISSING;
    m_eventQueue.push_back( newevt );
} /* increasePlayerCounts */

void
CookieRef::reducePlayerCounts( int socket )
{
    logf( XW_LOGVERBOSE1, "reducePlayerCounts on socket %d", socket );
    ASSERT_LOCKED();
    vector<HostRec>::iterator iter;
    for ( iter = m_sockets.begin(); iter != m_sockets.end(); ++iter ) {
        if ( iter->m_socket == socket ) {
            if ( iter->m_hostID == HOST_ID_SERVER ) {
                m_nPlayersSought = 0;
            } else {
                assert( iter->m_nPlayersT == 0 );
            }
            m_nPlayersHere -= iter->m_nPlayersH;

            logf( XW_LOGVERBOSE1, "reducePlayerCounts: m_nPlayersHere=%d; m_nPlayersSought=%d", 
                  m_nPlayersHere, m_nPlayersSought );

            break;
        }
    }
} /* reducePlayerCounts */

/* Determine if adding this device to the game would give us too many
   players. */
void
CookieRef::checkCounts( const CRefEvent* evt )
{
    int nPlayersH = evt->u.con.nPlayersH;
/*     int nPlayersT = evt->u.con.nPlayersT; */
    HostID hid = evt->u.con.srcID;
    bool success;

    logf( XW_LOGVERBOSE1, "checkCounts: hid=%d, nPlayers=%d, m_nPlayersSought = %d, "
          "m_nPlayersHere = %d",
          hid, nPlayersH, m_nPlayersSought, m_nPlayersHere );

    if ( hid == HOST_ID_SERVER ) {
        success = m_nPlayersSought == 0;
    } else {
        success = (m_nPlayersSought == 0) /* if no server present yet */
            || (m_nPlayersSought >= m_nPlayersHere + nPlayersH);
    }
    logf( XW_LOGVERBOSE1, "success = %d", success );

    CRefEvent newevt;
    if ( success ) {
        newevt = *evt;
        newevt.type = XWE_OKTOSEND;
    } else {
        newevt.type = XWE_COUNTSBAD;
    }
    m_eventQueue.push_back( newevt );
} /* checkCounts */

void
CookieRef::setAllConnectedTimer()
{
    time_t inHowLong;
    RelayConfigs::GetConfigs()->GetValueFor( "ALLCONN", &inHowLong );
    TimerMgr::GetTimerMgr()->SetTimer( inHowLong,
                                       s_checkAllConnected, this, 0 );
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
    HostID id = evt->u.con.srcID;
    int nPlayersH = evt->u.con.nPlayersH;
    int nPlayersT = evt->u.con.nPlayersT;

    ASSERT_LOCKED();

    logf( XW_LOGINFO, "%s: remembering pair: hostid=%x, socket=%d (size=%d)", 
          __func__, id, socket, m_sockets.size());
    HostRec hr(id, socket, nPlayersH, nPlayersT);
    m_sockets.push_back( hr );

    /* Now send the response */
    unsigned char buf[1 +       /* cmd */
                      sizeof(short) + /* heartbeat */
                      sizeof(CookieID) +
                      1         /* hostID */
    ];

    unsigned char* bufp = buf;

    *bufp++ = initial ? XWRELAY_CONNECT_RESP : XWRELAY_RECONNECT_RESP;
    putNetShort( &bufp, GetHeartbeat() );
    putNetShort( &bufp, GetCookieID() );

    send_with_length( socket, buf, bufp - buf, true );
    logf( XW_LOGVERBOSE0, "sent %s", cmdToStr( XWRELAY_Cmd(buf[0]) ) );
} /* sendResponse */

void
CookieRef::forward( const CRefEvent* evt )
{
    unsigned char* buf = evt->u.fwd.buf;
    int buflen = evt->u.fwd.buflen;
    HostID dest = evt->u.fwd.dest;

    int destSocket = SocketForHost( dest );

    if ( destSocket != -1 ) {
        /* This is an ugly hack!!!! */
        *buf = XWRELAY_MSG_FROMRELAY;
        send_with_length( destSocket, buf, buflen, true );

        /* also note that we've heard from src recently */
#ifdef RELAY_HEARTBEAT
        HostID src = evt->u.fwd.src;
        pushHeartbeatEvent( src, SocketForHost(src) );
#endif
    } else {
        /* We're not really connected yet! */
    }
} /* forward */

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
CookieRef::sendAllHere( bool includeName )
{
    unsigned char buf[1 + 1 + MAX_CONNNAME_LEN];
    unsigned char* bufp = buf;
    unsigned char* idLoc;
    
    *bufp++ = XWRELAY_ALLHERE;
    idLoc = bufp++;                 /* space for hostId, remembering address */
    *bufp++ = includeName? 1 : 0;

    if ( includeName ) {
        const char* connName = ConnName();
        assert( !!connName && connName[0] );
        int len = strlen( connName );
        assert( len < MAX_CONNNAME_LEN );
        *bufp++ = (char)len;
        memcpy( bufp, connName, len );
        bufp += len;
    }

    ASSERT_LOCKED();
    vector<HostRec>::iterator iter = m_sockets.begin();
    while ( iter != m_sockets.end() ) { 
        logf( XW_LOGINFO, "%s: sending to hostid %d", __func__, 
              iter->m_hostID );
        *idLoc = iter->m_hostID;   /* write in this target's hostId */
        send_with_length( iter->m_socket, buf, bufp-buf,
                          true );
        ++iter;
    }
} /* sendAllHere */

void
CookieRef::assignConnName( void )
{
    if ( !ConnName()[0] ) {
        m_connName = PermID::GetNextUniqueID();
        logf( XW_LOGINFO, "%s: assigning name: %s", __func__, ConnName() );
        assert( GetCookieID() != 0 );
    } else {
        logf( XW_LOGINFO, "%s: has name: %s", __func__, ConnName() );
    }
} /* assignConnName */

void
CookieRef::assignHostIds( void )
{
    ASSERT_LOCKED();
    HostID nextId = HOST_ID_SERVER;

    unsigned int bits = 0;

    vector<HostRec>::iterator iter;
    for ( iter = m_sockets.begin(); iter != m_sockets.end(); ++iter ) {
        if ( iter->m_hostID != HOST_ID_NONE ) {
            bits |= 1 << iter->m_hostID;
        }
    }

    assert( (bits & (1 << HOST_ID_SERVER)) != 0 );

    for ( iter = m_sockets.begin(); iter != m_sockets.end(); ++iter ) {
        if ( iter->m_hostID == HOST_ID_NONE ) {
            while ( ((1 << nextId) & bits) != 0 ) {
                ++nextId;
            }
            iter->m_hostID = nextId++; /* ++: don't reuse */
        }
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
            logf( XW_LOGERROR, "wrong socket record for HostID %x; wanted %d, found %d", 
                  id, socket, second_socket );
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

void
CookieRef::_CheckAllConnected()
{
    logf( XW_LOGVERBOSE0, "%s", __func__ );
/*     MutexLock ml( &m_EventsMutex ); */
    CRefEvent newEvt;
    newEvt.type = XWE_CONNTIMER;
    m_eventQueue.push_back( newEvt );
    handleEvents();
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
    char buf[MAX_CONNNAME_LEN+MAX_COOKIE_LEN];

    snprintf( buf, sizeof(buf), "%s\n", ConnName() );
    out += buf;

    snprintf( buf, sizeof(buf), "id=%d\n", GetCookieID() );
    out += buf;

    snprintf( buf, sizeof(buf), "Bytes sent=%d\n", m_totalSent );
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
CookieRef::_FormatHostInfo( string* hostIds, string* addrs )
{
    ASSERT_LOCKED();
    vector<HostRec>::iterator iter;
    for ( iter = m_sockets.begin(); iter != m_sockets.end(); ++iter ) {

        if ( !!hostIds ) {
            char buf[8];
            snprintf( buf, sizeof(buf), "%d ", iter->m_hostID );
            *hostIds += buf;
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
