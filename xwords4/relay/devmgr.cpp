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

#include "devmgr.h"
#include "mlock.h"

static DevMgr* s_instance = NULL;

/* static */ DevMgr*
DevMgr::Get() 
{
    if ( NULL == s_instance ) {
        s_instance = new DevMgr();
    }
    return s_instance;
} /* Get */

void
DevMgr::Remember( DevIDRelay devid, const AddrInfo::AddrUnion* saddr )
{
    assert( DBMgr::DEVID_NONE != devid );
    gchar* b64 = g_base64_encode( (unsigned char*)&saddr->u.addr, 
                                  sizeof(saddr->u.addr) );

    XW_LogLevel level = XW_LOGINFO;
    if ( willLog( level ) ) {
        logf( level, "%s(devid=%d, saddr='%s')", __func__, devid, b64 );
    }

    time_t now = time( NULL );
    UDPAddrRec rec( saddr, now );

    MutexLock ml( &m_mapLock );

    // C++'s insert doesn't replace, but the result tells whether the key was
    // already there and provides an iterator via which it can be updated
    pair<map<DevIDRelay,UDPAddrRec>::iterator, bool> result = 
        m_devAddrMap.insert( pair<DevIDRelay,UDPAddrRec>( devid, rec ) );
    if ( !result.second ) {
        logf( XW_LOGINFO, "%s: replacing address for %d", __func__, devid );
        result.first->second = rec;
    }

    map<AddrInfo::AddrUnion, DevIDRelay>::iterator iter = 
        m_addrDevMap.find(*saddr); 
    if ( m_addrDevMap.end() != iter && devid != iter->second ) {
        logf( XW_LOGERROR, "%s: addr '%s' already listed (for devid %d)",
              __func__, b64, iter->second );
        // assert(0);
        iter->second = devid;
    } else {
        m_addrDevMap.insert( pair<AddrInfo::AddrUnion, 
                             DevIDRelay>(*saddr, devid ) );
    }

    logf( XW_LOGINFO, "dev->addr map now contains %d entries", 
          m_devAddrMap.size() );
    g_free( b64 );
}

void
DevMgr::Remember( DevIDRelay devid, const AddrInfo* addr )
{
    Remember( devid, addr->saddr() );
}

const AddrInfo::AddrUnion* 
DevMgr::get( DevIDRelay devid )
{
    const AddrInfo::AddrUnion* result = NULL;
    MutexLock ml( &m_mapLock );
    map<DevIDRelay,UDPAddrRec>::const_iterator iter;
    iter = m_devAddrMap.find( devid );
    if ( m_devAddrMap.end() != iter ) {
        result = &iter->second.m_addr;
        logf( XW_LOGINFO, "%s: found addr for %.8x; is %d seconds old", __func__,
              devid, time(NULL) - iter->second.m_added );
    }
    logf( XW_LOGINFO, "%s(devid=%d)=>%p", __func__, devid, result );
    return result;
}

