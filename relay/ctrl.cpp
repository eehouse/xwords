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
#include <iostream>
#include <sstream>
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

typedef int (*CmdPtr)( int socket, const char** args );

typedef struct FuncRec {
    char* name;
    CmdPtr func;
} FuncRec;

static int cmd_quit( int socket, const char** args );
static int cmd_printCookies( int socket, const char** args );
static int cmd_lock( int socket, const char** args );
static int cmd_help( int socket, const char** args );

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

static const FuncRec gFuncs[] = {
    { "q", cmd_quit },
    { "cook", cmd_printCookies },
    { "lock", cmd_lock },
    { "help", cmd_help },
    { "?", cmd_help },
};

static int
cmd_quit( int socket, const char** args )
{
    if ( 0 == strcmp( "help", args[1] ) ) {
        print_sock( socket, "%s (close console connection)", args[0] );
        return 0;
    }
    return 1;
}

static int
cmd_printCookies( int socket, const char** args )
{
    if ( 0 == strcmp( "help", args[1] ) ) {
        print_sock( socket, "%s (list all cookies)", args[0] );
    } else {
        print_sock( socket, "******************************" );
    
        CookieMapIterator iter = CookieRef::GetCookieNameIterator();
        const char* str;
        for ( str = iter.Next(); str != NULL; str = iter.Next() ) {
            print_sock( socket, str );
        }

        print_sock( socket, "******************************" );
    }
    return 0;
}

static int
cmd_lock( int socket, const char** args )
{
    if ( 0 == strcmp( "on", args[1] ) ) {
        pthread_mutex_lock( &gCookieMapMutex );
    } else if ( 0 == strcmp( "off", args[1] ) ) {
        pthread_mutex_unlock( &gCookieMapMutex );
    } else {
        print_sock( socket, "%s [on|off] (lock/unlock mutex)", args[0] );
    }
    
    return 0;
} /* cmd_lock */

static int
cmd_help( int socket, const char** args )
{
    if ( 0 == strcmp( "help", args[1] ) ) {
    } else {

        const char* help[] = { NULL, "help", NULL, NULL };
        const FuncRec* fp = gFuncs;
        const FuncRec* last = fp + (sizeof(gFuncs) / sizeof(gFuncs[0]));
        while (  fp < last ) {
            help[0] = fp->name;
            (*fp->func)( socket, help );
            ++fp;
        }
    }
    return 0;
}

static int
dispatch_command( int sock, const char** args )
{
    const char* cmd = args[0];
    const FuncRec* fp = gFuncs;
    const FuncRec* last = fp + (sizeof(gFuncs) / sizeof(gFuncs[0]));
    while (  fp < last ) {
        if ( 0 == strcmp( cmd, fp->name ) ) {
            return (*fp->func)( sock, args );
        }
        ++fp;
    }
    
    if ( fp == last ) {
        print_sock( sock, "unknown command: \"%s\"", cmd );
        cmd_help( sock, args );
    }

    return 0;
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

        buf[nGot] = '\0';

        string arg0, arg1, arg2, arg3;
        istringstream cmd( buf );
        cmd >> arg0 >> arg1 >> arg2 >> arg3;

        const char* args[] = {
            arg0.c_str(), 
            arg1.c_str(), 
            arg2.c_str(), 
            arg3.c_str()
        };

        if ( dispatch_command( socket, args ) ) {
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
