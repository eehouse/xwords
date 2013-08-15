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
#include <algorithm>

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
    if ( DBMgr::DEVID_NONE != devid ) {
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

        // Don't think we need m_addrDevMap anymore
        map<AddrInfo::AddrUnion, DevIDRelay>::iterator iter = 
            m_addrDevMap.find(*saddr); 
        if ( m_addrDevMap.end() != iter && devid != iter->second ) {
            logf( XW_LOGERROR, "%s: addr '%s' already listed (for devid %d)",
                  __func__, b64, iter->second );
            // assert(0);              // assert instead about age?
            iter->second = devid;
        } else {
            m_addrDevMap.insert( pair<AddrInfo::AddrUnion, 
                                 DevIDRelay>(*saddr, devid ) );
        }

        logf( XW_LOGINFO, "dev->addr map now contains %d entries", 
              m_devAddrMap.size() );
        g_free( b64 );
    }
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

// Print info about every device, ordered by how old they are (how long since
// last remembered).  Build a separate array with the mutex held, then release
// it, so that as much work as possible is done on the calling thread without
// holding up more important stuff.
void
DevMgr::printDevices( string& str )
{
    map<uint32_t, DevIDRelay> agedDevs;
    {
        MutexLock ml( &m_mapLock );
        map<DevIDRelay,UDPAddrRec>::const_iterator iter;
        for ( iter = m_devAddrMap.begin(); iter != m_devAddrMap.end(); ++iter ) {
            DevIDRelay devid = iter->first;
            uint32_t added = iter->second.m_added;
            agedDevs.insert( pair<uint32_t, DevIDRelay>(added, devid) );
        }
    }

    // Now sort by age and print
    vector<uint32_t> keys;
    map<uint32_t, DevIDRelay>::const_iterator iter1;
    for ( iter1 = agedDevs.begin(); agedDevs.end() != iter1; ++iter1 ) {
        keys.push_back( iter1->first );
    }

    std::sort( keys.begin(), keys.end() );

    time_t now = time(NULL);
    vector<uint32_t>::const_iterator keysIter;
    for ( keysIter = keys.begin(); keys.end() != keysIter; ++keysIter ) {
        uint32_t age = *keysIter;
        DevIDRelay devid = agedDevs.find( age )->second;
        age = now - age;
        string_printf( str, "devid: %d; age: %d seconds\n", devid, age );
    }

}
