/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */

/* 
 * Copyright 2005-2011 by Eric House (xwords@eehouse.org).  All rights
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

#include <assert.h>
#include <string.h>

#include "addrinfo.h"

bool
AddrInfo::equals( const AddrInfo& other ) const
{ 
    bool equal = other.isTCP() == isTCP();
    if ( equal ) {
        if ( isTCP() ) {
            equal = m_socket == other.m_socket;
        } else {
            // assert( m_socket == other.m_socket ); /* both same UDP socket */
            /* what does equal mean on udp addresses?  Same host, or same host AND game */
            equal = m_clientToken == other.m_clientToken
                && 0 == memcmp( &m_saddr, &other.m_saddr, sizeof(m_saddr) );
        }
    }
    return equal;
}

