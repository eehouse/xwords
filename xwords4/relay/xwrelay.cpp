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

//////////////////////////////////////////////////////////////////////////////
//
// This program is a *very rough* cut at a message forwarding server that's
// meant to sit somewhere that cellphones can reach and forward packets across
// connections so that they can communicate.  It exists to work around the
// fact that many cellular carriers prevent direct incoming connections from
// reaching devices on their networks.  It's meant for Crosswords, but might
// be useful for other things.  It also needs a lot of work, and I hacked it
// up before making an exhaustive search for other alternatives.
//
//////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <unistd.h>
#include <netdb.h>		/* gethostbyname */
#include <errno.h>
#include <signal.h>
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
#include <syslog.h>
#include <sys/wait.h>
#include <sys/prctl.h>

#if defined(__FreeBSD__)
# if (OSVERSION > 500000)
#  include "getopt.h"
# else
#  include "unistd.h"
# endif
#else
# include <getopt.h>
#endif

#include <sys/time.h>

#include "xwrelay.h"
#include "crefmgr.h"
#include "ctrl.h"
#include "http.h"
#include "mlock.h"
#include "tpool.h"
#include "configs.h"
#include "timermgr.h"
#include "permid.h"
#include "lstnrmgr.h"

static int s_nSpawns = 0;

void
logf( XW_LogLevel level, const char* format, ... )
{
    RelayConfigs* rc = RelayConfigs::GetConfigs();
    int configLevel = level;

    if ( NULL != rc ) {

        if ( ! rc->GetValueFor( "LOGLEVEL", &configLevel ) ) {
            configLevel = level - 1; /* drop it */
        }
    }

    if ( level <= configLevel ) {
#ifdef USE_SYSLOG
        char buf[256];
        va_list ap;
        va_start( ap, format );
        vsnprintf( buf, sizeof(buf), format, ap );
        syslog( LOG_LOCAL0 | LOG_INFO, buf );
        va_end(ap);
#else
        FILE* where = NULL;
        struct tm* timp;
        struct timeval tv;
        bool useFile;
        char logFile[256];

        useFile = rc->GetValueFor( "LOGFILE_PATH", logFile, sizeof(logFile) );

        if ( useFile ) {
            where = fopen( logFile, "a" );
        } else  {
            where = stderr;
        }

        if ( !!where ) {
            gettimeofday( &tv, NULL );
            struct tm result;
            timp = localtime_r( &tv.tv_sec, &result );

            pthread_t me = pthread_self();

            fprintf( where, "<%p>%.2d:%.2d:%.2d: ", (void*)me, timp->tm_hour, 
                     timp->tm_min, timp->tm_sec );

            va_list ap;
            va_start( ap, format );
            vfprintf( where, format, ap );
            va_end(ap);
            fprintf( where, "\n" );

            if ( useFile && !!where ) {
                fclose( where );
            }
        }
#endif
    }
} /* logf */

const char*
cmdToStr( XWRELAY_Cmd cmd )
{
# define CASESTR(s)  case s: return #s
    switch( cmd ) {
        CASESTR(XWRELAY_NONE);
        CASESTR(XWRELAY_GAME_CONNECT);
        CASESTR(XWRELAY_GAME_RECONNECT);
        CASESTR(XWRELAY_GAME_DISCONNECT);
        CASESTR(XWRELAY_CONNECT_RESP);
        CASESTR(XWRELAY_RECONNECT_RESP);
        CASESTR(XWRELAY_ALLHERE);
        CASESTR(XWRELAY_DISCONNECT_YOU);
        CASESTR(XWRELAY_DISCONNECT_OTHER);
        CASESTR(XWRELAY_CONNECTDENIED);
        CASESTR(XWRELAY_HEARTBEAT);
        CASESTR(XWRELAY_MSG_FROMRELAY);
        CASESTR(XWRELAY_MSG_TORELAY);
    default:
        logf( XW_LOGERROR, "%s: unknown command %d", __func__, cmd );
        return "<unknown>";
    }
}

