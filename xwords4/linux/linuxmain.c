/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 2000 by Eric House (fixin@peak.org).  All rights reserved.
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

#include <netdb.h>		/* gethostbyname */
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <time.h>

#include "linuxmain.h"
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

#define DEFAULT_SEND_PORT 4999
#define DEFAULT_LISTEN_PORT 4998

#ifdef DEBUG
void 
linux_debugf( char* format, ... )
{
    char buf[1000];
    va_list ap;
    //    time_t tim;
    struct tm* timp;
    struct timeval tv;
    struct timezone tz;

    gettimeofday( &tv, &tz );
    timp = localtime( &tv.tv_sec );

    sprintf( buf, "%d:%d:%d: ", timp->tm_hour, timp->tm_min, timp->tm_sec );

    va_start(ap, format);

    vsprintf(buf+strlen(buf), format, ap);

    va_end(ap);
    
    fprintf( stderr, buf );
    fprintf( stderr, "\n" );
}
#endif

void
catOnClose( XWStreamCtxt* stream, void* closure )
{
    XP_U16 nBytes;
    char* buffer;

    XP_LOGF( "catOnClose" );

    nBytes = stream_getSize( stream );
    buffer = malloc( nBytes + 1 );
    stream_getBytes( stream, buffer, nBytes );
    buffer[nBytes] = '\0';

    fprintf( stderr, buffer );

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

XP_UCHAR*
linux_getErrString( UtilErrID id )
{
    char* message = NULL;

    switch( id ) {
    case ERR_TILES_NOT_IN_LINE:
	message = "All tiles played must be in a line.";
	break;
    case ERR_NO_EMPTIES_IN_TURN:
	message = "Empty squares cannot separate tiles played.";
	break;

    case ERR_TWO_TILES_FIRST_MOVE:
	message = "Must play two or more pieces on the first move.";
	break;
    case ERR_TILES_MUST_CONTACT:
	message = "New pieces must contact others already in place (or "
	    "the middle square on the first move).";
	break;
    case ERR_NOT_YOUR_TURN:
	message = "You can't do that; it's not your turn!";
	break;
    case ERR_NO_PEEK_ROBOT_TILES:
	message = "No peeking at the robot's tiles!";
	break;
    case ERR_NO_PEEK_REMOTE_TILES:
	message = "No peeking at remote players' tiles!";
	break;
    case ERR_REG_UNEXPECTED_USER:
	message = "Refused attempt to register unexpected user[s].";
	break;
    case ERR_SERVER_DICT_WINS:
	message = "Conflict between Host and Guest dictionaries; Host wins.";
	XP_WARNF( "GTK may have problems here." );
	break;
    default:
	message = "<unrecognized error code reported>";
    }

    return message;
} /* linux_getErrString */

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
	     "\t [-o]             # tray overlaps board (like small screen)\n"
	     "\t [-k]             # ask for parameters via \"new games\" dlg\n"
#endif
	     "\t [-f file]        # use this file to save/load game\n"
	     "\t [-q]             # quit when game over (useful for robot-only)\n"
	     "\t [-S]             # slow robot down \n"
	     "\t [-i]             # print game history when game over\n"
	     "\t [-U]             # call 'Undo' after game ends\n"
	     "\t [-r name]*       # same-process robot\n"
	     "\t [-n name]*       # same-process player (no network used)\n"
	     "\t [-w pwd]*        # passwd for matching local player\n"
	     "\t [-v]             # put scoreboard in vertical mode\n"
	     "\t [-m]             # make the robot duMb (smart is default)\n"
	     "\t [-c]             # explain robot scores after each move\n"
	     "\t\t # (max of four players total, local and remote)\n"
	     "\t [-b boardSize]   # number of columns and rows\n"
	     "\t [-l listen_port] # port for inet connection (4999 default)\n"
	     "\t [-e random_seed] \n"
	     "\t [-t initial_minutes_on_timer] \n"
	     "# --------------- choose client or server ----------\n"
	     "\t -s               # be the server\n"
	     "\t -d xwd_file      # provides tile counts & values\n"
	     "\t\t # list each player as local or remote\n"
	     "\t [-N]*            # remote client (listen for connection)\n"
	     "# --------------- OR client-only ----------\n"
	     "\t [-a server_addr] # be a client (on port spec'd above)\n"
	     "\t [-p client_port] # must != server's port if on same device"
	     ""                   " (default localhost)\n"
         "\nexample: \n"
             "\tserver: ./xwords -d dict.xwd -s -r Eric -N\n"
             "\tclient: ./xwords -d dict.xwd -r Kati -p 4999 -l 6000\n"
	     , appName );
    exit(1);
}

