/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */

#include <string>
#include <map>
#include <assert.h>

#include "cref.h"
#include "xwrelay.h"

using namespace std;

static CookieMap gCookieMap;

int CookieRef::ms_nextConnectionID = 1000;

CookieRef* 
get_make_cookieRef( char* cookie )
{
    string s(cookie);
    CookieMap::iterator iter = gCookieMap.find(s);
    if ( iter != gCookieMap.end() ) {
        logf( "ref found for cookie %s", cookie );
        return iter->second;
    }

    CookieRef* ref = new CookieRef();
    gCookieMap.insert( pair<string, CookieRef*>(s, ref ) );
    return ref;
}

CookieRef* 
get_cookieRef( unsigned short cookieID )
{
    CookieMap::iterator iter = gCookieMap.begin();
    while ( iter != gCookieMap.end() ) {
        CookieRef* ref = iter->second;
        if ( ref->GetConnID() == cookieID ) {
            return ref;
        }
        ++iter;
    }
    return NULL;
} /* get_cookieRef */

static void
ForgetCref( CookieRef* cref )
{
    CookieMap::iterator iter = gCookieMap.begin();
    while ( iter != gCookieMap.end() ) {
        CookieRef* ref = iter->second;
        if ( ref == cref ) {
            logf( "erasing cref" );
            gCookieMap.erase( iter );
            break;
        }
        ++iter;
    }
    assert( iter != gCookieMap.end() ); /* didn't find it */
}

typedef map< int, pair<CookieRef*, pthread_t> > SocketMap;
static SocketMap gSocketStuff;

void
Associate( int socket, CookieRef* cref )
{
    SocketMap::iterator iter = gSocketStuff.find( socket );
    if ( iter == gSocketStuff.end() ) {
        logf( "replacing existing cref/threadID pair for socket %d", socket );
    }
    
    pthread_t self = pthread_self();
    pair<CookieRef*,pthread_t> pr( cref, self );
    gSocketStuff.insert( pair< int, pair< CookieRef*,pthread_t > >
                         ( socket, pr ) );
} /* Associate */

static CookieRef*
getCookieRefForSocket( int socket )
{
    SocketMap::iterator iter = gSocketStuff.find( socket );
    if ( iter != gSocketStuff.end() ) {
        pair<CookieRef*,pthread_t>pr = iter->second;
        return pr.first;
    } else {
        return NULL;
    }
}

void
RemoveSocketRefs( int socket )
{
    CookieRef* cref = getCookieRefForSocket( socket );
    cref->Remove( socket );

    SocketMap::iterator iter = gSocketStuff.find( socket );
    assert( iter != gSocketStuff.end() );
    gSocketStuff.erase( iter );

    if ( cref->CountSockets() == 0 ) {
        ForgetCref( cref );
        delete cref;
    }
}

/*****************************************************************************
 * CookieRef class
 *****************************************************************************/

CookieRef::CookieRef()
{
    m_connectionID = ms_nextConnectionID++; /* needs a mutex!!! */
}

CookieRef::~CookieRef()
{
    logf( "CookieRef for %d being deleted", m_connectionID );
}

void
CookieRef::Associate( int socket, HostID srcID )
{
    assert( srcID != HOST_ID_NONE );
    logf( "remembering pair: hostid=%x, socket=%d", srcID, socket );
    m_hostSockets.insert( pair<HostID,int>(srcID,socket) );
}

int
CookieRef::SocketForHost( HostID dest )
{
    int socket;
    map<HostID,int>::iterator iter = m_hostSockets.find( dest );
    if ( iter == m_hostSockets.end() ) {
        socket = -1;
    } else {
        socket = iter->second;
        logf( "socketForHost(%x) => %d", dest, socket );
    }
    logf( "returning socket=%d for hostid=%x", socket, dest );
    return socket;
}

void
CookieRef::Remove( int socket )
{
    map<HostID,int>::iterator iter = m_hostSockets.begin();
    while ( iter != m_hostSockets.end() ) {
        if ( iter->second == socket ) {
            m_hostSockets.erase(iter);
            break;
        }
        ++iter;
    }
}

/* static */ CookieMapIterator
CookieRef::GetCookieNameIterator()
{
    CookieMapIterator iter;
    return iter;
}


CookieMapIterator:: CookieMapIterator()
     : _iter( gCookieMap.begin() )
{
}

const char* 
CookieMapIterator::Next()
{
    const char* str = NULL;
    if ( _iter != gCookieMap.end() ) {
        str = _iter->first.c_str();
        ++_iter;
    }
    return str;
}
