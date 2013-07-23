/* -*- compile-command: "make -k -j3"; -*- */

/* 
 * Copyright 2013 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#include <glib.h>

#include "udpager.h"
#include "configs.h"
#include "mlock.h"

static UDPAger* s_instance = NULL;

/* static */ UDPAger*
UDPAger::Get() 
{
    if ( NULL == s_instance ) {
        s_instance = new UDPAger();
    }
    return s_instance;
} /* Get */

UDPAger::UDPAger()
{
    if ( !RelayConfigs::GetConfigs()-> GetValueFor( "UDP_RECYLE_INTERVAL",
                                                    &m_maxInterval ) ) {
        assert(0);
    }
    logf( XW_LOGINFO, "read %d from configs for UDP_RECYLE_INTERVAL", 
          m_maxInterval );
    m_maxInterval *= 1000;      // make it milliseconds

    pthread_mutex_init( &m_addrTimeMapLock, NULL );
}

// An address is valid as long as we keep hearing from it within a certain
// frequency.  When we hear from it but it's been too long, assume it's new
// and give it a new timestamp.
void
UDPAger::Refresh( const AddrInfo* addr )
{
    const AddrInfo::AddrUnion* saddr = addr->saddr();
    uint32_t readWhen = addr->created();
    gchar* b64 = g_base64_encode( (unsigned char*)&saddr->u.addr, 
                                  sizeof(saddr->u.addr) );

    MutexLock ml( &m_addrTimeMapLock );

    map<AddrInfo::AddrUnion, AgePair*>::iterator iter = 
        m_addrTimeMap.find( *saddr ); 
    if ( m_addrTimeMap.end() == iter ) { // it's new; just insert
        AgePair* ap = new AgePair( readWhen, readWhen );
        m_addrTimeMap.insert( pair<AddrInfo::AddrUnion, 
                              AgePair*>(*saddr, ap ) );
            logf( XW_LOGINFO, "%s: adding '%s'", __func__, b64 );
    } else {
        AgePair* ap = iter->second;
        assert( ap->lastSeen() <= readWhen );
        int interval = readWhen - ap->lastSeen();
        if ( m_maxInterval >= interval ) {
            logf( XW_LOGINFO, "%s: refreshing '%s'; last seen %d "
                  "milliseconds ago", __func__, b64, interval );
            ap->update( readWhen );
        } else {
            logf( XW_LOGINFO, "%s: RESETTING '%s'; last seen %d "
                  "milliseconds ago", __func__, b64, interval );
            delete ap;
            iter->second = new AgePair( readWhen, readWhen );
        }
    }

    g_free( b64 );
}

bool
UDPAger::IsCurrent( const AddrInfo* addr )
{
    const AddrInfo::AddrUnion* saddr = addr->saddr();
    uint32_t readWhen = addr->created();

    MutexLock ml( &m_addrTimeMapLock );
    map<AddrInfo::AddrUnion, AgePair*>::const_iterator iter = 
        m_addrTimeMap.find( *saddr ); 
    assert( m_addrTimeMap.end() != iter );

    AgePair* ap = iter->second;
    bool result = readWhen >= ap->created();
    if ( !result ) {
        logf( XW_LOGINFO, "%s() => %d", __func__, result );
    }
    return result;
 }
