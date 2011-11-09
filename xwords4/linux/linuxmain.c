/* -*- compile-command: "make MEMDEBUG=TRUE -j3"; -*- */
/* 
 * Copyright 2000 - 2011 by Eric House (xwords@eehouse.org).  All rights
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
#include <getopt.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <locale.h>

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
#include "dictiter.h"
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
#include "dictiter.h"
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
    if ( 1 != fread( buf, statBuf.st_size, 1, f ) ) {
        XP_ASSERT( 0 );
    }
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
    int fd;
    XP_U16 len;
    CommonGlobals* cGlobals = (CommonGlobals*)closure;

    len = stream_getSize( stream );
    buf = malloc( len );
    stream_getBytes( stream, buf, len );

    fd = open( cGlobals->params->fileName, O_CREAT|O_TRUNC|O_WRONLY, 
               S_IRUSR|S_IWUSR );
    if ( fd < 0 ) {
        XP_LOGF( "%s: open => %d (%s)", __func__, errno, strerror(errno) );
    } else {
        ssize_t nWritten = write( fd, buf, len );
        if ( len == nWritten ) {
            XP_LOGF( "%s: wrote %d bytes to %s", __func__, len,
                     cGlobals->params->fileName );
        } else {
            XP_LOGF( "%s: write => %s", __func__, strerror(errno) );
            XP_ASSERT( 0 );
        }
        fsync( fd );
        close( fd );
    }

    free( buf );
} /* writeToFile */

