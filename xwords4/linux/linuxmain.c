/* -*-mode: C; fill-column: 78; c-basic-offset: 4; compile-command: "make MEMDEBUG=TRUE"; -*- */
/* 
 * Copyright 2000-2008 by Eric House (xwords@eehouse.org).  All rights
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
#include <sys/time.h>
#include <sys/stat.h>

#include <netdb.h>		/* gethostbyname */
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <syslog.h>
#include <stdarg.h>

#ifdef XWFEATURE_BLUETOOTH
# include <bluetooth/bluetooth.h>
# include <bluetooth/hci.h>
# include <bluetooth/hci_lib.h>
#endif

/* #include <pthread.h> */

#include "linuxmain.h"
#include "linuxutl.h"
#include "linuxbt.h"
#include "linuxsms.h"
#include "linuxudp.h"
#include "main.h"
#ifdef PLATFORM_NCURSES
# include "cursesmain.h"
#endif
#ifdef PLATFORM_GTK
# include "gtkmain.h"
#endif
#include "model.h"
#include "util.h"
#include "strutils.h"
/* #include "commgr.h" */
/* #include "compipe.h" */
#include "memstream.h"
#include "LocalizedStrIncludes.h"

#define DEFAULT_PORT 10999
#define DEFAULT_LISTEN_PORT 4998

XP_Bool
file_exists( const char* fileName ) 
{
    struct stat statBuf;

    int statResult = stat( fileName, &statBuf );
    return statResult == 0;
} /* file_exists */

XWStreamCtxt*
streamFromFile( CommonGlobals* cGlobals, char* name, void* closure )
{
    XP_U8* buf;
    struct stat statBuf;
    FILE* f;
    XWStreamCtxt* stream;

    (void)stat( name, &statBuf );
    buf = malloc( statBuf.st_size );
    f = fopen( name, "r" );
    fread( buf, statBuf.st_size, 1, f );
    fclose( f );

    stream = mem_stream_make( MPPARM(cGlobals->params->util->mpool)
                              cGlobals->params->vtMgr, 
                              closure, CHANNEL_NONE, NULL );
    stream_putBytes( stream, buf, statBuf.st_size );
    free( buf );

    return stream;
} /* streamFromFile */

void
writeToFile( XWStreamCtxt* stream, void* closure )
{
    void* buf;
    FILE* file;
    XP_U16 len;
    CommonGlobals* cGlobals = (CommonGlobals*)closure;

    len = stream_getSize( stream );
    buf = malloc( len );
    stream_getBytes( stream, buf, len );

    file = fopen( cGlobals->params->fileName, "w" );
    fwrite( buf, 1, len, file );
    fclose( file );

    free( buf );
} /* writeToFile */

void
catOnClose( XWStreamCtxt* stream, void* XP_UNUSED(closure) )
{
    XP_U16 nBytes;
    char* buffer;

    XP_LOGF( "catOnClose" );

    nBytes = stream_getSize( stream );
    buffer = malloc( nBytes + 1 );
    stream_getBytes( stream, buffer, nBytes );
    buffer[nBytes] = '\0';

    fprintf( stderr, "%s", buffer );

    free( buffer );
} /* catOnClose */

void
catGameHistory( CommonGlobals* cGlobals )
{
    if ( !!cGlobals->game.model ) {
        XP_Bool gameOver = server_getGameIsOver( cGlobals->game.server );
        XWStreamCtxt* stream = 
            mem_stream_make( MPPARM(cGlobals->params->util->mpool)
                             cGlobals->params->vtMgr,
                             NULL, CHANNEL_NONE, catOnClose );
        model_writeGameHistory( cGlobals->game.model, stream, 
                                cGlobals->game.server, gameOver );
        stream_putU8( stream, '\n' );
        stream_destroy( stream );
    }
} /* catGameHistory */

XP_UCHAR*
strFromStream( XWStreamCtxt* stream )
{
    XP_U16 len = stream_getSize( stream );
    XP_UCHAR* buf = (XP_UCHAR*)malloc( len + 1 );
    stream_getBytes( stream, buf, len );
    buf[len] = '\0';

    return buf;
} /* strFromStream */


