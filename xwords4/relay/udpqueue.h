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


#ifndef _UDPQUEUE_H_
#define _UDPQUEUE_H_

#include <pthread.h>

#include "xwrelay_priv.h"
#include "addrinfo.h"

using namespace std;

class UdpThreadClosure {
public:
    UdpThreadClosure( const AddrInfo::AddrUnion* saddr, unsigned char* buf, int len ) { 
        m_saddr = *saddr;
        m_buf = new unsigned char[len]; 
        memcpy( m_buf, buf, len ); 
        m_len = len;
    }
    ~UdpThreadClosure() { delete m_buf; }

    const unsigned char* buf() const { return m_buf; } 
    int len() const { return m_len; }
    const AddrInfo::AddrUnion* saddr() const { return &m_saddr; }

 private:
    unsigned char* m_buf;
    int m_len;
    AddrInfo::AddrUnion m_saddr;
};

typedef void (*QueueCallback)( UdpThreadClosure* closure );

class UdpQueue {
 public:
    static UdpQueue* get();
    void handle( const AddrInfo::AddrUnion* saddr, unsigned char* buf, int len,
                 QueueCallback cb );

 private:
};

#endif
