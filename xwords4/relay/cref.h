/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */

#ifndef _CREF_H_
#define _CREF_H_

#include <map>
#include <vector>
#include "xwrelay_priv.h"

using namespace std;

class CookieMapIterator;        /* forward */

class CookieRef {

 public:
    CookieRef();
    ~CookieRef();

    /* Within this cookie, remember that this hostID and socket go together.
       If the hostID is HOST_ID_SERVER, it's the server. */
    void Associate( int socket, HostID srcID );
    short GetHeartbeat() { return 60; }
    short GetConnID() { return m_connectionID; }
    int SocketForHost( HostID dest );
    void Remove( int socket );
    int CountSockets() { return m_hostSockets.size(); }

    static CookieMapIterator GetCookieNameIterator();

 private:
    map<HostID,int> m_hostSockets;
/*     HostID m_clientHostIDs[3]; */
/*     int    m_sockets[3]; */
/*     int    m_serverSocket; */
/*     int    m_nClients; */
    unsigned short m_connectionID;

    static int ms_nextConnectionID;
};

typedef map<string,CookieRef*> CookieMap;

class CookieMapIterator {
 public:
    CookieMapIterator();
    ~CookieMapIterator() {}
    const char* Next();
 private:
    CookieMap::const_iterator _iter;
};

CookieRef* get_make_cookieRef( char* cookie );
CookieRef* get_cookieRef( unsigned short cookieID );
void Associate( int socket, CookieRef* cref );
void RemoveSocketRefs( int socket );

#endif
