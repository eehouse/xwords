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
#include <glib.h>

#include "ctrl.h"
#include "cref.h"
#include "crefmgr.h"
#include "mlock.h"
#include "xwrelay_priv.h"
#include "configs.h"
#include "lstnrmgr.h"
#include "tpool.h"
#include "devmgr.h"
#include "udpack.h"

/* this is *only* for testing.  Don't abuse!!!! */
extern pthread_rwlock_t gCookieMapRWLock;

/* Return of true means exit the ctrl thread */
typedef bool (*CmdPtr)( int socket, const char* cmd, int argc, gchar** argv );

typedef struct FuncRec {
    const char* name;
    CmdPtr func;
} FuncRec;

vector<int> g_ctrlSocks;
pthread_mutex_t g_ctrlSocksMutex = PTHREAD_MUTEX_INITIALIZER;


static bool cmd_acks( int socket, const char* cmd, int argc, gchar** argv );
static bool cmd_quit( int socket, const char* cmd, int argc, gchar** argv );
static bool cmd_print( int socket, const char* cmd, int argc, gchar** argv );
static bool cmd_devs( int socket, const char* cmd, int argc, gchar** argv );
/* static bool cmd_lock( int socket, gchar** argv ); */
static bool cmd_help( int socket, const char* cmd, int argc, gchar** argv );
static bool cmd_start( int socket, const char* cmd, int argc, gchar** argv );
static bool cmd_stop( int socket, const char* cmd, int argc, gchar** argv );
/* static bool cmd_kill_eject( int socket, gchar** argv ); */
static bool cmd_get( int socket, const char* cmd, int argc, gchar** argv );
static bool cmd_set( int socket, const char* cmd, int argc, gchar** argv );
static bool cmd_shutdown( int socket, const char* cmd, int argc, gchar** argv );
static bool cmd_rev( int socket, const char* cmd, int argc, gchar** argv );
static bool cmd_uptime( int socket, const char* cmd, int argc, gchar** argv );
static bool cmd_crash( int socket, const char* cmd, int argc, gchar** argv );

static void print_prompt( int socket );


static int
match( string* cmd, const char * const* first, int incr, int count )
{
    int cmdlen = cmd->length();
    int nFound = 0;
    const char* cmdFound = NULL;
    int which = -1;
    int ii;
    for ( ii = 0; (ii < count) && (nFound <= 1); ++ii ) {
        if ( 0 == strncmp( cmd->c_str(), *first, cmdlen ) ) {
            ++nFound;
            which = ii;
            cmdFound = *first;
        }
        first = (char* const*)(((char*)first) + incr);
    }

    if ( nFound == 1 ) {
        cmd->assign(cmdFound);
    } else {
        which = -1;
    }
    return which;
}

static void
print_to_sock( int sock, bool addCR, const char* what, ... )
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
    { "acks", cmd_acks },
    { "crash", cmd_crash },
    /* { "eject", cmd_kill_eject }, */
    { "get", cmd_get },
    { "help", cmd_help },
    /* { "kill", cmd_kill_eject }, */
    /* { "lock", cmd_lock }, */
    { "print", cmd_print },
    { "devs", cmd_devs },
    { "quit", cmd_quit },
    { "rev", cmd_rev },
    { "set", cmd_set },
    { "shutdown", cmd_shutdown },
    { "start", cmd_start },
    { "stop", cmd_stop },
    { "uptime", cmd_uptime },
};

static bool
cmd_quit( int socket, const char* cmd, int argc, gchar** argv )
{
    bool result;
    if ( 1 == argc ) {
        print_to_sock( socket, true, "bye bye" );
        result = true;
    } else {
        print_to_sock( socket, true, "* %s (disconnect from ctrl port)", 
                       cmd );
        result = false;
    }
    return result;
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

            print_to_sock( socket, true, s.c_str() );
        }
    }
}

static bool
cmd_start( int socket, const char* cmd, int argc, gchar** argv )
{
    print_to_sock( socket, true, "* %s (unimplemented)", cmd );
    return false;
}

