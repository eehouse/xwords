/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */

#ifndef _CREF_H_
#define _CREF_H_

#include <map>
#include <vector>
#include <string>
#include <deque>
#include <pthread.h>
#include "xwrelay_priv.h"
#include "states.h"

#ifndef HEARTBEAT
# define HEARTBEAT 60
#endif

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
    void Connect( int socket, HostID srcID );
    short GetHeartbeat() { return HEARTBEAT; }
    CookieID GetCookieID() { return m_connectionID; }
    int SocketForHost( HostID dest );
    void Remove( int socket );
    int CountSockets() { return m_hostSockets.size(); }
    string Name() { return m_name; }

    int NotFullyConnected() { return m_curState != XW_ST_ALLCONNECTED; }

    void HandleHeartbeat( HostID id, int socket );
    void CheckHeartbeats( time_t now, vector<int>* victims );
    void Forward( HostID src, HostID dest, unsigned char* buf, int buflen );

    /* for console */
    void PrintCookieInfo( string& out );
    void PrintSocketInfo( string& out, int socket );

    static CookieMapIterator GetCookieIterator();
    static CookieRef* AddNew( string s, CookieID id );
    /* Nuke an existing */
    static void Delete( CookieID id );
    static void Delete( const char* name );

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
                
            } recon;
            struct {
                HostID id;
                int socket;
            } heart;
            struct {
                time_t now;
                vector<int>* victims;
            } htime;
        } u;
    } CRefEvent;

    CookieRef( string s, CookieID id );

    void RecordSent( int nBytes, int socket ) {
        /* This really needs a lock.... */
        m_totalSent += nBytes;
    }

    void pushConnectEvent( int socket, HostID srcID );
    void pushHeartbeatEvent( HostID id, int socket );
    void pushHeartTimerEvent( time_t now, vector<int>* victims );
    void pushForwardEvent( HostID src, HostID dest, unsigned char* buf, 
                           int buflen );
    void pushDestBadEvent();
    void pushDestOkEvent( const CRefEvent* evt );


    void handleEvents();

    void sendResponse(const CRefEvent* evt);
    void forward(const CRefEvent* evt);
    void checkDest( const CRefEvent* evt );

    void disconnectAll(const CRefEvent* evt);
    void noteHeartbeat(const CRefEvent* evt);
    void checkHeartbeats(const CRefEvent* evt);

    map<HostID,HostRec> m_hostSockets;
    pthread_rwlock_t m_sockets_rwlock;
    CookieID m_connectionID;
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

typedef map<CookieID,CookieRef*> CookieMap;

class CookieMapIterator {
 public:
    CookieMapIterator();
    ~CookieMapIterator() {}
    CookieID Next();
 private:
    CookieMap::const_iterator _iter;
};

CookieRef* get_make_cookieRef( const char* cookie, CookieID connID );
CookieRef* get_cookieRef( CookieID cookieID );
CookieID CookieIdForName( const char* name );
void CheckHeartbeats( time_t now, vector<int>* victims );

class SocketStuff;
typedef map< int, SocketStuff* > SocketMap;

class SocketsIterator {
 public:
    SocketsIterator( SocketMap::iterator iter );
    int Next();
 private:
    SocketMap::iterator m_iter;
};

class SocketMgr {
 public:
    static void Associate( int socket, CookieRef* cref );
    static pthread_mutex_t* GetWriteMutexForSocket( int socket );
    static void RemoveSocketRefs( int socket );
    static void PrintSocketInfo( int socket, string& out );
    static SocketsIterator MakeSocketsIterator();

 private:
    static CookieRef* CookieRefForSocket( int socket );
    static SocketMap ms_SocketStuff;
    static pthread_mutex_t ms_SocketStuffMutex;
};


#endif
