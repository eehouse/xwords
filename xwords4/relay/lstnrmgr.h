/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */

/* 
 * Copyright 2007 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#ifndef _LSTNRMGR_H_
#define _LSTNRMGR_H_

#include <string>
#include <vector>
#include <map>
#include <sys/select.h>

#include "xwrelay_priv.h"

using namespace std;

class ListenerMgr {
 public:
    void RemoveAll();
/*     void RemoveListener( int listener ); */
    bool AddListener( int port );
    void SetAll( const vector<int>* iv ); /* replace current set with this new one */
    void AddToFDSet( fd_set* rfds );
    int GetHighest();
    bool PortInUse( int port );

 private:
    void removeSocket( int sock );
    void removePort( int port );
    bool addOne( int listener );
    bool portInUse( int port );

    map<int,int> m_socks_to_ports;
    pthread_mutex_t m_mutex;
    friend class ListenersIter;
};

class ListenersIter {
 public:
    ListenersIter(ListenerMgr* lm, bool fds) {
        m_fds = fds;
        m_lm = lm;
        pthread_mutex_lock( &m_lm->m_mutex );
        m_iter = lm->m_socks_to_ports.begin();
    }

    ~ListenersIter() {
        pthread_mutex_unlock( &m_lm->m_mutex );
    }

    int next();

 private:
    bool m_fds;
    map<int,int>::const_iterator m_iter;
    ListenerMgr* m_lm;
};

#endif
