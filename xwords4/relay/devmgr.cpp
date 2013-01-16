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

#include "devmgr.h"
#include "mlock.h"

static DevMgr* s_instance = NULL;

/* static */ DevMgr*
DevMgr::Get() 
{
    if ( s_instance == NULL ) {
        s_instance = new DevMgr();
    }
    return s_instance;
} /* Get */

void
DevMgr::Remember( DevIDRelay devid, const AddrInfo::AddrUnion* saddr )
{
    time_t now = time( NULL );
    MutexLock ml( &m_mapLock );
    UDPAddrRec rec( saddr, now );
    m_devAddrMap.insert( pair<DevIDRelay,UDPAddrRec>( devid, rec ) );
    logf( XW_LOGINFO, "dev->addr map now contains %d entries", m_devAddrMap.size() );
}

#if 0                        // not used yet
const AddrInfo::AddrUnion* 
DevMgr::get( DevIDRelay devid )
{
    const AddrInfo::AddrUnion* result = NULL;
    MutexLock ml( &m_mapLock );
    map<DevIDRelay,UDPAddrRec>::iterator iter;
    iter = m_devAddrMap.find( devid );
    if ( m_devAddrMap.end() != iter ) {
        result = &iter->second.m_addr;
        logf( XW_LOGINFO, "%s: found addr for %.8x; is %d seconds old", __func__,
              devid, time(NULL) - iter->second.m_added );
    }
    return result;
}
#endif