XP_S16
linux_udp_send( XP_U8* buf, XP_U16 buflen, CommsAddrRec* addrRec, 
                void* closure )
{
    CommonGlobals* globals = (CommonGlobals*)closure;
    struct sockaddr_in to_sock;
    struct hostent* host;
    XP_S16 result;
    int sock;
/*     XP_U8* msg; */

    XP_LOGF( "linux_udp_send" );

    /* make a local copy of the address to send to */
    sock = socket( AF_INET, SOCK_DGRAM, 0 );
    if ( sock == -1 ) {
        XP_DEBUGF( "socket returned -1\n" );
        return -1;
    }

    if ( !!addrRec ) {

        XP_ASSERT( addrRec->conType == COMMS_CONN_IP );
        XP_MEMSET( &to_sock, 0, sizeof(to_sock) );
        XP_STATUSF( "target IP = 0x%lx", addrRec->u.ip.ipAddr );
        to_sock.sin_addr.s_addr = htonl(addrRec->u.ip.ipAddr);
        to_sock.sin_port = htons(addrRec->u.ip.port);

        XP_STATUSF( "0: sending to port %d(0x%x)", 
                    addrRec->u.ip.port, addrRec->u.ip.port );
    } else {

        to_sock.sin_port = htons(globals->params->defaultSendPort);
        XP_STATUSF( "1: sending to port %d", 
                    globals->params->defaultSendPort );
        if (( host = gethostbyname(globals->defaultServerName) ) == NULL ) {
            XP_WARNF( "gethostbyname returned -1\n" );
            return -1;
        } else {
            XP_WARNF( "gethostbyname for %s worked", 
                      globals->defaultServerName );
        }
        memcpy( &(to_sock.sin_addr.s_addr), host->h_addr_list[0],  
                sizeof(struct in_addr));
    }
    to_sock.sin_family = AF_INET;
   
/*     msg = malloc( buflen ); */
/*     XP_MEMCPY( msg, buf, buflen ); */

    errno = 0;
    XP_STATUSF( "checking: errno=%d", (short)errno );
    result = sendto( sock, buf, buflen, 0, 
                     (struct sockaddr *)&to_sock, sizeof(to_sock) );
    XP_STATUSF( "foo: sendto returned %d of %d (err=%d)", 
                result, buflen, errno );
    close(sock);

/*     free( msg ); */
    
    return result;
} /* linux_udp_send */

int
initListenerSocket( int port )
{
    int newSocket;
    int result;
    struct sockaddr_in sockAddr;

    XP_DEBUGF( "opening socket to listen on port %d", port );

    newSocket = socket( AF_INET, DGRAM_TYPE, 0 );
    XP_DEBUGF( "socket returned %d", newSocket );

    sockAddr.sin_family = AF_INET;
    sockAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    sockAddr.sin_port = htons(port);

    result = bind( newSocket, (struct sockaddr*)&sockAddr, sizeof(sockAddr) );
    XP_LOGF( "bind on port %d returned %d; errno=%d", 
             port, result, errno );


/*     result = listen( newSocket, 5 ); */
/*     XP_DEBUGF( "listen returned %d; errno=%d\n", result, errno ); */

    return newSocket;
} /* initListenerSocket */

/* Create a stream for the incomming message buffer, and read in any
   information specific to our platform's comms layer (return address, say)
 */
XWStreamCtxt*
stream_from_msgbuf( CommonGlobals* globals, char* bufPtr, XP_U16 nBytes )
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

static DictionaryCtxt*
linux_util_makeEmptyDict( XW_UtilCtxt* uctx )
{
    XP_DEBUGF( "linux_util_makeEmptyDict called\n" );
    return linux_dictionary_make( MPPARM(uctx->mpool) NULL );
} /* linux_util_makeEmptyDict */

#define EM BONUS_NONE
#define DL BONUS_DOUBLE_LETTER
#define DW BONUS_DOUBLE_WORD
#define TL BONUS_TRIPLE_LETTER
#define TW BONUS_TRIPLE_WORD

