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

#include "permid.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "mlock.h"

using namespace std;

pthread_mutex_t PermID::s_guard = PTHREAD_MUTEX_INITIALIZER;
string          PermID::s_serverName;
int             PermID::s_nextId = 0;
time_t          PermID::s_startTime;

string
PermID::GetNextUniqueID()
{
    MutexLock ml( &s_guard );

    char buf[64];
    snprintf( buf, sizeof(buf), "%s:%x:%d", s_serverName.c_str(), 
              (unsigned int)s_startTime, ++s_nextId );
    string s(buf);

    return s;
}

/* static */ void
PermID::SetServerName( const char* name )
{
    s_serverName = name;
}

/* static */ void
PermID::SetStartTime( time_t startTime )
{
    s_startTime = startTime;
    logf( XW_LOGINFO, "assigned startTime: %ld", s_startTime );
}