static bool
getNetShort( unsigned char** bufpp, unsigned char* end, unsigned short* out )
{
    bool ok = *bufpp + 2 <= end;
    if ( ok ) {
        unsigned short tmp;
        memcpy( &tmp, *bufpp, 2 );
        *bufpp += 2;
        *out = ntohs( tmp );
    }
    return ok;
} /* getNetShort */

static bool
getNetByte( unsigned char** bufpp, unsigned char* end, unsigned char* out )
{
    bool ok = *bufpp < end;
    if ( ok ) {
        *out = **bufpp;
        ++*bufpp;
    }
    return ok;
} /* getNetByte */

#ifdef RELAY_HEARTBEAT
static bool
processHeartbeat( unsigned char* buf, int bufLen, int socket )
{
    unsigned char* end = buf + bufLen;
    CookieID cookieID; 
    HostID hostID;
    bool success = false;

    if ( getNetShort( &buf, end, &cookieID ) /* may be wrong if ALLCONN hasn't been sent */
         && getNetByte( &buf, end, &hostID ) ) {
        logf( XW_LOGINFO, "processHeartbeat: cookieID 0x%lx, hostID 0x%x", 
              cookieID, hostID );

        {
            SafeCref scr( socket );
            success = scr.HandleHeartbeat( hostID, socket );
        }
        if ( !success ) {
            killSocket( socket, "no cref for socket" );
        }
    }
    return success;
} /* processHeartbeat */
#endif

static bool
readStr( unsigned char** bufp, const unsigned char* end, 
         char* outBuf, int bufLen )
{
    unsigned char clen = **bufp;
    ++*bufp;
    if ( ((*bufp + clen) <= end) && (clen < bufLen) ) {
        memcpy( outBuf, *bufp, clen );
        outBuf[clen] = '\0';
        *bufp += clen;
        return true;
    }
    return false;
} /* readStr */

static XWREASON
flagsOK( unsigned char flags )
{
    return flags == XWRELAY_PROTO_VERSION ?
        XWRELAY_ERROR_NONE : XWRELAY_ERROR_OLDFLAGS;
} /* flagsOK */

void
denyConnection( int socket, XWREASON err )
{
    unsigned char buf[2];

    buf[0] = XWRELAY_CONNECTDENIED;
    buf[1] = err;

    send_with_length_unsafe( socket, buf, sizeof(buf) );
}

/* No mutex here.  Caller better be ensuring no other thread can access this
 * socket. */
bool
send_with_length_unsafe( int socket, unsigned char* buf, int bufLen )
{
    bool ok = false;
    unsigned short len = htons( bufLen );
    ssize_t nSent = send( socket, &len, 2, 0 );
    if ( nSent == 2 ) {
        nSent = send( socket, buf, bufLen, 0 );
        if ( nSent == bufLen ) {
            logf( XW_LOGINFO, "sent %d bytes on socket %d", nSent, socket );
            ok = true;
        }
    }

    if ( !ok ) {
        logf( XW_LOGERROR, "%s(socket=%d) failed", __func__, socket );
    }

    return ok;
} /* send_with_length_unsafe */


/* A CONNECT message from a device gives us the hostID and socket we'll
 * associate with one participant in a relayed session.  We'll store this
 * information with the cookie where other participants can find it when they
 * arrive.
 *
 * What to do if we already have a game going?  In that case the connection ID
 * passed in will be non-zero.  If the device can be associated with an
 * ongoing game, with its new socket, associate it and forward any messages
 * outstanding.  Otherwise close down the socket.  And maybe the others in the
 * game?
 */
