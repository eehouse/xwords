/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 2013 by Eric House (xwords@eehouse.org).  All rights reserved.
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


#ifndef _DEVMGR_H_
#define _DEVMGR_H_

#include <pthread.h>

#include "xwrelay_priv.h"
#include "addrinfo.h"

using namespace std;

class DevMgr {
 public:
    static DevMgr* Get();
    void rememberDevice( DevIDRelay devid, const AddrInfo* addr );
    const AddrInfo::AddrUnion* get( DevIDRelay devid );

    /* Called from ctrl port */
    void printDevices( string& str, DevIDRelay devid /* 0 means all */ );
    bool forgetDevice( DevIDRelay devid );
    void getKnownDevices( vector<DevIDRelay>& devids );

 private:

    void rememberDevice( DevIDRelay devid, const AddrInfo::AddrUnion* saddr );

    /* destructor's never called.... 
    ~DevMgr() { pthread_mutex_destroy( &m_mapLock ); }
    */

    class UDPAddrRec {
    public:
        UDPAddrRec( const AddrInfo::AddrUnion* addr, time_t tim ) {
            m_addr = *addr; m_added = tim;
        }
        AddrInfo::AddrUnion m_addr;
        time_t m_added;
    };

    DevMgr() { pthread_mutex_init( &m_mapLock, NULL ); }
    void addDevice( map<uint32_t, DevIDRelay>& devs, 
                    map<DevIDRelay,UDPAddrRec>::const_iterator iter );

    map<DevIDRelay,UDPAddrRec> m_devAddrMap;
    map<AddrInfo::AddrUnion, DevIDRelay> m_addrDevMap; 
    pthread_mutex_t m_mapLock;
};

#endif
