/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */

#ifndef _CREF_H_
#define _CREF_H_

#include <map>
#include <vector>
#include <string>
#include <pthread.h>
#include "xwrelay_priv.h"

typedef unsigned short CookieID;

using namespace std;

class CookieMapIterator;        /* forward */

class CookieRef {

 public:

    ~CookieRef();

    /* Within this cookie, remember that this hostID and socket go together.
       If the hostID is HOST_ID_SERVER, it's the server. */
    void Associate( int socket, HostID srcID );
    short GetHeartbeat() { return 60; }
    CookieID GetCookieID() { return m_connectionID; }
    int SocketForHost( HostID dest );
    void Remove( int socket );
    int CountSockets() { return m_hostSockets.size(); }
    string Name() { return m_name; }

    void RecordSent( int nBytes, int socket ) {
        /* This really needs a lock.... */
        m_totalSent += nBytes;
    }

    /* for console */
    void PrintCookieInfo( string& out );
    void PrintSocketInfo( string& out, int socket );

    static CookieMapIterator GetCookieIterator();
    static CookieRef* AddNew( string s );
    /* Nuke an existing */
    static void Delete( CookieID id );
    static void Delete( const char* name );

 private:
    CookieRef( string s );

    map<HostID,int> m_hostSockets;
    pthread_rwlock_t m_sockets_rwlock;
    CookieID m_connectionID;
    string m_name;
    int m_totalSent;

    static CookieID ms_nextConnectionID;
};

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
CookieRef* get_cookieRef( unsigned short cookieID );
CookieID CookieIdForName( const char* name );

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
