/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */

/* 
 * Copyright 2005 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "configs.h"

#define MAX_LINE 128

#ifndef _HEARTBEAT
# define _HEARTBEAT 60
#endif

#ifndef _ALLCONN
# define _ALLCONN 120
#endif

RelayConfigs* RelayConfigs::instance = NULL;


/* static */ RelayConfigs* 
RelayConfigs::GetConfigs()
{
    return instance;
}

/* static */ void
RelayConfigs::InitConfigs( const char* cfile )
{
    assert( instance == NULL );
    instance = new RelayConfigs( cfile );
}

RelayConfigs::RelayConfigs( const char* cfile )
    : m_allConnInterval(_ALLCONN)
    , m_heartbeatInterval(_HEARTBEAT)
{
    /* There's an order here.  Open multiple files, if present.  File in /etc
       is first, but overridden by local file which is in turn overridden by
       file passed in. */
    ino_t prev = parse( "/etc/xwrelay/xwrelay.conf", 0 );
    prev = parse( "./xwrelay.conf", prev );
    (void)parse( cfile, prev );
} /* RelayConfigs::RelayConfigs */

void
RelayConfigs::GetPorts( std::vector<int>::const_iterator* iter, std::vector<int>::const_iterator* end)
{
    *iter = m_ports.begin();
    *end = m_ports.end();
}

ino_t
RelayConfigs::parse( const char* fname, ino_t prev )
{
    ino_t inode = 0;
    if ( fname != NULL ) {
        struct stat sbuf;
        stat( fname, &sbuf );
        inode = sbuf.st_ino;
        if ( inode != prev ) {
            FILE* f = fopen( fname, "r" );
            if ( f != NULL ) {
                logf( XW_LOGINFO, "config: reading from %s", fname );
                char line[MAX_LINE];

                for ( ; ; ) {
                    if ( !fgets( line, sizeof(line), f ) ) {
                        break;
                    }

                    int len = strlen( line );
                    if ( line[len-1] == '\n' ) {
                        line[--len] = '\0';
                    }

                    if ( len == 0 || line[0] == '#' ) {
                        continue;
                    }

                    char* value = strchr( line, '=' );
                    if ( value == NULL ) {
                        continue;
                    }

                    *value++ = '\0';    /* terminate "key" substring */
                    if ( 0 == strcmp( line, "HEARTBEAT" ) ) {
                        m_heartbeatInterval = atoi( value );
                    } else if ( 0 == strcmp( line, "ALLCONN" ) ) {
                        m_allConnInterval = atoi( value );
                    } else if ( 0 == strcmp( line, "CTLPORT" ) ) {
                        m_ctrlport = atoi( value );
                    } else if ( 0 == strcmp( line, "WWWPORT" ) ) {
                        m_httpport = atoi( value );
                    } else if ( 0 == strcmp( line, "PORT" ) ) {
                        m_ports.push_back( atoi( value ) );
                    } else if ( 0 == strcmp( line, "NTHREADS" ) ) {
                        m_nWorkerThreads = atoi( value );
                    } else if ( 0 == strcmp( line, "SERVERNAME" ) ) {
                        m_serverName = value;
                    } else if ( 0 == strcmp( line, "IDFILE" ) ) {
                        m_idFileName = value;
                    } else if ( 0 == strcmp( line, "LOGLEVEL" ) ) {
                        m_logLevel = atoi(value);
                    } else {
                        logf( XW_LOGERROR, "unknown key %s with value %s\n",
                              line, value );
                        assert( 0 );
                    }
                }
                fclose( f );
            }
        }
    }
    return inode;
} /* parse */
