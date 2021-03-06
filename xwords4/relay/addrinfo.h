/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */

/* 
 * Copyright 2005 - 2013 by Eric House (xwords@eehouse.org).  All rights
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

#ifndef _ADDRINFO_H_
#define _ADDRINFO_H_

#include <netinet/in.h>
#include <string.h>
#include <assert.h>

class AddrInfo {
 public:
    typedef uint32_t ClientToken;
    static const ClientToken NULL_TOKEN = 0;

    class AddrUnion {
    public:
        union {
            struct sockaddr addr;
            struct sockaddr_in addr_in;
        } u;

        bool operator<(const AddrUnion& other) const {
            return 0 > memcmp( &this->u, &other.u, sizeof(this->u) );
        }
    };

    /* Those constructed without params are only valid after another copied on
       top of it */
    AddrInfo() { 
        m_isValid = false; 
    }

    AddrInfo( int sock, const AddrUnion* saddr, bool isTCP ) {
        assert( -1 != sock );
        construct( sock, saddr, isTCP );
    }

    AddrInfo( const AddrUnion* saddr ) {
        init( -1, 0, saddr );
    }

    AddrInfo( ClientToken clientToken, const AddrUnion* saddr ) {
        init( -1, clientToken, saddr );
    }

    AddrInfo( int sock, ClientToken clientToken, const AddrUnion* saddr ) {
        assert( -1 != sock );
        init( sock, clientToken, saddr );
    }

    void setIsTCP( bool val ) { m_isTCP = val; }
    bool isTCP() const { return m_isTCP; }
    bool isUDP() const { return !m_isTCP; }
    int getSocket() const { assert(m_isValid); return m_socket; }
    ClientToken clientToken() const { assert(m_isValid); return m_clientToken; }
    struct in_addr sin_addr() const { return m_saddr.u.addr_in.sin_addr; }
    const struct sockaddr* sockaddr() const { assert(m_isValid); 
        return &m_saddr.u.addr; }
    const AddrUnion* saddr() const { assert(m_isValid); return &m_saddr; }
    uint32_t created() const { return m_createdMillis; }
    bool isCurrent() const;

    bool equals( const AddrInfo& other ) const;

    /* refcount the underlying socket (doesn't modify instance) */
    void ref() const;
    void unref() const;
    int getref() const;

 private:
    void construct( int sock, const AddrUnion* saddr, bool isTCP );
    void init( int sock, ClientToken clientToken, const AddrUnion* saddr ) {
        construct( sock, saddr, false );
        m_clientToken = clientToken;
    }
    void printRefMap() const;

    // AddrInfo& operator=(const AddrInfo&);      // Prevent assignment
    int m_socket;
    bool m_isTCP;
    bool m_isValid;
    ClientToken m_clientToken;   /* must be 32 bit */
    AddrUnion m_saddr;
    uint32_t m_createdMillis;    /* milliseconds since boot, from clock_gettime() */
};

#endif
