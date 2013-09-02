/* -*- -*- */

/* 
 * Copyright 2005-2011 by Eric House (xwords@eehouse.org).  All rights
 * reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
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

#include <stdio.h>
#include <assert.h>

#include "cidlock.h"
#include "mlock.h"
#include "strwpf.h"

// #define CIDLOCK_DEBUG

const vector<AddrInfo>
CidInfo::GetAddrs( void )
{
    return 0 == m_owner || NULL == m_cref ? 
        m_addrs : m_cref->GetAddrs();
}

void
CidInfo::SetOwner( pthread_t owner )
{
    if ( 0 == owner ) {
        if ( 0 == --m_ownerCount ) {
            m_owner = 0;
        }
    } else {
        ++m_ownerCount;
        assert( 0 == m_owner || owner == m_owner );
        m_owner = owner; 
    }
    assert( 0 <= m_ownerCount );
    logf( XW_LOGINFO, "%s(owner=%u); m_ownerCount now %d", __func__, owner, 
          m_ownerCount );
}

CidLock* CidLock::s_instance = NULL;

CidLock::CidLock() : m_nextCID(0)
{
    pthread_mutex_init( &m_infos_mutex, NULL );
    pthread_cond_init( &m_infos_condvar, NULL );
}
 
CidLock::~CidLock()
{
    pthread_mutex_destroy( &m_infos_mutex );
}

#ifdef CIDLOCK_DEBUG
# define PRINT_CLAIMED() print_claimed(__func__)
void 
CidLock::print_claimed( const char* caller )
{
    int unclaimed = 0;
    StrWPF str;
    str.printf( "after %s: ", caller );
    // Assume we have the mutex!!!!
    map< CookieID, CidInfo*>::const_iterator iter;
    for ( iter = m_infos.begin(); iter != m_infos.end(); ++iter ) {
        CidInfo* info = iter->second;
        if ( 0 == info->GetOwner() ) {
            ++unclaimed;
        } else {
            str. printf( "%d,", info->GetCid() );
        }
    }
    str.printf( " (plus %d unclaimed.)", unclaimed );
    logf( XW_LOGINFO, "%s: claimed: %s", __func__, str.c_str() );
}
#else
# define PRINT_CLAIMED()
#endif

CidInfo* 
CidLock::Claim( const CookieID origCid )
{
    CookieID cid = origCid;
#ifdef CIDLOCK_DEBUG
    logf( XW_LOGINFO, "%s(%d)", __func__, origCid );
#endif
    CidInfo* info = NULL;
    pthread_t self = pthread_self();
    for ( ; ; ) {
        MutexLock ml( &m_infos_mutex );

        if ( 0 == cid ) {
            cid = ++m_nextCID;
            logf( XW_LOGINFO, "%s: assigned cid: %d", __func__, cid );
        }

        map< CookieID, CidInfo*>::iterator iter = m_infos.find( cid );
        if ( iter == m_infos.end() ) { // not there at all
            info = new CidInfo( cid );
            m_infos.insert( pair<CookieID, CidInfo*>( cid, info ) );
        } else {
            pthread_t owner = iter->second->GetOwner();
            if ( 0 == owner || self == owner ) {
                info = iter->second;
            }
        }

        if ( NULL != info ) {   // we're done
            info->SetOwner( self );
            PRINT_CLAIMED();
            break;
        }

#ifdef CIDLOCK_DEBUG
        logf( XW_LOGINFO, "%s(%d): waiting....", __func__, cid );
#endif
        pthread_cond_wait( &m_infos_condvar, &m_infos_mutex );
    }
#ifdef CIDLOCK_DEBUG
    logf( XW_LOGINFO, "%s(%d): DONE", __func__, origCid );
#endif
    return info;
} /* CidLock::Claim */

CidInfo* 
CidLock::ClaimSocket( const AddrInfo* addr )
{
    CidInfo* info = NULL;
#ifdef CIDLOCK_DEBUG
    logf( XW_LOGINFO, "%s(sock=%d)", __func__, addr->socket() );
#endif
    for ( ; ; ) {
        MutexLock ml( &m_infos_mutex );

        map<CookieID, CidInfo*>::const_iterator iter;
        for ( iter = m_infos.begin(); NULL == info && iter != m_infos.end(); 
              ++iter ) {
            const vector<AddrInfo>& addrs = iter->second->GetAddrs();
            vector<AddrInfo>::const_iterator iter2;
            for ( iter2 = addrs.begin(); iter2 != addrs.end(); ++iter2 ) {
                if ( iter2->equals(*addr) ) {
                    assert( !info ); // I hit this -- twice!!!!
                    if ( 0 == iter->second->GetOwner() ) {
                        info = iter->second;
                        info->SetOwner( pthread_self() );
                        PRINT_CLAIMED();
                    }
                    // break;
                }
            }
        }

        /* break if socket isn't here or if it's not claimed */
        if ( iter == m_infos.end() || NULL != info ) {
            break;
        }
#ifdef CIDLOCK_DEBUG
        logf( XW_LOGINFO, "%s(sock=%d): waiting....", __func__, addr->socket() );
#endif
        pthread_cond_wait( &m_infos_condvar, &m_infos_mutex );
    }

#ifdef CIDLOCK_DEBUG
    logf( XW_LOGINFO, "%s(%d): DONE", __func__, info? info->GetCid():0 );
#endif
    return info;
}

void
CidLock::Relinquish( CidInfo* claim, bool drop )
{
    CookieID cid = claim->GetCid();
#ifdef CIDLOCK_DEBUG
    logf( XW_LOGINFO, "%s(%d,drop=%d)", __func__, cid, drop );
#endif

    MutexLock ml( &m_infos_mutex );
    map< CookieID, CidInfo*>::iterator iter = m_infos.find( cid );
    assert( iter != m_infos.end() );
    assert( iter->second == claim );
    assert( claim->GetOwner() == pthread_self() );
    if ( drop ) {
#ifdef CIDLOCK_DEBUG
        logf( XW_LOGINFO, "%s: deleting %p (cid=%d)",
              __func__, claim, claim->GetCid() );
#endif
        m_infos.erase( iter );
        claim->SetOwner( 0 );
        delete claim;
    } else {
        CookieRef* ref = claim->GetRef();
        if ( NULL != ref ) {
            claim->SetAddrs( ref->GetAddrs() ); /* cache these */
        }
        claim->SetOwner( 0 );
    }
    PRINT_CLAIMED();
    pthread_cond_signal( &m_infos_condvar );
#ifdef CIDLOCK_DEBUG
    logf( XW_LOGINFO, "%s(%d,drop=%d): DONE", __func__, cid, drop );
#endif
}