static void
usage( char* appName, char* msg )
{
    if ( msg != NULL ) {
        fprintf( stderr, "Error: %s\n\n", msg );
    }
    fprintf( stderr, "usage: %s \n"
#if defined PLATFORM_GTK && defined PLATFORM_NCURSES
	     "\t [-g]             # gtk (default)\n"
	     "\t [-u]             # ncurses (for dumb terminal)\n"
#endif
#if defined PLATFORM_GTK
	     "\t [-k]             # ask for parameters via \"new games\" dlg\n"
	     "\t [-h numRowsHidded] \n"
# ifdef XWFEATURE_SEARCHLIMIT
	     "\t [-I]             # don't support hint rect dragging\n"
# endif
#endif
	     "\t [-f file]        # use this file to save/load game\n"
	     "\t [-q nSecs]       # quit with pause when game over (useful for robot-only)\n"
	     "\t [-S]             # slow robot down \n"
	     "\t [-i]             # print game history when game over\n"
	     "\t [-U]             # call 'Undo' after game ends\n"
#ifdef XWFEATURE_RELAY
	     "\t [-H]             # Don't send heartbeats to relay\n"
#endif
#ifdef XWFEATURE_SMS
	     "\t [-M phone]       # Server phone number for SMS\n"
#endif
	     "\t [-r name]*       # same-process robot\n"
	     "\t [-n name]*       # same-process player (no network used)\n"
	     "\t [-w pwd]*        # passwd for matching local player\n"
	     "\t [-v]             # put scoreboard in vertical mode\n"
	     "\t [-m]             # make the robot duMb (smart is default)\n"
	     "\t [-l]             # disallow hints\n"
	     "\t [-c]             # explain robot scores after each move\n"
	     "\t [-C COOKIE]      # cookie used to groups games on relay\n"
	     "\t\t # (max of four players total, local and remote)\n"
	     "\t [-b boardSize]   # number of columns and rows\n"
	     "\t [-e random_seed] \n"
	     "\t [-t initial_minutes_on_timer] \n"
	     "# --------------- choose client or server ----------\n"
	     "\t -s               # be the server\n"
	     "\t -d xwd_file      # provides tile counts & values\n"
	     "\t\t # list each player as local or remote\n"
	     "\t [-N]*            # remote client (listen for connection)\n"
#ifdef XWFEATURE_RELAY
	     "\t [-p relay_port]  # relay is at this port\n"
	     "\t [-a relay_addr]  # use relay (via port spec'd above)\n"
	     ""                   " (default localhost)\n"
#endif
#ifdef XWFEATURE_BLUETOOTH
	     "\t [-B n:name|a:00:11:22:33:44:55]\n"
             "\t\t\t# connect via bluetooth [param ignored if -s]\n"
#endif
#ifdef XWFEATURE_IP_DIRECT
         "\t [-D host_addr]\t\t\tConnect directly to host [param ignored if -s]\n"
	     "\t [-p host_port]  # put/look for host on this port\n"
#endif

/* 	     "# --------------- OR client-only ----------\n" */
/* 	     "\t [-p client_port] # must != server's port if on same device" */
#ifdef XWFEATURE_RELAY
         "\nrelay example: \n"
             "\t host: ./xwords -d dict.xwd -r Eric -s -N -a localhost -p 10999 -C COOKIE\n"
             "\tguest: ./xwords -d dict.xwd -r Kati -a localhost -p 10999 -C COOKIE"
#endif
#ifdef XWFEATURE_BLUETOOTH
         "\nBluetooth example: \n"
             "\t host: ./xwords -d dict.xwd -r Eric -s -N -B ignored\n"
             "\tguest: ./xwords -d dict.xwd -r Kati -B n:treo_bt_name (OR b:11:22:33:44:55:66)"
#endif
#ifdef XWFEATURE_IP_DIRECT
         "\nDirect example: \n"
             "\t host: ./xwords -d dict.xwd -r Eric -s -N -N -D localhost -p 10999\n"
             "\tguest: ./xwords -d dict.xwd -r Kati -D localhost -p 10999\n"
             "\tguest: ./xwords -d dict.xwd -r Ariynn -D localhost -p 10999"
#endif

             "\n"
	     , appName );
    fprintf( stderr, "\n(revision: %s)\n", SVN_REV);
    exit(1);
} /* usage */

