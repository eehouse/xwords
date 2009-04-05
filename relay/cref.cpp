/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */

/* 
 * Copyright 2005 - 2009 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#include "cref.h"
#include "xwrelay.h"
#include "mlock.h"
#include "tpool.h"
#include "states.h"
#include "timermgr.h"
#include "configs.h"
#include "crefmgr.h"

using namespace std;

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

CookieRef::CookieRef( const char* cookie, const char* connName, CookieID id )
    : m_cookie(cookie==NULL?"":cookie)
    , m_connName(connName)
    , m_cookieID(id)
    , m_totalSent(0)
    , m_curState(XWS_INITED)
    , m_nextState(XWS_INITED)
    , m_eventQueue()
    , m_nextHostID(HOST_ID_SERVER)
    , m_nPlayersTotal(0)
    , m_nPlayersHere(0)
{
    RelayConfigs::GetConfigs()->GetValueFor( "HEARTBEAT", &m_heatbeat );
    logf( XW_LOGINFO, "creating cref for cookie %s, connName %s",
          m_cookie.c_str(), m_connName.c_str() );
}

CookieRef::~CookieRef()
{
    cancelAllConnectedTimer();

    /* get rid of any sockets still contained */
    XWThreadPool* tPool = XWThreadPool::GetTPool();

    for ( ; ; ) {
        map<HostID,HostRec>::iterator iter = m_sockets.begin();

        if ( iter == m_sockets.end() ) {
            break;
        }

        int socket = iter->second.m_socket;
        tPool->CloseSocket( socket );
        m_sockets.erase( iter );
    }

    logf( XW_LOGINFO, "CookieRef for %d being deleted; sent %d bytes", 
          m_cookieID, m_totalSent );
} /* ~CookieRef */

