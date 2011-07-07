/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */

/* 
 * Copyright 2005-2009 by Eric House (xwords@eehouse.org).  All rights
 * reserved.
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

#include <list>

#include "cref.h"
#include "dbmgr.h"
#include "mlock.h"
#include "cidlock.h"

typedef map<CookieID,CookieRef*> CookieMap;
class CookieMapIterator;

class CrefInfo {
 public:
    string m_cookie;
    string m_connName;
    CookieID m_cid;
    int m_nPlayersSought;
    int m_nPlayersHere;
    XW_RELAY_STATE m_curState;
    time_t m_startTime;
    int m_nHosts;
    int m_langCode;
    string m_hostsIds;
    string m_hostSeeds;
    string m_hostIps;
};

class CrefMgrInfo {
 public:
    const char* m_ports;
    int m_nRoomsFilled;
    int m_nCrefsCurrent;
    time_t m_startTimeSpawn;
    vector<CrefInfo> m_crefInfo;
};

class CRefMgr {
    /* Maintain access to CookieRef instances, ultimately to ensure that no
       single instance is being acted on by more than one thread at a time,
       and that once one is destroyed no additional threads attempt to access
       it.
     */

 private:
    CRefMgr();

 public:
    static CRefMgr* Get();

    ~CRefMgr();

    void CloseAll();

    CookieMapIterator GetCookieIterator();

    /* PENDING.  These need to go through SafeCref */
    void Recycle( CookieID cid );
    void Recycle_locked( CookieRef* cref );
    void Recycle( const char* connName );
    CookieID CookieIdForName( const char* name );

    /* For use from ctrl only!!!! */
    /* void LockAll() { pthread_rwlock_wrlock( &m_cookieMapRWLock ); } */
    /* void UnlockAll() { pthread_rwlock_unlock( &m_cookieMapRWLock ); } */


    void MoveSockets( vector<int> sockets, CookieRef* cref );
    pthread_mutex_t* GetWriteMutexForSocket( int socket );
    void RemoveSocketRefs( int socket );
    void PrintSocketInfo( int socket, string& out );

    void IncrementFullCount( void );
    int GetNumRoomsFilled( void );

    int GetSize( void );

    time_t uptime();

    void GetStats( CrefMgrInfo& info );

 private:
    friend class SafeCref;

    /* We'll recycle cref instances rather than free and new them.  This
       solves, inelegantly, a problem where I want to free an instance (while
       holding its mutex) but can't know if other threads are trying to obtain
       that mutex.  It's illegal to destroy a mutex somebody's trying to lock.
       So we recycle, let the other thread succeed in locking but then quickly
       discover that the cref it got isn't what it wants.  See the SafeCref
       class.  */
    list<CookieRef*> m_freeList;
    pthread_mutex_t m_freeList_mutex;
    void addToFreeList( CookieRef* cref );
    CookieRef* getFromFreeList( void );

    /* connect case */
    CidInfo* getMakeCookieRef( const char* cookie, 
                               HostID hid, int socket, int nPlayersH,
                               int nPlayersS, int langCode, int seed,
                               bool wantsPublic, bool makePublic, 
                               bool* seenSeed );

    /* reconnect case; just the stuff we don't have in db */
    CidInfo* getMakeCookieRef( const char* connName, const char* cookie, 
                               HostID hid, int socket, int nPlayersH, 
                               int nPlayersS, int seed, int langCode, 
                               bool isPublic, bool* isDead );

    CidInfo* getMakeCookieRef( const char* const connName, bool* isDead );

    CidInfo* getCookieRef( CookieID cid, bool failOk = false );
    CidInfo* getCookieRef( int socket );
    bool checkCookieRef_locked( CookieRef* cref );
    CidInfo* getCookieRef_impl( CookieID cid );
    CookieRef* AddNew( const char* cookie, const char* connName, CookieID cid,
                       int langCode, int nPlayers, int nAlreadyHere );
    CookieRef* FindOpenGameFor( const char* cookie, const char* connName,
                                HostID hid, int socket, int nPlayersH, 
                                int nPlayersS, int gameSeed, int langCode, 
                                bool wantsPublic, bool* alreadyHere );

    CookieID cookieIDForConnName( const char* connName );
    CookieID nextCID( const char* connName );

    static void heartbeatProc( void* closure );
    void checkHeartbeats( time_t now );

    pthread_mutex_t m_roomsFilledMutex;
    int m_nRoomsFilled;

    pthread_rwlock_t m_cookieMapRWLock;
    CookieMap m_cookieMap;

    time_t m_startTime;
    string m_ports;

    DBMgr* m_db;
    CidLock* m_cidlock;

    friend class CookieMapIterator;
}; /* CRefMgr */


class SafeCref {

    /* Stack-based class that keeps more than one thread from accessing a
       CookieRef instance at a time. */

 public:
    /* for connect */
    SafeCref( const char* cookie, int socket, int nPlayersH, int nPlayersS, 
              unsigned short gameSeed, int langCode, bool wantsPublic, 
              bool makePublic );
    /* for reconnect */
    SafeCref( const char* connName, const char* cookie, HostID hid, 
              int socket, int nPlayersH, int nPlayersS, 
              unsigned short gameSeed, int langCode, 
              bool wantsPublic, bool makePublic );
    SafeCref( const char* const connName );
    SafeCref( CookieID cid, bool failOk = false );
    SafeCref( int socket );
    /* SafeCref( CookieRef* cref ); */
    ~SafeCref();

