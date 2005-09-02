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

#ifndef _CREFMGR_H_
#define _CREFMGR_H_

#include "cref.h"

typedef map<CookieID,CookieRef*> CookieMap;
class CookieMapIterator;
class SocketStuff;
typedef map< int, SocketStuff* > SocketMap;

class SocketsIterator {
 public:
    SocketsIterator( SocketMap::iterator iter, SocketMap::iterator end, 
                     pthread_mutex_t* mutex );
    ~SocketsIterator();
    int Next();
 private:
    SocketMap::iterator m_iter;
    SocketMap::iterator m_end;
    pthread_mutex_t* m_mutex; /* locked */
};


class CRefMgr {
    /* Maintain access to CookieRef instances, ultimately to ensure that no
       single instance is being acted on by more than one thread at a time,
       and that once one is destroyed no additional threads attempt to access
       it.
     */

 public:
    static CRefMgr* Get();

    CRefMgr();
    ~CRefMgr();

    CookieMapIterator GetCookieIterator();

    /* PENDING.  These need to go through SafeCref */
    void CheckHeartbeats( time_t now, vector<int>* sockets );
    void Delete( CookieID id );
    void Delete( CookieRef* cref );
    void Delete( const char* name );
    CookieID CookieIdForName( const char* name );

    /* For use from ctrl only!!!! */
    void LockAll() { pthread_rwlock_wrlock( &m_cookieMapRWLock ); }
    void UnlockAll() { pthread_rwlock_unlock( &m_cookieMapRWLock ); }

    /* Track sockets independent of cookie refs */
    void Associate( int socket, CookieRef* cref );
    pthread_mutex_t* GetWriteMutexForSocket( int socket );
    void RemoveSocketRefs( int socket );
    void PrintSocketInfo( int socket, string& out );
    SocketsIterator MakeSocketsIterator();

 private:
    friend class SafeCref;
    CookieRef* getMakeCookieRef_locked( const char* cookie, CookieID connID );
    CookieRef* getCookieRef_locked( CookieID cookieID );
    CookieRef* getCookieRef_locked( int socket );
    int checkCookieRef_locked( CookieRef* cref );
    CookieRef* getCookieRef_impl( CookieID cookieID );
    CookieRef* AddNew( string s, CookieID id );

    int LockCref( CookieRef* cref );
    void UnlockCref( CookieRef* cref );

    pthread_mutex_t m_guard;
    map<CookieRef*,pthread_mutex_t*> m_crefMutexes;

    pthread_rwlock_t m_cookieMapRWLock;
    CookieMap m_cookieMap;

    pthread_mutex_t m_SocketStuffMutex;
    SocketMap m_SocketStuff;

    friend class CookieMapIterator;
}; /* CRefMgr */


class SafeCref {

    /* Stack-based class that keeps more than one thread from accessing a
       CookieRef instance at a time. */

 public:
    SafeCref( const char* cookie, CookieID connID );
    SafeCref( CookieID connID );
    SafeCref( int socket );
    SafeCref( CookieRef* cref );
    ~SafeCref();

    int IsValid()        { return m_cref != NULL; }

    void Forward( HostID src, HostID dest, unsigned char* buf, int buflen ) {
        m_cref->_Forward( src, dest, buf, buflen );
    }
    void Connect( int socket, HostID srcID ) {
        m_cref->_Connect( socket, srcID );
    }
    void Reconnect( int socket, HostID srcID ) {
        m_cref->_Reconnect( socket, srcID );
    }
    void Remove( int socket ) {
        m_cref->_Remove( socket );
    }
    void HandleHeartbeat( HostID id, int socket ) {
        m_cref->_HandleHeartbeat( id, socket );
    }
    void PrintCookieInfo( string& out ) {
        m_cref->_PrintCookieInfo( out );
    }
    void CheckAllConnected() {
        m_cref->_CheckAllConnected();
    }
    string Name() { return m_cref->Name(); }
    

 private:
    CookieRef* m_cref;
    CookieID m_connID;
    CRefMgr* m_mgr;
};


class CookieMapIterator {
 public:
    CookieMapIterator();
    ~CookieMapIterator() {}
    CookieID Next();
 private:
    CookieMap::const_iterator _iter;
};

#endif