static bool
processConnect( unsigned char* bufp, int bufLen, int socket )
{
    char cookie[MAX_INVITE_LEN+1];
    unsigned char* end = bufp + bufLen;
    bool success = false;

    logf( XW_LOGINFO, "%s()", __func__ );

    cookie[0] = '\0';

    unsigned char flags = *bufp++;
    XWREASON err = flagsOK( flags );
    if ( err == XWRELAY_ERROR_NONE ) {
        HostID srcID;
        unsigned char nPlayersH;
        unsigned char nPlayersT;
        unsigned short gameSeed;
        if ( readStr( &bufp, end, cookie, sizeof(cookie) ) 
             && getNetByte( &bufp, end, &srcID )
             && getNetByte( &bufp, end, &nPlayersH )
             && getNetByte( &bufp, end, &nPlayersT )
             && getNetShort( &bufp, end, &gameSeed ) ) {

            /* Make sure second thread can't create new cref for same cookie
               this one just handled.*/
            static pthread_mutex_t s_newCookieLock = PTHREAD_MUTEX_INITIALIZER;
            MutexLock ml( &s_newCookieLock );

            SafeCref scr( cookie, NULL, srcID, socket, nPlayersH, nPlayersT, 
                          gameSeed );
            /* nPlayersT etc could be slots in SafeCref to avoid passing
               here */
            success = scr.Connect( socket, srcID, nPlayersH, nPlayersT, 
                                   gameSeed );
        } else {
            err = XWRELAY_ERROR_BADPROTO;
        }
    }

    if ( err != XWRELAY_ERROR_NONE ) {
        denyConnection( socket, err );
    }
    return success;
} /* processConnect */

static bool
processReconnect( unsigned char* bufp, int bufLen, int socket )
{
    unsigned char* end = bufp + bufLen;
    bool success = false;

    logf( XW_LOGINFO, "processReconnect" );

    unsigned char flags = *bufp++;
    XWREASON err = flagsOK( flags );
    if ( err != XWRELAY_ERROR_NONE ) {
        denyConnection( socket, err );
    } else {
        char cookie[MAX_INVITE_LEN+1];
        char connName[MAX_CONNNAME_LEN+1];
        HostID srcID;
        unsigned char nPlayersH;
        unsigned char nPlayersT;
        unsigned short gameSeed;

        connName[0] = '\0';
        if ( readStr( &bufp, end, cookie, sizeof(cookie) )
             && getNetByte( &bufp, end, &srcID )
             && getNetByte( &bufp, end, &nPlayersH )
             && getNetByte( &bufp, end, &nPlayersT )
             && getNetShort( &bufp, end, &gameSeed )
             && readStr( &bufp, end, connName, sizeof(connName) ) ) {

            SafeCref scr( cookie[0]? cookie : NULL, 
                          connName[0]? connName : NULL, 
                          srcID, socket, nPlayersH, 
                          nPlayersT, gameSeed );
            success = scr.Reconnect( socket, srcID, nPlayersH, nPlayersT, 
                                     gameSeed );
        }

        if ( !success ) {
            denyConnection( socket, XWRELAY_ERROR_BADPROTO );
        }
    }
    return success;
} /* processReconnect */

static bool
processDisconnect( unsigned char* bufp, int bufLen, int socket )
{
    unsigned char* end = bufp + bufLen;
    CookieID cookieID;
    HostID hostID;
    bool success = false;

    if ( getNetShort( &bufp, end, &cookieID ) 
         && getNetByte( &bufp, end, &hostID ) ) {

        SafeCref scr( socket );
        scr.Disconnect( socket, hostID );
        success = true;
    } else {
        logf( XW_LOGERROR, "dropping XWRELAY_GAME_DISCONNECT; wrong length" );
    }
    return success;
} /* processDisconnect */

void
killSocket( int socket, const char* why )
{
    logf( XW_LOGINFO, "killSocket(%d): %s", socket, why );
    CRefMgr::Get()->RemoveSocketRefs( socket );
    /* Might want to kill the thread it belongs to if we're not in it,
       e.g. when unable to write to another socket. */
    logf( XW_LOGINFO,  "killSocket done" );
    XWThreadPool::GetTPool()->CloseSocket( socket );
}

