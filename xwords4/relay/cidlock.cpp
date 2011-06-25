/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */

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

const set<int>
CidInfo::GetSockets( void )
{
    return 0 == m_owner || NULL == m_cref ? 
        m_sockets : m_cref->GetSockets();
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

#define PRINT_CLAIMED() print_claimed(__func__)
void 
CidLock::print_claimed( const char* caller )
{
    char buf[512] = {0};
    int len = snprintf( buf, sizeof(buf), "after %s: ", caller );
    // Assume we have the mutex!!!!
    map< CookieID, CidInfo*>::iterator iter;
    for ( iter = m_infos.begin(); iter != m_infos.end(); ++iter ) {
        CidInfo* info = iter->second;
        CookieID cid = 0 == info->GetOwner() ? 0 : info->GetCid();
        len += snprintf( &buf[len], sizeof(buf)-len, "%d,", cid );
    }
    logf( XW_LOGINFO, "%s: claimed: %s", __func__, buf );
}

CidInfo* 
CidLock::Claim( CookieID cid )
{
    logf( XW_LOGINFO, "%s(%d)", __func__, cid );
    CidInfo* info = NULL;
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
            if ( 0 == iter->second->GetOwner() ) {
                info = iter->second;
            }
        }

        if ( NULL != info ) {   // we're done
            info->SetOwner( pthread_self() );
            PRINT_CLAIMED();
            break;
        }

        logf( XW_LOGINFO, "%s(%d): waiting....", __func__, cid );
        pthread_cond_wait( &m_infos_condvar, &m_infos_mutex );
    }
    logf( XW_LOGINFO, "%s(%d): DONE", __func__, cid );
    return info;
}

CidInfo* 
CidLock::ClaimSocket( int sock )
{
    CidInfo* info = NULL;
    logf( XW_LOGINFO, "%s(sock=%d)", __func__, sock );

    for ( ; ; ) {
        MutexLock ml( &m_infos_mutex );

        map< CookieID, CidInfo*>::iterator iter;
        for ( iter = m_infos.begin(); iter != m_infos.end(); ++iter ) {
            const set<int>& sockets = iter->second->GetSockets();
            if ( sockets.end() != sockets.find( sock ) ) {
                if ( 0 == iter->second->GetOwner() ) {
                    info = iter->second;
                    info->SetOwner( pthread_self() );
                    PRINT_CLAIMED();
                }
                break;
            }
        }

        /* break if socket isn't here or if it's not claimed */
        if ( iter == m_infos.end() || NULL != info ) {
            break;
        }
        logf( XW_LOGINFO, "%s(sock=%d): waiting....", __func__, sock );
        pthread_cond_wait( &m_infos_condvar, &m_infos_mutex );
    }

    logf( XW_LOGINFO, "%s(%d): DONE", __func__, info? info->GetCid():0 );
    return info;
}

void
CidLock::Relinquish( CidInfo* claim, bool drop )
{
    CookieID cid = claim->GetCid();
    logf( XW_LOGINFO, "%s(%d,drop=%d)", __func__, cid, drop );

    MutexLock ml( &m_infos_mutex );
    map< CookieID, CidInfo*>::iterator iter = m_infos.find( cid );
    assert( iter != m_infos.end() );
    assert( iter->second == claim );
    assert( claim->GetOwner() == pthread_self() );
    if ( drop ) {
        logf( XW_LOGINFO, "%s: deleting %p", __func__, iter->second );
        m_infos.erase( iter );
        delete claim;
    } else {
        claim->SetSockets( claim->GetRef()->GetSockets() ); /* cache these */
        claim->SetOwner( 0 );
    }
    PRINT_CLAIMED();
    pthread_cond_signal( &m_infos_condvar );
    logf( XW_LOGINFO, "%s(%d,drop=%d): DONE", __func__, cid, drop );
}
