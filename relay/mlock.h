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

#ifndef _MLOCK_H_
#define _MLOCK_H_

#include <pthread.h>

#include "xwrelay_priv.h"
#include "cref.h"

class MutexLock {
 public:
    MutexLock( pthread_mutex_t* mutex ) { 
        m_mutex = mutex;
#ifdef DEBUG_LOCKS
        logf( XW_LOGINFO, "tlm %p", mutex );
#endif
        pthread_mutex_lock( mutex );
#ifdef DEBUG_LOCKS
        logf( XW_LOGINFO, "slm %p", mutex );
#endif
    }
    ~MutexLock() { 
#ifdef DEBUG_LOCKS
        logf( XW_LOGINFO, "ULM %p", m_mutex );
#endif
        pthread_mutex_unlock( m_mutex );
    }

 private:
    pthread_mutex_t* m_mutex;
};

class SocketWriteLock {
 public:
    SocketWriteLock( int socket, pthread_mutex_t* mutex )
        : m_socket( socket )
        , m_mutex( mutex )
        {
#ifdef DEBUG_LOCKS
        logf( XW_LOGINFO, "tlm %p for socket %d", m_mutex, socket );
#endif
        if ( m_mutex != NULL ) {
            pthread_mutex_lock( m_mutex );
        }
#ifdef DEBUG_LOCKS
        logf( XW_LOGINFO, "slm %p for socket %d", m_mutex, socket );
#endif
        }

    ~SocketWriteLock() {
#ifdef DEBUG_LOCKS
        logf( XW_LOGINFO, "ULM %p for socket %d", m_mutex, m_socket );
#endif
        if  ( m_mutex != NULL ) {
            pthread_mutex_unlock( m_mutex );
        }
    }

    bool socketFound() { return (m_mutex != NULL); }

 private:
    int m_socket;
    pthread_mutex_t* m_mutex;
};

class RWReadLock {
 public:
    RWReadLock( pthread_rwlock_t* rwl ) {
#ifdef DEBUG_LOCKS
        logf( XW_LOGINFO, "tlrr %p", rwl );
#endif
        pthread_rwlock_rdlock( rwl );
#ifdef DEBUG_LOCKS
        logf( XW_LOGINFO, "slrr %p", rwl );
#endif
        _rwl = rwl;
    }
    ~RWReadLock() {
        pthread_rwlock_unlock( _rwl );
#ifdef DEBUG_LOCKS
        logf( XW_LOGINFO, "ULRR %p", _rwl );
#endif
    }

 private:
    pthread_rwlock_t* _rwl;
};

class RWWriteLock {
 public:
    RWWriteLock( pthread_rwlock_t* rwl ) : _rwl(rwl) {
#ifdef DEBUG_LOCKS
        logf( XW_LOGINFO, "tlww %p", rwl );
#endif
        pthread_rwlock_wrlock( rwl );
#ifdef DEBUG_LOCKS
        logf( XW_LOGINFO, "slww %p", rwl );
#endif
    }
    ~RWWriteLock() { 
        pthread_rwlock_unlock( _rwl );
#ifdef DEBUG_LOCKS
        logf( XW_LOGINFO, "ULWW %p", _rwl );
#endif
    }
 private:
    pthread_rwlock_t* _rwl;
};

#endif