#ifdef KEYBOARD_NAV
XP_Bool
linShiftFocus( CommonGlobals* cGlobals, XP_Key key, const BoardObjectType* order,
               BoardObjectType* nxtP )
{
    BoardCtxt* board = cGlobals->game.board;
    XP_Bool handled = XP_FALSE;
    BoardObjectType nxt = OBJ_NONE;
    BoardObjectType cur;
    XP_U16 i, curIndex = 0;

    cur = board_getFocusOwner( board );
    if ( cur == OBJ_NONE ) {
        cur = order[0];
    }
    for ( i = 0; i < 3; ++i ) {
        if ( cur == order[i] ) {
            curIndex = i;
            break;
        }
    }
    XP_ASSERT( curIndex < 3 );

    curIndex += 3;
    if ( key == XP_CURSOR_KEY_DOWN || key == XP_CURSOR_KEY_RIGHT ) {
        ++curIndex;
    } else if ( key == XP_CURSOR_KEY_UP || key == XP_CURSOR_KEY_LEFT ) {
        --curIndex;
    } else {
        XP_ASSERT(0);
    }
    curIndex %= 3;

    nxt = order[curIndex];
    handled = board_focusChanged( board, nxt, XP_TRUE );

    if ( !!nxtP ) {
        *nxtP = nxt;
    }

    return handled;
} /* linShiftFocus */
#endif

#ifndef XWFEATURE_STANDALONE_ONLY
#ifdef XWFEATURE_RELAY
static int
linux_init_relay_socket( CommonGlobals* cGlobals )
{
    struct sockaddr_in to_sock;
    struct hostent* host;
    int sock = cGlobals->socket;
    if ( sock == -1 ) {

        /* make a local copy of the address to send to */
        sock = socket( AF_INET, SOCK_STREAM, 0 );
        if ( sock == -1 ) {
            XP_DEBUGF( "socket returned -1\n" );
            return -1;
        }

        to_sock.sin_port = htons(cGlobals->params->
                                 connInfo.relay.defaultSendPort );
        XP_STATUSF( "1: sending to port %d", 
                    cGlobals->params->connInfo.relay.defaultSendPort );
        host = gethostbyname( cGlobals->params->connInfo.relay.relayName );
        if ( NULL == host ) {
            XP_WARNF( "gethostbyname returned -1\n" );
            return -1;
        } else {
            XP_WARNF( "gethostbyname for %s worked", 
                      cGlobals->defaultServerName );
        }
        memcpy( &(to_sock.sin_addr.s_addr), host->h_addr_list[0],  
                sizeof(struct in_addr));
        to_sock.sin_family = AF_INET;

        errno = 0;
        if ( 0 == connect( sock, (const struct sockaddr*)&to_sock, 
                           sizeof(to_sock) ) ) {
            cGlobals->socket = sock;
        } else {
            close( sock );
            sock = -1;
            XP_STATUSF( "%s: connect failed: %s (%d)", __func__, strerror(errno), errno );
        }
    }
   
    return sock;
} /* linux_init_relay_socket */

static XP_S16
linux_tcp_send( const XP_U8* buf, XP_U16 buflen, 
                const CommsAddrRec* XP_UNUSED(addrRec), 
                CommonGlobals* globals )
{
    XP_S16 result = 0;
    int socket = globals->socket;
    
    if ( socket == -1 ) {
        XP_STATUSF( "%s: socket uninitialized", __func__ );
        socket = linux_init_relay_socket( globals );
        if ( socket != -1 ) {
            assert( globals->socket == socket );
            (*globals->socketChanged)( globals->socketChangedClosure, 
                                       -1, socket,
                                       &globals->storage );
        }
    }

    if ( socket != -1 ) {
        XP_U16 netLen = htons( buflen );
        errno = 0;

        result = send( socket, &netLen, sizeof(netLen), 0 );
        if ( result == sizeof(netLen) ) {
            result = send( socket, buf, buflen, 0 ); 
        }
        if ( result <= 0 ) {
            XP_STATUSF( "closing non-functional socket" );
            close( socket );
            (*globals->socketChanged)( globals->socketChangedClosure, 
                                       socket, -1, &globals->storage );
            globals->socket = -1;
        }

        XP_STATUSF( "linux_tcp_send: send returned %d of %d (err=%d)", 
                    result, buflen, errno );
    }
 
    return result;
} /* linux_tcp_send */
#endif  /* XWFEATURE_RELAY */

#ifdef XWFEATURE_RELAY
static void
linux_tcp_reset( CommonGlobals* globals )
{
    LOG_FUNC();
    if ( globals->socket != -1 ) {
        (void)close( globals->socket );
        globals->socket = -1;
    }
}
#endif

