/* -*- compile-command: "make -j3"; -*- */

/* 
 * Copyright 2005 - 2012 by Eric House (xwords@eehouse.org).  All rights
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
#include "strwpf.h"

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
    free( m_threadInfos );
} /* ~XWThreadPool */

void
XWThreadPool::Setup( int nThreads, kill_func kFunc )
{
    m_nThreads = nThreads;
    m_threadInfos = (ThreadInfo*)malloc( nThreads * sizeof(*m_threadInfos) );
    m_kFunc = kFunc;

    for ( int ii = 0; ii < nThreads; ++ii ) {
        ThreadInfo* tip = &m_threadInfos[ii];
        tip->me = this;
        int result = pthread_create( &tip->thread, NULL, tpool_main, tip );
        assert( result == 0 );
        pthread_detach( tip->thread );
    }

    pthread_t thread;
    int result = pthread_create( &thread, NULL, listener_main, this );
    assert( result == 0 );
    result = pthread_detach( thread );
    assert( result == 0 );
}

void
XWThreadPool::Stop()
{
    m_timeToDie = true;

    int ii;
    for ( ii = 0; ii < m_nThreads; ++ii ) {
        SockInfo si;
        si.m_type = STYPE_UNKNOWN;
        enqueue( si );
    }

    interrupt_poll();
}

void
XWThreadPool::AddSocket( SockType stype, QueueCallback proc, const AddrInfo* from )
{
    from->ref();

    int sock = from->getSocket();
    logf( XW_LOGVERBOSE0, "%s(sock=%d, isTCP=%d)", __func__, sock, from->isTCP() );
    SockInfo si = { .m_type = stype,
                    .m_proc = proc,
                    .m_addr = *from
    };
    {
        RWWriteLock ml( &m_activeSocketsRWLock );
        assert( m_activeSockets.find( sock ) == m_activeSockets.end() );
        m_activeSockets.insert( pair<int, SockInfo>( sock, si ) );
    }
    interrupt_poll();
}

bool
XWThreadPool::SocketFound( const AddrInfo* addr )
{
    assert( addr->isTCP() );
    bool found = false;
    {
        RWWriteLock ml( &m_activeSocketsRWLock );

        map<int, SockInfo>::iterator iter = m_activeSockets.find( addr->getSocket() );
        if ( m_activeSockets.end() != iter 
             && iter->second.m_addr.equals( *addr ) ) {
            found = true;
        }
    }
    return found;
}

bool
XWThreadPool::RemoveSocket( const AddrInfo* addr )
{
    assert( addr->isTCP() );
    bool found = false;
    {
        RWWriteLock ml( &m_activeSocketsRWLock );

        size_t prevSize = m_activeSockets.size();

        int sock = addr->getSocket();
        map<int, SockInfo>::iterator iter = m_activeSockets.find( sock );
        if ( m_activeSockets.end() != iter && iter->second.m_addr.equals( *addr ) ) {
            m_activeSockets.erase( iter );
            found = true;
        }
        logf( XW_LOGINFO, "%s(): AFTER closing %d: %d sockets active (was %d)", __func__,
              sock, m_activeSockets.size(), prevSize );
    }
    return found;
} /* RemoveSocket */

void
XWThreadPool::CloseSocket( const AddrInfo* addr )
{
    if ( addr->isTCP() ) {
        if ( !RemoveSocket( addr ) ) {
            MutexLock ml( &m_queueMutex );
            deque<QueuePr>::iterator iter = m_queue.begin();
            while ( iter != m_queue.end() ) {
                if ( iter->m_info.m_addr.equals( *addr ) ) {
                    m_queue.erase( iter );
                    break;
                }
                ++iter;
            }
        }
        int sock = addr->getSocket();
        int err = close( sock );
        if ( 0 != err ) {
            logf( XW_LOGERROR, "%s(): close(socket=%d) => %d/%s", __func__,
                  sock, errno, strerror(errno) );
        } else {
            logf( XW_LOGINFO, "%s(): close(socket=%d) succeeded", __func__, sock );
        }

        /* We always need to interrupt the poll because the socket we're closing
           will be in the list being listened to.  That or we need to drop sockets
           that have been removed on some other thread while the poll call's
           blocking.*/
        interrupt_poll();
    }
}

void
XWThreadPool::EnqueueKill( const AddrInfo* addr, const char* const why )
{
    logf( XW_LOGINFO, "%s(socket = %d) reason: %s", __func__, addr->getSocket(), why );
    if ( addr->isTCP() ) {
        SockInfo si;
        si.m_type = STYPE_UNKNOWN;
        si.m_addr = *addr;
        enqueue( si, Q_KILL );
    }
}

