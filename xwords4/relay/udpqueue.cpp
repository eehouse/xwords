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

UdpQueue::UdpQueue() 
{
    m_nextID = 0;
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
}

UdpQueue* 
UdpQueue::get()
{
    if ( s_instance == NULL ) {
        s_instance = new UdpQueue();
    }
    return s_instance;
}

bool
UdpQueue::handle( const AddrInfo* addr, QueueCallback cb )
{
    bool success = false;
    int sock = addr->socket();
    unsigned short msgLen;
    ssize_t nRead = recv( sock, &msgLen, sizeof(msgLen), MSG_WAITALL );
    if ( 0 == nRead ) {
        logf( XW_LOGINFO, "%s: recv(sock=%d) => 0: remote closed", __func__, sock );
    } else if ( nRead != sizeof(msgLen) ) {
        logf( XW_LOGERROR, "%s: first recv => %d: %s", __func__, 
              nRead, strerror(errno) );
    } else {
        msgLen = ntohs( msgLen );
        if ( MAX_MSG_LEN <= msgLen ) {
            logf( XW_LOGERROR, "%s: message of len %d too large; dropping", __func__, msgLen );
        } else {
            unsigned char buf[msgLen];
            nRead = recv( sock, buf, msgLen, MSG_WAITALL );
            if ( nRead == msgLen ) {
                logf( XW_LOGINFO, "%s: read %d bytes on socket %d", __func__, nRead, sock );
                handle( addr, buf, msgLen, cb );
                success = true;
            } else {
                logf( XW_LOGERROR, "%s: second recv failed: %s", __func__, 
                      strerror(errno) );
            }
        }
    }
    return success;
}

void 
UdpQueue::handle( const AddrInfo* addr, unsigned char* buf, int len, 
                  QueueCallback cb )
{
    UdpThreadClosure* utc = new UdpThreadClosure( addr, buf, len, cb );
    MutexLock ml( &m_queueMutex );
    int id = ++m_nextID;
    utc->setID( id );
    logf( XW_LOGINFO, "%s: enqueuing packet %d", __func__, id );
    m_queue.push_back( utc );

    int sock = addr->socket();
    map<int, vector<UdpThreadClosure*> >::iterator iter = m_bySocket.find( sock );
    if ( iter == m_bySocket.end() ) {
        logf( XW_LOGINFO, "%s: creating vector for socket %d", __func__, sock );
        vector<UdpThreadClosure*> vect;
        vect.push_back( utc );
        m_bySocket.insert( pair<int, vector<UdpThreadClosure*> >(sock, vect) );
    } else {
        iter->second.push_back( utc );
        logf( XW_LOGINFO, "%s: now have %d packets for socket %d", 
              __func__, iter->second.size(), sock );
    }

    pthread_cond_signal( &m_queueCondVar );
}

void
UdpQueue::forgetSocket( const AddrInfo* addr )
{
    assert( addr->isTCP() );
    int sock = addr->socket();
    MutexLock ml( &m_queueMutex );

    map<int, vector<UdpThreadClosure*> >::iterator iter = m_bySocket.find( sock );
    if ( m_bySocket.end() != iter ) {
        vector<UdpThreadClosure*>& vect = iter->second;
        vector<UdpThreadClosure*>::iterator iter2;
        for ( iter2 = vect.begin(); vect.end() != iter2; ++ iter2 ) {
            UdpThreadClosure* utc = *iter2;
            assert( -1 != utc->addr()->socket() );
            utc->invalSocket();
            logf( XW_LOGINFO, "%s: invalidating socket %d in packet %d",
                  __func__, sock, utc->getID() );
            // vect.erase( iter2 );
        }
        vect.clear();
    }

    // deque<UdpThreadClosure*>::iterator iter;
    // for ( iter = m_queue.begin(); iter != m_queue.end(); ++iter ) {
    //     const AddrInfo* addr = (*iter)->addr();
    //     if ( sock == addr->socket() ) {
    //         logf( XW_LOGINFO, "%s: invalidating socket %d in packet %d",
    //               __func__, sock, (*iter)->getID() );
    //         (*iter)->invalSocket();
    //     }
    // }
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

        int sock = utc->addr()->socket();
        if ( -1 != sock ) {
            map<int, vector<UdpThreadClosure*> >::iterator iter = m_bySocket.find( sock );
            assert ( iter != m_bySocket.end() );
            vector<UdpThreadClosure*>& vect = iter->second;
            assert( utc == *vect.begin() );
            vect.erase( vect.begin() );
            logf( XW_LOGINFO, "%s: %d packets remaining for socket %d", 
                  __func__, vect.size(), sock );
        }
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

