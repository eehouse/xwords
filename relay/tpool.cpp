/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */

/* 
 * Copyright 2005 by Eric House (fixin@peak.org).  All rights reserved.
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
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/poll.h>
#include <errno.h>

#include "tpool.h"
#include "xwrelay_priv.h"
#include "xwrelay.h"
#include "timermgr.h"
#include "mlock.h"

XWThreadPool* XWThreadPool::g_instance = NULL;

/* static */ XWThreadPool*
XWThreadPool::GetTPool()
{
    XWThreadPool* me = g_instance;
    if ( me == NULL ) {
        me = new XWThreadPool();
        g_instance = me;
    }
    return me;
}

XWThreadPool::XWThreadPool()
{
    pthread_rwlock_init( &m_activeSocketsRWLock, NULL );
    pthread_mutex_init ( &m_queueMutex, NULL );

    pthread_cond_init( &m_queueCondVar, NULL );

    int fd[2];
    if ( pipe( fd ) ) {
        logf( XW_LOGERROR, "pipe failed" );
    }
    m_pipeRead = fd[0];
    m_pipeWrite = fd[1];

    m_nThreads = 0;
}

void
XWThreadPool::Setup( int nThreads, packet_func pFunc )
{
    m_nThreads = nThreads;
    m_pFunc = pFunc;

    pthread_t thread;

    int i;
    for ( i = 0; i < nThreads; ++i ) {
        int result = pthread_create( &thread, NULL, tpool_main, this );
        assert( result == 0 );
    }

    int result = pthread_create( &thread, NULL, listener_main, this );
    assert( result == 0 );
}

void
XWThreadPool::AddSocket( int socket )
{
    logf( XW_LOGINFO, "AddSocket(%d)", socket );
    {
        RWWriteLock ml( &m_activeSocketsRWLock );
        m_activeSockets.push_back( socket );
    }
    interrupt_poll();
}

int
XWThreadPool::RemoveSocket( int socket )
{
    int found = 0;
    {
        RWWriteLock ml( &m_activeSocketsRWLock );

        vector<int>::iterator iter = m_activeSockets.begin();
        while ( iter != m_activeSockets.end() ) {
            if ( *iter == socket ) {
                m_activeSockets.erase( iter );
                found = 1;
                break;
            }
            ++iter;
        }
    }
    return found;
} /* RemoveSocket */

void
XWThreadPool::CloseSocket( int socket )
{
    int do_interrupt = 0;
    if ( !RemoveSocket( socket ) ) {
        RWWriteLock rwl( &m_activeSocketsRWLock );
        deque<int>::iterator iter = m_queue.begin();
        while ( iter != m_queue.end() ) {
            if ( *iter == socket ) {
                m_queue.erase( iter );
                do_interrupt = 1;
                break;
            }
            ++iter;
        }
    }
/*     close( socket ); */
/*     if ( do_interrupt ) { */
    /* We always need to interrupt the poll because the socket we're closing
       will be in the list being listened to.  That or we need to drop sockets
       that have been removed on some other thread while the poll call's
       blocking.*/
    interrupt_poll();
/*     } */
}

int
XWThreadPool::get_process_packet( int socket )
{
    short packetSize;
    assert( sizeof(packetSize) == 2 );

    ssize_t nRead = recv( socket, &packetSize, 
                          sizeof(packetSize), MSG_WAITALL );
    if ( nRead != 2 ) {
        killSocket( socket, "nRead != 2" );
        return 0;
    }

    packetSize = ntohs( packetSize );
    if ( packetSize < 0 || packetSize > MAX_MSG_LEN ) {
        killSocket( socket, "packetSize wrong" );
        return 0;
    }

    unsigned char buf[MAX_MSG_LEN];
    nRead = recv( socket, buf, packetSize, MSG_WAITALL );
    if ( nRead != packetSize ) {
        killSocket( socket, "nRead != packetSize" ); 
        return 0;
    }
    logf( XW_LOGINFO, "read %d bytes\n", nRead );

    logf( XW_LOGINFO, "calling m_pFunc" );
    (*m_pFunc)( buf, packetSize, socket );

    return 1;
} /* get_process_packet */

/* static */ void*
XWThreadPool::tpool_main( void* closure )
{
    XWThreadPool* me = (XWThreadPool*)closure;
    return me->real_tpool_main();
}

