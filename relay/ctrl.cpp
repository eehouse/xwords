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
#include <unistd.h>
#include <netdb.h>		/* gethostbyname */
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>
#include <sys/select.h>
#include <stdarg.h>
#include <sys/time.h>

#include "ctrl.h"
#include "cref.h"
#include "xwrelay_priv.h"

/* this is *only* for testing.  Don't abuse!!!! */
extern pthread_mutex_t gCookieMapMutex;

static void
print_sock( int sock, const char* what, ... )
{
    char buf[256];

    va_list ap;
    va_start( ap, what );
    vsnprintf( buf, sizeof(buf) - 1, what, ap );
    va_end(ap);

    strncat( buf, "\n", sizeof(buf) );
    send( sock, buf, strlen(buf), 0 );
}

static void
print_help( int socket )
{
    char* help = 
        "Welcome to the console\n"
        "Commands are:\n"
        "? : prints this message\n"
        "q : quits\n"
        "cook : lists active cookies\n"
        "lock : locks the main cref mutex\n"
        "unlock : UNlocks the main cref mutex\n"
        ;
    print_sock( socket, help );
} /* print_help */

static void
print_cookies( int socket )
{
    print_sock( socket, "******************************" );
    
    CookieMapIterator iter = CookieRef::GetCookieNameIterator();
    const char* str;
    for ( str = iter.Next(); str != NULL; str = iter.Next() ) {
        print_sock( socket, str );
    }

    print_sock( socket, "******************************" );
}

static int
handle_command( const char* buf, int sock )
{
    if ( 0 == strcmp( buf, "?" ) ) {
        print_help( sock );
    } else if ( 0 == strcmp( buf, "cook" ) ) {
        print_cookies( sock );
    } else if ( 0 == strcmp( buf, "lock" ) ) {
        pthread_mutex_lock( &gCookieMapMutex );
    } else if ( 0 == strcmp( buf, "unlock" ) ) {
        pthread_mutex_unlock( &gCookieMapMutex );
    } else if ( 0 == strcmp( buf, "q" ) ) {
        return 0;
    } else {
        print_sock( sock, "unknown command: \"%s\"", buf );
        print_help( sock );
    }
    return 1;
}

static void*
ctrl_thread_main( void* arg )
{
    int socket = (int)arg;

    for ( ; ; ) {
        char buf[512];
        ssize_t nGot = recv( socket, buf, sizeof(buf)-1, 0 );
        if ( nGot <= 1 ) {      /* break when just \n comes in */
            break;
        }

        buf[nGot-2] = '\0';     /* kill \r\n stuff */
        if ( !handle_command( buf, socket ) ) {
            break;
        }
    }
    close ( socket );
}

void
run_ctrl_thread( int ctrl_listener )
{
    logf( "calling accept on socket %d\n", ctrl_listener );

    sockaddr newaddr;
    socklen_t siz = sizeof(newaddr);
    int newSock = accept( ctrl_listener, &newaddr, &siz );
    logf( "got one for ctrl: %d", newSock );

    pthread_t thread;
    int result = pthread_create( &thread, NULL, 
                                 ctrl_thread_main, (void*)newSock );
}