static bool
cmd_stop( int socket, const char* cmd, int argc, gchar** argv )
{
    print_to_sock( socket, true, "* %s (unimplemented)", cmd );
    return false;
}

#if 0
static bool
cmd_kill_eject( int socket, gchar** argv )
{
    bool found = false;
    int isKill = 0 == strcmp( argv[0], "kill" );

    if ( 0 == strcmp( argv[1], "socket" ) ) {
        int victim = atoi( argv[2] );
        if ( victim != 0 ) {
            XWThreadPool::GetTPool()-> EnqueueKill( victim, "ctrl command" );
            found = true;
        }
    } else if ( 0 == strcmp( argv[1], "cref" ) ) {
        const char* idhow = argv[2];
        for ( int indx = 3; ; ++indx ) {
            const char* id = argv[indx];
            if ( idhow == NULL || id == NULL ) {
                break;
            }
            if ( 0 == strcmp( idhow, "name" ) ) {
                CRefMgr::Get()->Recycle( id );
                found = true;
            } else if ( 0 == strcmp( idhow, "id" ) ) {
                CRefMgr::Get()->Recycle( atoi( id ) );
                found = true;
            }
        }
    } else if ( 0 == strcmp( argv[1], "relay" ) ) {
        print_to_sock( socket, true, "not yet unimplemented" );
    }

    const char* expl = isKill? 
        "silently remove from game"
        : "remove from game with error to device";
    if ( !found ) {
        const char* msg =
            "* %s socket <num>  -- %s\n"
            "  %s cref name <connName>+\n"
            "  %s cref id <id>+"
            ;
        print_to_sock( socket, true, msg, argv[0], expl, cmd, cmd );
    }
    return false;
} /* cmd_kill_eject */
#endif

static bool
cmd_get( int socket, const char* cmd, int argc, gchar** argv )
{
    bool needsHelp = true;
    if ( 2 >= argc ) {
        int val;
        RelayConfigs* rc = RelayConfigs::GetConfigs();
        string attr(argv[1]);
        const char* const attrs[] = { "help", "listeners", "loglevel" , "acklimit" };
        int index = match( &attr, attrs, sizeof(attrs[0]), 
                           sizeof(attrs)/sizeof(attrs[0]));

        switch( index ) {
        case 0:
            break;
        case 1: {
            char buf[128];
            int len = 0;
            ListenersIter iter(&g_listeners, false);
            for ( ; ; ) {
                int listener = iter.next();
                if ( listener == -1 ) {
                    break;
                }
                len += snprintf( &buf[len], sizeof(buf)-len, "%d,", listener );
            }
            print_to_sock( socket, true, "%s", buf );
            needsHelp = false;
        }
            break;
        case 2:
        case 3: {
            const char* key = 2 == index? "LOGLEVEL" : "UDP_ACK_LIMIT";
            if ( NULL != rc && rc->GetValueFor( key, &val ) ) {
                print_to_sock( socket, true, "%s=%d\n",  attrs[index], val );
                needsHelp = false;
            }
        }
            break;

        default:
            print_to_sock( socket, true, "unknown or ambiguous attribute: %s", 
                           attr.c_str() );
        }
    }
    if ( needsHelp ) {
        /* includes help */
        print_to_sock( socket, false,
                       "* %s -- lists all attributes (unimplemented)\n"
                       "* %s listener\n"
                       "* %s loglevel\n"
                       "* %s acklimit\n"
                       , cmd, cmd, cmd, cmd );
    }

    return false;
} /* cmd_get */

