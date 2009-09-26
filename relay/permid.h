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


#ifndef _PERMID_H
#define _PERMID_H

#include <string>
#include <pthread.h>

/* Able to get a unique ID for every game ever started.  A simple file
 * somewhere stores an ascii string representing a number.  That's
 * incremented, saved and returned each time.  In a world with multiple relays
 * (multiple machines) the ID could include a host identifier to guarantee
 * uniqueness.
 */
class PermID {
 public:
    static void SetServerName( const char* name );
    static void SetStartTime( time_t startTime );
    static std::string GetNextUniqueID();

 private:
    static pthread_mutex_t    s_guard;        /* guard access to the whole
                                                 process */
    static std::string        s_serverName;   /* All ID's generated start with
                                                 this, which is supposed to be
                                                 unique to this relay
                                                 instance. */
    static int                s_nextId;       /* numeric part of ID */
    static time_t             s_startTime;    /* allows multiple servers per
                                                 host to have uniquie names
                                                 and to ensure no duplications
                                                 after a crash. */
};
#endif
