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
#include <deque>

#include "xwrelay_priv.h"
#include "addrinfo.h"

using namespace std;

class UdpThreadClosure;

typedef void (*QueueCallback)( UdpThreadClosure* closure );

class UdpThreadClosure {
public:
    UdpThreadClosure( const AddrInfo* addr, unsigned char* buf, 
                      int len, QueueCallback cb )
        : m_buf(new unsigned char[len])
        , m_len(len)
        , m_addr(*addr)
        , m_cb(cb)
        , m_created(time( NULL ))
        { 
            memcpy( m_buf, buf, len ); 
        }

    ~UdpThreadClosure() { delete m_buf; }

    const unsigned char* buf() const { return m_buf; } 
    int len() const { return m_len; }
    const AddrInfo::AddrUnion* saddr() const { return m_addr.saddr(); }
    const AddrInfo* addr() const { return &m_addr; }
    void noteDequeued() { m_dequed = time( NULL ); }
    void logStats();
    const QueueCallback cb() const { return m_cb; }

 private:
    unsigned char* m_buf;
    int m_len;
    AddrInfo m_addr;
    QueueCallback m_cb;
    time_t m_created;
    time_t m_dequed;
};

class UdpQueue {
 public:
    static UdpQueue* get();
    UdpQueue();
    ~UdpQueue();
    void handle( const AddrInfo* addr, unsigned char* buf, int len,
                 QueueCallback cb );

 private:
    static void* thread_main_static( void* closure );
    void* thread_main();

    pthread_mutex_t m_queueMutex;
    pthread_cond_t m_queueCondVar;
    deque<UdpThreadClosure*> m_queue;

};

#endif