time_t
uptime( void ) 
{
    static time_t startTime = time(NULL);
    return time(NULL) - startTime;
}

int
GetNSpawns(void)
{
    return s_nSpawns;
}

/* forward the message.  Need only change the command after looking up the
 * socket and it's ready to go. */
static bool
forwardMessage( unsigned char* buf, int buflen, int srcSocket )
{
    bool success = false;
    unsigned char* bufp = buf + 1; /* skip cmd */
    unsigned char* end = buf + buflen;
    CookieID cookieID;
    HostID src;
    HostID dest;

    if ( getNetShort( &bufp, end, &cookieID )
         && getNetByte( &bufp, end, &src ) 
         && getNetByte( &bufp, end, &dest ) ) {
        logf( XW_LOGINFO, "cookieID = %d", cookieID );

        if ( COOKIE_ID_NONE == cookieID ) {
            SafeCref scr( srcSocket );
            success = scr.Forward( src, dest, buf, buflen );
        } else {
            SafeCref scr( cookieID ); /* won't work if not allcon; will be 0 */
            success = scr.Forward( src, dest, buf, buflen );
        }
    }
    return success;
} /* forwardMessage */

static bool
processMessage( unsigned char* buf, int bufLen, int socket )
{
    bool success = false;            /* default is failure */
    XWRELAY_Cmd cmd = *buf;

    logf( XW_LOGINFO, "%s got %s", __func__, cmdToStr(cmd) );

    switch( cmd ) {
    case XWRELAY_GAME_CONNECT: 
        success = processConnect( buf+1, bufLen-1, socket );
        break;
    case XWRELAY_GAME_RECONNECT: 
        success = processReconnect( buf+1, bufLen-1, socket );
        break;
    case XWRELAY_GAME_DISCONNECT:
        success = processDisconnect( buf+1, bufLen-1, socket );
        break;
    case XWRELAY_HEARTBEAT:
        success = processHeartbeat( buf + 1, bufLen - 1, socket );
        break;
    case XWRELAY_MSG_TORELAY:
        success = forwardMessage( buf, bufLen, socket );
        break;
    default:
        logf( XW_LOGERROR, "%s bad: %d", __func__, cmd );
        break;
        /* just drop it */
    }

    if ( !success ) {
        killSocket( socket, "couldn't forward message" );
    }

    return success;        /* caller defines non-0 as failure */
} /* processMessage */

int 
make_socket( unsigned long addr, unsigned short port )
{
    int sock = socket( AF_INET, SOCK_STREAM, 0 );
    assert( sock );

    /* We may be relaunching after crashing with sockets open.  SO_REUSEADDR
       allows them to be immediately rebound. */
    int t = true;
    if ( 0 != setsockopt( sock, SOL_SOCKET, SO_REUSEADDR, &t, sizeof(t) ) ) {
        logf( XW_LOGERROR, "setsockopt failed. errno = %s (%d)\n", 
              strerror(errno), errno );
        return -1;
    }

    struct sockaddr_in sockAddr;
    sockAddr.sin_family = AF_INET;
    sockAddr.sin_addr.s_addr = htonl(addr);
    sockAddr.sin_port = htons(port);

    int result = bind( sock, (struct sockaddr*)&sockAddr, sizeof(sockAddr) );
    if ( result != 0 ) {
        logf( XW_LOGERROR, "exiting: unable to bind port %d: %d, "
              "errno = %s (%d)\n", port, result, strerror(errno), errno );
        return -1;
    }
    logf( XW_LOGINFO, "bound socket %d on port %d", sock, port );

    result = listen( sock, 5 );
    if ( result != 0 ) {
        logf( XW_LOGERROR, "exiting: unable to listen: %d, "
              "errno = %s (%d)\n", result, strerror(errno), errno );
        return -1;
    }
    return sock;
} /* make_socket */

