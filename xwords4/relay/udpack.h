/* -*- compile-command: "make -j3"; -*- */
/* 
 * Copyright 2013 - 2018 by Eric House (xwords@eehouse.org).  All rights
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

#ifndef _UDPACK_H_
#define _UDPACK_H_

#include <stdio.h>

#include "xwrelay_priv.h"
#include "xwrelay.h"
#include "strwpf.h"

typedef void (*OnAckProc)( bool acked, uint32_t packetID, void* data );

class AckRecord {
 public: 
    AckRecord( XWRelayReg cmd, uint32_t id ) {
        m_createTime = time( NULL );
        m_proc = NULL;
        m_cmd = cmd;
        m_id = id;
    }

    string toStr()
    {
        char buf[64];
        sprintf( buf, "%d/%s", m_id, msgToStr( m_cmd ) );
        string str(buf);
        return str;
    }
    time_t m_createTime;
    OnAckProc m_proc;
    XWRelayReg m_cmd;
    void* m_data;
 private:
    uint32_t m_id;
};

class UDPAckTrack {
 public:
    static const uint32_t PACKETID_NONE = 0;
    
    static uint32_t nextPacketID( XWRelayReg cmd );
    static string recordAck( uint32_t packetID );
    static bool setOnAck( OnAckProc proc, uint32_t packetID, void* data );
    static bool shouldAck( XWRelayReg cmd );
    /* called from ctrl port */
    static void printAcks( StrWPF& out );
    static void doNack( vector<uint32_t> ids );

 private:
    static UDPAckTrack* get();
    static void* thread_main( void* arg );
    UDPAckTrack();
    time_t ackLimit();
    uint32_t nextPacketIDImpl( XWRelayReg cmd );
    string recordAckImpl( uint32_t packetID );
    bool setOnAckImpl( OnAckProc proc, uint32_t packetID, void* data );
    void callProc( const map<uint32_t, AckRecord>::iterator iter, bool acked );
    void printAcksImpl( StrWPF& out );
    void doNackImpl( vector<uint32_t>& ids );
    void* threadProc();

    static UDPAckTrack* s_self;
    uint32_t m_nextID;
    pthread_mutex_t m_mutex;
    map<uint32_t, AckRecord> m_pendings;
};

#endif
