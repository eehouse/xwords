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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "configs.h"
#include "mlock.h"

#define MAX_LINE 128

RelayConfigs* RelayConfigs::instance = NULL;

static bool strs_comp( const char* key1, const char* key2 );

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
    :m_values(strs_comp)
{
    pthread_mutex_init( &m_values_mutex, NULL );
    
    /* There's an order here.  Open multiple files, if present.  File in /etc
       is first, but overridden by local file which is in turn overridden by
       file passed in. */
    ino_t prev = parse( "/etc/xwrelay/xwrelay.conf", 0 );
    prev = parse( "./xwrelay.conf", prev );
    (void)parse( cfile, prev );
} /* RelayConfigs::RelayConfigs */

bool
RelayConfigs::GetValueFor( const char* key, int* value )
{
    char buf[32];
    bool found = GetValueFor( key, buf, sizeof(buf) );
    if ( found ) {
        *value = atoi( buf );
    }
    return found;
}

bool
RelayConfigs::GetValueFor( const char* key, time_t* value )
{
    int val;
    bool success = GetValueFor( key, &val );
    if ( success ) {
        *value = val;
    }
    return success;
}

bool
RelayConfigs::GetValueFor( const char* key, char* buf, int len )
{
    MutexLock ml( &m_values_mutex );
    map<const char*,const char*>::const_iterator iter = m_values.find(key);
    bool found = iter != m_values.end();
    if ( found ) {
        snprintf( buf, len, "%s", iter->second );
    }
    return found;
}

bool
RelayConfigs::GetValueFor( const char* key, vector<int>& ints )
{
    char str[256];
    bool found = GetValueFor( key, str, sizeof(str) );
    int len = strlen(str);
    char buf[len+1];
    strcpy( buf, str );
    char* port = buf;

    for ( ; ; ) {
        char* end = strstr( port, "," );
        if ( !!end ) {
            *end = '\0';
        }

        fprintf( stderr, "adding %s to ports\n", port );
        ints.push_back( atoi(port) );

        if ( !end ) {
            break;
        }
        port = end + 1;
    }


    return found;
}

void 
RelayConfigs::SetValueFor( const char* key, const char* value )
{
    MutexLock ml( &m_values_mutex );

    /* Remove any entry already there */
    map<const char*,const char*>::iterator iter = m_values.find(key);
    if ( iter != m_values.end() ) {
        m_values.erase(iter);
    }

    m_values.insert( pair<const char*,const char*>(strdup(key),strdup(value) ) );
}

ino_t
RelayConfigs::parse( const char* fname, ino_t prev )
{
    ino_t inode = 0;
    if ( fname != NULL ) {
        struct stat sbuf;
        if ( 0 == stat( fname, &sbuf ) ) {
            inode = sbuf.st_ino;
            if ( inode != prev ) {
                FILE* f = fopen( fname, "r" );
                if ( f != NULL ) {
                    fprintf( stderr, "config: reading from %s\n", fname );
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

                        SetValueFor( line, value );
                    }
                    fclose( f );
                }
            }
        }
    }
    return inode;
} /* parse */

static bool
strs_comp( const char* key1, const char* key2 )
{
    bool result = strcmp( key1, key2 ) < 0;
    return result;
}