static void
usage( char* arg0 )
{
    fprintf( stderr, "usage: %s \\\n", arg0 );

    fprintf( stderr,
             "\t-?                   (print this help)\\\n"
             "\t-c <cport>           (localhost port for control console)\\\n"
#ifdef DO_HTTP
             "\t-w <cport>           (localhost port for web interface)\\\n"
#endif
             "\t-D                   (don't become daemon)\\\n"
             "\t-F                   (don't fork and wait to respawn child)\\\n"
             "\t-f <conffile>        (config file)\\\n"
             "\t-h                   (print this help)\\\n"
             "\t-i <idfile>          (file where next global id stored)\\\n"
             "\t-l <logfile>         (write logs here, not stderr)\\\n"
             "\t-n <serverName>      (used in permID generation)\\\n"
             "\t-p <port>            (port to listen on)\\\n"
#ifdef DO_HTTP
             "\t-s <path>            (path to css file for http iface)\\\n"
#endif
             "\t-t <nWorkerThreads>  (how many worker threads to use)\\\n"
             );
    fprintf( stderr, "svn rev. %s\n", SVN_REV );
}

/* sockets that need to be closable from interrupt handler */
ListenerMgr g_listeners;
int g_control;
#ifdef DO_HTTP
static int g_http = -1;
#endif

void
shutdown()
{
    XWThreadPool* tPool = XWThreadPool::GetTPool();
    if ( tPool != NULL ) {
        tPool->Stop();
    }

    CRefMgr* cmgr = CRefMgr::Get();
    if ( cmgr != NULL ) {
        cmgr->CloseAll();
        delete cmgr;
    }

    delete tPool;

    stop_ctrl_threads();

    g_listeners.RemoveAll();
    close( g_control );
#ifdef DO_HTTP
    close( g_http );
#endif
    exit( 0 );
    logf( XW_LOGINFO, "exit done" );
}

static void
SIGINT_handler( int sig )
{
    logf( XW_LOGERROR, "%s", __func__ );
    shutdown();
}

#ifdef SPAWN_SELF
static void
printWhy( int status ) 
{
    if ( WIFEXITED(status) ) {
        logf( XW_LOGINFO, "why: exited" );
    } else if ( WIFSIGNALED(status) ) {
        logf( XW_LOGINFO, "why: signaled; signal: %d", WTERMSIG(status) );
    } else if ( WCOREDUMP(status) ) {
        logf( XW_LOGINFO, "why: core" );
    } else if ( WIFSTOPPED(status) ) {
        logf( XW_LOGINFO, "why: traced" );
    }
} /* printWhy */
#endif

static void
parentDied( int sig )
{
    logf( XW_LOGINFO, "%s", __func__ );
    exit(0);
}

static void
handlePipe( int sig )
{
    logf( XW_LOGINFO, "%s", __func__ );
}

