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


#ifndef _CREF_H_
#define _CREF_H_

#include <map>
#include <vector>
#include <string>
#include <deque>
#include <pthread.h>
#include "xwrelay_priv.h"
#include "xwrelay.h"
#include "states.h"

using namespace std;

class CookieMapIterator;        /* forward */

class HostRec {
 public:
    HostRec(int socket) : m_socket(socket), m_lastHeartbeat(now()) {}
    ~HostRec() {}
    int m_socket;
    time_t m_lastHeartbeat;
};

class CookieRef {

 private:
    /* These classes have access to CookieRef.  All others should go through
       SafeCref instances. */
    friend class CRefMgr;
    friend class SafeCref;
    friend class CookieMapIterator;

    CookieRef( const char* cookie, const char* connName, CookieID id );
    ~CookieRef();

    /* Within this cookie, remember that this hostID and socket go together.
       If the hostID is HOST_ID_SERVER, it's the server. */
    CookieID GetCookieID() { return m_cookieID; }

    int HostKnown( HostID host ) { return -1 != SocketForHost( host ); }
    int CountSockets() { return m_sockets.size(); }
    int HasSocket( int socket );
    const char* Name() { return m_name.c_str(); }
    const char* ConnName() { return m_connName.c_str(); }

    short GetHeartbeat() { return m_heatbeat; }
    int SocketForHost( HostID dest );

    int NeverFullyConnected();
    int AcceptingConnections();

    /* for console */
    void _PrintCookieInfo( string& out );
    void PrintSocketInfo( string& out, int socket );

    static CookieMapIterator GetCookieIterator();

    /* Nuke an existing */
    static void Delete( CookieID id );
    static void Delete( const char* name );

    void _Connect( int socket, HostID srcID );
    void _Reconnect( int socket, HostID srcID );
    void _Disconnect(int socket, HostID hostID );
    void _HandleHeartbeat( HostID id, int socket );
    void _CheckHeartbeats( time_t now );
    void _Forward( HostID src, HostID dest, unsigned char* buf, int buflen );
    void _Remove( int socket );
    void _CheckAllConnected();

    int ShouldDie() { return m_curState == XW_ST_DEAD; }

    typedef struct CRefEvent {
        XW_RELAY_EVENT type;
        union {
            struct {
                HostID src;
                HostID dest;
                unsigned char* buf;
                int buflen;
            } fwd;
            struct {
                int socket;
                HostID srcID;
            } con;
            struct {
                int socket;
                HostID srcID;
            } discon;
            struct {
                HostID id;
                int socket;
            } heart;
            struct {
                time_t now;
                vector<int>* victims;
            } htime;
            struct {
                int socket;
            } rmsock;
            struct {
                int socket;
                XWREASON why;
            } disnote;
        } u;
    } CRefEvent;

    void send_with_length( int socket, unsigned char* buf, int bufLen );
    void send_msg( int socket, HostID id, XWRelayMsg msg, XWREASON why );

    void RecordSent( int nBytes, int socket ) {
        m_totalSent += nBytes;
    }

    void pushConnectEvent( int socket, HostID srcID );
    void pushReconnectEvent( int socket, HostID srcID );
    void pushHeartbeatEvent( HostID id, int socket );
    void pushHeartFailedEvent( int socket );
    
    void pushForwardEvent( HostID src, HostID dest, unsigned char* buf, 
                           int buflen );
    void pushDestBadEvent();
    void pushLastSocketGoneEvent();
    void pushRemoveSocketEvent( int socket );
    void pushNotifyDisconEvent( int socket, XWREASON why );

    void pushDestOkEvent( const CRefEvent* evt );
    void pushCanLockEvent( const CRefEvent* evt );
    void pushCantLockEvent( const CRefEvent* evt );

    void handleEvents();

    void sendResponse( const CRefEvent* evt, int initial );
    void setAllConnectedTimer();
    void cancelAllConnectedTimer();

    void forward( const CRefEvent* evt );
    void checkDest( const CRefEvent* evt );
    void checkFromServer( const CRefEvent* evt );
    void notifyOthers( int socket, XWRelayMsg msg, XWREASON why );

    void disconnectSockets( int socket, XWREASON why );
    void noteHeartbeat(const CRefEvent* evt);
    void notifyDisconn(const CRefEvent* evt);
    void removeSocket( int socket );
    
    HostID nextHostID() { return ++m_nextHostID; }
    /* timer callback */
    static void s_checkAllConnected( void* closure );

    map<HostID,HostRec> m_sockets;
/*     pthread_rwlock_t m_sockets_rwlock; */

    short m_heatbeat;           /* might change per carrier or something. */
    string m_name;              /* cookie used for initial connections */
    string m_connName;          /* globally unique name */
    CookieID m_cookieID;        /* Unique among current games on this server */
    int m_totalSent;

    /* Guard the event queue.  Only one thread at a time can post to the
       queue, but once in a thread can post new events while processing
       current ones. */
/*     pthread_mutex_t    m_EventsMutex; */


    XW_RELAY_STATE     m_curState;
    XW_RELAY_STATE     m_nextState;
    deque<CRefEvent>   m_eventQueue;

    HostID m_nextHostID;
}; /* CookieRef */

#endif
