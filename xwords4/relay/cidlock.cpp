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

void 
CidLock::print_claimed()
{
    char buf[256] = {0};
    int len = 0;
    // Assume we have the mutex!!!!
    map< CookieID, CidInfo*>::iterator iter = m_infos.begin();
    while ( iter != m_infos.end() ) {
        CidInfo* info = iter->second;
        if ( 0 != info->GetOwner() ) {
            len += snprintf( &buf[len], sizeof(buf)-len, "%d,", 
                             info->GetCid() );
        }
        ++iter;
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
            print_claimed();
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
    logf( XW_LOGINFO, "%s(%d)", __func__, sock );

    MutexLock ml( &m_infos_mutex );
    map< CookieID, CidInfo*>::iterator iter = m_infos.begin();
    while ( iter != m_infos.end() ) {
        if ( sock == iter->second->GetSocket() ) {
            info = iter->second;
            break;
        }
        ++iter;
    }

    logf( XW_LOGINFO, "%s(%d): DONE", __func__, info? info->GetCid():0 );
    return info;
}

bool
CidLock::Associate_locked( const CookieRef* cref, int socket )
{
    map< CookieID, CidInfo*>::iterator iter = m_infos.begin();
    while ( iter != m_infos.end() ) {
        if ( cref == iter->second->GetRef() ) {
            iter->second->SetSocket( socket );
            break;
        }
        ++iter;
    }
    bool isNew = m_sockets.find( socket ) == m_sockets.end();
    if ( isNew ) {
        m_sockets.insert( socket );
    }
    return isNew;
}

bool
CidLock::Associate( const CookieRef* cref, int socket )
{
    MutexLock ml( &m_infos_mutex );
    return Associate_locked( cref, socket );
}

void
CidLock::DisAssociate( const CookieRef* cref, int socket )
{
    MutexLock ml( &m_infos_mutex );
    Associate_locked( cref, 0 );
    m_sockets.erase( socket );
}

void
CidLock::Relinquish( CidInfo* claim, bool drop )
{
    CookieID cid = claim->GetCid();
    logf( XW_LOGINFO, "%s(%d)", __func__, cid );

    MutexLock ml( &m_infos_mutex );
    map< CookieID, CidInfo*>::iterator iter = m_infos.find( cid );
    assert( iter != m_infos.end() );
    if ( drop ) {
        delete iter->second;
        m_infos.erase( iter );
    } else {
        iter->second->SetOwner( 0 );
    }
    print_claimed();
    pthread_cond_signal( &m_infos_condvar );
    logf( XW_LOGINFO, "%s(%d): DONE", __func__, cid );
}
