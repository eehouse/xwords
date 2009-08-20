/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */

/* 
 * Copyright 2005-2009 by Eric House (xwords@eehouse.org).  All rights
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
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/poll.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

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
    : m_timeToDie(false)
    , m_nThreads(0)
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
    logf( XW_LOGINFO, "pipes: m_pipeRead: %d; m_pipeWrite: %d",
          m_pipeRead, m_pipeWrite );
}

XWThreadPool::~XWThreadPool()
{
    pthread_cond_destroy( &m_queueCondVar );

    pthread_rwlock_destroy( &m_activeSocketsRWLock );
    pthread_mutex_destroy ( &m_queueMutex );
} /* ~XWThreadPool */

void
XWThreadPool::Setup( int nThreads, packet_func pFunc )
{
    m_nThreads = nThreads;
    m_pFunc = pFunc;

    pthread_t thread;

    int ii;
    for ( ii = 0; ii < nThreads; ++ii ) {
        int result = pthread_create( &thread, NULL, tpool_main, this );
        assert( result == 0 );
        pthread_detach( thread );
    }

    int result = pthread_create( &thread, NULL, listener_main, this );
    assert( result == 0 );
}

void
XWThreadPool::Stop()
{
    m_timeToDie = true;

    int ii;
    for ( ii = 0; ii < m_nThreads; ++ii ) {
        enqueue( 0 );
    }

    interrupt_poll();
}

void
XWThreadPool::AddSocket( int socket )
{
    logf( XW_LOGINFO, "%s(%d)", __func__, socket );
    {
        RWWriteLock ml( &m_activeSocketsRWLock );
        m_activeSockets.push_back( socket );
    }
    interrupt_poll();
}

