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

    typedef union _AddrUnion {
        struct sockaddr addr;
        struct sockaddr_in addr_in;
    } AddrUnion;

    /* Those constructed without params are only valid after another copied on
       top of it */
    AddrInfo() { 
        m_isValid = false; 
    }

    AddrInfo( int socket, const AddrUnion* saddr, bool isTCP ) {
        construct( socket, saddr, isTCP );
    }

    AddrInfo( int socket, ClientToken clientToken, const AddrUnion* saddr ) {
        init( socket, clientToken, saddr );
    }

    void init( int socket, ClientToken clientToken, const AddrUnion* saddr ) {
        construct( socket, saddr, false );
        m_clientToken = clientToken;
    }

    void setIsTCP( bool val ) { m_isTCP = val; }
    bool isTCP() const { return m_isTCP; } /* later UDP will be here too */
    int socket() const { assert(m_isValid); return m_socket; }
    ClientToken clientToken() const { assert(m_isValid); return m_clientToken; }
    struct in_addr sin_addr() const { return m_saddr.addr_in.sin_addr; }
    const struct sockaddr* sockaddr() const { assert(m_isValid); return &m_saddr.addr; }
    const AddrUnion* saddr() const { assert(m_isValid); return &m_saddr; }

    bool equals( const AddrInfo& other ) const;

 private:
    void construct( int socket, const AddrUnion* saddr, bool isTCP ) {
        memset( this, 0, sizeof(*this) ); 

        m_socket = socket;
        m_isTCP = isTCP;
        memcpy( &m_saddr, saddr, sizeof(m_saddr) );
        m_isValid = true;
    }

    // AddrInfo& operator=(const AddrInfo&);      // Prevent assignment
    int m_socket;
    bool m_isTCP;
    bool m_isValid;
    ClientToken m_clientToken;   /* must be 32 bit */
    AddrUnion m_saddr;
};

#endif