static XWBonusType
linux_util_getSquareBonus( XW_UtilCtxt* uc, ModelCtxt* model,
			   XP_U16 col, XP_U16 row )
{
    XP_U16 index;
    /* This must be static or won't compile under multilink (for Palm).
       Fix! */
    static char scrabbleBoard[8*8] = { 
	TW,EM,EM,DL,EM,EM,EM,TW,
	EM,DW,EM,EM,EM,TL,EM,EM,

	EM,EM,DW,EM,EM,EM,DL,EM,
	DL,EM,EM,DW,EM,EM,EM,DL,
                            
	EM,EM,EM,EM,DW,EM,EM,EM,
	EM,TL,EM,EM,EM,TL,EM,EM,
                            
	EM,EM,DL,EM,EM,EM,DL,EM,
	TW,EM,EM,DL,EM,EM,EM,DW,
    }; /* scrabbleBoard */

    if ( col > 7 ) col = 14 - col;
    if ( row > 7 ) row = 14 - row;
    index = (row*8) + col;
    if ( index >= 8*8 ) {
	return (XWBonusType)EM;
    } else {
	return (XWBonusType)scrabbleBoard[index];
    }
} /* linux_util_getSquareBonus */

static XP_U32
linux_util_getCurSeconds( XW_UtilCtxt* uc ) 
{
    return (XP_U32)time(NULL);//tv.tv_sec;
} /* gtk_util_getCurSeconds */

static XP_UCHAR*
linux_util_getUserString( XW_UtilCtxt* uc, XP_U16 code )
{
    switch( code ) {
    case STRD_REMAINING_TILES_ADD:
        return "+ %d [all remaining tiles]";
    case STRD_UNUSED_TILES_SUB:
        return "- %d [unused tiles]";
    case STR_COMMIT_CONFIRM:
        return "Are you sure you want to commit the current move?\n";
    case STRD_TURN_SCORE:
        return "Score for turn: %d\n";
    case STR_BONUS_ALL:
        return "Bonus for using all tiles: 50\n";
    case STR_LOCAL_NAME:
        return "%s";
    case STR_NONLOCAL_NAME:
        return "%s (remote)";
    case STRD_TIME_PENALTY_SUB:
        return " - %d [time]";
        /* added.... */
    case STRD_CUMULATIVE_SCORE:
        return "Cumulative score: %d\n";
    case STRS_TRAY_AT_START:
        return "Tray at start: %s\n";
    case STRS_MOVE_DOWN:
        return "move (from %s down)\n";
    case STRS_MOVE_ACROSS:
        return "move (from %s across)\n";
    case STRS_NEW_TILES:
        return "New tiles: %s\n";
    case STRSS_TRADED_FOR:
        return "Traded %s for %s.";
    case STR_PASS:
        return "pass\n";
    case STR_PHONY_REJECTED:
        return "Illegal word in move; turn lost!\n";

    case STRD_ROBOT_TRADED:
        return "%d tiles traded this turn.";
    case STR_ROBOT_MOVED:
        return "The robot moved:\n";
    case STR_REMOTE_MOVED:
        return "Remote player moved:\n";

    case STR_PASSED: 
        return "Passed";
    case STRSD_SUMMARYSCORED: 
        return "%s:%d";
    case STRD_TRADED: 
        return "Traded %d";
    case STR_LOSTTURN:
        return "Lost turn";

    default:
        return "unknown code to linux_util_getUserString";
    }
} /* linux_util_getUserString */

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