void*
XWThreadPool::real_tpool_main()
{
    logf( XW_LOGINFO, "worker thread starting" );
    for ( ; ; ) {

        pthread_mutex_lock( &m_queueMutex );
        while ( m_queue.size() == 0 ) {
            pthread_cond_wait( &m_queueCondVar, &m_queueMutex );
        }

        int socket = m_queue.front();
        m_queue.pop_front();
        pthread_mutex_unlock( &m_queueMutex );
        logf( XW_LOGINFO, "worker thread got socket %d from queue", socket );

        if ( get_process_packet( socket ) ) {
            AddSocket( socket );
        } /* else drop it: error */
    }
    logf( XW_LOGINFO, "worker thread exiting" );
    return NULL;
}

void
XWThreadPool::interrupt_poll()
{
    logf( XW_LOGINFO, "interrupt_poll" );
    unsigned char byt = 0;
    int nSent = write( m_pipeWrite, &byt, 1 );
    if ( nSent != 1 ) {
        logf( XW_LOGERROR, "errno = %d", errno );
    }
}

void*
XWThreadPool::real_listener()
{
    int flags = POLLIN | POLLERR | POLLHUP;
    TimerMgr* tmgr = TimerMgr::getTimerMgr();

    for ( ; ; ) {

        pthread_rwlock_rdlock( &m_activeSocketsRWLock );
        int nSockets = m_activeSockets.size() + 1; /* for pipe */
        pollfd* fds = (pollfd*)malloc( sizeof(fds[0]) * nSockets );
        pollfd* curfd = fds;
        char* log = (char*)malloc( 4 * nSockets );
        log[0] = '\0';
        int len = 0;
        
        curfd->fd = m_pipeRead;
        curfd->events = flags;
        len += sprintf( log+len, "%d,", curfd->fd );
        ++curfd;

        vector<int>::iterator iter = m_activeSockets.begin();
        while ( iter != m_activeSockets.end() ) {
            curfd->fd = *iter++;
            curfd->events = flags;
            len += sprintf( log+len, "%d,", curfd->fd );
            ++curfd;
        }
        pthread_rwlock_unlock( &m_activeSocketsRWLock );

        int nMillis = tmgr->getPollTimeout();

        int nEvents = poll( fds, nSockets, nMillis ); /* -1: infinite timeout */
        logf( XW_LOGINFO, "back from poll: %d", nEvents );
        if ( nEvents == 0 ) {
            tmgr->fireElapsedTimers();
        } else if ( nEvents < 0 ) {
            logf( XW_LOGERROR, "errno: %d", errno );
        } 

        if ( fds[0].revents != 0 ) {
            logf( XW_LOGINFO, "poll interrupted" );
            assert( fds[0].revents == POLLIN );
            unsigned char byt;
            read( fds[0].fd, &byt, 1 );
            --nEvents;
        }

        if ( nEvents > 0 ) {
            --nSockets;
            curfd = &fds[1];

            int i;
            for ( i = 0; i < nSockets && nEvents > 0; ++i ) {

                if ( curfd->revents != 0 ) {
                    int socket = curfd->fd;
                    if ( !RemoveSocket( socket ) ) {
                        /* no further processing if it's been removed while
                           we've been sleeping in poll */
                        --nEvents;
                        continue;
                    }

                    if ( curfd->revents == POLLIN 
                         ||  curfd->revents == POLLPRI ) {
                        logf( XW_LOGINFO, "enqueuing %d", socket );
                        enqueue( socket );
                    } else {
                        logf( XW_LOGERROR, "odd revents: %d", curfd->revents );
                        killSocket( socket, "error/hup in poll()" ); 
                    }
                    --nEvents;
                }
                ++curfd;
            }
            assert( nEvents == 0 );
        }

        free( fds );
        free( log );
    }
    return NULL;
} /* real_listener */

/* static */ void*
XWThreadPool::listener_main( void* closure )
{
    XWThreadPool* me = (XWThreadPool*)closure;
    return me->real_listener();
}

void
XWThreadPool::enqueue( int socket ) 
{
    MutexLock ml( &m_queueMutex );
    m_queue.push_back( socket );

    logf( XW_LOGINFO, "calling pthread_cond_signal" );
    pthread_cond_signal( &m_queueCondVar );
    /* implicit unlock */
}