int
main( int argc, char** argv )
{
    int port = 0;
    int ctrlport = 0;
#ifdef DO_HTTP
    int httpport = 0;
    const char* cssFile = NULL;
#endif
    int nWorkerThreads = 0;
    char* conffile = NULL;
    const char* serverName = NULL;
    const char* idFileName = NULL;
    const char* logFile = NULL;
    bool doDaemon = true;
    bool doFork = true;

    (void)uptime();                /* force capture of start time */

    /* Verify sizes here... */
    assert( sizeof(CookieID) == 2 );
                   

    /* Read options. Options trump config file values when they conflict, but
       the name of the config file is an option so we have to get that
       first. */

    for ( ; ; ) {
       int opt = getopt(argc, argv, "h?c:p:n:i:f:l:t:"
#ifdef DO_HTTP
                        "w:s:"
#endif
                        "DF" );

       if ( opt == -1 ) {
           break;
       }
       switch( opt ) {
       case '?':
       case 'h':
           usage( argv[0] );
           exit( 0 );
       case 'c':
           ctrlport = atoi( optarg );
           break;
#ifdef DO_HTTP
       case 'w':
           httpport = atoi( optarg );
           break;
       case 's':
           cssFile = optarg;
           break;
#endif
       case 'D':
           doDaemon = false;
           break;
       case 'F':
           doFork = false;
           break;
       case 'f':
           conffile = optarg;
           break;
       case 'i':
           idFileName = optarg;
           break;
       case 'l':
           logFile = optarg;
           break;
       case 'n':
           serverName = optarg;
           break;
       case 'p':
           port = atoi( optarg );
           break;
       case 't':
           nWorkerThreads = atoi( optarg );
           break;
       default:
           usage( argv[0] );
           exit( 1 );
       }
    }

    /* Did we consume all the options passed in? */
    if ( optind != argc ) {
        usage( argv[0] );
        exit( 1 );
    }

    RelayConfigs::InitConfigs( conffile );
    RelayConfigs* cfg = RelayConfigs::GetConfigs();

    if ( NULL != logFile ) {
        cfg->SetValueFor( "LOGFILE_PATH", logFile );
    }

    if ( ctrlport == 0 ) {
        (void)cfg->GetValueFor( "CTLPORT", &ctrlport );
    }
#ifdef DO_HTTP
    if ( httpport == 0 ) {
        (void)cfg->GetValueFor( "WWW_PORT", &httpport );
    }
#endif
    if ( nWorkerThreads == 0 ) {
        (void)cfg->GetValueFor( "NTHREADS", &nWorkerThreads );
    }
    char serverNameBuf[128];
    if ( serverName == NULL ) {
        if ( cfg->GetValueFor( "SERVERNAME", serverNameBuf, 
                               sizeof(serverNameBuf) ) ) {
            serverName = serverNameBuf;
        }
    }

#ifdef DO_HTTP
    /* http module uses this */
    if ( !!cssFile ) {
        cfg->SetValueFor( "WWW_CSS_PATH", cssFile );
    }
#endif

    PermID::SetServerName( serverName );
    /* add signal handling here */

    /*
      The daemon() function is for programs wishing to detach themselves from
      the controlling terminal and run in the background as system daemons.
 
      Unless the argument nochdir is non-zero, daemon() changes the current
      working directory to the root ("/").
 
      Unless the argument noclose is non-zero, daemon() will redirect standard
      input, standard output and standard error to /dev/null.

      (This function forks, and if the fork() succeeds, the parent does
      _exit(0), so that further errors are seen by the child only.)  On
      success zero will be returned.  If an error occurs, daemon() returns -1
      and sets the global variable errno to any of the errors specified for
      the library functions fork(2) and setsid(2).
    */
    if ( doDaemon ) {
        if ( 0 != daemon( true, false ) ) {
            logf( XW_LOGERROR, "dev() => %s", strerror(errno) );
            exit( -1 );
        }
    }

#ifdef SPAWN_SELF
    /* loop forever, relaunching children as they die. */
    while ( doFork ) {
        ++s_nSpawns;             /* increment in parent *before* copy */
        pid_t pid = fork();
        if ( pid == 0 ) {       /* child */
            break;
        } else if ( pid > 0 ) {
            int status;
            logf( XW_LOGINFO, "parent waiting on child pid=%d", pid );
            time_t time_before = time( NULL );
            waitpid( pid, &status, 0 );
            printWhy( status );
            time_t time_after = time( NULL );
            doFork = time_after > time_before;
            if ( !doFork ) {
                logf( XW_LOGERROR, "exiting b/c respawned too quickly" );
            }
        } else {
            logf( XW_LOGERROR, "fork() => %s", strerror(errno) );
        }
    }
#endif

    /* Needs to be reset after a crash/respawn */
    PermID::SetStartTime( time(NULL) );

    logf( XW_LOGERROR, "***** forked %dth new process *****", s_nSpawns );

    /* Arrange to be sent SIGUSR1 on death of parent. */
    prctl( PR_SET_PDEATHSIG, SIGUSR1 );

    struct sigaction sact;
    memset( &sact, 0, sizeof(sact) );
    sact.sa_handler = parentDied;
    (void)sigaction( SIGUSR1, &sact, NULL );

    memset( &sact, 0, sizeof(sact) );
    sact.sa_handler = handlePipe;
    (void)sigaction( SIGPIPE, &sact, NULL );

    if ( port != 0 ) {
        g_listeners.AddListener( port );
    }
    vector<int> ints;
    if ( !cfg->GetValueFor( "PORTS", ints ) ) {
        exit( 1 );
    }

    vector<int>::const_iterator iter;
    for ( iter = ints.begin(); iter != ints.end(); ++iter ) {
        int port = *iter;
        if ( !g_listeners.PortInUse( port ) ) {
            if ( !g_listeners.AddListener( port ) ) {
                exit( 1 );
            }
        } else {
            logf( XW_LOGERROR, "port %d was in use", port );
        }
    }

    g_control = make_socket( INADDR_LOOPBACK, ctrlport );
    if ( g_control == -1 ) {
        exit( 1 );
    }

#ifdef DO_HTTP
    HttpState http_state;
    int addr;

    memset( &http_state, 0, sizeof(http_state) );
    if ( cfg->GetValueFor( "WWW_SAMPLE_INTERVAL", 
                           &http_state.m_sampleInterval )
         && cfg->GetValueFor( "WWW_LISTEN_ADDR", &addr ) ) {
        g_http = make_socket( addr, httpport );
        if ( g_http == -1 ) {
            exit( 1 );
        }
        http_state.ctrl_sock = g_http;
    }
    if ( -1 != g_http ) {
        pthread_mutex_init( &http_state.m_dataMutex, NULL );
    }
#endif

    struct sigaction act;
    memset( &act, 0, sizeof(act) );
    act.sa_handler = SIGINT_handler;
    (void)sigaction( SIGINT, &act, NULL );

    XWThreadPool* tPool = XWThreadPool::GetTPool();
    tPool->Setup( nWorkerThreads, processMessage );

    /* set up select call */
    fd_set rfds;
    for ( ; ; ) {
        FD_ZERO(&rfds);
        g_listeners.AddToFDSet( &rfds );
        FD_SET( g_control, &rfds );
#ifdef DO_HTTP
        if ( -1 != g_http ) {
            FD_SET( g_http, &rfds );
        }
#endif
        int highest = g_listeners.GetHighest();
        if ( g_control > highest ) {
            highest = g_control;
        }
#ifdef DO_HTTP
        if ( g_http > highest ) {
            highest = g_http;
        }
#endif
        ++highest;

        int retval = select( highest, &rfds, NULL, NULL, NULL );
        if ( retval < 0 ) {
            if ( errno != 4 ) { /* 4's what we get when signal interrupts */
                logf( XW_LOGINFO, "errno: %s (%d)", strerror(errno), errno );
            }
        } else {
            ListenersIter iter(&g_listeners, true);
            while ( retval > 0 ) {
                int listener = iter.next();
                if ( listener < 0 ) {
                    break;
                }

                if ( FD_ISSET( listener, &rfds ) ) {
                    struct sockaddr_in newaddr;
                    socklen_t siz = sizeof(newaddr);
                    int newSock = accept( listener, (sockaddr*)&newaddr,
                                          &siz );

                    logf( XW_LOGINFO, "accepting connection from %s", 
                          inet_ntoa(newaddr.sin_addr) );

                    tPool->AddSocket( newSock );
                    --retval;
                }
            }
            if ( FD_ISSET( g_control, &rfds ) ) {
                run_ctrl_thread( g_control );
                --retval;
            }
#ifdef DO_HTTP
            if ( FD_ISSET( g_http, &rfds ) ) {
                run_http_thread( &http_state );
                --retval;
            }
#endif
            assert( retval == 0 );
        }
    }

    g_listeners.RemoveAll();
    close( g_control );

    delete cfg;

    return 0;
} // main
