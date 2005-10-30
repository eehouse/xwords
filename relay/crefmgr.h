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

    void CloseAll();

    CookieMapIterator GetCookieIterator();

    /* PENDING.  These need to go through SafeCref */
    void Delete( CookieID id );
    void Delete( CookieRef* cref );
    void Delete( const char* connName );
    CookieID CookieIdForName( const char* name );

    /* For use from ctrl only!!!! */
    void LockAll() { pthread_rwlock_wrlock( &m_cookieMapRWLock ); }
    void UnlockAll() { pthread_rwlock_unlock( &m_cookieMapRWLock ); }

    /* Track sockets independent of cookie refs */
    void Associate( int socket, CookieRef* cref );
    void Disassociate( int socket, CookieRef* cref );
    pthread_mutex_t* GetWriteMutexForSocket( int socket );
    void RemoveSocketRefs( int socket );
    void PrintSocketInfo( int socket, string& out );
    SocketsIterator MakeSocketsIterator();

 private:
    friend class SafeCref;
    CookieRef* getMakeCookieRef_locked( const char* cORn, int isCookie, 
                                        HostID hid, 
                                        int nPlayersH, int nPlayersT );
    CookieRef* getCookieRef_locked( CookieID cookieID );
    CookieRef* getCookieRef_locked( int socket );
    int checkCookieRef_locked( CookieRef* cref );
    CookieRef* getCookieRef_impl( CookieID cookieID );
    CookieRef* AddNew( const char* cookie, const char* connName, CookieID id );
    CookieRef* FindOpenGameFor( const char* cORn, int isCookie,
                                HostID hid, int nPlayersH, int nPlayersT );

    CookieID cookieIDForConnName( const char* connName );
    CookieID nextCID( const char* connName );

    static void heartbeatProc( void* closure );
    void checkHeartbeats( time_t now );

    CookieID m_nextCID;

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
    SafeCref( const char* cookieOrConnName, int cookie, HostID hid, 
              int nPlayersH, int nPlayersT );
    SafeCref( CookieID cid );
    SafeCref( int socket );
    SafeCref( CookieRef* cref );
    ~SafeCref();

    int Forward( HostID src, HostID dest, unsigned char* buf, int buflen ) {
        if ( IsValid() ) {
            m_cref->_Forward( src, dest, buf, buflen );
            return 1;
        } else {
            return 0;
        }
    }
    int Connect( int socket, HostID srcID, int nPlayersH, int nPlayersT ) {
        if ( IsValid() ) {
            m_cref->_Connect( socket, srcID, nPlayersH, nPlayersT );
            return 1;
        } else {
            return 0;
        }
    }
    int Reconnect( int socket, HostID srcID, int nPlayersH, int nPlayersT ) {
        if ( IsValid() ) {
            m_cref->_Reconnect( socket, srcID, nPlayersH, nPlayersT );
            return 1;
        } else {
            return 0;
        }
    }
    void Disconnect(int socket, HostID hostID ) {
        if ( IsValid() ) {
            m_cref->_Disconnect( socket, hostID );
        }
    }
    void Shutdown() {
        if ( IsValid() ) {
            m_cref->_Shutdown();
        }
    }
    void Remove( int socket ) {
        if ( IsValid() ) {
            m_cref->_Remove( socket );
        }
    }
    int HandleHeartbeat( HostID id, int socket ) {
        if ( IsValid() ) {
            m_cref->_HandleHeartbeat( id, socket );
            return 1;
        } else {
            return 0;
        }
    }
    void CheckHeartbeats( time_t now ) {
        if ( IsValid() ) {
            m_cref->_CheckHeartbeats( now );
        }
    }
    void PrintCookieInfo( string& out ) {
        if ( IsValid() ) {
            m_cref->_PrintCookieInfo( out );
        }
    }
    void CheckAllConnected() {
        if ( IsValid() ) {
            m_cref->_CheckAllConnected();
        }
    }
    const char* Cookie() { 
        if ( IsValid() ) {
            return m_cref->Cookie();
        } else {
            return "";          /* so don't crash.... */
        }
    }
    const char* ConnName() { 
        if ( IsValid() ) {
            return m_cref->ConnName();
        } else {
            return "";          /* so don't crash.... */
        }
    }
    

 private:
    int IsValid()        { return m_cref != NULL; }

    CookieRef* m_cref;
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
