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

#ifndef _UDPAGER_H_
#define _UDPAGER_H_

#include <map>

#include "addrinfo.h"

using namespace std;

class UDPAger {
 public:
    static UDPAger* Get();
    UDPAger();
    void Refresh( const AddrInfo* addr );
    bool IsCurrent( const AddrInfo* addr );
    uint16_t MaxIntervalSeconds() const { return m_maxInterval / 1000; }

 private:

    class AgePair {
    public:
        AgePair( uint32_t created, uint32_t lastSeen ) {
            m_created = created;
            m_lastSeen = lastSeen;
        }
        void update( uint32_t lastSeen ) { m_lastSeen = lastSeen; }
        uint32_t lastSeen() const { return m_lastSeen; }
        uint32_t created() const { return m_created; }
    private:
        uint32_t m_created;
        uint32_t m_lastSeen;
    };

    /* Map socket addresses against times, moving the time forward only
       when it's been too long since we saw it. */
    int m_maxInterval;          /* config: how long since we heard */

    map<AddrInfo::AddrUnion, AgePair*> m_addrTimeMap; 
    pthread_mutex_t m_addrTimeMapLock;


};

#endif
