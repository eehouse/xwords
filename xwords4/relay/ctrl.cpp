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
#include "crefmgr.h"
#include "xwrelay_priv.h"

/* this is *only* for testing.  Don't abuse!!!! */
extern pthread_rwlock_t gCookieMapRWLock;

typedef int (*CmdPtr)( int socket, const char** args );

typedef struct FuncRec {
    char* name;
    CmdPtr func;
} FuncRec;

static int cmd_quit( int socket, const char** args );
static int cmd_print( int socket, const char** args );
static int cmd_lock( int socket, const char** args );
static int cmd_help( int socket, const char** args );
static int cmd_start( int socket, const char** args );
static int cmd_stop( int socket, const char** args );
static int cmd_kill_eject( int socket, const char** args );
static int cmd_get( int socket, const char** args );
static int cmd_set( int socket, const char** args );
static int cmd_shutdown( int socket, const char** args );

static void
print_to_sock( int sock, int addCR, const char* what, ... )
{
    char buf[256];

    va_list ap;
    va_start( ap, what );
    vsnprintf( buf, sizeof(buf) - 1, what, ap );
    va_end(ap);

    if ( addCR ) {
        strncat( buf, "\n", sizeof(buf) );
    }
    send( sock, buf, strlen(buf), 0 );
}

static const FuncRec gFuncs[] = {
    { "?", cmd_help },
    { "help", cmd_help },
    { "quit", cmd_quit },
    { "print", cmd_print },
    { "lock", cmd_lock },
    { "start", cmd_start },
    { "stop", cmd_stop },
    { "kill", cmd_kill_eject },
    { "eject", cmd_kill_eject },
    { "shutdown", cmd_shutdown },
    { "get", cmd_get },
    { "set", cmd_set },
};

static int
cmd_quit( int socket, const char** args )
{
    if ( 0 == strcmp( "help", args[1] ) ) {
        print_to_sock( socket, 1, "* %s (disconnect from ctrl port)", args[0] );
    } else {
    }
    return 0;
}

static void
print_cookies( int socket, CookieID theID )
{
    CRefMgr* cmgr = CRefMgr::Get();
    CookieMapIterator iter = cmgr->GetCookieIterator();
    CookieID id;
    for ( id = iter.Next(); id != 0; id = iter.Next() ) {
        if ( theID == 0 || theID == id ) {
            SafeCref scr( id );
            string s;
            scr.PrintCookieInfo( s );

            print_to_sock( socket, 1, s.c_str() );
        }
    }
}

static int
cmd_start( int socket, const char** args )
{
    print_to_sock( socket, 1, "* %s (unimplemented)", args[0] );
    return 1;
}

static int
cmd_stop( int socket, const char** args )
{
    print_to_sock( socket, 1, "* %s (unimplemented)", args[0] );
    return 1;
}

static int
cmd_kill_eject( int socket, const char** args )
{
    int found = 0;
    int isKill = 0 == strcmp( args[0], "kill" );

    if ( 0 == strcmp( args[1], "socket" ) ) {
        int victim = atoi( args[2] );
        if ( victim != 0 ) {
            killSocket( victim, "ctrl command" );
            found = 1;
        }
    } else if ( 0 == strcmp( args[1], "cref" ) ) {
        const char* idhow = args[2];
        const char* id = args[3];
        if ( idhow != NULL && id != NULL ) {
            if ( 0 == strcmp( idhow, "name" ) ) {
                CRefMgr::Get()->Delete( id );
                found = 1;
            } else if ( 0 == strcmp( idhow, "id" ) ) {
                CRefMgr::Get()->Delete( atoi( id ) );
                found = 1;
            }
        }
    } else if ( 0 == strcmp( args[1], "relay" ) ) {
        print_to_sock( socket, 1, "not yet unimplemented" );
    }

    const char* expl = isKill? 
        "silently remove from game"
        : "remove from game with error to device";
    if ( !found ) {
        char* msg =
            "* %s socket <num>  -- %s\n"
            "  %s cref connName <connName>\n"
            "  %s cref id <id>"
            ;
        print_to_sock( socket, 1, msg, args[0], expl, args[0], args[0] );
    }
    return 1;
} /* cmd_kill_eject */

static int
cmd_get( int socket, const char** args )
{
    print_to_sock( socket, 1,
                   "* %s -- lists all attributes (unimplemented)\n"
                   "* %s <attribute> (unimplemented)",
                   args[0], args[0] );
    return 1;
}

static int
cmd_set( int socket, const char** args )
{
    print_to_sock( socket, 1, "* %s <attribute> (unimplemented)", args[0] );
    return 1;
}

static int
cmd_shutdown( int socket, const char** args )
{
    print_to_sock( socket, 1,
                   "* %s  -- shuts down relay (exiting main) (unimplemented)",
                   args[0] );
    return 1;
}