static bool
cmd_set( int socket, const char* cmd, int argc, gchar** argv )
{
    bool needsHelp = true;
    if ( 3 >= argc ) {
        RelayConfigs* rc;
        const char* val = argv[2];
        const char* const attrs[] = { "help", "listeners", "loglevel", "acklimit" };
        string attr(argv[1]);
        int index = match( &attr, attrs, sizeof(attrs[0]), 
                           sizeof(attrs)/sizeof(attrs[0]));

        switch( index ) {
        case 1:
            if ( NULL != val && val[0] != '\0' ) {
                istringstream str( val );
                vector<int> sv;
                while ( !str.eof() ) {
                    int sock;
                    char comma;
                    str >> sock >> comma;
                    logf( XW_LOGERROR, "%s: read %d", __func__, sock );
                    sv.push_back( sock );
                }
                g_listeners.SetAll( &sv );
                needsHelp = false;
            }
            break;
        case 2:
            if ( NULL != val && val[0] != '\0' ) {
                rc = RelayConfigs::GetConfigs();
                if ( rc != NULL ) {
                    rc->SetValueFor( "LOGLEVEL", val );
                    needsHelp = false;
                }
            }
            break;
        case 3:
            rc = RelayConfigs::GetConfigs();
            if ( rc != NULL ) {
                rc->SetValueFor( "UDP_ACK_LIMIT", val );
                needsHelp = false;
            }
            break;
        default:
            break;
        }
    }
    if ( needsHelp ) {
        print_to_sock( socket, true, 
                       "* %s listeners <n>,[<n>,..<n>,]"
                       "\n* %s loglevel <n>"
                       "\n* %s acklimit <n>"
                       ,cmd, cmd, cmd );
    }
    return false;
}

void
format_rev( char* buf, int len )
{
    snprintf( buf, len, "git rev: %s", SVN_REV );
}

static bool
cmd_rev( int socket, const char* cmd, int argc, gchar** argv )
{
    if ( 1 == argc ) {
        char buf[128];
        format_rev( buf, sizeof(buf) );
        print_to_sock( socket, true, "%s", buf );
    } else {
        print_to_sock( socket, true,
                       "* %s -- prints svn rev number of build",
                       cmd );
    }
    return false;
}

void
format_uptime( time_t seconds, char* buf, int len )
{
    int days = seconds / (24*60*60);
    seconds %= (24*60*60);

    int hours = seconds / (60*60);
    seconds %= (60*60);

    int minutes = seconds / 60;
    seconds %= 60;

    if ( days > 0 ) {
        snprintf( buf, len, "%d D %.2d:%.2d:%.2ld", days, hours,
                  minutes, seconds );
    } else if ( hours > 0 ) {
        snprintf( buf, len, "%.2d:%.2d:%.2ld", hours,
                  minutes, seconds );
    } else if ( minutes > 0 ) {
        snprintf( buf, len, "%.2d:%.2ld", minutes, seconds );
    } else {
        snprintf( buf, len, "%.2ld s", seconds );
    }
}

static bool
cmd_uptime( int socket, const char* cmd, int argc, gchar** argv )
{
    if ( 1 == argc ) {
        char buf[128];
        format_uptime( uptime(), buf, sizeof(buf) );
        print_to_sock( socket, true, "uptime: %s", buf );
    } else {
        print_to_sock( socket, true,
                       "* %s -- prints how long the relay's been running",
                       cmd );
    }
    return false;
}

static bool
cmd_crash( int socket, const char* cmd, int argc, gchar** argv )
{
    if ( 1 == argc ) {
        logf( XW_LOGERROR, "crashing..." );
        assert(0);
        int ii = 1;
        while ( ii > 0 ) --ii;
        return 6/ii > 0;
    } else {
        print_to_sock( socket, true,
                       "* %s -- fires an assert (debug case) or divides-by-zero",
                       cmd );
    }
    return false;
}

static bool
cmd_shutdown( int socket, const char* cmd, int argc, gchar** argv )
{
    print_to_sock( socket, true,
                   "* %s  -- shuts down relay (exiting main) (unimplemented)",
                   cmd );
    return false;
}