// return true if the addr passed in has a timestamp >= what we have as the
// creation time of the now-open socket.  If the socket isn't open, return false.
bool
XWThreadPool::IsCurrent( const AddrInfo* addr )
{
    bool result = false;
    int sock = addr->getSocket();
    if ( -1 != sock ) {
        RWReadLock ml( &m_activeSocketsRWLock );
        map<int, SockInfo>::const_iterator iter = m_activeSockets.find( sock ); 
        if ( iter != m_activeSockets.end() ) {
            result = iter->second.m_addr.created() <= addr->created();
            logf( XW_LOGINFO, "%s(sock=%d)=>%d (%lx vs %lx)",
                  __func__, sock, result,
                  iter->second.m_addr.created(), addr->created() );
        }
    }
    return result;
}

void*
XWThreadPool::tpool_main( void* closure )
{
    blockSignals();

    ThreadInfo* tip = (ThreadInfo*)closure;
    return tip->me->real_tpool_main( tip );
}

void*
XWThreadPool::real_tpool_main( ThreadInfo* tip )
{
    logf( XW_LOGINFO, "tpool worker thread starting" );
    int sock = -1;
    for ( ; ; ) {
        pthread_mutex_lock( &m_queueMutex );
        tip->recentTime = 0;

        release_socket_locked( sock );

        while ( !m_timeToDie && m_queue.size() == 0 ) {
            pthread_cond_wait( &m_queueCondVar, &m_queueMutex );
        }

        if ( m_timeToDie ) {
            logf( XW_LOGINFO, "%s: unlocking b/c m_timeToDie set", __func__ );
            pthread_mutex_unlock( &m_queueMutex );
            break;
        }

        QueuePr pr;
        bool gotOne = grab_elem_locked( &pr );

        tip->recentTime = time( NULL );
        pthread_mutex_unlock( &m_queueMutex );

        if ( gotOne ) {
            sock = pr.m_info.m_addr.getSocket();
            switch ( pr.m_act ) {
            case Q_READ:
                assert( 0 );
                // assert( socket >= 0 );
                // if ( get_process_packet( pr.m_info.m_type, pr.m_info.m_proc, &pr.m_info.m_addr ) ) {
                //     AddSocket( pr.m_info.m_type, pr.m_info.m_proc, &pr.m_info.m_addr );
                // }
                break;
            case Q_KILL:
                logf( XW_LOGINFO, "worker thread got socket %d from queue (to close it)", sock );
                (*m_kFunc)( &pr.m_info.m_addr );
                pr.m_info.m_addr.unref();
                break;
            }
        } else {
            sock = -1;
        }
    }
    logf( XW_LOGINFO, "tpool worker thread exiting" );
    return NULL;
}

void
XWThreadPool::interrupt_poll()
{
#ifdef LOG_POLL
    logf( XW_LOGINFO, __func__ );
#endif
    uint8_t byt = 0;
    int nSent = write( m_pipeWrite, &byt, 1 );
    if ( nSent != 1 ) {
        logf( XW_LOGERROR, "errno = %s (%d)", strerror(errno), errno );
    }
}

void*
XWThreadPool::real_listener()
{
    int flags = POLLIN | POLLERR | POLLHUP | POLLRDHUP;
    TimerMgr* tmgr = TimerMgr::GetTimerMgr();
    int nSocketsAllocd = 1;

    struct pollfd* fds = (pollfd*)calloc( nSocketsAllocd, sizeof(fds[0]) );
    SockInfo* sinfos = (SockInfo*)calloc( nSocketsAllocd, sizeof(sinfos[0]) );
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
            sinfos = (SockInfo*)realloc( sinfos, nSockets * sizeof(sinfos[0]) );
#ifdef LOG_POLL
            log = (char*)realloc( log, nSockets * 4 );
#endif
            nSocketsAllocd = nSockets;
        }
        int curfd = 0;
        
        fds[curfd].fd = m_pipeRead;
        fds[curfd].events = flags;
#ifdef LOG_POLL
        logLen += snprintf( log+logLen, logCapacity - logLen, "%d,", 
                            fds[curfd].fd );
#endif
        ++curfd;

        map<int, SockInfo>::iterator iter;
        for ( iter = m_activeSockets.begin(); iter != m_activeSockets.end(); 
              ++iter ) {
            fds[curfd].fd = iter->first;
            sinfos[curfd] = iter->second;
            fds[curfd].events = flags;
#ifdef LOG_POLL
            if ( logCapacity > logLen ) {
                logLen += snprintf( log+logLen, logCapacity - logLen, "%d,", 
                                    fds[curfd].fd );
            }
#endif
            assert( curfd < nSockets );
            ++curfd;
        }
        pthread_rwlock_unlock( &m_activeSocketsRWLock );

        int nMillis = tmgr->GetPollTimeoutMillis();

#ifdef LOG_POLL
        logf( XW_LOGINFO, "polling %s nmillis=%d", log, nMillis );
