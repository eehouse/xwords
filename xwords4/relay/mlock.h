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

#ifndef _MLOCK_H_
#define _MLOCK_H_

#include <pthread.h>

#include "xwrelay_priv.h"
#include "cref.h"

class MutexLock {
 public:
    MutexLock( pthread_mutex_t* mutex ) { 
        m_mutex = mutex;
        logf( "locking mutex %x", mutex );
        pthread_mutex_lock( mutex );
        logf( "successfully locked mutex %x", mutex );
    }
    ~MutexLock() { 
        logf( "UNlocking mutex %x", m_mutex );
        pthread_mutex_unlock( m_mutex );
    }

 private:
    pthread_mutex_t* m_mutex;
};

class SocketWriteLock {
 public:
    SocketWriteLock( int socket ) {
        m_socket = socket;
        m_mutex = GetWriteMutexForSocket( socket );
        logf( "locking mutex %x for socket %d", m_mutex, socket );
        pthread_mutex_lock( m_mutex );
        logf( "successfully locked mutex %x for socket %d", m_mutex, socket );
    }
    ~SocketWriteLock() {
        logf( "UNlocking mutex %x for socket %d", m_mutex, m_socket );
        pthread_mutex_unlock( m_mutex );
    }

 private:
    pthread_mutex_t* m_mutex;
    int m_socket;
};

class RWReadLock {
 public:
    RWReadLock( pthread_rwlock_t* rwl ) {
        logf( "locking rwlock for read" );
        pthread_rwlock_rdlock( rwl );
        logf( "locked rwlock for read" );
        _rwl = rwl;
    }
    ~RWReadLock() {
        pthread_rwlock_unlock( _rwl );
        logf( "unlocked rwlock" );
    }

 private:
    pthread_rwlock_t* _rwl;
};

class RWWriteLock {
 public:
    RWWriteLock( pthread_rwlock_t* rwl ) : _rwl(rwl) {
        logf( "locking rwlock for write" );
        pthread_rwlock_wrlock( rwl );
        logf( "locked rwlock for write" );
    }
    ~RWWriteLock() { 
        pthread_rwlock_unlock( _rwl );         
        logf( "unlocked rwlock" ); 
    }
 private:
    pthread_rwlock_t* _rwl;
};

#endif
