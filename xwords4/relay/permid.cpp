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

#include "permid.h"

#include <stdio.h>
#include "mlock.h"

using namespace std;

pthread_mutex_t PermID::s_guard = PTHREAD_MUTEX_INITIALIZER;
string          PermID::s_serverName;
string          PermID::s_idFileName;

string
PermID::GetNextUniqueID()
{
    const char* fileName = s_idFileName.c_str();
    MutexLock ml( &s_guard );

    string s = s_serverName;
    assert( s.length() > 0 );
    s += ":";
    
    char buf[32];               /* should last for a while :-) */

    FILE* f = fopen( fileName, "r+" );
    if ( f ) {
        fscanf( f, "%s\n", buf );
        rewind( f );
    } else {
        f = fopen( fileName, "w" );
        assert ( f != NULL );
        buf[0] = '0';
        buf[1] = '\0';
    }

    int n = atoi(buf) + 1;
    sprintf( buf, "%d", n );

    fprintf( f, "%s\n", buf );
    fclose( f );
    
    s += buf;

    return s;
}

/* static */ void
PermID::SetServerName( const char* name )
{
    s_serverName = name;
}

/* static */ void
PermID::SetIDFileName( const char* name )
{
    s_idFileName = name;
}
