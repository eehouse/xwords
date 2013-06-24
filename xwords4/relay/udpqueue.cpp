/* -*- compile-command: "make -k -j3"; -*- */

/* 
 * Copyright 2010-2012 by Eric House (xwords@eehouse.org).  All rights
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

#include <errno.h>
#include "udpqueue.h"
#include "mlock.h"


static UdpQueue* s_instance = NULL;


void 
UdpThreadClosure::logStats()
{
    time_t now = time( NULL );
    if ( 1 < now - m_created ) {
        logf( XW_LOGERROR, "packet waited %d s for processing which then took %d s",
              m_dequed - m_created, now - m_dequed );
    }
}

bool
PartialPacket::stillGood() const
{
    return 0 == m_errno
        || EAGAIN == m_errno
        || EWOULDBLOCK == m_errno;
}

bool
PartialPacket::readAtMost( int len )
{
    bool success = false;
    uint8_t tmp[len];
    ssize_t nRead = recv( m_sock, tmp, len, 0 );
    if ( 0 > nRead ) {          // error case
        m_errno = errno;
        logf( XW_LOGERROR, "%s(len=%d): recv failed: %d (%s)", __func__, len,
              m_errno, strerror(m_errno) );
    } else if ( 0 == nRead ) {  // remote socket closed
        logf( XW_LOGINFO, "%s: remote closed", __func__ );
        m_errno = -1;           // so stillGood will fail
    } else {
        m_errno = 0;
        success = len == nRead;
        int curSize = m_buf.size();
        m_buf.resize( nRead + curSize );
        memcpy( &m_buf[curSize], tmp, nRead );
    }
    return success;
}

UdpQueue::UdpQueue() 
{
    m_nextID = 0;
    pthread_mutex_init ( &m_partialsMutex, NULL );
    pthread_mutex_init ( &m_queueMutex, NULL );
    pthread_cond_init( &m_queueCondVar, NULL );

    pthread_t thread;
    int result = pthread_create( &thread, NULL, thread_main_static, this );
    assert( result == 0 );
    result = pthread_detach( thread );
    assert( result == 0 );
}

UdpQueue::~UdpQueue() 
{
    pthread_cond_destroy( &m_queueCondVar );
    pthread_mutex_destroy ( &m_queueMutex );
    pthread_mutex_destroy ( &m_partialsMutex );
}

UdpQueue* 
UdpQueue::get()
{
    if ( s_instance == NULL ) {
        s_instance = new UdpQueue();
    }
    return s_instance;
}

// return false if socket should no longer be used
bool
UdpQueue::handle( const AddrInfo* addr, QueueCallback cb )
{
    PartialPacket* packet;

    int sock = addr->socket();

    // Hang onto this mutex for as long as we may be writing to the packet
    // since having it deleted while in use would be bad.
    MutexLock ml( &m_partialsMutex );

    map<int, PartialPacket*>::iterator iter = m_partialPackets.find( sock );
    if ( m_partialPackets.end() == iter ) {
        packet = new PartialPacket( sock );
        m_partialPackets.insert( pair<int, PartialPacket*>( sock, packet ) );
    } else {
        packet = iter->second;
    }

    // First see if we've read the length bytes
    if ( packet->readSoFar() < sizeof( packet->m_len ) ) {
        if ( packet->readAtMost( sizeof(packet->m_len) - packet->readSoFar() ) ) {
            packet->m_len = ntohs(*(unsigned short*)packet->data());
        }
    }

    if ( packet->readSoFar() >= sizeof( packet->m_len ) ) {
        assert( 0 < packet->m_len );
        int leftToRead = 
            packet->m_len - (packet->readSoFar() - sizeof(packet->m_len));
        if ( packet->readAtMost( leftToRead ) ) {
            handle( addr, packet->data() + sizeof(packet->m_len), 
                    packet->m_len, cb );
            packet = NULL;
            newSocket_locked( sock );
        }
    }

    return NULL == packet || packet->stillGood();
}

void 
UdpQueue::handle( const AddrInfo* addr, const uint8_t* buf, int len, 
                  QueueCallback cb )
{
    UdpThreadClosure* utc = new UdpThreadClosure( addr, buf, len, cb );
    MutexLock ml( &m_queueMutex );
    int id = ++m_nextID;
    utc->setID( id );
    logf( XW_LOGINFO, "%s: enqueuing packet %d (len %d)", __func__, id, len );
    m_queue.push_back( utc );

    pthread_cond_signal( &m_queueCondVar );
}

void
UdpQueue::newSocket_locked( int sock )
{
    map<int, PartialPacket*>::iterator iter = m_partialPackets.find( sock );
    if ( m_partialPackets.end() != iter ) {
        delete iter->second;
        m_partialPackets.erase( iter );
    }
}
void
UdpQueue::newSocket( int sock )
{
    MutexLock ml( &m_partialsMutex );
    newSocket_locked( sock );
}

void
UdpQueue::newSocket( const AddrInfo* addr )
{
    assert( addr->isTCP() );
    newSocket( addr->socket() );
}

void* 
UdpQueue::thread_main()
{
    for ( ; ; ) {
        pthread_mutex_lock( &m_queueMutex );
        while ( m_queue.size() == 0 ) {
            pthread_cond_wait( &m_queueCondVar, &m_queueMutex );
        }
        UdpThreadClosure* utc = m_queue.front();
        m_queue.pop_front();

        pthread_mutex_unlock( &m_queueMutex );

        utc->noteDequeued();
        logf( XW_LOGINFO, "%s: dispatching packet %d", __func__, utc->getID() );
        (*utc->cb())( utc );
        utc->logStats();
        delete utc;
    }
    return NULL;
}

/* static */ void*
UdpQueue::thread_main_static( void* closure )
{
    blockSignals();

    UdpQueue* me = (UdpQueue*)closure;
    return me->thread_main();
}