#ifdef COMMS_HEARTBEAT
void
linux_reset( void* closure )
{
    CommonGlobals* globals = (CommonGlobals*)closure;
    CommsConnType conType = globals->params->conType;
    if ( 0 ) {
#ifdef XWFEATURE_BLUETOOTH
    } else if ( conType == COMMS_CONN_BT ) {
        linux_bt_reset( globals );
#endif
#ifdef XWFEATURE_IP_DIRECT
    } else if ( conType == COMMS_CONN_IP_DIRECT ) {
        linux_udp_reset( globals );
#endif
#ifdef XWFEATURE_RELAY
    } else if ( conType == COMMS_CONN_RELAY ) {
        linux_tcp_reset( globals );
#endif
    }

}
#endif

XP_S16
linux_send( const XP_U8* buf, XP_U16 buflen, 
            const CommsAddrRec* addrRec, 
            void* closure )
{
    XP_S16 nSent = -1;
    CommonGlobals* globals = (CommonGlobals*)closure;   
    CommsConnType conType;

    if ( !!addrRec ) {
        conType = addrRec->conType;
    } else {
        conType = globals->params->conType;
    }

    if ( 0 ) {
#ifdef XWFEATURE_RELAY
    } else if ( conType == COMMS_CONN_RELAY ) {
        nSent = linux_tcp_send( buf, buflen, addrRec, globals );
#endif
#if defined XWFEATURE_BLUETOOTH
    } else if ( conType == COMMS_CONN_BT ) {
        XP_Bool isServer = comms_getIsServer( globals->game.comms );
        linux_bt_open( globals, isServer );
        nSent = linux_bt_send( buf, buflen, addrRec, globals );
#endif
#if defined XWFEATURE_IP_DIRECT
    } else if ( conType == COMMS_CONN_IP_DIRECT ) {
        CommsAddrRec addr;
        comms_getAddr( globals->game.comms, &addr );
        linux_udp_open( globals, &addr );
        nSent = linux_udp_send( buf, buflen, addrRec, globals );
#endif
#if defined XWFEATURE_SMS
    } else if ( COMMS_CONN_SMS == conType ) {
        CommsAddrRec addr;
        if ( !addrRec ) {
            comms_getAddr( globals->game.comms, &addr );
            addrRec = &addr;
        }
        nSent = linux_sms_send( globals, buf, buflen, 
                                addrRec->u.sms.phone, addrRec->u.sms.port );
#endif
    } else {
        XP_ASSERT(0);
    }
    return nSent;
} /* linux_send */

#ifdef XWFEATURE_RELAY
static void
linux_close_socket( CommonGlobals* cGlobals )
{
    int socket = cGlobals->socket;
    cGlobals->socket = -1;

    XP_LOGF( "linux_close_socket" );
    close( socket );
}

int
linux_relay_receive( CommonGlobals* cGlobals, unsigned char* buf, int bufSize )
{
    int sock = cGlobals->socket;
    unsigned short tmp;
    unsigned short packetSize;
    ssize_t nRead = recv( sock, &tmp, sizeof(tmp), 0 );
    if ( nRead != 2 ) {
        XP_LOGF( "recv => %d, errno=%d", nRead, errno );
        linux_close_socket( cGlobals );
        nRead = -1;
    } else {

        packetSize = ntohs( tmp );
        assert( packetSize <= bufSize );
        nRead = recv( sock, buf, packetSize, 0 );
        if ( nRead < 0 ) {
            XP_WARNF( "linuxReceive: errno=%d\n", errno );
        }
    }
    return nRead;
} /* linuxReceive */
#endif  /* XWFEATURE_RELAY */
#endif  /* XWFEATURE_STANDALONE_ONLY */

/* Create a stream for the incoming message buffer, and read in any
   information specific to our platform's comms layer (return address, say)
 */
XWStreamCtxt*
stream_from_msgbuf( CommonGlobals* globals, unsigned char* bufPtr, 
                    XP_U16 nBytes )
{
    XWStreamCtxt* result;
    result = mem_stream_make( MPPARM(globals->params->util->mpool)
                              globals->params->vtMgr,
                              globals, CHANNEL_NONE, NULL );
    stream_putBytes( result, bufPtr, nBytes );

    return result;
} /* stream_from_msgbuf */

#if 0
static void
streamClosed( XWStreamCtxt* stream, XP_PlayerAddr addr, void* closure )
{
    fprintf( stderr, "streamClosed called\n" );
} /* streamClosed */