static void
print_cookies( int socket, const char* cookie, const char* connName )
{
    CookieMapIterator iter = CRefMgr::Get()->GetCookieIterator();
    CookieID id;

    for ( id = iter.Next(); id != 0; id = iter.Next() ) {
        SafeCref scr( id );
        if ( cookie != NULL && 0 == strcasecmp( scr.Cookie(), cookie ) ) {
            /* print this one */
        } else if ( connName != NULL && 
                    0 == strcmp( scr.ConnName(), connName ) ) {
            /* print this one */
        } else {
            continue;
        }
        string s;
        scr.PrintCookieInfo( s );

        print_to_sock( socket, true, s.c_str() );
    }
}

#if 0
static void
print_socket_info( int out, int which )
{
    string s;
    CRefMgr::Get()->PrintSocketInfo( which, s );
    print_to_sock( out, 1, s.c_str() );
}
#endif

static void
print_sockets( int out, int sought )
{
    /* SocketsIterator iter = CRefMgr::Get()->MakeSocketsIterator(); */
    /* int sock; */
    /* while ( (sock = iter.Next()) != 0 ) { */
    /*     if ( sought == 0 || sought == sock ) { */
    /*         print_socket_info( out, sock ); */
    /*     } */
    /* } */
}

static bool
cmd_print( int socket, const char* cmd, int argc, gchar** argv )
{
    bool found = false;
    if ( 2 <= argc ) {
        if ( 0 == strcmp( "cref", argv[1] ) ) {
            if ( 3 <= argc ) {
                if ( 0 == strcmp( "all", argv[2] ) ) {
                    print_cookies( socket, (CookieID)0 );
                    found = true;
                } else if ( 0 == strcmp( "cookie", argv[2] ) ) {
                    print_cookies( socket, argv[3], NULL );
                    found = true;
                } else if ( 0 == strcmp( "connName", argv[2] ) && 4 <= argc ) {
                    print_cookies( socket, NULL, argv[3] );
                    found = true;
                } else if ( 0 == strcmp( "id", argv[2] ) && 4 <= argc ) {
                    print_cookies( socket, atoi(argv[3]) );
                    found = true;
                }
            }
        } else if ( 0 == strcmp( "socket", argv[1] ) ) {
            if ( 3 <= argc ) {
                if ( 0 == strcmp( "all", argv[2] ) ) {
                    print_sockets( socket, 0 );
                    found = true;
                } else if ( 0 == strcmp( "id", argv[2] ) && 4 <= argc) {
                    print_sockets( socket, atoi(argv[3]) );
                    found = true;
                }
            }
        }
    }
    if ( !found ) {
        const char* str =
            "* %s cref all\n"
            "  %s cref name <name>\n"
            "  %s cref connName <name>\n"
            "  %s cref id <id>\n"
            "  %s dev all -- list all known devices (by how recently connected)\n"
            "  %s socket all\n"
            "  %s socket <num>  -- print info about crefs and sockets";
        print_to_sock( socket, true, str, cmd, cmd, cmd, 
                       cmd, cmd, cmd, cmd );
    }
    return false;
} /* cmd_print */

/* NOTE: this will probably crash if socket is closed before the ack comes
   back or times out */
static void 
onAckProc( bool acked, DevIDRelay devid, uint32_t packetID, void* data )
{
    int socket = (int)data;
    if ( acked ) {
        print_to_sock( socket, true, "got ack for packet %d from dev %d", 
                       packetID, devid );
    } else {
        print_to_sock( socket, true, "NO ACK for packetID %d from dev %d", 
                       packetID, devid );
    }
    print_prompt( socket );
}

static bool
cmd_acks( int socket, const char* cmd, int argc, gchar** argv )
{
    bool found = false;
    string result;

    if ( 1 >= argc ) {
        /* missing param; let help print */
    } else if ( 0 == strcmp( "list", argv[1] ) ) {
        UDPAckTrack::printAcks( result );
        found = true;
    } else if ( 3 == argc && 0 == strcmp( "nack", argv[1] ) 
                && 0 == strcmp( "all", argv[2] ) ) {
        vector<uint32_t> packets;
        UDPAckTrack::doNack( packets );
        found = true;
    }

    if ( found ) {
        if ( 0 < result.size() ) {
            send( socket, result.c_str(), result.size(), 0 );
        }
    } else {
        const char* strs[] = {
            "* %s list\n"
            ,"* %s nack all\n"
        };
        string help;
        for ( size_t ii = 0; ii < VSIZE(strs); ++ii ) {
            string_printf( help, strs[ii], cmd );
        }
        send( socket, help.c_str(), help.size(), 0 );
    }
    return false;
}

