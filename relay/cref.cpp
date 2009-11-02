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
    m_nextHostID = HOST_ID_SERVER;
    m_nPlayersSought = 0;
    m_nPlayersHere = 0;
    m_locking_thread = 0;
    m_starttime = uptime();
    m_gameFull = false;

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
CookieRef::_Connect( int socket, HostID hid, int nPlayersH, int nPlayersS,
                     int seed )
{
    if ( CRefMgr::Get()->Associate( socket, this ) ) {
        if ( hid == HOST_ID_NONE ) {
            logf( XW_LOGINFO, "%s: Waiting to assign host id", __func__ );
        } else {
            logf( XW_LOGINFO, "NOT assigned host id; why?" );
        }
        pushConnectEvent( socket, hid, nPlayersH, nPlayersS, seed );
        handleEvents();
    } else {
        logf( XW_LOGINFO, "dropping connect event; already connected" );
    }
}

void
CookieRef::_Reconnect( int socket, HostID hid, int nPlayersH, int nPlayersS,
                       int seed )
{
    (void)CRefMgr::Get()->Associate( socket, this );
    pushReconnectEvent( socket, hid, nPlayersH, nPlayersS, seed );
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

/* The idea here is: have we never seen the XW_ST_ALLCONNECTED state.  This
   needs to include any states reachable from XW_ST_ALLCONNECTED from which
   recovery back to XW_ST_ALLCONNECTED is possible.  This is used to decide
   whether to admit a connection based on its cookie -- whether that coookie
   should join an existing cref or get a new one? */
bool
CookieRef::NeverFullyConnected()
{
    return m_curState != XWS_ALLCONND
        && m_curState != XWS_MISSING;
}

bool
CookieRef::GameOpen( const char* cookie, int nPlayersH, bool isNew, 
                     bool* alreadyHere )
{
    bool accept = false;
    *alreadyHere = false;
    /* First, do we have room.  Second, are we missing this guy? */

    if ( isNew && m_gameFull ) {
        /* do nothing; reject */
        logf( XW_LOGINFO, "reject: game for %s is full", cookie );
    } else if ( m_curState != XWS_INITED
         && m_curState != XWS_CONNECTING
         && m_curState != XWS_MISSING ) {
        /* do nothing; reject */
        logf( XW_LOGINFO, "reject: bad state %s", stateString(m_curState) );
    } else {
        accept = true;
    }

    /* Error to connect if cookie doesn't match. */
    if ( accept && !!cookie && 0 != strcmp( cookie, Cookie() ) ) {
        logf( XW_LOGERROR, "%s: not accepting b/c cookie mismatch: %s vs %s",
              __func__, cookie, Cookie() );
        accept = false;
    }

    return accept;
} /* GameOpen */

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
        if ( count > 0 ) {
            vector<HostRec>::iterator iter;
            for ( iter = m_sockets.begin(); iter != m_sockets.end(); ++iter ) {
                if ( iter->m_socket == socket ) {
                    m_sockets.erase(iter);
                    --count;
                    break;
                }
            }
        } else {
            logf( XW_LOGERROR, "%s: no socket %d to remove", __func__, socket );
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
                             int nPlayersH, int nPlayersS,
                             int seed )
{
    CRefEvent evt;
    evt.type = XWE_CONNECTMSG;
    evt.u.con.socket = socket;
    evt.u.con.srcID = srcID;
    evt.u.con.nPlayersH = nPlayersH;
    evt.u.con.nPlayersS = nPlayersS;
    evt.u.con.seed = seed;
    m_eventQueue.push_back( evt );
} /* pushConnectEvent */

void 
CookieRef::pushReconnectEvent( int socket, HostID srcID, int nPlayersH,
                               int nPlayersS, int seed )
{
    CRefEvent evt;
    evt.type = XWE_RECONNECTMSG;
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
        XW_RELAY_STATE nextState;
        CRefEvent evt = m_eventQueue.front();
        m_eventQueue.pop_front();

        XW_RELAY_ACTION takeAction;
        if ( getFromTable( m_curState, evt.type, &takeAction, &nextState ) ) {

            logf( XW_LOGINFO, "%s: %s -> %s on evt %s, act=%s", __func__,
                  stateString(m_curState), stateString(nextState),
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
                notifyOthers( evt.u.discon.socket, XWRELAY_DISCONNECT_OTHER,
                              XWRELAY_ERROR_OTHER_DISCON );
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
                assignHostIds();
                assignConnName();
                sendAllHere();
                break;

            case XWA_POSTCLONE:
                moveSockets();
                break;

            case XWA_REJECT:

            case XWA_NONE: 
                /* nothing to do for these */
                break;

            default:
                assert(0); 
                break;
            }

            m_curState = nextState;
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

/* static bool */
/* hostRecComp( const HostRec& aa, const HostRec& bb ) */
/* { */
/*     /\* first order, hosts go before guests; second order, by m_nPlayersHere  *\/ */
/* /\*     if ( aa.m_nPlayersS *\/ */

/*     return aa.m_nPlayersH < bb.m_nPlayersH; */
/* } */

static void
print_sockets( const char* caller, vector<HostRec>& sockets )
{
    logf( XW_LOGINFO, "  %s from %s", __func__, caller );
    vector<HostRec>::iterator iter;
    for ( iter = sockets.begin(); iter != sockets.end(); ++iter ) {
        logf( XW_LOGINFO, "isHost: %d; nPlayersH: %d; seed=%.4X; socket: %d", 
                 iter->m_nPlayersS > 0, iter->m_nPlayersH, iter->m_seed,
                 iter->m_socket );
    }
} /* print_sockets */

#define MAX_PER_SIZE 16         /* need to enforce this when connections arrive */
bool
CookieRef::tryMakeGame( vector<HostRec>& remaining )
{
    assert( remaining.size() == 0 );
    int nHosts = 0;
    int nGuests;
    bool complete = false;
    unsigned int nRecords = m_sockets.size();
    int ii;

    /* m_sockets is sorted with hosts first, guests after, and within each in
       descending order by number of players provided.  Start by finding where
       the host/guest break is. */
    vector<HostRec>::iterator iter;
    for ( iter = m_sockets.begin(); iter != m_sockets.end(); ++iter ) {
        if ( iter->m_nPlayersS == 0 ) {
            break;
        }
        ++nHosts;
    }
    logf( XW_LOGINFO, "%s: there are %d hosts", __func__, nHosts );

    nGuests = m_sockets.size() - nHosts;
    int inUse[nGuests];
    int nUsed;

    /* Now for each host, try to give him a set of guests.  Assumption:
       there's no way we can find more than one complete game here since we
       try after every host addition.  The algorithm, which assumes guests are
       sorted from most-players-provided down, is to try the largest numbers
       first, skipping each that won't fit.  If we fail, we try again starting
       from the next host.
    */
    int hostIndex;
    for ( hostIndex = 0; hostIndex < nHosts; ++hostIndex ) {
        unsigned int firstGuest;
        for ( firstGuest = nHosts; firstGuest < nRecords; ++firstGuest ) {
            int sought = m_sockets[hostIndex].m_nPlayersS - m_sockets[hostIndex].m_nPlayersH;

            nUsed = 0;
            for ( ii = (int)firstGuest; ii < (int)nRecords; ++ii ) {
                int one = m_sockets[ii].m_nPlayersH;
                if ( one <= sought ) { /* not too big? */
                    sought -= one;
                    inUse[nUsed++] = ii;
                    if ( sought == 0 ) {
                        complete = true;
                        goto loop_end;
                    }
                }
            }
        }
    }
 loop_end:

    /* If we have a full compliment of devices now, remove all the others into
       remaining */
    if ( complete ) {
        int nRemaining = nRecords-nUsed-1;   /* -1 for host */
        int lastUsed = nUsed-1;

        /* guest[s] first */
        for ( ii = nRecords - 1; ii >= nHosts; --ii ) {
            if ( ii == inUse[lastUsed] ) {
                --lastUsed;
            } else {
                assert( nRemaining > 0 );
                HostRec hr = m_sockets[ii];
                m_nPlayersHere -= hr.m_nPlayersH;
                assert( hr.m_nPlayersS == 0 );
                remaining.insert( remaining.begin(), hr ); /* insert at start */
                --nRemaining;
                m_sockets.erase( m_sockets.begin() + ii );
            }
        }

        /* now remove the host we chose */
        for ( ii = nHosts - 1; ii >= 0; --ii ) {
            if ( ii != hostIndex ) {
                assert( nRemaining > 0 );
                HostRec hr = m_sockets[ii];
                m_nPlayersHere -= hr.m_nPlayersH;
                m_nPlayersSought -= hr.m_nPlayersS;
                remaining.insert( remaining.begin(), hr ); /* insert at start */
                --nRemaining;
                m_sockets.erase( m_sockets.begin() + ii );
            }
        }

        assert( 0 == nRemaining );
    }

    print_sockets( __func__, m_sockets );
    print_sockets( __func__, remaining );

    assert( remaining.size() + m_sockets.size() == nRecords );

    logf( XW_LOGINFO, "%s => %d", __func__, complete );
    return complete;
} /* tryMakeGame */

/* Maintain order first by whether is host or not, and then by number of
   players provided */
void
CookieRef::insertSorted( HostRec hr )
{
    bool newIsHost = hr.m_nPlayersS > 0;
    int newPlayersH = hr.m_nPlayersH;

    vector<HostRec>::iterator iter;
    for ( iter = m_sockets.begin(); iter != m_sockets.end(); ++iter ) {
        bool curIsHost = iter->m_nPlayersS > 0;
        bool belongsBySort = newPlayersH <= iter->m_nPlayersH;

        /* We're done when we're inserting a host and have found a guest; or
           when we're in the right region (host or guest) and the secondary
           sort says do-it.*/

        if ( newIsHost && !curIsHost ) {
            break;
        } else if ( (newIsHost == curIsHost) && belongsBySort ) {
            break;
        }
    }
        
    m_sockets.insert( iter, hr );
    logf( XW_LOGINFO, "m_sockets.size() now %d", m_sockets.size() );
    print_sockets( __func__, m_sockets );

    m_nPlayersHere += hr.m_nPlayersH;
    m_nPlayersSought += hr.m_nPlayersS;
} /* insertSorted */

void
CookieRef::populate( vector<HostRec> hosts )
{
    /* copy enough state that it can live on own */
    m_sockets = hosts;
    m_curState = XWS_CLONED;

    vector<HostRec>::iterator iter;
    for ( iter = m_sockets.begin(); iter != m_sockets.end(); ++iter ) {
        m_nPlayersSought += iter->m_nPlayersS;
        m_nPlayersHere += iter->m_nPlayersH;
    }
    print_sockets( __func__, m_sockets );
}

void
CookieRef::increasePlayerCounts( const CRefEvent* evt )
{
    int nPlayersH = evt->u.con.nPlayersH;
    int nPlayersS = evt->u.con.nPlayersS;
    int socket = evt->u.con.socket;
    HostID hid = evt->u.con.srcID;
    int seed = evt->u.con.seed;
    assert( hid <= 4 );
 
    logf( XW_LOGINFO, "%s: hid=%d, nPlayersH=%d, "
          "nPlayersS=%d", __func__, hid, nPlayersH, nPlayersS );

    /* Up to this point we should just be adding host records to this cref.
       Maybe even including multiple hosts.  Now we go through what we have
       and try to build a game. sendResponse() is where new hostrecs are
       actually added.  That seems broken! */

    ASSERT_LOCKED();

    /* first add the rec here, whether it'll stay for not */
    logf( XW_LOGINFO, "%s: remembering pair: hostid=%x, socket=%d (size=%d)", 
          __func__, hid, socket, m_sockets.size());
    HostRec hr( hid, socket, nPlayersH, nPlayersS, seed );

    insertSorted( hr );

    vector<HostRec> remaining;
    bool gameComplete = tryMakeGame( remaining );

    /* If we built a game but had leftover HostRecs, they're now in remaining.
       Build a new cref for them, and process its first event now.  It'll then
       be ready to receive new messages, e.g. new connections. */
    if ( remaining.size() > 0 ) {
        CookieRef* clone = CRefMgr::Get()->Clone( this );
        assert( !!clone );
        clone->Lock();

        clone->populate( remaining );

        assert( clone->m_eventQueue.size() == 0 );
        CRefEvent evt;
        evt.type = XWE_CLONECHKMSG;
        clone->m_eventQueue.push_back( evt );
        clone->handleEvents();

        clone->Unlock();
    }

    logf( XW_LOGVERBOSE1, "%s: here=%d; total=%d", __func__,
          m_nPlayersHere, m_nPlayersSought );

    CRefEvent newevt;
    if ( gameComplete ) {
        newevt.type = XWE_ALLHERE;
        m_gameFull = true;
    } else {
        newevt.type = XWE_SOMEMISSING;
    }
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
                m_nPlayersSought -= iter->m_nPlayersS;
            } else {
                assert( iter->m_nPlayersS == 0 );
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
    int nPlayersS = evt->u.con.nPlayersS;
    HostID hid = evt->u.con.srcID;
    bool success;

    logf( XW_LOGVERBOSE1, "%s: hid:%d; nPlayersS:%d; nPlayersH:%d; "
          "m_nPlayersSought:%d; m_nPlayersHere:%d", __func__, 
          hid, nPlayersS, nPlayersH, m_nPlayersSought, m_nPlayersHere );

    /* increasePlayerCounts() is where we actually increase the numbers.  Is
       that right? */

    if ( hid == HOST_ID_SERVER ) {
        success = m_nPlayersSought == 0;
    } else {
        success = (m_nPlayersSought == 0) /* if no server present yet */
            || (m_nPlayersSought >= m_nPlayersHere + nPlayersH);
    }
    logf( XW_LOGVERBOSE1, "success = %d", success );

    CRefEvent newevt;
    if ( success ) {
        newevt = *evt;          /* this is a gross hack!!! */
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

    /* Now send the response */
    unsigned char buf[1       /* cmd */
                      + sizeof(short) /* heartbeat */
                      + sizeof(short) /* total here */
                      + sizeof(short) /* total expected */
    ];

    unsigned char* bufp = buf;

    *bufp++ = initial ? XWRELAY_CONNECT_RESP : XWRELAY_RECONNECT_RESP;
    putNetShort( &bufp, GetHeartbeat() );
    *bufp++ = GetPlayersHere();
    *bufp++ = GetPlayersSought();

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
CookieRef::sendAllHere( void )
{
    unsigned char buf[1 + 1     /* hostID */
                      + sizeof(CookieID) 
                      + 1 + MAX_CONNNAME_LEN];

    unsigned char* bufp = buf;
    unsigned char* idLoc;
    
    *bufp++ = XWRELAY_ALLHERE;
    idLoc = bufp++;                 /* space for hostId, remembering address */

    putNetShort( &bufp, GetCookieID() );

    const char* connName = ConnName();
    assert( !!connName && connName[0] );
    int len = strlen( connName );
    assert( len < MAX_CONNNAME_LEN );
    *bufp++ = (char)len;
    memcpy( bufp, connName, len );
    bufp += len;

    ASSERT_LOCKED();
    vector<HostRec>::iterator iter = m_sockets.begin();
    while ( iter != m_sockets.end() ) { 
        logf( XW_LOGINFO, "%s: sending to hostid %d", __func__, 
              iter->m_hostID );
        *idLoc = iter->m_hostID;   /* write in this target's hostId */
        send_with_length( iter->m_socket, buf, bufp-buf, true );
        ++iter;
    }
} /* sendAllHere */

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

        vector<HostRec>::iterator iter;
        for ( iter = m_sockets.begin(); iter != m_sockets.end(); ++iter ) {
            char buf[5];
            snprintf( buf, sizeof(buf), "%.4X", iter->m_seed );
            m_connName += buf;
        }

        m_connName += CONNNAME_DELIM + PermID::GetNextUniqueID();

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
    char buf[MAX_CONNNAME_LEN+MAX_INVITE_LEN];

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
