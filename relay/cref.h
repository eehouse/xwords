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
    short GetConnID() { return m_connectionID; }
    int SocketForHost( HostID dest );
    void Remove( int socket );
    int CountSockets() { return m_hostSockets.size(); }
    string Name() { return m_name; }

    static CookieMapIterator GetCookieNameIterator();
    static CookieRef* AddNew( string s );

 private:
    CookieRef( string s );

    map<HostID,int> m_hostSockets;
    pthread_mutex_t m_mutex;
    CookieID m_connectionID;
    string m_name;

    static CookieID ms_nextConnectionID;
};

typedef map<CookieID,CookieRef*> CookieMap;

class CookieMapIterator {
 public:
    CookieMapIterator();
    ~CookieMapIterator() {}
    const char* Next();
 private:
    CookieMap::const_iterator _iter;
};

CookieRef* get_make_cookieRef( char* cookie, CookieID connID );
CookieRef* get_cookieRef( unsigned short cookieID );
void Associate( int socket, CookieRef* cref );
void RemoveSocketRefs( int socket );
pthread_mutex_t* GetWriteMutexForSocket( int socket );

#endif