static bool
cmd_devs( int socket, const char* cmd, int argc, gchar** argv )
{
    bool found = false;
    string result;
    if ( 1 >= argc ) {
        /* missing param; let help print */
    } else {
        const gchar* arg1 = argv[1];
        if ( 0 == strcmp( "list", arg1 ) || 0 == strcmp( "rm", arg1 ) ) {
            if ( 3 <= argc ) {
                found = true;
                vector<DevIDRelay> devids;
                if ( 3 == argc && 0 == strcmp( "all", argv[2] ) ) {
                    /* do nothing; empty vector means all */
                } else {
                    found = false; /* if all are bogus numbers, drop */
                    for ( int ii = 2; ii < argc; ++ii ) {
                        DevIDRelay devid = (DevIDRelay)strtoul( argv[ii], 
                                                                NULL, 10 );
                        if ( 0 != devid ) {
                            devids.push_back( devid );
                            found = true;
                        }
                    }
                }
                if ( !found ) {
                    /* do nothing */
                } else if ( 0 == strcmp( "list", arg1 ) ) {
                    DevMgr::Get()->printDevices( result, devids );
                } else {
                    int deleted = DevMgr::Get()->forgetDevices( devids );
                    string_printf( result, "Deleted %d devices\n", deleted );
                }
            }
        } else if ( 0 == strcmp( "ping", arg1 ) ) {
        } else if ( 0 == strcmp( "msg", arg1 ) && 3 < argc ) {
            /* Get the list to send to */
            vector<DevIDRelay> devids;
            if ( 0 == strcmp( "all", argv[3] ) ) {
                DevMgr::Get()->getKnownDevices( devids );
            } else {
                for ( int ii = 3; ii < argc; ++ii ) {
                    DevIDRelay devid = (DevIDRelay)strtoul( argv[ii], NULL, 10 );
                    devids.push_back( devid );
                }
            }

            /* Send to each in the list */
            const char* msg = argv[2];
            gchar* unesc = g_strcompress( msg );
            vector<DevIDRelay>::const_iterator iter;
            for ( iter = devids.begin(); devids.end() != iter; ++iter ) {
                DevIDRelay devid = *iter;
                if ( 0 != devid ) {
                    if ( post_message( devid, unesc, onAckProc, 
                                       (void*)socket ) ) {
                        string_printf( result, "posted message: %s\n", unesc );
                    } else {
                        string_printf( result, "unable to post; does "
                                       "dev %d exist\n", devid );
                    }
                }
            }
            g_free( unesc );

            found = true;
        }
    }

    if ( found ) {
        if ( 0 < result.size() ) {
            send( socket, result.c_str(), result.size(), 0 );
        }
    } else {
        const char* strs[] = {
            "* %s list [all | <id>+] -- prints all ids, listed ids, or all if none specified\n",
            "  %s ping\n",
            "  %s msg <msg_text> <devid>+\n",
            "  %s msg <msg_text> all\n",
            "  %s rm [all | <devid>]\n",
            "* %s rm [all | <id>+]\n"
        };

        string help;
        for ( size_t ii = 0; ii < VSIZE(strs); ++ii ) {
            string_printf( help, strs[ii], cmd );
        }
        send( socket, help.c_str(), help.size(), 0 );
    }
    return false;
}

#if 0
static bool
cmd_lock( int socket, gchar** argv )
{
    CRefMgr* mgr = CRefMgr::Get();
    if ( 0 == strcmp( "on", argv[1] ) ) {
        mgr->LockAll();
    } else if ( 0 == strcmp( "off", argv[1] ) ) {
        mgr->UnlockAll();
    } else {
        print_to_sock( socket, true, "* %s [on|off]  -- lock/unlock access mutex", 
                       cmd );
    }
    
    return 0;
} /* cmd_lock */
#endif