static XWStreamCtxt*
linux_util_makeStreamFromAddr( XW_UtilCtxt* uctx, XP_U16 channelNo )
{
#if 1
/*     XWStreamCtxt* stream = linux_mem_stream_make( uctx->closure, channelNo,  */
/* 						  sendOnClose, NULL ); */
#else
    struct sockaddr* returnAddr = (struct sockaddr*)addr;
    int newSocket;
    int result;

    newSocket = socket( AF_INET, DGRAM_TYPE, 0 );
    fprintf( stderr, "linux_util_makeStreamFromAddr: made socket %d\n",
	     newSocket );
    /* #define	EADDRINUSE 98 */
    result = bind( newSocket, (struct sockaddr*)returnAddr, addrLen );
    fprintf( stderr, "bind returned %d; errno=%d\n", result, errno );

    return linux_make_socketStream( newSocket );
#endif
} /* linux_util_makeStreamFromAddr */
#endif

void
linuxFireTimer( CommonGlobals* cGlobals, XWTimerReason why )
{
    XWTimerProc proc = cGlobals->timerProcs[why];
    void* closure = cGlobals->timerClosures[why];

    cGlobals->timerProcs[why] = NULL;

    (*proc)( closure, why );
} /* fireTimer */

#ifndef XWFEATURE_STANDALONE_ONLY
static void
linux_util_addrChange( XW_UtilCtxt* uc, 
                       const CommsAddrRec* XP_UNUSED(oldAddr),
                       const CommsAddrRec* newAddr )
{
    CommonGlobals* cGlobals = (CommonGlobals*)uc->closure;
    if ( 0 ) {
#ifdef XWFEATURE_BLUETOOTH
    } else if ( newAddr->conType == COMMS_CONN_BT ) {
        XP_Bool isServer = comms_getIsServer( cGlobals->game.comms );
        linux_bt_open( cGlobals, isServer );
#endif
#if defined XWFEATURE_IP_DIRECT
    } else if ( newAddr->conType == COMMS_CONN_IP_DIRECT ) {
        linux_udp_open( cGlobals, newAddr );
#endif
#if defined XWFEATURE_SMS
    } else if ( COMMS_CONN_SMS == newAddr->conType ) {
        linux_sms_init( cGlobals, newAddr );
#endif
    }
}
#endif

static unsigned int
defaultRandomSeed()
{
    /* use kernel device rather than time() so can run multiple times/second
       without getting the same results. */
    unsigned int rs;
    FILE* rfile = fopen( "/dev/urandom", "ro" );
    fread( &rs, sizeof(rs), 1, rfile );
    fclose( rfile );
    return rs;
} /* defaultRandomSeed */

/* This belongs in linuxbt.c */
#ifdef XWFEATURE_BLUETOOTH
static XP_Bool
nameToBtAddr( const char* name, bdaddr_t* ba )
{
    XP_Bool success = XP_FALSE;
    int id, socket;
    LOG_FUNC();
# define RESPMAX 5

    id = hci_get_route( NULL );
    socket = hci_open_dev( id );
    if ( id >= 0 && socket >= 0 ) {
        long flags = 0L;
        inquiry_info inqInfo[RESPMAX];
        inquiry_info* inqInfoP = inqInfo;
        int count = hci_inquiry( id, 10, RESPMAX, NULL, &inqInfoP, flags );
        int i;

        for ( i = 0; i < count; ++i ) {
            char buf[64];
            if ( 0 >= hci_read_remote_name( socket, &inqInfo[i].bdaddr, 
                                            sizeof(buf), buf, 0)) {
                if ( 0 == strcmp( buf, name ) ) {
                    XP_MEMCPY( ba, &inqInfo[i].bdaddr, sizeof(*ba) );
                    success = XP_TRUE;
                    XP_LOGF( "%s: matched %s", __func__, name );
                    char addrStr[32];
                    ba2str(ba, addrStr);
                    XP_LOGF( "bt_addr is %s", addrStr );
                    break;
                }
            }
        }
    }
    return success;
} /* nameToBtAddr */
#endif