int
main( int argc, char** argv )
{
    XP_Bool useGTK, useCurses;
    int opt;
    int totalPlayerCount = 0;
    XP_Bool isServer = XP_FALSE;
    char* listenPortNumString = NULL;
    char* sendPortNumString = NULL;
    char* serverName = "localhost";
    unsigned int seed = defaultRandomSeed();
    LaunchParams mainParams;
    XP_U16 robotCount = 0;

#ifdef DEBUG
    {
        int i;
        for ( i = 0; i < argc; ++i ) {
            XP_LOGF( argv[i] );
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
    mainParams.defaultListenPort = DEFAULT_LISTEN_PORT;
    mainParams.defaultSendPort = DEFAULT_SEND_PORT;
    mainParams.trayOverlaps = XP_FALSE;
    mainParams.gi.boardSize = 15;
    mainParams.quitAfter = XP_FALSE;
    mainParams.sleepOnAnchor = XP_FALSE;
    mainParams.printHistory = XP_FALSE;
    mainParams.undoWhenDone = XP_FALSE;
    mainParams.gi.timerEnabled = XP_FALSE;
    mainParams.gi.robotSmartness = SMART_ROBOT;
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
                      "o"
#endif
                      "kKf:l:n:Nsd:a:p:e:r:b:qw:Sit:Umvc" );
        switch( opt ) {
        case 'h':
        case '?':
            usage(argv[0], NULL);
            break;
        case 'c':
            mainParams.showRobotScores = XP_TRUE;
            break;
        case 'd':
            mainParams.gi.dictName = copyString( MPPARM(mainParams.util->mpool) 
                                                 optarg );
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
        case 'K':
            mainParams.skipWarnings = 1;
            break;
        case 'w':
            mainParams.gi.players[mainParams.nLocalPlayers-1].password
                = optarg;
            break;
        case 'm':		/* dumb robot */
            mainParams.gi.robotSmartness = DUMB_ROBOT;
            break;
        case 'n':
            index = mainParams.gi.nPlayers++;
            ++mainParams.nLocalPlayers;
            mainParams.gi.players[index].isRobot = XP_FALSE;
            mainParams.gi.players[index].isLocal = XP_TRUE;
            mainParams.gi.players[index].name = 
                copyString(MPPARM(mainParams.util->mpool) optarg);
            break;
        case 'N':
            index = mainParams.gi.nPlayers++;
            mainParams.gi.players[index].isLocal = XP_FALSE;
            ++mainParams.info.serverInfo.nRemotePlayers;
            break;
        case 'l':
            listenPortNumString = optarg;
            break;
        case 'p':
            sendPortNumString = optarg;
            break;
        case 'r':
            ++robotCount;
            index = mainParams.gi.nPlayers++;
            ++mainParams.nLocalPlayers;
            mainParams.gi.players[index].isRobot = XP_TRUE;
            mainParams.gi.players[index].isLocal = XP_TRUE;
            mainParams.gi.players[index].name = 
                copyString(MPPARM(mainParams.util->mpool) optarg);
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
        case 'a':
            /* mainParams.info.clientInfo.serverName =  */
            serverName = optarg;
            break;
        case 'q':
            mainParams.quitAfter = XP_TRUE;
            break;
        case 'b':
            mainParams.gi.boardSize = atoi(optarg);
            break;
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
        case 'o':
            mainParams.trayOverlaps = XP_TRUE;
            break;
        case 'k':
            mainParams.askNewGame = XP_TRUE;
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

    /* convert strings to whatever */
    if ( listenPortNumString ) {
        mainParams.defaultListenPort = atoi( listenPortNumString );
    }
    if ( sendPortNumString != NULL ) {
        mainParams.defaultSendPort = atoi( sendPortNumString );
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

    /* setup sockets and any other stuff not specific to GTK or ncurses */
    if ( isServer ) {
    } else {
        /* 	struct hostent* hostinfo; */
        /* 	hostinfo = gethostbyname( serverName ); */
        /* 	if ( !hostinfo ) { */
        /* 	    fprintf( stderr, "unable to get host info for %s\n", serverName ); */
        /* 	    exit( 0 ); */
        /* 	} else { */
        /* 	    char* hostName = inet_ntoa( *(struct in_addr*)hostinfo->h_addr ); */
        /* 	    unsigned long int serverAddr; */
        /* 	    fprintf( stderr,  "gethostbyname returned %s\n", hostName ); */
        /* 	    serverAddr = inet_addr(hostName); */
        /* 	    fprintf( stderr,  "inet_addr returned %lu\n", serverAddr ); */

        /* 	    mainParams.info.clientInfo.stream = */
        /* 		linux_make_outbound_socketStream( serverAddr, serverPort ); */

        /* 	    if ( !stream_open( mainParams.info.clientInfo.stream ) { */
        /* 		fprintf( stderr, "ERROR: unable to connect to server\n" ); */
        /* 		exit(0); */
        /* 	    } */
        /* 	} */
        mainParams.info.clientInfo.serverName = serverName;
    }

    /*     mainParams.pipe = linuxCommPipeCtxtMake( isServer ); */

    mainParams.util->vtable = malloc( sizeof(UtilVtable) );
    /*     mainParams.util->vtable->m_util_makeStreamFromAddr =  */
    /* 	linux_util_makeStreamFromAddr; */

    mainParams.util->gameInfo = &mainParams.gi;

    mainParams.util->vtable->m_util_makeEmptyDict = 
        linux_util_makeEmptyDict;
    mainParams.util->vtable->m_util_getSquareBonus = 
        linux_util_getSquareBonus;
    mainParams.util->vtable->m_util_getCurSeconds = 
        linux_util_getCurSeconds;
    mainParams.util->vtable->m_util_getUserString = 
        linux_util_getUserString;

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
            gtkmain( isServer, &mainParams, argc, argv );
#endif
        }
    } else {
        /* run server as faceless process? */
    }

    dict_destroy( mainParams.dict );

    return 0;
} /* main */

