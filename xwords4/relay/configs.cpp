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
    assert( instance != NULL );
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
    FILE* f = NULL;
    if ( cfile != NULL ) {
        f = fopen( cfile, "r" );
    }
    if ( f == NULL ) {
        f = fopen( "./xwrelay.conf", "r" );
    }        
    if ( f == NULL ) {
        f = fopen( "/etc/xwrelay/xwrelay.conf", "r" );
    }        
    if ( f != NULL ) {
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
            } else if ( 0 == strcmp( line, "PORT" ) ) {
                m_port = atoi( value );
            } else if ( 0 == strcmp( line, "NTHREADS" ) ) {
                m_nWorkerThreads = atoi( value );
            } else if ( 0 == strcmp( line, "SERVERNAME" ) ) {
                m_serverName = value;
            } else {
                logf( XW_LOGERROR, "unknown key %s with value %s\n",
                      line, value );
                assert( 0 );
            }
        }
        fclose( f );
    }
}

    
RelayConfigs::~RelayConfigs()
{
}
 