int
main( int argc, char** argv )
{
    XP_Bool useGTK, useCurses;
    int opt;
    int totalPlayerCount = 0;
    XP_Bool isServer = XP_FALSE;
    char* portNum = NULL;
    char* hostName = "localhost";
    char* serverPhone = NULL;         /* sms */
    unsigned int seed = defaultRandomSeed();
    LaunchParams mainParams;
    XP_U16 robotCount = 0;
    
    CommsConnType conType = COMMS_CONN_NONE;
#ifdef XWFEATURE_BLUETOOTH
    const char* btaddr = NULL;
#endif

    XP_LOGF( "main started: pid = %d", getpid() );
#ifdef DEBUG
    syslog( LOG_DEBUG, "main started: pid = %d", getpid() );
#endif

#ifdef DEBUG
    {
        int i;
        for ( i = 0; i < argc; ++i ) {
            XP_LOGF( "%s", argv[i] );
        }
    }
#endif

    memset( &mainParams, 0, sizeof(mainParams) );

    mainParams.util = malloc( sizeof(*mainParams.util) );
    XP_MEMSET( mainParams.util, 0, sizeof(*mainParams.util) );

#ifdef MEM_DEBUG
    mainParams.util->mpool = mpool_make();
#endif

    mainParams.vtMgr = make_vtablemgr(MPPARM_NOCOMMA(mainParams.util->mpool));

    /*     fprintf( stdout, "press <RET> to start\n" ); */
    /*     (void)fgetc( stdin ); */

    /* defaults */
#ifdef XWFEATURE_RELAY
    mainParams.connInfo.relay.defaultSendPort = DEFAULT_PORT;
    mainParams.connInfo.relay.cookie = "COOKIE";
#endif
#ifdef XWFEATURE_IP_DIRECT
    mainParams.connInfo.ip.port = DEFAULT_PORT;
    mainParams.connInfo.ip.hostName = "localhost";
#endif
    mainParams.gi.boardSize = 15;
    mainParams.quitAfter = -1;
    mainParams.sleepOnAnchor = XP_FALSE;
    mainParams.printHistory = XP_FALSE;
    mainParams.undoWhenDone = XP_FALSE;
    mainParams.gi.timerEnabled = XP_FALSE;
    mainParams.gi.robotSmartness = SMART_ROBOT;
    mainParams.noHeartbeat = XP_FALSE;
    mainParams.nHidden = 0;
#ifdef XWFEATURE_SEARCHLIMIT
    mainParams.allowHintRect = XP_TRUE;
#endif
    
    /*     serverName = mainParams.info.clientInfo.serverName = "localhost"; */

#if defined PLATFORM_GTK
    useGTK = 1;
    useCurses = 0;
#else  /* curses is the default if GTK isn't available */
    useGTK = 0;
    useCurses = 1;
#endif



    do {
        short index;
        opt = getopt( argc, argv, "?"
#if defined PLATFORM_GTK && defined PLATFORM_NCURSES
                      "gu"
#endif
#if defined PLATFORM_GTK
                      "h:I"
#endif
                      "kKf:ln:Nsd:e:r:b:q:w:Sit:Umvc"
#ifdef XWFEATURE_SMS
                      "M:"
#endif
#ifdef XWFEATURE_RELAY
                      "a:p:C:H"
#endif
#if defined XWFEATURE_RELAY || defined XWFEATURE_IP_DIRECT
                      "p:"
#endif
#ifdef XWFEATURE_BLUETOOTH
                      "B:" 
#endif
#ifdef XWFEATURE_IP_DIRECT
                      "D:" 
#endif
                      );
        switch( opt ) {
        case '?':
            usage(argv[0], NULL);
            break;
        case 'c':
            mainParams.showRobotScores = XP_TRUE;
            break;
#ifdef XWFEATURE_RELAY
        case 'C':
            XP_ASSERT( conType == COMMS_CONN_NONE ||
                       conType == COMMS_CONN_RELAY );
            mainParams.connInfo.relay.cookie = optarg;
            conType = COMMS_CONN_RELAY;
            break;
#endif
        case 'D':
            XP_ASSERT( conType == COMMS_CONN_NONE ||
                       conType == COMMS_CONN_IP_DIRECT );
            hostName = optarg;
            conType = COMMS_CONN_IP_DIRECT;
            break;
        case 'd':
            mainParams.gi.dictName = copyString( mainParams.util->mpool,
                                                 (XP_UCHAR*)optarg );
            break;
        case 'e':
            seed = atoi(optarg);
            break;
        case 'f':
            mainParams.fileName = optarg;
            break;
        case 'i':
            mainParams.printHistory = 1;
            break;
#ifdef XWFEATURE_SEARCHLIMIT
        case 'I':
            mainParams.allowHintRect = XP_FALSE;
            break;
#endif
        case 'K':
            mainParams.skipWarnings = 1;
            break;
        case 'w':
            mainParams.gi.players[mainParams.nLocalPlayers-1].password
                = (XP_UCHAR*)optarg;
            break;
#ifdef XWFEATURE_SMS
        case 'M':		/* SMS phone number */
            XP_ASSERT( COMMS_CONN_NONE == conType );
            serverPhone = optarg;
            conType = COMMS_CONN_SMS;
            break;
#endif
        case 'm':		/* dumb robot */
            mainParams.gi.robotSmartness = DUMB_ROBOT;
            break;
        case 'l':
            mainParams.gi.hintsNotAllowed = XP_TRUE;
            break;
        case 'n':
            index = mainParams.gi.nPlayers++;
            ++mainParams.nLocalPlayers;
            mainParams.gi.players[index].isRobot = XP_FALSE;
            mainParams.gi.players[index].isLocal = XP_TRUE;
            mainParams.gi.players[index].name = 
                copyString( mainParams.util->mpool, (XP_UCHAR*)optarg);
            break;
        case 'N':
            index = mainParams.gi.nPlayers++;
            mainParams.gi.players[index].isLocal = XP_FALSE;
            ++mainParams.info.serverInfo.nRemotePlayers;
            break;
        case 'p':
            /* could be RELAY or IP_DIRECT or SMS */
            portNum = optarg;
            break;
        case 'r':
            ++robotCount;
            index = mainParams.gi.nPlayers++;
            ++mainParams.nLocalPlayers;
            mainParams.gi.players[index].isRobot = XP_TRUE;
            mainParams.gi.players[index].isLocal = XP_TRUE;
            mainParams.gi.players[index].name = 
                copyString( mainParams.util->mpool, (XP_UCHAR*)optarg);
            break;
        case 's':
            isServer = XP_TRUE;
            break;
        case 'S':
            mainParams.sleepOnAnchor = XP_TRUE;
            break;
        case 't':
            mainParams.gi.gameSeconds = atoi(optarg) * 60;
            mainParams.gi.timerEnabled = XP_TRUE;
            break;
        case 'U':
            mainParams.undoWhenDone = XP_TRUE;
            break;
        case 'H':
            mainParams.noHeartbeat = XP_TRUE;
            break;
        case 'a':
            /* mainParams.info.clientInfo.serverName =  */
            XP_ASSERT( conType == COMMS_CONN_NONE ||
                       conType == COMMS_CONN_RELAY );
            conType = COMMS_CONN_RELAY;
            hostName = optarg;
            break;
        case 'q':
            mainParams.quitAfter = atoi(optarg);
            break;
        case 'b':
            mainParams.gi.boardSize = atoi(optarg);
            break;
#ifdef XWFEATURE_BLUETOOTH
        case 'B':
            XP_ASSERT( conType == COMMS_CONN_NONE ||
                       conType == COMMS_CONN_BT );
            conType = COMMS_CONN_BT;
            btaddr = optarg;
            break;
#endif
        case 'v':
            mainParams.verticalScore = XP_TRUE;
            break;
#if defined PLATFORM_GTK && defined PLATFORM_NCURSES
        case 'g':
            useGTK = 1;
            break;
        case 'u':
            useCurses = 1;
            useGTK = 0;
            break;
#endif
#if defined PLATFORM_GTK
        case 'k':
            mainParams.askNewGame = XP_TRUE;
            break;
        case 'h':
            mainParams.nHidden = atoi(optarg);
            break;
#endif
        }
    } while ( opt != -1 );

    XP_ASSERT( mainParams.gi.nPlayers == mainParams.nLocalPlayers
               + mainParams.info.serverInfo.nRemotePlayers );

    if ( isServer ) {
        if ( mainParams.info.serverInfo.nRemotePlayers == 0 ) {
            mainParams.gi.serverRole = SERVER_STANDALONE;
        } else {
            mainParams.gi.serverRole = SERVER_ISSERVER;
        }
    } else {
        mainParams.gi.serverRole = SERVER_ISCLIENT;
    }

    /* sanity checks */
    totalPlayerCount = mainParams.nLocalPlayers 
        + mainParams.info.serverInfo.nRemotePlayers;
    if ( !mainParams.fileName ) {
        if ( (totalPlayerCount < 1) || 
             (totalPlayerCount > MAX_NUM_PLAYERS) ) {
            usage( argv[0], "Need between 1 and 4 players" );
        }
    }

    if ( !!mainParams.gi.dictName ) {
        mainParams.dict = linux_dictionary_make( 
            MPPARM(mainParams.util->mpool) mainParams.gi.dictName );
        XP_ASSERT( !!mainParams.dict );
    } else if ( isServer ) {
#ifdef STUBBED_DICT
        mainParams.dict = make_stubbed_dict( 
            MPPARM_NOCOMMA(mainParams.util->mpool) );
        XP_WARNF( "no dictionary provided: using English stub dict\n" );
#else
        usage( argv[0], "Server needs a dictionary" );
#endif
    } else if ( robotCount > 0 ) {
        usage( argv[0], "Client can't have robots without a dictionary" );
    }

    if ( !isServer ) {
        if ( mainParams.info.serverInfo.nRemotePlayers > 0 ) {
            usage( argv[0], "Client can't have remote players" );
        }	    
    }

    if ( 0 ) {
#ifdef XWFEATURE_RELAY
    } else if ( conType == COMMS_CONN_RELAY ) {
        mainParams.connInfo.relay.relayName = hostName;
        if ( NULL == portNum ) {
            portNum = "10999";
        }
        mainParams.connInfo.relay.defaultSendPort = atoi( portNum );
#endif
#ifdef XWFEATURE_IP_DIRECT
    } else if ( conType == COMMS_CONN_IP_DIRECT ) {
        mainParams.connInfo.ip.hostName = hostName;
        if ( NULL == portNum ) {
            portNum = "10999";
        }
        mainParams.connInfo.ip.port = atoi( portNum );
#endif
#ifdef XWFEATURE_SMS
    } else if ( conType == COMMS_CONN_SMS ) {
        mainParams.connInfo.sms.serverPhone = serverPhone;
        if ( !portNum ) {
            portNum = "1";
        }
        mainParams.connInfo.sms.port = atoi(portNum);
#endif
#ifdef XWFEATURE_BLUETOOTH
    } else if ( conType == COMMS_CONN_BT ) {
        bdaddr_t ba;
        XP_Bool success;
        XP_ASSERT( btaddr );
        if ( isServer ) {
            success = XP_TRUE;
            /* any format is ok */
        } else if ( btaddr[1] == ':' ) {
            success = XP_FALSE;
            if ( btaddr[0] == 'n' ) {
                if ( !nameToBtAddr( btaddr+2, &ba ) ) {
                    fprintf( stderr, "fatal error: unable to find device %s\n",
                             btaddr + 2 );
                    exit(0);
                }
                success = XP_TRUE;
            } else if ( btaddr[0] == 'a' ) {
                success = 0 == str2ba( &btaddr[2], &ba );
                XP_ASSERT( success );
            }
        }
        if ( !success ) {
            usage( argv[0], "bad format for -B param" );
        }
        XP_MEMCPY( &mainParams.connInfo.bt.hostAddr, &ba, 
                   sizeof(mainParams.connInfo.bt.hostAddr) );
#endif
    }
    mainParams.conType = conType;

    /*     mainParams.pipe = linuxCommPipeCtxtMake( isServer ); */

    /*     mainParams.util->vtable->m_util_makeStreamFromAddr =  */
    /* 	linux_util_makeStreamFromAddr; */

    mainParams.util->gameInfo = &mainParams.gi;

    linux_util_vt_init( MPPARM(mainParams.util->mpool) mainParams.util );

#if defined XWFEATURE_RELAY || defined XWFEATURE_BLUETOOTH
    mainParams.util->vtable->m_util_addrChange = linux_util_addrChange;
#endif

    srandom( seed );	/* init linux random number generator */
    XP_LOGF( "seeded srandom with %d", seed );

    if ( isServer ) {
        if ( mainParams.info.serverInfo.nRemotePlayers == 0 ) {
            mainParams.serverRole = SERVER_STANDALONE;
        } else {
            mainParams.serverRole = SERVER_ISSERVER;
        }	    
    } else {
        mainParams.serverRole = SERVER_ISCLIENT;
    }

    if ( mainParams.nLocalPlayers > 0 || !!mainParams.fileName) {
        if ( useCurses ) {
#if defined PLATFORM_NCURSES
            cursesmain( isServer, &mainParams );
#endif
        } else {
#if defined PLATFORM_GTK
            gtkmain( &mainParams, argc, argv );
#endif
        }
    } else {
        /* run server as faceless process? */
    }

    dict_destroy( mainParams.dict );
    linux_util_vt_destroy( mainParams.util );
    XP_LOGF( "exiting main" );
    return 0;
} /* main */

