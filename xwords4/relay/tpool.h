/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */

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

/* Runs a single thread polling for activity on any of the sockets in its
 * list.  When there is activity, removes the socket from that list and adds
 * it ot a queue of socket ready for reading.  Starts up a bunch of threads
 * waiting on that queue.  When a new socket appears, a thread grabs the
 * socket, reads from it, passes the buffer on, and puts the socket back in
 * the list being read from in our main thread.
 */

#include <vector>
#include <deque>
#include <set>

#include "addrinfo.h" 
#include "udpqueue.h"

using namespace std;

class XWThreadPool {

 public:
    typedef enum { STYPE_UNKNOWN, STYPE_GAME, STYPE_PROXY } SockType;
    typedef struct _SockInfo {
        SockType m_type;
        QueueCallback m_proc;
        AddrInfo m_addr;
    } SockInfo;

    typedef struct _ThreadInfo {
        XWThreadPool* me;
        pthread_t thread;
        time_t recentTime;
    } ThreadInfo;

    static XWThreadPool* GetTPool();
    typedef void (*kill_func)( const AddrInfo* addr );

    XWThreadPool();
    ~XWThreadPool();

    void Setup( int nThreads, kill_func kFunc );
    void Stop();

    /* Add to set being listened on */
    void AddSocket( SockType stype, QueueCallback proc, const AddrInfo* from );
    /* remove from tpool altogether, and close */
    void CloseSocket( const AddrInfo* addr );

    void EnqueueKill( const AddrInfo* addr, const char* const why );

 private:
    typedef enum { Q_READ, Q_KILL } QAction;
    typedef struct { QAction m_act; SockInfo m_info; } QueuePr;

    /* Remove from set being listened on */
    bool RemoveSocket( const AddrInfo* addr );

    void enqueue( QAction act = Q_READ );
    void enqueue( SockInfo si, QAction act = Q_READ );
    void release_socket_locked( int socket );
    bool grab_elem_locked( QueuePr* qpp );
    void print_in_use( void );
    void log_hung_threads( void );

    bool get_process_packet( SockType stype, QueueCallback proc, const AddrInfo* from );
    void interrupt_poll();

    void* real_tpool_main( ThreadInfo* tsp );
    static void* tpool_main( void* closure );

    void* real_listener();
    static void* listener_main( void* closure );

    /* Sockets main thread listens on */
    vector<SockInfo>m_activeSockets;
    pthread_rwlock_t m_activeSocketsRWLock;

    /* Sockets waiting for a thread to read 'em */
    deque<QueuePr> m_queue;
    set<int> m_sockets_in_use;
    pthread_mutex_t m_queueMutex;
    pthread_cond_t m_queueCondVar;

    /* for self-write pipe hack */
    int m_pipeRead;
    int m_pipeWrite;

    bool m_timeToDie;
    int m_nThreads;
    kill_func m_kFunc;
    ThreadInfo* m_threadInfos;

    static XWThreadPool* g_instance;
};