bool
XWThreadPool::RemoveSocket( int socket )
{
    bool found = false;
    {
        RWWriteLock ml( &m_activeSocketsRWLock );

        vector<int>::iterator iter = m_activeSockets.begin();
        while ( iter != m_activeSockets.end() ) {
            if ( *iter == socket ) {
                m_activeSockets.erase( iter );
                found = true;
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
/*     bool do_interrupt = false; */
    if ( !RemoveSocket( socket ) ) {
        RWWriteLock rwl( &m_activeSocketsRWLock );
        deque<int>::iterator iter = m_queue.begin();
        while ( iter != m_queue.end() ) {
            if ( *iter == socket ) {
                m_queue.erase( iter );
/*                 do_interrupt = true; */
                break;
            }
            ++iter;
        }
    }
    logf( XW_LOGINFO, "CLOSING socket %d", socket );
    close( socket );
/*     if ( do_interrupt ) { */
    /* We always need to interrupt the poll because the socket we're closing
       will be in the list being listened to.  That or we need to drop sockets
       that have been removed on some other thread while the poll call's
       blocking.*/
    interrupt_poll();
/*     } */
}

bool
XWThreadPool::get_process_packet( int socket )
{
    bool success = false;
    short packetSize;
    assert( sizeof(packetSize) == 2 );

    ssize_t nRead = recv( socket, &packetSize, 
                          sizeof(packetSize), MSG_WAITALL );
    if ( nRead != 2 ) {
        killSocket( socket, "nRead != 2" );
    } else {
        packetSize = ntohs( packetSize );

        if ( packetSize < 0 || packetSize > MAX_MSG_LEN ) {
            killSocket( socket, "packetSize wrong" );
        } else {
            unsigned char buf[MAX_MSG_LEN];
            nRead = recv( socket, buf, packetSize, MSG_WAITALL );

            if ( nRead != packetSize ) {
                killSocket( socket, "nRead != packetSize" ); 
            } else {
                logf( XW_LOGINFO, "read %d bytes", nRead );

                logf( XW_LOGINFO, "calling m_pFunc" );
                success = (*m_pFunc)( buf, packetSize, socket );
            }
        }
    }
    return success;
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
    logf( XW_LOGINFO, "tpool worker thread starting" );
    for ( ; ; ) {

        pthread_mutex_lock( &m_queueMutex );
        while ( !m_timeToDie && m_queue.size() == 0 ) {
            pthread_cond_wait( &m_queueCondVar, &m_queueMutex );
        }

        if ( m_timeToDie ) {
            break;
        }

        int socket = m_queue.front();
        m_queue.pop_front();
        pthread_mutex_unlock( &m_queueMutex );
        logf( XW_LOGINFO, "worker thread got socket %d from queue", socket );

        if ( get_process_packet( socket ) ) {
            AddSocket( socket );
        } /* else drop it: error */
    }
    logf( XW_LOGINFO, "tpool worker thread exiting" );
    return NULL;
}

void
XWThreadPool::interrupt_poll()
{
    logf( XW_LOGINFO, __func__ );
    unsigned char byt = 0;
    int nSent = write( m_pipeWrite, &byt, 1 );
    if ( nSent != 1 ) {
        logf( XW_LOGERROR, "errno = %s (%d)", strerror(errno), errno );
    }
}

void*
XWThreadPool::real_listener()
{
    int flags = POLLIN | POLLERR | POLLHUP;
    TimerMgr* tmgr = TimerMgr::GetTimerMgr();
    int nSocketsAllocd = 1;

    struct pollfd* fds = (pollfd*)calloc( nSocketsAllocd, sizeof(fds[0]) );
#ifdef LOG_POLL
    char* log = (char*)malloc( 4 * nSocketsAllocd );
#endif

    for ( ; ; ) {

        pthread_rwlock_rdlock( &m_activeSocketsRWLock );
        int nSockets = m_activeSockets.size() + 1; /* for pipe */
#ifdef LOG_POLL
        int logCapacity = 4 * nSockets;
        int logLen = 0;
#endif

        if ( nSockets > nSocketsAllocd ) {
            fds = (struct pollfd*)realloc( fds, nSockets * sizeof(fds[0]) );
#ifdef LOG_POLL
            log = (char*)realloc( log, nSockets * 4 );
#endif
            nSocketsAllocd = nSockets;
        }
        struct pollfd* curfd = fds;
        
        curfd->fd = m_pipeRead;
        curfd->events = flags;
#ifdef LOG_POLL
        logLen += snprintf( log+logLen, logCapacity - logLen, "%d,", curfd->fd );
#endif
        ++curfd;

        vector<int>::iterator iter = m_activeSockets.begin();
        while ( iter != m_activeSockets.end()  ) {
            curfd->fd = *iter++;
            curfd->events = flags;
#ifdef LOG_POLL
            if ( logCapacity > logLen ) {
                logLen += snprintf( log+logLen, logCapacity - logLen, "%d,", 
                                    curfd->fd );
            }
#endif
            assert( curfd < fds + nSockets );
            ++curfd;
        }
        pthread_rwlock_unlock( &m_activeSocketsRWLock );

        int nMillis = tmgr->GetPollTimeout();

#ifdef LOG_POLL
        logf( XW_LOGINFO, "polling %s nmillis=%d", log, nMillis );
#endif
        int nEvents = poll( fds, nSockets, nMillis );
        logf( XW_LOGINFO, "back from poll: %d", nEvents );
        if ( m_timeToDie ) {
            break;
        }

        if ( nEvents == 0 ) {
            tmgr->FireElapsedTimers();
        } else if ( nEvents < 0 ) {
            logf( XW_LOGERROR, "poll failed: errno: %s (%d)", 
                  strerror(errno), errno );
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

            int ii;
            for ( ii = 0; ii < nSockets && nEvents > 0; ++ii ) {

                if ( curfd->revents != 0 ) {
                    int socket = curfd->fd;
                    if ( !RemoveSocket( socket ) ) {
                        /* no further processing if it's been removed while
                           we've been sleeping in poll */
                        --nEvents;
                        continue;
                    }

                    if ( 0 != (curfd->revents & (POLLIN | POLLPRI)) ) {
                        logf( XW_LOGINFO, "enqueuing %d", socket );
                        enqueue( socket );
                    } else {
                        logf( XW_LOGERROR, "odd revents: %x", curfd->revents );
                        killSocket( socket, "error/hup in poll()" ); 
                    }
                    --nEvents;
                }
                ++curfd;
            }
            assert( nEvents == 0 );
        }
    }

    logf( XW_LOGINFO, "real_listener returning" );
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