void
catOnClose( XWStreamCtxt* stream, void* XP_UNUSED(closure) )
{
    XP_U16 nBytes;
    char* buffer;

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

void
catFinalScores( const CommonGlobals* cGlobals )
{
    XWStreamCtxt* stream;

    stream = mem_stream_make( MPPARM(cGlobals->params->util->mpool)
                              cGlobals->params->vtMgr,
                              NULL, CHANNEL_NONE, catOnClose );
    server_writeFinalScores( cGlobals->game.server, stream );
    stream_putU8( stream, '\n' );
    stream_destroy( stream );
} /* printFinalScores */

XP_UCHAR*
strFromStream( XWStreamCtxt* stream )
{
    XP_U16 len = stream_getSize( stream );
    XP_UCHAR* buf = (XP_UCHAR*)malloc( len + 1 );
    stream_getBytes( stream, buf, len );
    buf[len] = '\0';

    return buf;
} /* strFromStream */

void
read_pipe_then_close( CommonGlobals* cGlobals )
{
    LaunchParams* params = cGlobals->params;
    XWStreamCtxt* stream = 
        streamFromFile( cGlobals, params->fileName, cGlobals );

#ifdef DEBUG
    XP_Bool opened = 
#endif
        game_makeFromStream( MPPARM(cGlobals->params->util->mpool) 
                             stream, &cGlobals->game, 
                             &params->gi, params->dict, 
                             &params->dicts, params->util, 
                             NULL /*draw*/,
                             &cGlobals->cp, NULL );
    XP_ASSERT( opened );
    stream_destroy( stream );

    XP_Bool handled = XP_FALSE;
    int fd = open( params->pipe, O_RDONLY );
    while ( fd >= 0 ) {
        unsigned short len;
        ssize_t nRead = blocking_read( fd, (unsigned char*)&len, sizeof(len) );
        if ( nRead != 2 ) {
            break;
        }
        len = ntohs( len );
        unsigned char buf[len];
        nRead = blocking_read( fd, buf, len );
        if ( nRead != len ) {
            break;
        }
        stream = mem_stream_make( MPPARM(cGlobals->params->util->mpool) 
                                  params->vtMgr, cGlobals, CHANNEL_NONE, NULL );
        stream_putBytes( stream, buf, len );

        if ( comms_checkIncomingStream( cGlobals->game.comms, 
                                        stream, NULL ) ) {
            handled = server_receiveMessage( cGlobals->game.server,
                                             stream ) || handled;
        }
        stream_destroy( stream );
    }
    LOG_RETURNF( "%d", handled );

    /* Write it out */
    /* stream = mem_stream_make( MEMPOOLCG(cGlobals) params->vtMgr,  */
    /*                           cGlobals, 0, writeToFile ); */
    /* stream_open( stream ); */
    /* game_saveToStream( &cGlobals->game, &params->gi, stream ); */
    /* stream_destroy( stream ); */
} /* read_pipe_then_close */

typedef enum {
    CMD_SKIP_GAMEOVER
    ,CMD_SHOW_OTHERSCORES
    ,CMD_HOSTIP
    ,CMD_DICT
#ifdef XWFEATURE_WALKDICT
    ,CMD_TESTDICT
    ,CMD_TESTPRFX
#endif
    ,CMD_PLAYERDICT
    ,CMD_SEED
    ,CMD_GAMESEED
    ,CMD_GAMEFILE
    ,CMD_NOMMAP
    ,CMD_PRINTHISORY
    ,CMD_SKIPWARNINGS
    ,CMD_LOCALPWD
    ,CMD_DUPPACKETS
    ,CMD_DROPNTHPACKET
    ,CMD_NOHINTS
    ,CMD_PICKTILESFACEUP
    ,CMD_PLAYERNAME
    ,CMD_REMOTEPLAYER
    ,CMD_PORT
    ,CMD_ROBOTNAME
    ,CMD_LOCALSMARTS
    ,CMD_SORTNEW
    ,CMD_ISSERVER
    ,CMD_SLEEPONANCHOR
    ,CMD_TIMERMINUTES
    ,CMD_UNDOWHENDONE
    ,CMD_NOHEARTBEAT
    ,CMD_HOSTNAME
    ,CMD_CLOSESTDIN
    ,CMD_QUITAFTER
    ,CMD_BOARDSIZE
    ,CMD_HIDEVALUES
    ,CMD_SKIPCONFIRM
    ,CMD_VERTICALSCORE
    ,CMD_NOPEEK
    ,CMD_ADDPIPE
#ifdef XWFEATURE_SEARCHLIMIT
    ,CMD_HINTRECT
#endif
#ifdef XWFEATURE_SMS
    ,CMD_SMSNUMBER		/* SMS phone number */
#endif
#ifdef XWFEATURE_RELAY
    ,CMD_ROOMNAME
    ,CMD_ADVERTISEROOM
    ,CMD_JOINADVERTISED
    ,CMD_PHONIES
    ,CMD_BONUSFILE
#endif
#ifdef XWFEATURE_BLUETOOTH
    ,CMD_BTADDR
#endif
#ifdef XWFEATURE_SLOW_ROBOT
    ,CMD_SLOWROBOT
#endif
#if defined PLATFORM_GTK && defined PLATFORM_NCURSES
    ,CMD_GTK
    ,CMD_CURSES
#endif
#if defined PLATFORM_GTK
    ,CMD_ASKNEWGAME
    ,CMD_NHIDDENROWS
#endif
    ,N_CMDS
} XwLinuxCmd;

typedef struct _CmdInfoRec {
    XwLinuxCmd cmd;
    bool hasArg;
    const char* param;
    const char* hint;
} CmdInfoRec;

static CmdInfoRec CmdInfoRecs[] = {
    { CMD_SKIP_GAMEOVER, false, "skip-final", "skip final scores display" }
    ,{ CMD_SHOW_OTHERSCORES, false, "show-other", "show robot/remote scores" }
    ,{ CMD_HOSTIP, true, "hostip", "remote host ip address (for direct connect)" }
    ,{ CMD_DICT, true, "game-dict", "dictionary name for game" }
#ifdef XWFEATURE_WALKDICT
    ,{ CMD_TESTDICT, true, "test-dict", "dictionary to be used for iterator test" }
    ,{ CMD_TESTPRFX, true, "test-prefix", "list first word starting with this" }
#endif
    ,{ CMD_PLAYERDICT, true, "player-dict", "dictionary name for player (in sequence)" }
    ,{ CMD_SEED, true, "seed", "random seed" }
    ,{ CMD_GAMESEED, true, "game-seed", "game seed (for relay play)" }
    ,{ CMD_GAMEFILE, true, "file", "file to save to/read from" }
    ,{ CMD_NOMMAP, false, "no-mmap", "copy dicts to memory rather than mmap them" }
    ,{ CMD_PRINTHISORY, false, "print-history", "print history on game over" }
    ,{ CMD_SKIPWARNINGS, false, "skip-warnings", "no modals on phonies" }
    ,{ CMD_LOCALPWD, true, "password", "password for user (in sequence)" }
    ,{ CMD_DUPPACKETS, false, "dup-packets", "send two of each to test dropping" }
    ,{ CMD_DROPNTHPACKET, true, "drop-nth-packet", "drop this packet; default 0 (none)" }
    ,{ CMD_NOHINTS, false, "no-hints", "disallow hints" }
    ,{ CMD_PICKTILESFACEUP, false, "pick-face-up", "allow to pick tiles" }
    ,{ CMD_PLAYERNAME, true, "name", "name of local, non-robot player" }
    ,{ CMD_REMOTEPLAYER, false, "remote-player", "add an expected player" }
    ,{ CMD_PORT, true, "port", "port to connect to on host" }
    ,{ CMD_ROBOTNAME, true, "robot", "name of local, robot player" }
    ,{ CMD_LOCALSMARTS, true, "robot-iq", "smarts for robot (in sequence)" }
    ,{ CMD_SORTNEW, false, "sort-tiles", "sort tiles each time assigned" }
    ,{ CMD_ISSERVER, false, "server", "this device acting as host" }
    ,{ CMD_SLEEPONANCHOR, false, "sleep-on-anchor", "slow down hint progress" }
    ,{ CMD_TIMERMINUTES, true, "timer-minutes", "initial timer setting" }
    ,{ CMD_UNDOWHENDONE, false, "undo-after", "undo the game after finishing" }
    ,{ CMD_NOHEARTBEAT, false, "no-heartbeat", "don't send heartbeats" }
    ,{ CMD_HOSTNAME, true, "host", "name of remote host" }
    ,{ CMD_CLOSESTDIN, false, "close-stdin", "close stdin on start" }
    ,{ CMD_QUITAFTER, true, "quit-after", "exit <n> seconds after game's done" }
    ,{ CMD_BOARDSIZE, true, "board-size", "board is <n> by <n> cells" }
    ,{ CMD_HIDEVALUES, false, "hide-values", "show letters, not nums, on tiles" }
    ,{ CMD_SKIPCONFIRM, false, "skip-confirm", "don't confirm before commit" }
    ,{ CMD_VERTICALSCORE, false, "vertical", "scoreboard is vertical" }
    ,{ CMD_NOPEEK, false, "no-peek", "disallow scoreboard tap changing player" }
    ,{ CMD_ADDPIPE, true, "with-pipe", "named pipe to listen on for relay msgs" }
#ifdef XWFEATURE_SEARCHLIMIT
    ,{ CMD_HINTRECT, false, "hintrect", "enable draggable hint-limits rect" }
#endif
#ifdef XWFEATURE_SMS
    ,{ CMD_SMSNUMBER, true, "sms-number", "phone number of host for sms game" }
#endif
#ifdef XWFEATURE_RELAY
    ,{ CMD_ROOMNAME, true, "room", "name of room on relay" }
    ,{ CMD_ADVERTISEROOM, false, "make-public", "make room public on relay" }
    ,{ CMD_JOINADVERTISED, false, "join-public", "look for a public room" }
    ,{ CMD_PHONIES, true, "phonies", "ignore (0, default), warn (1) or lose turn (2)" }
    ,{ CMD_BONUSFILE, true, "bonus-file", "provides bonus info: . + * ^ and ! are legal" }
#endif
#ifdef XWFEATURE_BLUETOOTH
    ,{ CMD_BTADDR, true, "btaddr", "bluetooth address of host" }
#endif
#ifdef XWFEATURE_SLOW_ROBOT
    ,{ CMD_SLOWROBOT, true, "slow-robot", "make robot slower to test network" }
#endif
#if defined PLATFORM_GTK && defined PLATFORM_NCURSES
    ,{ CMD_GTK, false, "gtk", "use GTK for display" }
    ,{ CMD_CURSES, false, "curses", "use curses for display" }
#endif
#if defined PLATFORM_GTK
    ,{ CMD_ASKNEWGAME, false, "ask-new", "put up ui for new game params" }
    ,{ CMD_NHIDDENROWS, true, "hide-rows", "number of rows obscured by tray" }
#endif
};

static struct option* 
make_longopts()
{
    int count = VSIZE( CmdInfoRecs );
    struct option* result = calloc( count+1, sizeof(*result) );
    int ii;
    for ( ii = 0; ii < count; ++ii ) {
        result[ii].name = CmdInfoRecs[ii].param;
        result[ii].has_arg = CmdInfoRecs[ii].hasArg;
        XP_ASSERT( ii == CmdInfoRecs[ii].cmd );
        result[ii].val = ii;
    }
    return result;
}

static void
usage( char* appName, char* msg )
{
    int ii;
    if ( msg != NULL ) {
        fprintf( stderr, "Error: %s\n\n", msg );
    }
    fprintf( stderr, "usage: %s \n", appName );
    for ( ii = 0; ii < VSIZE(CmdInfoRecs); ++ii ) {
        const CmdInfoRec* rec = &CmdInfoRecs[ii];
        fprintf( stderr, "    --%s %-20s # %s\n", rec->param,
                 rec->hasArg? "<param>" : "", rec->hint );
    }
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

#ifdef XWFEATURE_RELAY
static int
linux_init_relay_socket( CommonGlobals* cGlobals, const CommsAddrRec* addrRec )
{
    struct sockaddr_in to_sock;
    struct hostent* host;
    int sock = cGlobals->socket;
    if ( sock == -1 ) {

        /* make a local copy of the address to send to */
        sock = socket( AF_INET, SOCK_STREAM, 0 );
        if ( sock == -1 ) {
            XP_DEBUGF( "socket returned -1\n" );
            goto done;
        }

        to_sock.sin_port = htons( addrRec->u.ip_relay.port );
        XP_STATUSF( "1: sending to port %d", addrRec->u.ip_relay.port );
        host = gethostbyname( addrRec->u.ip_relay.hostName );
        if ( NULL == host ) {
            XP_WARNF( "%s: gethostbyname(%s) returned -1",  __func__, 
                      addrRec->u.ip_relay.hostName );
            sock = -1;
            goto done;
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
            XP_STATUSF( "%s: connect failed: %s (%d)", __func__, 
                        strerror(errno), errno );
        }
    }
 done:
    return sock;
} /* linux_init_relay_socket */

static XP_S16
linux_tcp_send( const XP_U8* buf, XP_U16 buflen, 
                CommonGlobals* globals, const CommsAddrRec* addrRec )
{
    XP_S16 result = 0;
    int sock = globals->socket;
    
    if ( sock == -1 ) {
        XP_LOGF( "%s: socket uninitialized", __func__ );
        sock = linux_init_relay_socket( globals, addrRec );
        if ( sock != -1 ) {
            assert( globals->socket == sock );
            (*globals->socketChanged)( globals->socketChangedClosure, 
                                       -1, sock, &globals->storage );
        }
    }

    if ( sock != -1 ) {
        XP_U16 netLen = htons( buflen );
        errno = 0;

        result = send( sock, &netLen, sizeof(netLen), 0 );
        if ( result == sizeof(netLen) ) {
            result = send( sock, buf, buflen, 0 ); 
        }
        if ( result <= 0 ) {
            XP_STATUSF( "closing non-functional socket" );
            close( sock );
            (*globals->socketChanged)( globals->socketChangedClosure, 
                                       sock, -1, &globals->storage );
            globals->socket = -1;
        }

        XP_STATUSF( "%s: send(sock=%d) returned %d of %d (err=%d)", 
                    __func__, sock, result, buflen, errno );
    } else {
        XP_LOGF( "%s: socket still -1", __func__ );
    }
 
    return result;
} /* linux_tcp_send */
#endif  /* XWFEATURE_RELAY */

#ifdef COMMS_HEARTBEAT
# ifdef XWFEATURE_RELAY
static void
linux_tcp_reset( CommonGlobals* globals )
{
    LOG_FUNC();
    if ( globals->socket != -1 ) {
        (void)close( globals->socket );
        globals->socket = -1;
    }
}
# endif

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
        nSent = linux_tcp_send( buf, buflen, globals, addrRec );
        if ( nSent == buflen && globals->params->duplicatePackets ) {
#ifdef DEBUG
            XP_S16 sentAgain = 
#endif
                linux_tcp_send( buf, buflen, globals, addrRec );
            XP_ASSERT( sentAgain == nSent );
        }

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
void
linux_close_socket( CommonGlobals* cGlobals )
{
    int socket = cGlobals->socket;

    (*cGlobals->socketChanged)( cGlobals->socketChangedClosure, 
                                socket, -1, &cGlobals->storage );

    XP_ASSERT(  -1 == cGlobals->socket );

    XP_LOGF( "linux_close_socket" );
    close( socket );
}

int
blocking_read( int fd, unsigned char* buf, int len )
{
    int nRead = 0;
    while ( nRead < len ) {
       ssize_t siz = read( fd, buf + nRead, len - nRead );
       if ( siz <= 0 ) {
           XP_LOGF( "read => %d, errno=%d (\"%s\")", nRead, 
                    errno, strerror(errno) );
           nRead = -1;
           break;
       }
       nRead += siz;
    }
    return nRead;
}

int
linux_relay_receive( CommonGlobals* cGlobals, unsigned char* buf, int bufSize )
{
    int sock = cGlobals->socket;
    unsigned short tmp;
    ssize_t nRead = blocking_read( sock, (unsigned char*)&tmp, sizeof(tmp) );
    if ( nRead != 2 ) {
        linux_close_socket( cGlobals );
        comms_transportFailed( cGlobals->game.comms );
        nRead = -1;
    } else {
        unsigned short packetSize = ntohs( tmp );
        assert( packetSize <= bufSize );
        nRead = blocking_read( sock, buf, packetSize );
        if ( nRead == packetSize ) {
            LaunchParams* params = cGlobals->params;
            ++params->nPacketsRcvd;
            if ( params->dropNthRcvd == 0 ) {
                /* do nothing */
            } else if ( params->dropNthRcvd > 0 ) {
                if ( params->nPacketsRcvd == params->dropNthRcvd ) {
                    XP_LOGF( "%s: dropping %dth packet per --drop-nth-packet",
                             __func__, params->nPacketsRcvd );
                    nRead = -1;
                }
            } else {
                if ( 0 == XP_RANDOM() % -params->dropNthRcvd ) {
                    XP_LOGF( "%s: RANDOMLY dropping %dth packet "
                             "per --drop-nth-packet",
                             __func__, params->nPacketsRcvd );
                    nRead = -1;
                }
            }
        }
    }
    XP_LOGF( "%s=>%d", __func__, nRead );
    return nRead;
} /* linux_relay_receive */
#endif  /* XWFEATURE_RELAY */

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
    fprintf( stderr, "bind returned %d; errno=%d (\"%s\")\n", result, errno,
             strerror(errno) );

    return linux_make_socketStream( newSocket );
#endif
} /* linux_util_makeStreamFromAddr */
#endif

XP_Bool
linuxFireTimer( CommonGlobals* cGlobals, XWTimerReason why )
{
    TimerInfo* tip = &cGlobals->timerInfo[why];
    XWTimerProc proc = tip->proc;
    void* closure = tip->closure;
    XP_Bool draw = false;

    tip->proc = NULL;

    if ( !!proc ) {
        draw = (*proc)( closure, why );
    } else {
        XP_LOGF( "%s: skipping timer %d; cancelled?", __func__, why );
    }
    return draw;
} /* linuxFireTimer */

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

static void
linux_util_setIsServer( XW_UtilCtxt* uc, XP_Bool isServer )
{
    XP_LOGF( "%s(%d)", __func__, isServer );
    CommonGlobals* cGlobals = (CommonGlobals*)uc->closure;
    DeviceRole newRole = isServer? SERVER_ISSERVER : SERVER_ISCLIENT;
    cGlobals->params->serverRole = newRole;
    cGlobals->params->gi.serverRole = newRole;
}
#endif

static unsigned int
defaultRandomSeed()
{
    /* use kernel device rather than time() so can run multiple times/second
       without getting the same results. */
    unsigned int rs;
    FILE* rfile = fopen( "/dev/urandom", "ro" );
    if ( 1 != fread( &rs, sizeof(rs), 1, rfile ) ) {
        XP_ASSERT( 0 );
    }
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

#ifdef XWFEATURE_SLOW_ROBOT
static bool
parsePair( const char* optarg, XP_U16* min, XP_U16* max )
{
    bool success = false;
    char* colon = strstr( optarg, ":" );
    if ( !colon ) {
        XP_LOGF( ": not found in argument\n" );
    } else {
        int intmin, intmax;
        if ( 2 == sscanf( optarg, "%d:%d", &intmin, &intmax ) ) {
            if ( intmin <= intmin ) {
                *min = intmin;
                *max = intmax;
                success = true;
            }
        }
    }
    return success;
}
#endif

static void
tmp_noop_sigintterm( int XP_UNUSED(sig) )
{
    LOG_FUNC();
}

#ifdef XWFEATURE_WALKDICT
//# define PRINT_ALL
static void
testGetNthWord( const DictionaryCtxt* dict, char** words,
                XP_U16 depth, IndexData* data )
{
    XP_U32 half = dict_getWordCount( dict ) / 2;
    XP_UCHAR buf[64];
    XP_U32 ii, jj;
    DictIter iter;
    XP_U32 interval = 1000;

    dict_initIter( dict, &iter );

    for ( ii = 0, jj = half; ii < half; ii += interval, jj += interval ) {
        if ( dict_getNthWord( &iter, ii, depth, data ) ) {
            dict_wordToString( &iter, buf, VSIZE(buf) );
            XP_ASSERT( 0 == strcmp( buf, words[ii] ) );
            // XP_LOGF( "%s: word[%ld]: %s", __func__, ii, buf );
        } else {
            XP_ASSERT( 0 );
        }
        if ( dict_getNthWord( &iter, jj, depth, data ) ) {
            dict_wordToString( &iter, buf, VSIZE(buf) );
            XP_ASSERT( 0 == strcmp( buf, words[jj] ) );
            // XP_LOGF( "%s: word[%ld]: %s", __func__, jj, buf );
        } else {
            XP_ASSERT( 0 );
        }
    }
}

static void
walk_dict_test( const LaunchParams* params, const DictionaryCtxt* dict, 
                GSList* testPrefixes )
{
    /* This is just to test that the dict-iterating code works.  The words are
       meant to be printed e.g. in a scrolling dialog on Android. */
    DictIter iter;
    long jj;
    XP_Bool gotOne;

    XP_U32 count = dict_getWordCount( dict );
    XP_ASSERT( count > 0 );
    XP_ASSERT( count == dict_countWords( dict ) );
    char** words = g_malloc( count * sizeof(char*) );
    XP_ASSERT( !!words );

    /* if ( dict_firstWord( dict, &word ) */
    /*      && dict_getNextWord( dict, &word ) */
    /*      && dict_getPrevWord( dict, &word ) ) { */
    /*     fprintf( stderr, "yay!: dict_getPrevWord returned\n" ); */
    /* } */
    /* exit( 0 ); */

    dict_initIter( dict, &iter );
    for ( jj = 0, gotOne = dict_firstWord( &iter );
          gotOne;
          ++jj, gotOne = dict_getNextWord( &iter ) ) {
        XP_ASSERT( dict_getPosition( &iter ) == jj );
        XP_UCHAR buf[64];
        dict_wordToString( &iter, buf, VSIZE(buf) );
# ifdef PRINT_ALL
        fprintf( stderr, "%.6ld: %s\n", jj, buf );
# endif
        if ( !!words ) {
            words[jj] = g_strdup( buf );
        }
    }
    XP_ASSERT( count == jj );

    for ( jj = 0, gotOne = dict_lastWord( &iter );
          gotOne;
          ++jj, gotOne = dict_getPrevWord( &iter ) ) {
        XP_ASSERT( dict_getPosition(&iter) == count-jj-1 );
        XP_UCHAR buf[64];
        dict_wordToString( &iter, buf, VSIZE(buf) );
# ifdef PRINT_ALL
        fprintf( stderr, "%.6ld: %s\n", jj, buf );
# endif
        if ( !!words ) {
            if ( strcmp( buf, words[count-jj-1] ) ) {
                fprintf( stderr, "failure at %ld: %s going forward; %s "
                         "going backward\n", jj, words[count-jj-1], buf );
                break;
            }
        }
    }
    XP_ASSERT( count == jj );
    XP_LOGF( "finished comparing runs in both directions" );

    XP_LOGF( "testing getNth" );
    testGetNthWord( dict, words, 0, NULL );

    XP_U16 depth = 2;
    XP_U16 maxCount = dict_numTileFaces( dict );
    IndexData data;
    data.count = maxCount * maxCount;
    data.indices = XP_MALLOC( params->util->mpool, 
                              data.count * depth * sizeof(data.indices[0]) );
    data.prefixes = XP_MALLOC( params->util->mpool, 
                               depth * data.count * sizeof(data.prefixes[0]) );

    XP_LOGF( "making index..." );
    dict_makeIndex( &iter, depth, &data );
    XP_LOGF( "DONE making index" );

    data.indices = XP_REALLOC( params->util->mpool, data.indices, 
                               data.count * depth * sizeof(*data.indices) );
    data.prefixes = XP_REALLOC( params->util->mpool, data.prefixes,
                               depth * data.count * sizeof(*data.prefixes) );
#if 0
    for ( ii = 0; ii < nIndices; ++ii ) {
        if ( !dict_getNthWord( dict, &word, indices[ii] ) ) {
            XP_ASSERT( 0 );
        }
        XP_ASSERT( word.index == indices[ii] );
        XP_UCHAR buf1[64];
        dict_wordToString( dict, &word, buf1, VSIZE(buf1) );
        XP_UCHAR buf2[64] = {0};
        if ( ii > 0 && dict_getNthWord( dict, &word, indices[ii]-1 ) ) {
            dict_wordToString( dict, &word, buf2, VSIZE(buf2) );
        }
        char prfx[8];
        dict_tilesToString( dict, &prefixes[depth*ii], depth, prfx, VSIZE(prfx) );
        fprintf( stderr, "%d: index: %ld; prefix: %s; word: %s (prev: %s)\n", 
                 ii, indices[ii], prfx, buf1, buf2 );
    }
#endif

    XP_LOGF( "testing getNth WITH INDEXING" );
    testGetNthWord( dict, words, depth, &data );

    if ( !!testPrefixes ) {
        int ii;
        guint count = g_slist_length( testPrefixes );
        for ( ii = 0; ii < count; ++ii ) {
            gchar* prefix = (gchar*)g_slist_nth_data( testPrefixes, ii );
            Tile tiles[MAX_COLS];
            XP_U16 nTiles = VSIZE(tiles);
            if ( dict_tilesForString( dict, prefix, tiles, &nTiles ) ) {
                if ( dict_findStartsWith( &iter, NULL, tiles, nTiles ) ) {
                    XP_UCHAR buf[32];
                    XP_UCHAR bufPrev[32] = {0};
                    dict_wordToString( &iter, buf, VSIZE(buf) );

                    XP_ASSERT( 0 == strncmp( buf, prefix, strlen(prefix) ) );

                    DictPosition pos = dict_getPosition( &iter );
                    XP_ASSERT( 0 == strcmp( buf, words[pos] ) );
                    if ( pos > 0 ) {
                         if ( !dict_getNthWord( &iter, pos-1, depth, &data ) ) {
                            XP_ASSERT( 0 );
                        }
                        dict_wordToString( &iter, bufPrev, VSIZE(bufPrev) );
                        XP_ASSERT( 0 == strcmp( bufPrev, words[pos-1] ) );
                    }
                    XP_LOGF( "dict_getStartsWith(%s) => %s (prev=%s)", 
                             prefix, buf, bufPrev );
                } else {
                    XP_LOGF( "nothing starts with %s", prefix );
                }
            }
        }

    }
    XP_LOGF( "done" );

    XP_FREE( params->util->mpool, data.indices );
    XP_FREE( params->util->mpool, data.prefixes );
} /* walk_dict_test */

static void
walk_dict_test_all( const LaunchParams* params, GSList* testDicts, 
                    GSList* testPrefixes )
{
    int ii;
    guint count = g_slist_length( testDicts );
    for ( ii = 0; ii < count; ++ii ) {
        gchar* name = (gchar*)g_slist_nth_data( testDicts, ii );
        DictionaryCtxt* dict = 
            linux_dictionary_make( MPPARM(params->util->mpool) name,
                                   params->useMmap );
        if ( NULL != dict ) {
            XP_LOGF( "walk_dict_test(%s)", name );
            walk_dict_test( params, dict, testPrefixes );
            dict_destroy( dict );
        }
    }
}
#endif

int
main( int argc, char** argv )
{
    XP_Bool useCurses;
    int opt;
    int totalPlayerCount = 0;
    XP_Bool isServer = XP_FALSE;
    char* portNum = NULL;
    char* hostName = "localhost";
    XP_Bool closeStdin = XP_FALSE;
    unsigned int seed = defaultRandomSeed();
    LaunchParams mainParams;
    XP_U16 nPlayerDicts = 0;
    XP_U16 robotCount = 0;
    XP_U16 ii;
#ifdef XWFEATURE_WALKDICT
    GSList* testDicts = NULL;
    GSList* testPrefixes = NULL;
#endif

    /* install a no-op signal handler.  Later curses- or gtk-specific code
       will install one that does the right thing in that context */

    struct sigaction act = { .sa_handler = tmp_noop_sigintterm };
    sigaction( SIGINT, &act, NULL );
    sigaction( SIGTERM, &act, NULL );
    
    CommsConnType conType = COMMS_CONN_NONE;
#ifdef XWFEATURE_SMS
    char* serverPhone = NULL;
#endif
#ifdef XWFEATURE_BLUETOOTH
    const char* btaddr = NULL;
#endif

    setlocale(LC_ALL, "");

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
    mainParams.connInfo.relay.invite = "INVITE";
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
    mainParams.gi.dictLang = -1;
    mainParams.noHeartbeat = XP_FALSE;
    mainParams.nHidden = 0;
    mainParams.needsNewGame = XP_FALSE;
#ifdef XWFEATURE_SEARCHLIMIT
    mainParams.allowHintRect = XP_FALSE;
#endif
    mainParams.skipCommitConfirm = XP_FALSE;
    mainParams.showColors = XP_TRUE;
    mainParams.allowPeek = XP_TRUE;
    mainParams.showRobotScores = XP_FALSE;
    mainParams.useMmap = XP_TRUE;
    
    /*     serverName = mainParams.info.clientInfo.serverName = "localhost"; */

#if defined PLATFORM_GTK
    useCurses = XP_FALSE;
#else  /* curses is the default if GTK isn't available */
    useCurses = XP_TRUE;
#endif

    struct option* longopts = make_longopts();

    bool done = false;
    while ( !done ) {
        short index;
        opt = getopt_long_only( argc, argv, "", longopts, NULL );
        switch ( opt ) {
        case '?':
            usage(argv[0], NULL);
            break;
        case CMD_SKIP_GAMEOVER:
            mainParams.skipGameOver = XP_TRUE;
            break;
        case CMD_SHOW_OTHERSCORES:
            mainParams.showRobotScores = XP_TRUE;
            break;
#ifdef XWFEATURE_RELAY
        case CMD_ROOMNAME:
            XP_ASSERT( conType == COMMS_CONN_NONE ||
                       conType == COMMS_CONN_RELAY );
            mainParams.connInfo.relay.invite = optarg;
            conType = COMMS_CONN_RELAY;
            break;
#endif
        case CMD_HOSTIP:
            XP_ASSERT( conType == COMMS_CONN_NONE ||
                       conType == COMMS_CONN_IP_DIRECT );
            hostName = optarg;
            conType = COMMS_CONN_IP_DIRECT;
            break;
        case CMD_DICT:
            mainParams.gi.dictName = copyString( mainParams.util->mpool,
                                                 (XP_UCHAR*)optarg );
            break;
#ifdef XWFEATURE_WALKDICT
        case CMD_TESTDICT:
            testDicts = g_slist_prepend( testDicts, g_strdup(optarg) );
            break;
        case CMD_TESTPRFX:
            testPrefixes = g_slist_prepend( testPrefixes, g_strdup(optarg) );
            break;
#endif
        case CMD_PLAYERDICT:
            mainParams.gi.players[nPlayerDicts++].dictName = optarg;
            break;
        case CMD_SEED:
            seed = atoi(optarg);
            break;
        case CMD_GAMESEED:
            mainParams.gameSeed = atoi(optarg);
            break;
        case CMD_GAMEFILE:
            mainParams.fileName = optarg;
            break;
        case CMD_NOMMAP:
            mainParams.useMmap = false;
            break;
        case CMD_PRINTHISORY:
            mainParams.printHistory = 1;
            break;
#ifdef XWFEATURE_SEARCHLIMIT
        case CMD_HINTRECT:
            mainParams.allowHintRect = XP_TRUE;
            break;
#endif
        case CMD_SKIPWARNINGS:
            mainParams.skipWarnings = 1;
            break;
        case CMD_LOCALPWD:
            mainParams.gi.players[mainParams.nLocalPlayers-1].password
                = (XP_UCHAR*)optarg;
            break;
        case CMD_LOCALSMARTS:
            index = mainParams.gi.nPlayers - 1;
            XP_ASSERT( LP_IS_ROBOT( &mainParams.gi.players[index] ) );
            mainParams.gi.players[index].robotIQ = atoi(optarg);
            break;
#ifdef XWFEATURE_SMS
        case CMD_SMSNUMBER:		/* SMS phone number */
            XP_ASSERT( COMMS_CONN_NONE == conType );
            serverPhone = optarg;
            conType = COMMS_CONN_SMS;
            break;
#endif
        case CMD_DUPPACKETS:
            mainParams.duplicatePackets = XP_TRUE;
            break;
        case CMD_DROPNTHPACKET:
            mainParams.dropNthRcvd = atoi( optarg );
            break;
        case CMD_NOHINTS:
            mainParams.gi.hintsNotAllowed = XP_TRUE;
            break;
        case CMD_PICKTILESFACEUP:
            mainParams.gi.allowPickTiles = XP_TRUE;
            break;
        case CMD_PLAYERNAME:
            index = mainParams.gi.nPlayers++;
            ++mainParams.nLocalPlayers;
            mainParams.gi.players[index].robotIQ = 0; /* means human */
            mainParams.gi.players[index].isLocal = XP_TRUE;
            mainParams.gi.players[index].name = 
                copyString( mainParams.util->mpool, (XP_UCHAR*)optarg);
            break;
        case CMD_REMOTEPLAYER:
            index = mainParams.gi.nPlayers++;
            mainParams.gi.players[index].isLocal = XP_FALSE;
            ++mainParams.info.serverInfo.nRemotePlayers;
            break;
        case CMD_PORT:
            /* could be RELAY or IP_DIRECT or SMS */
            portNum = optarg;
            break;
        case CMD_ROBOTNAME:
            ++robotCount;
            index = mainParams.gi.nPlayers++;
            ++mainParams.nLocalPlayers;
            mainParams.gi.players[index].robotIQ = 1; /* real smart by default */
            mainParams.gi.players[index].isLocal = XP_TRUE;
            mainParams.gi.players[index].name = 
                copyString( mainParams.util->mpool, (XP_UCHAR*)optarg);
            break;
        case CMD_SORTNEW:
            mainParams.sortNewTiles = XP_TRUE;
            break;
        case CMD_ISSERVER:
            isServer = XP_TRUE;
            break;
        case CMD_SLEEPONANCHOR:
            mainParams.sleepOnAnchor = XP_TRUE;
            break;
        case CMD_TIMERMINUTES:
            mainParams.gi.gameSeconds = atoi(optarg) * 60;
            mainParams.gi.timerEnabled = XP_TRUE;
            break;
        case CMD_UNDOWHENDONE:
            mainParams.undoWhenDone = XP_TRUE;
            break;
        case CMD_NOHEARTBEAT:
            mainParams.noHeartbeat = XP_TRUE;
            XP_ASSERT(0);    /* not implemented!!!  Needs to talk to comms... */
            break;
        case CMD_HOSTNAME:
            /* mainParams.info.clientInfo.serverName =  */
            XP_ASSERT( conType == COMMS_CONN_NONE ||
                       conType == COMMS_CONN_RELAY );
            conType = COMMS_CONN_RELAY;
            hostName = optarg;
            break;
#ifdef XWFEATURE_RELAY
        case CMD_ADVERTISEROOM:
            mainParams.connInfo.relay.advertiseRoom = true;
            break;
        case CMD_JOINADVERTISED:
            mainParams.connInfo.relay.seeksPublicRoom = true;
            break;
        case CMD_PHONIES:
            switch( atoi(optarg) ) {
            case 0:
                mainParams.gi.phoniesAction = PHONIES_IGNORE;
                break;
            case 1:
                mainParams.gi.phoniesAction = PHONIES_WARN;
                break;
            case 2:
                mainParams.gi.phoniesAction = PHONIES_DISALLOW;
                break;
            default:
                usage( argv[0], "phonies takes 0 or 1 or 2" );
            }
            break;
        case CMD_BONUSFILE:
            mainParams.bonusFile = optarg;
            break;
#endif
        case CMD_CLOSESTDIN:
            closeStdin = XP_TRUE;
            break;
        case CMD_QUITAFTER:
            mainParams.quitAfter = atoi(optarg);
            break;
        case CMD_BOARDSIZE:
            mainParams.gi.boardSize = atoi(optarg);
            break;
#ifdef XWFEATURE_BLUETOOTH
        case CMD_BTADDR:
            XP_ASSERT( conType == COMMS_CONN_NONE ||
                       conType == COMMS_CONN_BT );
            conType = COMMS_CONN_BT;
            btaddr = optarg;
            break;
#endif
        case CMD_HIDEVALUES:
            mainParams.hideValues = XP_TRUE;
            break;
        case CMD_SKIPCONFIRM:
            mainParams.skipCommitConfirm = XP_TRUE;
            break;
        case CMD_VERTICALSCORE:
            mainParams.verticalScore = XP_TRUE;
            break;
        case CMD_NOPEEK:
            mainParams.allowPeek = XP_FALSE;
        case CMD_ADDPIPE:
            mainParams.pipe = optarg;
            break;
#ifdef XWFEATURE_SLOW_ROBOT
        case CMD_SLOWROBOT:
            if ( !parsePair( optarg, &mainParams.robotThinkMin,
                             &mainParams.robotThinkMax ) ) {
                usage(argv[0], "bad param" );
            }
            break;
#endif

#if defined PLATFORM_GTK && defined PLATFORM_NCURSES
        case CMD_GTK:
            useCurses = XP_FALSE;
            break;
        case CMD_CURSES:
            useCurses = XP_TRUE;
            break;
#endif
#if defined PLATFORM_GTK
        case CMD_ASKNEWGAME:
            mainParams.askNewGame = XP_TRUE;
            break;
        case CMD_NHIDDENROWS:
            mainParams.nHidden = atoi(optarg);
            break;
#endif
        default:
            done = true;
            break;
        }
    }

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
            mainParams.needsNewGame = XP_TRUE;
        }
    }

    if ( !!mainParams.gi.dictName ) {
        mainParams.dict = 
            linux_dictionary_make( MPPARM(mainParams.util->mpool) 
                                   mainParams.gi.dictName, 
                                   mainParams.useMmap );
        XP_ASSERT( !!mainParams.dict );
        mainParams.gi.dictLang = dict_getLangCode( mainParams.dict );
    } else if ( isServer ) {
#ifdef STUBBED_DICT
        mainParams.dict = make_stubbed_dict( 
            MPPARM_NOCOMMA(mainParams.util->mpool) );
        XP_WARNF( "no dictionary provided: using English stub dict\n" );
        mainParams.gi.dictLang = dict_getLangCode( mainParams.dict );
#else
        if ( 0 == nPlayerDicts ) {
            mainParams.needsNewGame = XP_TRUE;
        }
#endif
    } else if ( robotCount > 0 ) {
        mainParams.needsNewGame = XP_TRUE;
    }

    if ( 0 < mainParams.info.serverInfo.nRemotePlayers
         && SERVER_STANDALONE == mainParams.gi.serverRole ) {
        mainParams.needsNewGame = XP_TRUE;
    }

    for ( ii = 0; ii < nPlayerDicts; ++ii ) {
        XP_UCHAR* name = mainParams.gi.players[ii].dictName;
        if ( !!name ) {
            mainParams.dicts.dicts[ii] = 
                linux_dictionary_make( MPPARM(mainParams.util->mpool) name,
				       mainParams.useMmap );
        }
    }

    /* if ( !isServer ) { */
    /*     if ( mainParams.info.serverInfo.nRemotePlayers > 0 ) { */
    /*         mainParams.needsNewGame = XP_TRUE; */
    /*     }	     */
    /* } */
#ifdef XWFEATURE_WALKDICT
    if ( !!testDicts ) {
        walk_dict_test_all( &mainParams, testDicts, testPrefixes );
        exit( 0 );
    }
#endif
    if ( 0 ) {
#ifdef XWFEATURE_RELAY
    } else if ( conType == COMMS_CONN_RELAY ) {
        mainParams.connInfo.relay.relayName = hostName;
        if ( NULL == portNum ) {
            portNum = "10997";
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

#ifndef XWFEATURE_STANDALONE_ONLY
    mainParams.util->vtable->m_util_addrChange = linux_util_addrChange;
    mainParams.util->vtable->m_util_setIsServer = linux_util_setIsServer;
#endif

    srandom( seed );	/* init linux random number generator */
    XP_LOGF( "seeded srandom with %d", seed );

    if ( closeStdin ) {
        fclose( stdin );
        if ( mainParams.quitAfter < 0 ) {
            fprintf( stderr, "stdin closed; you'll need some way to quit\n" );
        }
    }

    if ( isServer ) {
        if ( mainParams.info.serverInfo.nRemotePlayers == 0 ) {
            mainParams.serverRole = SERVER_STANDALONE;
        } else {
            mainParams.serverRole = SERVER_ISSERVER;
        }	    
    } else {
        mainParams.serverRole = SERVER_ISCLIENT;
    }

    if ( mainParams.needsNewGame ) {
        gi_initPlayerInfo( MPPARM(mainParams.util->mpool) 
                           &mainParams.gi, NULL );
    }

    /* curses doesn't have newgame dialog */
    if ( useCurses && !mainParams.needsNewGame ) {
#if defined PLATFORM_NCURSES
        cursesmain( isServer, &mainParams );
#endif
    } else if ( !useCurses ) {
#if defined PLATFORM_GTK
        gtkmain( &mainParams, argc, argv );
#endif
    } else {
        usage( argv[0], "rtfm" );
    }

    vtmgr_destroy( MPPARM(mainParams.util->mpool) mainParams.vtMgr );

    linux_util_vt_destroy( mainParams.util );

    mpool_destroy( mainParams.util->mpool );

    free( mainParams.util );

    XP_LOGF( "exiting main" );
    return 0;
} /* main */