    bool Forward( HostID src, HostID dest, unsigned char* buf, int buflen ) {
        if ( IsValid() ) {
            CookieRef* cref = m_cinfo->GetRef();
            assert( 0 != cref->GetCid() );
            cref->_Forward( src, dest, buf, buflen );
            return true;
        } else {
            return false;
        }
    }
    bool Connect( int socket, int nPlayersH, int nPlayersS, int seed ) {
        if ( IsValid() ) {
            CookieRef* cref = m_cinfo->GetRef();
            assert( 0 != cref->GetCid() );
            return cref->_Connect( socket, nPlayersH, nPlayersS, seed, 
                                   m_seenSeed );
        } else {
            return false;
        }
    }
    bool Reconnect( int socket, HostID srcID, int nPlayersH, int nPlayersS,
                    int seed, XWREASON* errp ) {
        bool success = false;
        *errp = XWRELAY_ERROR_NONE;
        if ( IsValid() ) {
            CookieRef* cref = m_cinfo->GetRef();
            assert( 0 != cref->GetCid() );
            if ( m_dead ) {
                *errp = XWRELAY_ERROR_DEADGAME;
            } else {
                success = cref->_Reconnect( socket, srcID, nPlayersH, 
                                            nPlayersS, seed, m_dead );
            }
        }
        return success;
    }
    void Disconnect(int socket, HostID hostID ) {
        if ( IsValid() ) {
            CookieRef* cref = m_cinfo->GetRef();
            assert( 0 != cref->GetCid() );
            cref->_Disconnect( socket, hostID );
        }
    }

    void DeviceGone( HostID hid, int seed )
    {
        if ( IsValid() ) {
            CookieRef* cref = m_cinfo->GetRef();
            assert( 0 != cref->GetCid() );
            cref->_DeviceGone( hid, seed );
        }
    }

    bool HandleAck(HostID hostID ) {
        if ( IsValid() ) {
            CookieRef* cref = m_cinfo->GetRef();
            assert( 0 != cref->GetCid() );
            cref->_HandleAck( hostID );
            return true;
        } else {
            return false;
        }
    }
    void Shutdown() {
        if ( IsValid() ) {
            CookieRef* cref = m_cinfo->GetRef();
            assert( 0 != cref->GetCid() );
            cref->_Shutdown();
        }
    }
    void Remove( int socket ) {
        if ( IsValid() ) {
            CookieRef* cref = m_cinfo->GetRef();
            assert( 0 != cref->GetCid() );
            cref->_Remove( socket );
        }
    }

#ifdef RELAY_HEARTBEAT
    bool HandleHeartbeat( HostID id, int socket ) {
        if ( IsValid() ) {
            CookieRef* cref = m_cinfo->GetRef();
            assert( 0 != cref->GetCid() );
            cref->_HandleHeartbeat( id, socket );
            return true;
        } else {
            return false;
        }
    }
    void CheckHeartbeats( time_t now ) {
        if ( IsValid() ) {
            CookieRef* cref = m_cinfo->GetRef();
            assert( 0 != cref->GetCid() );
            cref->_CheckHeartbeats( now );
        }
    }
#endif

    void PrintCookieInfo( string& out ) {
        if ( IsValid() ) {
            CookieRef* cref = m_cinfo->GetRef();
            cref->_PrintCookieInfo( out );
        }
    }
    void CheckAllConnected() {
        if ( IsValid() ) {
            CookieRef* cref = m_cinfo->GetRef();
            cref->_CheckAllConnected();
        }
    }
    void CheckNotAcked( HostID hid ) {
        if ( IsValid() ) {
            CookieRef* cref = m_cinfo->GetRef();
            cref->_CheckNotAcked( hid );
        }
    }
    const char* Cookie() { 
        if ( IsValid() ) {
            CookieRef* cref = m_cinfo->GetRef();
            return cref->Cookie();
        } else {
            return "";          /* so don't crash.... */
        }
    }
    const char* ConnName() { 
        if ( IsValid() ) {
            CookieRef* cref = m_cinfo->GetRef();
            return cref->ConnName();
        } else {
            return "";          /* so don't crash.... */
        }
    }
    
    CookieID GetCid() { 
        if ( IsValid() ) {
            CookieRef* cref = m_cinfo->GetRef();
            return cref->GetCid();
        } else {
            return 0;          /* so don't crash.... */
        }
    }

    int GetPlayersTotal() { 
        if ( IsValid() ) {
            CookieRef* cref = m_cinfo->GetRef();
            return cref->GetPlayersSought();
        } else {
            return -1;          /* so don't crash.... */
        }
    }
    int GetPlayersHere() { 
        if ( IsValid() ) {
            CookieRef* cref = m_cinfo->GetRef();
            return cref->GetPlayersHere();
        } else {
            return -1;          /* so don't crash.... */
        }
    }

    const char* StateString() {
        if ( IsValid() ) {
            CookieRef* cref = m_cinfo->GetRef();
            return stateString( cref->CurState() );
        } else {
            return "";
        }
    }

    void GetHostsConnected( string* hosts, string* seeds, string* addrs ) {
        if ( IsValid() ) {
            CookieRef* cref = m_cinfo->GetRef();
            cref->_FormatHostInfo( hosts, seeds, addrs );
        }
    }

    time_t GetStartTime(void) {
        if ( IsValid() ) {
            CookieRef* cref = m_cinfo->GetRef();
            return cref->GetStarttime();
        } else {
            return 0;
        }
    }

    bool IsValid()        { return m_isValid; }
    bool SeenSeed()        { return m_seenSeed; }

 private:
    CidInfo* m_cinfo;
    CRefMgr* m_mgr;
    bool m_isValid;
    bool m_locked;
    bool m_dead;
    bool m_seenSeed;
}; /* SafeCref class */


class CookieMapIterator {
 public:
    CookieMapIterator(pthread_rwlock_t* rwlock);
    ~CookieMapIterator() {}
    CookieID Next();
 private:
    RWReadLock m_rwl;
    CookieMap::const_iterator _iter;
};

#endif
