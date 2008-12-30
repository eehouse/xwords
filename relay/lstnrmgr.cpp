/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */

/* 
 * Copyright 2007 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <assert.h>

#include <algorithm>

#include "lstnrmgr.h"
#include "mlock.h"

bool
ListenerMgr::AddListener( int port )
{
    logf( XW_LOGINFO, "%s(%d)", __func__, port );
    MutexLock ml( &m_mutex );
    return addOne( port );
}

void 
ListenerMgr::SetAll( const vector<int>* ivp )
{
    logf( XW_LOGINFO, "%s", __func__ );
    MutexLock ml( &m_mutex );

    vector<int> have;
    map<int,int>::iterator iter2 = m_socks_to_ports.begin();
    while ( iter2 != m_socks_to_ports.end() ) {
        have.push_back(iter2->second);
        ++iter2;
    }
    std::sort(have.begin(), have.end());

    vector<int> want = *ivp;
    std::sort(want.begin(), want.end());

    /* Now go through both lists in order, removing and adding as
       appropriate. */
    size_t iWant = 0;
    size_t iHave = 0;
    while ( (iHave < have.size()) || (iWant < want.size()) ) {
        assert( iHave <= have.size() && iWant <= want.size() );
        while( (iWant < want.size()) 
               && ((iHave == have.size() || want[iWant] < have[iHave])) ) {
            addOne( want[iWant] );
            ++iWant;
        }
        while ( (iHave < have.size()) 
                && (iWant == want.size() || (have[iHave] < want[iWant])) ) {
            removePort( have[iHave] );
            ++iHave;
        }
        while ( (iHave < have.size()) && (iWant < want.size())
                && (have[iHave] == want[iWant]) ) {
            /* keep both */
            ++iWant; ++iHave;
        }
    }

} /* SetAll */

/* void */
/* ListenerMgr::RemoveListener( int listener ) */
/* { */
/*     MutexLock ml( &m_mutex ); */
/*     removeFD( listener ); */
/* } */

void
ListenerMgr::RemoveAll()
{
    MutexLock ml( &m_mutex );
    for ( ; ; ) {
        map<int,int>::const_iterator iter = m_socks_to_ports.begin();
        if ( iter == m_socks_to_ports.end() ) {
            break;
        }
        removeSocket( iter->first );
    }
}

void
ListenerMgr::AddToFDSet( fd_set* rfds )
{
    MutexLock ml( &m_mutex );
    map<int,int>::const_iterator iter = m_socks_to_ports.begin();
    while ( iter != m_socks_to_ports.end() ) {
        FD_SET( iter->first, rfds );
        ++iter;
    }    
}

int
ListenerMgr::GetHighest()
{
    int highest = 0;
    MutexLock ml( &m_mutex );
    map<int,int>::const_iterator iter = m_socks_to_ports.begin();
    while ( iter != m_socks_to_ports.end() ) {
        if ( iter->first > highest ) {
            highest = iter->first;
        }
        ++iter;
    }    
    return highest;
}

bool 
ListenerMgr::PortInUse( int port )
{
    MutexLock ml( &m_mutex );
    return portInUse( port );
}

void
ListenerMgr::removeSocket( int sock )
{
    /* Assumption: we have the mutex! */
    logf( XW_LOGINFO, "%s(%d)", __func__, sock );
    map<int,int>::iterator iter = m_socks_to_ports.find( sock );
    assert( iter != m_socks_to_ports.end() );
    m_socks_to_ports.erase(iter);
    close(sock);
}

void
ListenerMgr::removePort( int port )
{
    /* Assumption: we have the mutex! */
    logf( XW_LOGINFO, "%s(%d)", __func__, port );
    map<int,int>::iterator iter = m_socks_to_ports.begin();
    while ( iter != m_socks_to_ports.end() ) {
        if ( iter->second == port ) {
            int sock = iter->first;
            close(sock);
            m_socks_to_ports.erase(iter);
            break;
        }
        ++iter;
    }
    assert( iter != m_socks_to_ports.end() ); /* we must have found it! */
}

bool
ListenerMgr::addOne( int port )
{
    logf( XW_LOGINFO, "%s(%d)", __func__, port );
    /* Assumption: we have the mutex! */
    assert( !portInUse(port) );
    bool success = false;
    int sock = make_socket( INADDR_ANY, port );
    success = sock != -1;
    if ( success ) {
        m_socks_to_ports.insert( pair<int,int>(sock, port) );
    }
    return success;
}

bool 
ListenerMgr::portInUse( int port )
{
    /* Assumption: we have the mutex! */
    bool found = false;
    map<int,int>::const_iterator iter = m_socks_to_ports.begin();
    while ( iter != m_socks_to_ports.end() ) {
        if ( iter->second == port ) {
            found = true;
            break;
        }
        ++iter;
    }
    return found;
}

int 
ListenersIter::next()
{
    int result = -1;
    if ( m_iter != m_lm->m_socks_to_ports.end() ) {
        result = m_fds? m_iter->first : m_iter->second;
        ++m_iter;
    }
/*     logf( XW_LOGINFO, "%s=>%d", __func__, result ); */
    return result;
}