static void
print_cookies( int socket, const char* cookie, const char* connName )
{
    CookieMapIterator iter = CRefMgr::Get()->GetCookieIterator();
    CookieID id;

    for ( id = iter.Next(); id != 0; id = iter.Next() ) {
        SafeCref scr( id );
        if ( cookie != NULL && 0 == strcmp( scr.Cookie(), cookie ) ) {
            /* print this one */
        } else if ( connName != NULL && 
                    0 == strcmp( scr.ConnName(), connName ) ) {
            /* print this one */
        } else {
            continue;
        }
        string s;
        scr.PrintCookieInfo( s );

        print_to_sock( socket, 1, s.c_str() );
    }
}

static void
print_socket_info( int out, int which )
{
    string s;
    CRefMgr::Get()->PrintSocketInfo( which, s );
    print_to_sock( out, 1, s.c_str() );
}

static void
print_sockets( int out, int sought )
{
    SocketsIterator iter = CRefMgr::Get()->MakeSocketsIterator();
    int sock;
    while ( (sock = iter.Next()) != 0 ) {
        if ( sought == 0 || sought == sock ) {
            print_socket_info( out, sock );
        }
    }
}

static int
cmd_print( int socket, const char** args )
{
    logf( XW_LOGINFO, "cmd_print called" );
    int found = 0;
    if ( 0 == strcmp( "cref", args[1] ) ) {
        if ( 0 == strcmp( "all", args[2] ) ) {
            print_cookies( socket, (CookieID)0 );
            found = 1;
        } else if ( 0 == strcmp( "cookie", args[2] ) ) {
            print_cookies( socket, args[3], NULL );
            found = 1;
        } else if ( 0 == strcmp( "connName", args[2] ) ) {
            print_cookies( socket, NULL, args[3] );
            found = 1;
        } else if ( 0 == strcmp( "id", args[2] ) ) {
            print_cookies( socket, atoi(args[3]) );
            found = 1;
        }
    } else if ( 0 == strcmp( "socket", args[1] ) ) {
        if ( 0 == strcmp( "all", args[2] ) ) {
            print_sockets( socket, 0 );
            found = 1;
        } else if ( 0 == strcmp( "id", args[2] ) ) {
            print_sockets( socket, atoi(args[3]) );
            found = 1;
        }
    }

    if ( !found ) {
        char* str =
            "* %s cref all\n"
            "  %s cref name <name>\n"
            "  %s cref connName <name>\n"
            "  %s cref id <id>\n"
            "  %s socket all\n"
            "  %s socket <num>  -- print info about crefs and sockets";
        print_to_sock( socket, 1, str, 
                       args[0], args[0], args[0], args[0], args[0] );
    }
    return 0;
} /* cmd_print */

static int
cmd_lock( int socket, const char** args )
{
    CRefMgr* mgr = CRefMgr::Get();
    if ( 0 == strcmp( "on", args[1] ) ) {
        mgr->LockAll();
    } else if ( 0 == strcmp( "off", args[1] ) ) {
        mgr->UnlockAll();
    } else {
        print_to_sock( socket, 1, "* %s [on|off]  -- lock/unlock access mutex", 
                       args[0] );
    }
    
    return 0;
} /* cmd_lock */

static int
cmd_help( int socket, const char** args )
{
    if ( 0 == strcmp( "help", args[1] ) ) {
        print_to_sock( socket, 1, "* %s  -- prints this", args[0] );
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

static void
print_prompt( int socket )
{
    print_to_sock( socket, 0, "=> " );
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
        print_to_sock( sock, 1, "unknown command: \"%s\"", cmd );
        cmd_help( sock, args );
    }

    return 0;
}

static void*
ctrl_thread_main( void* arg )
{
    int socket = (int)arg;
    string arg0, arg1, arg2, arg3;

    for ( ; ; ) {
        print_prompt( socket );

        char buf[512];
        ssize_t nGot = recv( socket, buf, sizeof(buf)-1, 0 );
        logf( XW_LOGINFO, "nGot=%d", nGot );
        if ( nGot <= 1 ) {      /* break when just \n comes in */
            break;
        } else if ( nGot > 2 ) {
            /* if nGot is 2, reuse prev string */
            buf[nGot] = '\0';
            istringstream cmd( buf );
            cmd >> arg0 >> arg1 >> arg2 >> arg3;
        }

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
    return NULL;
} /* ctrl_thread_main */

void
run_ctrl_thread( int ctrl_listener )
{
    logf( XW_LOGINFO, "calling accept on socket %d\n", ctrl_listener );

    sockaddr newaddr;
    socklen_t siz = sizeof(newaddr);
    int newSock = accept( ctrl_listener, &newaddr, &siz );
    logf( XW_LOGINFO, "got one for ctrl: %d", newSock );

    pthread_t thread;
    int result = pthread_create( &thread, NULL, 
                                 ctrl_thread_main, (void*)newSock );
    assert( result == 0 );
}