static bool
cmd_help( int socket, const char* cmd, int argc, gchar** argv )
{
    if ( 1 < argc && NULL != argv[1] && 0 == strcmp( "help", argv[1] ) ) {
        print_to_sock( socket, true, "* %s  -- prints this", cmd );
    } else {

        gchar* help[] = { NULL, (gchar*)"help" };
        const FuncRec* fp = gFuncs;
        const FuncRec* last = fp + (sizeof(gFuncs) / sizeof(gFuncs[0]));
        while ( fp < last ) {
            help[0] = (gchar*)fp->name;
            (*fp->func)( socket, (gchar*)fp->name, VSIZE(help), help );
            ++fp;
        }
    }
    return 0;
}

static void
print_prompt( int socket )
{
    print_to_sock( socket, false, "=> " );
}

static void*
ctrl_thread_main( void* arg )
{
    blockSignals();
    int sock = (int)arg;

    {
        MutexLock ml( &g_ctrlSocksMutex );
        g_ctrlSocks.push_back( sock );
    }

    gint argc = 0;
    gchar** argv = NULL;
    int index = -1;
    string cmd;

    for ( ; ; ) {

        print_prompt( sock );

        char buf[512];
        ssize_t nGot = recv( sock, buf, sizeof(buf)-1, 0 );
        if ( 0 >= nGot ) {
            break;
        } else if ( 1 == nGot ) {
            assert( 0 );        /* not happening, as getting \r\n terminator */
        } else if ( 2 == nGot ) {
            /* user hit return; repeat prev command */
        } else {
            buf[nGot-2] = '\0';
            if ( NULL != argv ) {
                g_strfreev( argv );
            }

            if ( !g_shell_parse_argv( buf, &argc, &argv, NULL ) ) {
                assert( 0 );
            } else {
                cmd = argv[0];
                index = match( &cmd, (char*const*)&gFuncs[0].name, 
                                   sizeof(gFuncs[0]), 
                                   sizeof(gFuncs)/sizeof(gFuncs[0]) );
            }
        }
        if ( index == -1 ) {
            print_to_sock( sock, 1, "unknown or ambiguous command: \"%s\"", 
                           cmd.c_str() );
            gchar* argv[] = { (gchar*)cmd.c_str() };
            (void)cmd_help( sock, "help", 1, argv );
        } else if ( (*gFuncs[index].func)( sock, cmd.c_str(), 
                                           argc, argv ) ) {
            break;
        }
    }

    close ( sock );

    MutexLock ml( &g_ctrlSocksMutex );
    vector<int>::iterator iter = g_ctrlSocks.begin();
    while ( iter != g_ctrlSocks.end() ) {
        if ( *iter == sock ) {
            g_ctrlSocks.erase(iter);
            break;
        }
    }
    return NULL;
} /* ctrl_thread_main */

void
run_ctrl_thread( int ctrl_sock )
{
    logf( XW_LOGINFO, "calling accept on socket %d", ctrl_sock );

    sockaddr newaddr;
    socklen_t siz = sizeof(newaddr);
    int newSock = accept( ctrl_sock, &newaddr, &siz );
    logf( XW_LOGINFO, "got one for ctrl: %d", newSock );

    pthread_t thread;
    int result = pthread_create( &thread, NULL, 
                                 ctrl_thread_main, (void*)newSock );
    pthread_detach( thread );

    assert( result == 0 );
}

void
stop_ctrl_threads()
{
    MutexLock ml( &g_ctrlSocksMutex );
    vector<int>::iterator iter = g_ctrlSocks.begin();
    while ( iter != g_ctrlSocks.end() ) {
        int sock = *iter++;
        print_to_sock( sock, 1, "relay going down..." );
        close( sock );
    }
}
