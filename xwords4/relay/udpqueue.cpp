/* -*- compile-command: "make -k -j3"; -*- */

/* 
 * Copyright 2010-2013 by Eric House (xwords@eehouse.org).  All rights
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
PacketThreadClosure::logStats()
{
    time_t now = time( NULL );
    if ( 1 < now - m_created ) {
        logf( XW_LOGERROR, "packet %d waited %d s for processing which then took %d s",
              getID(), m_dequed - m_created, now - m_dequed );
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
    assert( len > 0 );
    bool success = false;
    uint8_t tmp[len];
    ssize_t nRead = recv( m_sock, tmp, len, 0 );
    if ( 0 > nRead ) {          // error case
        m_errno = errno;
        if ( !stillGood() ) {
            logf( XW_LOGERROR, "%s(len=%d, socket=%d): recv failed: %d (%s)", __func__, 
                  len, m_sock, m_errno, strerror(m_errno) );
        }
    } else if ( 0 == nRead ) {  // remote socket closed
        logf( XW_LOGINFO, "%s(): remote closed (socket=%d)", __func__, m_sock );
        m_errno = -1;           // so stillGood will fail
    } else {
        // logf( XW_LOGVERBOSE0, "%s(): read %d bytes on socket %d", __func__,
        //       nRead, m_sock );
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
    assert( addr->isTCP() );
    PartialPacket* packet;
    bool success = true;

    int sock = addr->getSocket();

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
            uint16_t tmp;
            memcpy( &tmp, packet->data(), sizeof(tmp) );
            packet->m_len = ntohs(tmp);
            success = 0 < packet->m_len;
        }
    }

    if ( success && packet->readSoFar() >= sizeof( packet->m_len ) ) {
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

    success = success && (NULL == packet || packet->stillGood());
    return success;
}

void 
UdpQueue::handle( const AddrInfo* addr, const uint8_t* buf, int len, 
                  QueueCallback cb )
{
    PacketThreadClosure* ptc = new PacketThreadClosure( addr, buf, len, cb );
    MutexLock ml( &m_queueMutex );
    int id = ++m_nextID;
    ptc->setID( id );
    logf( XW_LOGINFO, "%s: enqueuing packet %d (socket %d, len %d)", 
          __func__, id, addr->getSocket(), len );
    m_queue.push_back( ptc );

    pthread_cond_signal( &m_queueCondVar );
}

// Remove any PartialPacket record with the same socket/fd. This makes sense
// when the socket's being reused or when we have just dealt with a single
// packet and might be getting more.
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
    newSocket( addr->getSocket() );
}

void* 
UdpQueue::thread_main()
{
    for ( ; ; ) {
        pthread_mutex_lock( &m_queueMutex );
        while ( m_queue.size() == 0 ) {
            pthread_cond_wait( &m_queueCondVar, &m_queueMutex );
        }
        PacketThreadClosure* ptc = m_queue.front();
        m_queue.pop_front();

        pthread_mutex_unlock( &m_queueMutex );

        ptc->noteDequeued();

        time_t age = ptc->ageInSeconds();
        if ( 30 > age ) {
            logf( XW_LOGINFO, "%s: dispatching packet %d (socket %d); "
                  "%d seconds old", __func__, ptc->getID(),
                  ptc->addr()->getSocket(), age );
            (*ptc->cb())( ptc );
            ptc->logStats();
        } else {
            logf( XW_LOGINFO, "%s: dropping packet %d; it's %d seconds old!", 
                  __func__, age );
        }
        delete ptc;
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