void
CookieRef::_Connect( int socket, HostID hid, int nPlayersH, int nPlayersT )
{
    if ( CRefMgr::Get()->Associate( socket, this ) ) {
        if ( hid == HOST_ID_NONE ) {
            hid = nextHostID();
            logf( XW_LOGINFO, "assigned host id: %x", hid );
        } else {
            logf( XW_LOGINFO, "NOT assigned host id; why?" );
        }
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
/*     MutexLock ml( &m_EventsMutex ); */
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
    int socket;
    map<HostID,HostRec>::iterator iter = m_sockets.find( dest );
    if ( iter == m_sockets.end() ) {
        socket = -1;
    } else {
        socket = iter->second.m_socket;
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
    return m_curState != XWS_ALLCONNECTED
        && m_curState != XWS_MISSING;
}

bool
CookieRef::AcceptingReconnections( HostID hid, int nPlayersH, int nPlayersT )
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
        if ( m_nPlayersTotal == 0 ) {
            accept = true;
        } else if ( nPlayersH + m_nPlayersHere <= m_nPlayersTotal ) {
            accept = true;
        } else {
            logf( XW_LOGINFO, "reject: m_nPlayersTotal=%d, m_nPlayersHere=%d",
                  m_nPlayersTotal, m_nPlayersHere );
        }
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

    send_with_length( socket, buf, sizeof(buf) );
} /* notifyDisconn */

void
CookieRef::removeSocket( int socket )
{
    logf( XW_LOGINFO, "%s(%d)", __func__, socket );
    int count;
    {
/*         RWWriteLock rwl( &m_sockets_rwlock ); */

        count = m_sockets.size();
        assert( count > 0 );
        map<HostID,HostRec>::iterator iter = m_sockets.begin();
        while ( iter != m_sockets.end() ) {
            if ( iter->second.m_socket == socket ) {
                m_sockets.erase(iter);
                --count;
                break;
            }
            ++iter;
        }
    }

    if ( count == 0 ) {
        pushLastSocketGoneEvent();
    }
} /* removeSocket */

bool
CookieRef::HasSocket( int socket )
{
    bool found = false;

    map<HostID,HostRec>::iterator iter = m_sockets.begin();
    while ( iter != m_sockets.end() ) {
        if ( iter->second.m_socket == socket ) {
            found = true;
            break;
        }
        ++iter;
    }
    return found;
} /* HasSocket */

#ifdef RELAY_HEARTBEAT
void
CookieRef::_HandleHeartbeat( HostID id, int socket )
{
/*     MutexLock ml( &m_EventsMutex ); */
    pushHeartbeatEvent( id, socket );
    handleEvents();
} /* HandleHeartbeat */

void
CookieRef::_CheckHeartbeats( time_t now )
{
    logf( XW_LOGINFO, "CookieRef::_CheckHeartbeats" );

    map<HostID,HostRec>::iterator iter = m_sockets.begin();
    while ( iter != m_sockets.end() ) {
        time_t last = iter->second.m_lastHeartbeat;
        if ( (now - last) > GetHeartbeat() ) {
            pushHeartFailedEvent( iter->second.m_socket );
        }
        ++iter;
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
CookieRef::send_with_length( int socket, unsigned char* buf, int bufLen )
{
    SocketWriteLock slock( socket );
    if ( slock.socketFound() ) {

        if ( send_with_length_unsafe( socket, buf, bufLen ) ) {
            RecordSent( bufLen, socket );
        } else {
            /* ok that the slock above is still in scope */
            killSocket( socket, "couldn't send" );
        }
    }
}

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
 
    logf( XW_LOGVERBOSE1, "increasePlayerCounts: hid=%d, nPlayersH=%d, "
          "nPlayersT=%d", hid, nPlayersH, nPlayersT );

    if ( hid == HOST_ID_SERVER ) {
        assert( m_nPlayersTotal == 0 );
        m_nPlayersTotal = nPlayersT;
    } else {
        assert( nPlayersT == 0 ); /* should catch this earlier!!! */
        assert( m_nPlayersTotal == 0 || m_nPlayersHere <= m_nPlayersTotal );
    }
    m_nPlayersHere += nPlayersH;

    logf( XW_LOGVERBOSE1, "increasePlayerCounts: here=%d; total=%d",
          m_nPlayersHere, m_nPlayersTotal );

    CRefEvent newevt;
    newevt.type = (m_nPlayersHere == m_nPlayersTotal) ? 
        XWE_ALLHERE : XWE_SOMEMISSING;
    m_eventQueue.push_back( newevt );
} /* increasePlayerCounts */

void
CookieRef::reducePlayerCounts( int socket )
{
    logf( XW_LOGVERBOSE1, "reducePlayerCounts on socket %d", socket );
    map<HostID,HostRec>::iterator iter = m_sockets.begin();
    while ( iter != m_sockets.end() ) {

        if ( iter->second.m_socket == socket ) {
            assert( iter->first != 0 );

            if ( iter->first == HOST_ID_SERVER ) {
                m_nPlayersTotal = 0;
            } else {
                assert( iter->second.m_nPlayersT == 0 );
            }
            m_nPlayersHere -= iter->second.m_nPlayersH;

            logf( XW_LOGVERBOSE1, "reducePlayerCounts: m_nPlayersHere=%d; m_nPlayersTotal=%d", 
                  m_nPlayersHere, m_nPlayersTotal );

            break;
        }
        ++iter;
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

    logf( XW_LOGVERBOSE1, "checkCounts: hid=%d, nPlayers=%d, m_nPlayersTotal = %d, "
          "m_nPlayersHere = %d",
          hid, nPlayersH, m_nPlayersTotal, m_nPlayersHere );

    if ( hid == HOST_ID_SERVER ) {
        success = m_nPlayersTotal == 0;
    } else {
        success = (m_nPlayersTotal == 0) /* if no server present yet */
            || (m_nPlayersTotal >= m_nPlayersHere + nPlayersH);
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

    assert( id != HOST_ID_NONE );
    logf( XW_LOGINFO, "remembering pair: hostid=%x, socket=%d", id, socket );
    HostRec hr(socket, nPlayersH, nPlayersT);
    m_sockets.insert( pair<HostID,HostRec>(id,hr) );

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
    logf( XW_LOGVERBOSE0, "writing hostID of %d into msg", id );
    *bufp++ = (char)id;

    send_with_length( socket, buf, bufp - buf );
    logf( XW_LOGVERBOSE0, "sent XWRELAY_CONNECTRESP" );
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
        send_with_length( destSocket, buf, buflen );

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
CookieRef::send_msg( int socket, HostID id, XWRelayMsg msg, XWREASON why )
{
    unsigned char buf[10];
    short tmp;
    int len = 0;
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
    send_with_length( socket, buf, len );
} /* send_msg */

void
CookieRef::notifyOthers( int socket, XWRelayMsg msg, XWREASON why )
{
    assert( socket != 0 );

/*     RWReadLock ml( &m_sockets_rwlock ); */

    map<HostID,HostRec>::iterator iter = m_sockets.begin();
    while ( iter != m_sockets.end() ) { 
        int other = iter->second.m_socket;
        if ( other != socket ) {
            send_msg( other, iter->first, msg, why );
        }
        ++iter;
    }
} /* notifyOthers */

void
CookieRef::sendAllHere( bool includeName )
{
    unsigned char buf[1 + 1 + MAX_CONNNAME_LEN];
    unsigned char* bufp = buf;
    
    *bufp++ = XWRELAY_ALLHERE;
    *bufp++ = includeName? 1 : 0;

    if ( includeName ) {
        const char* connName = ConnName();
        int len = strlen( connName );
        assert( len < MAX_CONNNAME_LEN );
        *bufp++ = (char)len;
        memcpy( bufp, connName, len );
        bufp += len;
    }

    map<HostID,HostRec>::iterator iter = m_sockets.begin();
    while ( iter != m_sockets.end() ) { 
        send_with_length( iter->second.m_socket, buf, bufp-buf );
        ++iter;
    }
} /* sendAllHere */

void
CookieRef::disconnectSockets( int socket, XWREASON why )
{
    if ( socket == 0 ) {
/*         RWReadLock ml( &m_sockets_rwlock ); */
        map<HostID,HostRec>::iterator iter = m_sockets.begin();
        while ( iter != m_sockets.end() ) { 
            assert( iter->second.m_socket != 0 );
            disconnectSockets( iter->second.m_socket, why );
            ++iter;
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

/*     RWWriteLock rwl( &m_sockets_rwlock ); */

    map<HostID,HostRec>::iterator iter = m_sockets.find(id);
    if ( iter == m_sockets.end() ) {
        logf( XW_LOGERROR, "no socket for HostID %x", id );
    } else {

        /* PENDING If the message came on an unexpected socket, kill the
           connection.  An attack is the most likely explanation. */
        assert( iter->second.m_socket == socket );

        logf( XW_LOGVERBOSE1, "upping m_lastHeartbeat from %d to %d",
              iter->second.m_lastHeartbeat, uptime() );
        iter->second.m_lastHeartbeat = uptime();
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
    logf( XW_LOGVERBOSE0, "checkAllConnected" );
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

    snprintf( buf, sizeof(buf), "Total players=%d\n", m_nPlayersTotal );
    out += buf;
    snprintf( buf, sizeof(buf), "Players here=%d\n", m_nPlayersHere );
    out += buf;

    snprintf( buf, sizeof(buf), "State=%s\n", stateString( m_curState ) );
    out += buf;

    /* Timer state: how long since last heartbeat; how long til disconn timer
       fires. */

    /* n messages */
    /* open since when */

    snprintf( buf, sizeof(buf), "Hosts connected=%d; cur time = %ld\n", 
              m_sockets.size(), uptime() );
    out += buf;
    map<HostID,HostRec>::iterator iter = m_sockets.begin();
    while ( iter != m_sockets.end() ) {
        snprintf( buf, sizeof(buf), "  HostID=%d; socket=%d;last hbeat=%ld\n", 
                  iter->first, iter->second.m_socket, 
                  iter->second.m_lastHeartbeat );
        out += buf;
        ++iter;
    }

} /* PrintCookieInfo */

void
CookieRef::_FormatSockets( string& out )
{
    map<HostID,HostRec>::iterator iter = m_sockets.begin();
    while ( iter != m_sockets.end() ) {
        char buf[8];
        snprintf( buf, sizeof(buf), "%d ", iter->first );
        out += buf;
        ++iter;
    }
}