#endif
        int nEvents = poll( fds, nSockets, nMillis );
#ifdef LOG_POLL
        logf( XW_LOGINFO, "back from poll: %d", nEvents );
#endif
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
#ifdef LOG_POLL
            logf( XW_LOGINFO, "poll interrupted" );
#endif
            assert( fds[0].revents == POLLIN );
            uint8_t byt;
            read( fds[0].fd, &byt, 1 );
            --nEvents;
        }

        if ( nEvents > 0 ) {
            --nSockets;
            curfd = 1;

            int ii;
            for ( ii = 0; ii < nSockets && nEvents > 0; ++ii, ++curfd ) {

                if ( fds[curfd].revents != 0 ) {
                    // int socket = fds[curfd].fd;
                    SockInfo* sinfo = &sinfos[curfd];
                    const AddrInfo* addr = &sinfo->m_addr;

                    int sock = addr->getSocket();
                    assert( fds[curfd].fd == sock );
                    if ( !SocketFound( addr ) ) {
                        logf( XW_LOGINFO, "%s(): dropping socket %d: not found",
                              __func__, addr->getSocket() );
                        /* no further processing if it's been removed while
                           we've been sleeping in poll. BUT: shouldn't curfd
                           be incremented?? */
                        --nEvents;
                        continue;
                    }

                    if ( 0 != (fds[curfd].revents & (POLLIN | POLLPRI)) ) {
                        if ( !UdpQueue::get()->handle( addr, sinfo->m_proc ) ) {
                            // This is likely wrong!!! return of 0 means
                            // remote closed, not error.
                            RemoveSocket( addr );
                            EnqueueKill( addr, "got EOF" );
                        }
                    } else {
                        logf( XW_LOGERROR, "%s(): odd revents: %x; bad socket %d",
                              __func__, fds[curfd].revents, sock );
                        RemoveSocket( addr );
                        EnqueueKill( addr, "error/hup in poll()" );
                    }
                    --nEvents;
                }
            }
            assert( nEvents == 0 );
        }
    }

    free( fds );
    free( sinfos );
#ifdef LOG_POLL
    free( log );
#endif

    logf( XW_LOGINFO, "real_listener returning" );
    return NULL;
} /* real_listener */

/* static */ void*
XWThreadPool::listener_main( void* closure )
{
    blockSignals();    

    XWThreadPool* me = (XWThreadPool*)closure;
    return me->real_listener();
}

void
XWThreadPool::enqueue( SockInfo si, QAction act ) 
{
    QueuePr pr = { act, si };
    MutexLock ml( &m_queueMutex );
    m_queue.push_back( pr );

    pthread_cond_signal( &m_queueCondVar );
    log_hung_threads();
}

bool
XWThreadPool::grab_elem_locked( QueuePr* prp )
{
    bool found = false;
    deque<QueuePr>::iterator iter;
    for ( iter = m_queue.begin(); !found && iter != m_queue.end(); ++iter ) {
        int sock = iter->m_info.m_addr.getSocket();
        /* If NOT found */
        if ( -1 != sock
             && m_sockets_in_use.end() == m_sockets_in_use.find( sock ) ) {
            *prp = *iter;
            m_queue.erase( iter ); /* this was a double-free once! */
            m_sockets_in_use.insert( sock );
            found = true;
        }
    }

    print_in_use();
    return found;
} /* grab_elem_locked */

void
XWThreadPool::release_socket_locked( int sock )
{
    if ( -1 != sock ) {
        set<int>::iterator iter = m_sockets_in_use.find( sock );
        assert( iter != m_sockets_in_use.end() );
        m_sockets_in_use.erase( iter );
    }
    print_in_use();
}

void
XWThreadPool::print_in_use( void )
{
    StrWPF str;
    set<int>::iterator iter;

    for ( iter = m_sockets_in_use.begin(); 
          iter != m_sockets_in_use.end(); ++iter ) {
        str.catf( "%d ", *iter );
    }
    if ( 0 < str.size() ) {
        logf( XW_LOGINFO, "Sockets in use: %s", str.c_str() );
    }
}

// We have the mutex when this is called
void
XWThreadPool::log_hung_threads( void )
{
    const time_t HUNG_THREASHHOLD = 300; // seconds
    int ii;
    time_t now = time( NULL );
    for ( ii = 0; ii < m_nThreads; ++ii ) {
        ThreadInfo* tip = &m_threadInfos[ii];
        time_t recentTime = tip->recentTime;
        if ( 0 != recentTime ) {
            time_t howLong = now - recentTime;
            if ( HUNG_THREASHHOLD < howLong ) {
                logf( XW_LOGERROR, "thread %d (%p) stopped for %d seconds!",
                      ii, tip->thread, howLong );
                tip->recentTime = 0;   // only log once
                assert(0);
            }
        }
    }
}
