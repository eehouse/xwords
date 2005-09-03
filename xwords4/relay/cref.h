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

 public:

    ~CookieRef();

    /* Within this cookie, remember that this hostID and socket go together.
       If the hostID is HOST_ID_SERVER, it's the server. */
    short GetHeartbeat() { return m_heatbeat; }
    CookieID GetCookieID() { return m_connectionID; }
    int HostKnown( HostID host ) { return -1 != SocketForHost( host ); }
    int CountSockets() { return m_hostSockets.size(); }
    int HasSocket( int socket );
    string Name() { return m_name; }

    int NotFullyConnected() { return m_curState != XW_ST_ALLCONNECTED; }

    /* for console */
    void _PrintCookieInfo( string& out );
    void PrintSocketInfo( string& out, int socket );

    static CookieMapIterator GetCookieIterator();
    static CookieRef* AddNew( string s, CookieID id );
    /* Nuke an existing */
    static void Delete( CookieID id );
    static void Delete( const char* name );

    /* These need to become private */
    void _Connect( int socket, HostID srcID );
    void _Reconnect( int socket, HostID srcID );
    void _HandleHeartbeat( HostID id, int socket );
    void _CheckHeartbeats( time_t now );
    void _Forward( HostID src, HostID dest, unsigned char* buf, int buflen );
    void _Remove( int socket );
    void _CheckAllConnected();

    int ShouldDie() { return m_curState == XW_ST_DEAD; }

 private:

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
            } recon;
            struct {
                HostID id;
                int socket;
            } heart;
            struct {
                time_t now;
                vector<int>* victims;
            } htime;
            struct {
                HostID hostID;
                int reason;
            } discon;
            struct {
                int socket;
            } rmsock;
            struct {
                int socket;
                XWREASON why;
            } disnote;
        } u;
    } CRefEvent;

    friend class CRefMgr;
    CookieRef( string s, CookieID id );

    int SocketForHost( HostID dest );

    void send_with_length( int socket, unsigned char* buf, int bufLen );
    void RecordSent( int nBytes, int socket ) {
        m_totalSent += nBytes;
    }

    void pushConnectEvent( int socket, HostID srcID );
    void pushReconnectEvent( int socket, HostID srcID );
    void pushHeartbeatEvent( HostID id, int socket );
    void pushHeartFailedEvent( int socket );
    

    void pushHeartTimerEvent( time_t now, vector<int>* victims );
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

    void sendResponse( const CRefEvent* evt );
    void setAllConnectedTimer();
    void cancelAllConnectedTimer();

    void forward( const CRefEvent* evt );
    void checkDest( const CRefEvent* evt );
    void checkFromServer( const CRefEvent* evt );

    void disconnectSockets( int socket, XWREASON why );
    void noteHeartbeat(const CRefEvent* evt);
    void checkHeartbeats(const CRefEvent* evt);
    void notifyDisconn(const CRefEvent* evt);
    void removeSocket(const CRefEvent* evt);
    

    /* timer callback */
    static void s_checkAllConnected( void* closure );

    map<HostID,HostRec> m_hostSockets;
    pthread_rwlock_t m_sockets_rwlock;
    CookieID m_connectionID;
    short m_heatbeat;           /* might change per carrier or something. */
    string m_name;
    int m_totalSent;

    /* Guard the event queue.  Only one thread at a time can post to the
       queue, but once in a thread can post new events while processing
       current ones. */
    pthread_mutex_t    m_EventsMutex;


    XW_RELAY_STATE     m_curState;
    XW_RELAY_STATE     m_nextState;
    deque<CRefEvent>   m_eventQueue;

    static CookieID ms_nextConnectionID;
}; /* CookieRef */

#endif
