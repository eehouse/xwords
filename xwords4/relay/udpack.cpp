/* -*- compile-command: "make MEMDEBUG=TRUE -j3"; -*- */
/* 
 * Copyright 2013 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#include "udpack.h"
#include "mlock.h" 

UDPAckTrack* UDPAckTrack::s_self = NULL;


/* static*/ bool
UDPAckTrack::shouldAck( XWRelayReg cmd )
{
    return ( XWPDEV_ACK != cmd && XWPDEV_ALERT != cmd );
}

/* static*/ uint32_t
UDPAckTrack::nextPacketID( XWRelayReg cmd )
{
    uint32_t result = 0;
    if ( shouldAck( cmd ) ) {
        result = get()->nextPacketIDImpl();
    }
    return result;
}

/* static*/ void
UDPAckTrack::recordAck( uint32_t packetID )
{
    get()->recordAckImpl( packetID );
}

/* static */ UDPAckTrack*
UDPAckTrack::get()
{
    if ( NULL == s_self ) {
        s_self = new UDPAckTrack();
    }
    return s_self;
}

UDPAckTrack::UDPAckTrack()
{
    m_nextID = 0;
    pthread_mutex_init( &m_mutex, NULL );

    pthread_t thread;
    pthread_create( &thread, NULL, thread_main, (void*)this );
    pthread_detach( thread );
}

uint32_t
UDPAckTrack::nextPacketIDImpl()
{
    MutexLock ml( &m_mutex );
    AckRecord record;
    uint32_t result = ++m_nextID;
    m_pendings.insert( pair<uint32_t,AckRecord>(result, record) );
    return result;
}

void
UDPAckTrack::recordAckImpl( uint32_t packetID )
{
    map<uint32_t, AckRecord>::iterator iter;
    MutexLock ml( &m_mutex );
    iter = m_pendings.find( packetID );
    if ( m_pendings.end() == iter ) {
        logf( XW_LOGERROR, "%s: packet ID %d not found", __func__, packetID );
    } else {
        time_t took = time( NULL ) - iter->second.m_createTime;
        if ( 5 < took  ) {
            logf( XW_LOGERROR, "%s: packet ID %d took %d seconds to get acked", __func__, packetID );
        }
        m_pendings.erase( iter );
    }
}

void*
UDPAckTrack::threadProc()
{
    for ( ; ; ) {
        sleep( 30 );
        time_t now = time( NULL );
        vector<uint32_t> older;
        {
            MutexLock ml( &m_mutex );
            map<uint32_t, AckRecord>::iterator iter;
            for ( iter = m_pendings.begin(); iter != m_pendings.end(); ++iter ) {
                time_t took = now - iter->second.m_createTime;
                if ( 60 < took ) {
                    older.push_back( iter->first );
                    m_pendings.erase( iter );
                }
            }
        }
        if ( 0 < older.size() ) {
            string leaked;
            vector<uint32_t>::const_iterator iter = older.begin();
            for ( ; ; ) {
                string_printf( leaked, "%d", *iter );
                if ( ++iter == older.end() ) {
                    break;
                }
                string_printf( leaked, ", " );
            }
            logf( XW_LOGERROR, "%s: these packets leaked: %s", __func__, 
                  leaked.c_str() );
        } else {
            logf( XW_LOGINFO, "%s: no packets leaked", __func__ );
        }
    }
    return NULL;
}

/* static */ void*
UDPAckTrack::thread_main( void* arg )
{
    UDPAckTrack* self = (UDPAckTrack*)arg;
    return self->threadProc();
}
