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
DevMgr::rememberDevice( DevIDRelay devid, const AddrInfo::AddrUnion* saddr )
{
    if ( DBMgr::DEVID_NONE != devid ) {
        gchar* b64 = g_base64_encode( (uint8_t*)&saddr->u.addr, 
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
DevMgr::rememberDevice( DevIDRelay devid, const AddrInfo* addr )
{
    rememberDevice( devid, addr->saddr() );
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

int
DevMgr::forgetDevices( vector<DevIDRelay>& devids )
{
    int count = 0;
    MutexLock ml( &m_mapLock );
    if ( 0 == devids.size() ) {
        count = m_devAddrMap.size();
        m_devAddrMap.clear();
    } else {
        vector<DevIDRelay>::const_iterator devidIter;
        for ( devidIter = devids.begin(); devids.end() != devidIter; ++devidIter ) {
            map<DevIDRelay,UDPAddrRec>::iterator iter = 
                m_devAddrMap.find( *devidIter );
            if ( m_devAddrMap.end() != iter ) {
                ++count;
                m_devAddrMap.erase( iter );
            }
        }
    }
    return count;
}

void
DevMgr::getKnownDevices( vector<DevIDRelay>& devids )
{
    MutexLock ml( &m_mapLock );
    map<DevIDRelay,UDPAddrRec>::const_iterator iter;
    for ( iter = m_devAddrMap.begin(); m_devAddrMap.end() != iter; ++iter ) {
        devids.push_back( iter->first );
    }
}

// Print info about every device, ordered by how old they are (how long since
// last remembered).  Build a separate array with the mutex held, then release
// it, so that as much work as possible is done on the calling thread without
// holding up more important stuff.
void
DevMgr::printDevices( StrWPF& str, const vector<DevIDRelay>& devids )
{
    map<uint32_t, DevIDRelay> agedDevs;
    {
        MutexLock ml( &m_mapLock );
        map<DevIDRelay, UDPAddrRec>::const_iterator iter;
        if ( 0 != devids.size() ) {
            for ( vector<DevIDRelay>::const_iterator iter2 = devids.begin();
                  devids.end() != iter2; ++iter2 ) {
                iter = m_devAddrMap.find(*iter2);
                if ( m_devAddrMap.end() != iter ) {
                    addDevice( agedDevs, iter );
                }
            }
        } else {
            for ( iter = m_devAddrMap.begin(); iter != m_devAddrMap.end(); ++iter ) {
                addDevice( agedDevs, iter );
            }
        }
    }

    // Now sort by age and print
    vector<uint32_t> keys;
    map<uint32_t, DevIDRelay>::const_iterator iter;
    for ( iter = agedDevs.begin(); agedDevs.end() != iter; ++iter ) {
        keys.push_back( iter->first );
    }

    std::sort( keys.begin(), keys.end() );
    std::reverse( keys.begin(), keys.end() );

    time_t now = time(NULL);
    vector<uint32_t>::const_iterator keysIter;
    int row = 0;
    for ( keysIter = keys.begin(); keys.end() != keysIter; ++keysIter ) {
        uint32_t age = *keysIter;
        DevIDRelay devid = agedDevs.find( age )->second;
        age = now - age;
        str.printf( "%.3d: devid: % 10d; age: %.3d seconds\n", ++row, 
                    devid, age );
    }
}

void
DevMgr::addDevice( map<uint32_t, DevIDRelay>& devs, 
                   map<DevIDRelay,UDPAddrRec>::const_iterator iter )
{
    DevIDRelay devid = iter->first;
    uint32_t added = iter->second.m_added;
    devs.insert( pair<uint32_t, DevIDRelay>(added, devid) );
}
