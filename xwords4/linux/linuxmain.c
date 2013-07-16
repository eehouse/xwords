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
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <locale.h>
#include <string.h>

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
#include <linux/un.h>
#ifdef USE_SQLITE
# include <sqlite3.h>
#endif

#ifdef XWFEATURE_BLUETOOTH
# include <bluetooth/bluetooth.h>
# include <bluetooth/hci.h>
# include <bluetooth/hci_lib.h>
#endif

#include "linuxmain.h"
#include "linuxutl.h"
#include "linuxbt.h"
#include "linuxsms.h"
#include "linuxudp.h"
#include "dictiter.h"
#include "main.h"
#include "gamesdb.h"
#include "linuxdict.h"
#include "relaycon.h"
#ifdef PLATFORM_NCURSES
# include "cursesmain.h"
#endif
#ifdef PLATFORM_GTK
# include "gtkboard.h"
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

#define DEFAULT_PORT 10997
#define DEFAULT_LISTEN_PORT 4998

static int blocking_read( int fd, unsigned char* buf, const int len );

XP_Bool
file_exists( const char* fileName )
{
    struct stat statBuf;

    int statResult = stat( fileName, &statBuf );
    // XP_LOGF( "%s(%s)=>%d", __func__, fileName, statResult == 0 );
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

    stream = mem_stream_make( MPPARM(cGlobals->util->mpool)
                              cGlobals->params->vtMgr, 
                              closure, CHANNEL_NONE, NULL );
    stream_putBytes( stream, buf, statBuf.st_size );
    free( buf );

    return stream;
} /* streamFromFile */

#ifdef USE_SQLITE
XWStreamCtxt*
streamFromDB( CommonGlobals* cGlobals, void* closure )
{
    XWStreamCtxt* stream = NULL;
    const LaunchParams* params = cGlobals->params;
    const char* name = params->dbFileName;
    XP_U32 rowid = params->dbFileID;
    sqlite3* ppDb;
    int res = sqlite3_open( name, &ppDb );
    if ( SQLITE_OK == res ) {
        sqlite3_blob* ppBlob;
        res = sqlite3_blob_open( ppDb, "main", "summaries", "SNAPSHOT", rowid,
                               0 /*flags*/, &ppBlob ); 
        if ( SQLITE_OK == res ) {
            int size = sqlite3_blob_bytes( ppBlob );
            XP_U8 buf[size];
            res = sqlite3_blob_read( ppBlob, buf, size, 0 );
            if ( SQLITE_OK == res ) {
                stream = mem_stream_make( MPPARM(cGlobals->util->mpool) 
                                          params->vtMgr, 
                                          closure, CHANNEL_NONE, NULL );
                stream_putBytes( stream, buf, size );
            }
        }
        sqlite3_blob_close( ppBlob );
    }

    if ( SQLITE_OK != res ) {
        XP_LOGF( "%s: error from sqlite: %s", __func__,
                 sqlite3_errmsg(ppDb) );
    }

    (void)sqlite3_close( ppDb );

    return stream;
}
#endif

DictionaryCtxt*
makeDictForStream( CommonGlobals* cGlobals, XWStreamCtxt* stream ) 
{
    CurGameInfo gi = {0};
    XWStreamPos pos = stream_getPos( stream, POS_READ );
    if ( !game_makeFromStream( MPPARM(cGlobals->util->mpool) stream, NULL, &gi,
                               NULL, NULL, NULL, NULL, NULL, NULL ) ) {
        XP_ASSERT(0);
    }
    stream_setPos( stream, POS_READ, pos );

    DictionaryCtxt* dict =
        linux_dictionary_make( MPPARM(cGlobals->util->mpool) cGlobals->params, 
                               gi.dictName, XP_TRUE );
    gi_disposePlayerInfo( MPPARM(cGlobals->util->mpool) &gi );
    XP_ASSERT( !!dict );
    return dict;
}

void
gameGotBuf( CommonGlobals* cGlobals, XP_Bool hasDraw, const XP_U8* buf, 
            XP_U16 len )
{
    XP_LOGF( "%s(hasDraw=%d)", __func__, hasDraw );
    XP_Bool redraw = XP_FALSE;
    XWGame* game = &cGlobals->game;
    XWStreamCtxt* stream = stream_from_msgbuf( cGlobals, buf, len );
    if ( !!stream ) {
        if ( comms_checkIncomingStream( game->comms, stream, NULL ) ) {
            redraw = server_receiveMessage( game->server, stream );
            if ( redraw ) {
                saveGame( cGlobals );
            }
        }
        stream_destroy( stream );

        /* if there's something to draw resulting from the message, we
           need to give the main loop time to reflect that on the screen
           before giving the server another shot.  So just call the idle
           proc. */
        if ( hasDraw && redraw ) {
            util_requestTime( cGlobals->util );
        } else {
            for ( int ii = 0; ii < 4; ++ii ) {
                redraw = server_do( game->server ) || redraw;
            }
        }
        if ( hasDraw && redraw ) {
            board_draw( game->board );
        }
    }
}

gint
requestMsgsIdle( gpointer data )
{
    CommonGlobals* cGlobals = (CommonGlobals*)data;
    XP_UCHAR devIDBuf[64] = {0};
    db_fetch( cGlobals->pDb, KEY_RDEVID, devIDBuf, sizeof(devIDBuf) );
    if ( '\0' != devIDBuf[0] ) {
        relaycon_requestMsgs( cGlobals->params, devIDBuf );
    } else {
        XP_LOGF( "%s: not requesting messages as don't have relay id", __func__ );
    }
    return 0;                   /* don't run again */
}

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
sendOnClose( XWStreamCtxt* stream, void* closure )
{
    CommonGlobals* cGlobals = (CommonGlobals*)closure;
    XP_LOGF( "%s called with msg of len %d", __func__, stream_getSize(stream) );
    (void)comms_send( cGlobals->game.comms, stream );
}

void
catGameHistory( CommonGlobals* cGlobals )
{
    if ( !!cGlobals->game.model ) {
        XP_Bool gameOver = server_getGameIsOver( cGlobals->game.server );
        XWStreamCtxt* stream = 
            mem_stream_make( MPPARM(cGlobals->util->mpool)
                             cGlobals->params->vtMgr,
                             NULL, CHANNEL_NONE, catOnClose );
        model_writeGameHistory( cGlobals->game.model, stream, 
                                cGlobals->game.server, gameOver );
        stream_putU8( stream, '\n' );
        stream_destroy( stream );
    }
} /* catGameHistory */

void
catFinalScores( const CommonGlobals* cGlobals, XP_S16 quitter )
{
    XWStreamCtxt* stream;
    XP_ASSERT( quitter < cGlobals->gi->nPlayers );

    stream = mem_stream_make( MPPARM(cGlobals->util->mpool)
                              cGlobals->params->vtMgr,
                              NULL, CHANNEL_NONE, catOnClose );
    if ( -1 != quitter ) {
        XP_UCHAR buf[128];
        XP_SNPRINTF( buf, VSIZE(buf), "Player %s resigned\n",
                     cGlobals->gi->players[quitter].name );
        stream_catString( stream, buf );
    }
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
saveGame( CommonGlobals* cGlobals )
{
    LOG_FUNC();
    if ( !!cGlobals->game.model &&
         (!!cGlobals->params->fileName || !!cGlobals->pDb) ) {
        XP_Bool doSave = XP_TRUE;
        XP_Bool newGame = !file_exists( cGlobals->params->fileName )
            || -1 == cGlobals->selRow;
        /* don't fail to save first time!  */
        if ( 0 < cGlobals->params->saveFailPct && !newGame ) {
            XP_U16 pct = XP_RANDOM() % 100;
            doSave = pct >= cGlobals->params->saveFailPct;
        }

        if ( doSave ) {
            XWStreamCtxt* outStream;
            MemStreamCloseCallback onClose = !!cGlobals->pDb?
                writeToDB : writeToFile;
            outStream = 
                mem_stream_make_sized( MPPARM(cGlobals->util->mpool)
                                       cGlobals->params->vtMgr, 
                                       cGlobals->lastStreamSize,
                                       cGlobals, 0, onClose );
            stream_open( outStream );

            game_saveToStream( &cGlobals->game, cGlobals->gi, 
                               outStream, ++cGlobals->curSaveToken );
            cGlobals->lastStreamSize = stream_getSize( outStream );
            stream_destroy( outStream );

            game_saveSucceeded( &cGlobals->game, cGlobals->curSaveToken );
            XP_LOGF( "%s: saved", __func__ );

            if ( !!cGlobals->pDb ) {
                summarize( cGlobals );
            }
        } else {
            XP_LOGF( "%s: simulating save failure", __func__ );
        }
    }
}

static void
handle_messages_from( CommonGlobals* cGlobals, const TransportProcs* procs,
                      int fdin )
{
    LOG_FUNC();
    LaunchParams* params = cGlobals->params;
    XWStreamCtxt* stream = 
        streamFromFile( cGlobals, params->fileName, cGlobals );

#ifdef DEBUG
    XP_Bool opened = 
#endif
        game_makeFromStream( MPPARM(cGlobals->util->mpool) 
                             stream, &cGlobals->game, 
                             cGlobals->gi, cGlobals->dict, 
                             &cGlobals->dicts, cGlobals->util, 
                             NULL /*draw*/,
                             &cGlobals->cp, procs );
    XP_ASSERT( opened );
    stream_destroy( stream );

    XP_Bool handled = XP_FALSE;
    unsigned short len;
    for ( ; ; ) {
        ssize_t nRead = blocking_read( fdin, (unsigned char*)&len, 
                                       sizeof(len) );
        if ( nRead != sizeof(len) ) {
            XP_LOGF( "%s: 1: unexpected nRead: %d", __func__, nRead );
            break;
        }
        len = ntohs( len );
        if ( 0 == len ) {
            break;
        }
        unsigned char buf[len];
        nRead = blocking_read( fdin, buf, len );
        if ( nRead != len ) {
            XP_LOGF( "%s: 2: unexpected nRead: %d", __func__, nRead );
            break;
        }
        stream = mem_stream_make( MPPARM(cGlobals->util->mpool) 
                                  params->vtMgr, cGlobals, CHANNEL_NONE, 
                                  NULL );
        stream_putBytes( stream, buf, len );

        if ( comms_checkIncomingStream( cGlobals->game.comms, 
                                        stream, NULL ) ) {
            ServerCtxt* server = cGlobals->game.server;
            (void)server_do( server );
            handled = server_receiveMessage( server, stream ) || handled;

            XP_U16 ii;
            for ( ii = 0; ii < 5; ++ii ) {
                (void)server_do( server );
            }
        }
        stream_destroy( stream );
    }

    LOG_RETURN_VOID();
} /* handle_messages_from */

void
read_pipe_then_close( CommonGlobals* cGlobals, const TransportProcs* procs )
{
    LOG_FUNC();
    LaunchParams* params = cGlobals->params;
    XWStreamCtxt* stream = 
        streamFromFile( cGlobals, params->fileName, cGlobals );

#ifdef DEBUG
    XP_Bool opened = 
#endif
        game_makeFromStream( MPPARM(cGlobals->util->mpool) 
                             stream, &cGlobals->game, 
                             cGlobals->gi, cGlobals->dict, 
                             &cGlobals->dicts, cGlobals->util, 
                             NULL /*draw*/,
                             &cGlobals->cp, procs );
    XP_ASSERT( opened );
    stream_destroy( stream );

    XP_Bool handled = XP_FALSE;
    int fd = open( params->pipe, O_RDWR );
    XP_ASSERT( fd >= 0 );
    if ( fd >= 0 ) {
        unsigned short len;
        for ( ; ; ) {
            ssize_t nRead = blocking_read( fd, (unsigned char*)&len, 
                                           sizeof(len) );
            if ( nRead != sizeof(len) ) {
                XP_LOGF( "%s: 1: unexpected nRead: %d", __func__, nRead );
                break;
            }
            len = ntohs( len );
            if ( 0 == len ) {
                break;
            }
            unsigned char buf[len];
            nRead = blocking_read( fd, buf, len );
            if ( nRead != len ) {
                XP_LOGF( "%s: 2: unexpected nRead: %d", __func__, nRead );
                break;
            }
            stream = mem_stream_make( MPPARM(cGlobals->util->mpool) 
                                      params->vtMgr, cGlobals, CHANNEL_NONE, 
                                      NULL );
            stream_putBytes( stream, buf, len );

            if ( comms_checkIncomingStream( cGlobals->game.comms, 
                                            stream, NULL ) ) {
                ServerCtxt* server = cGlobals->game.server;
                (void)server_do( server );
                handled = server_receiveMessage( server, stream ) || handled;

                XP_U16 ii;
                for ( ii = 0; ii < 5; ++ii ) {
                    (void)server_do( server );
                }
            }
            stream_destroy( stream );
        }

        /* 0-length packet closes it off */
        XP_LOGF( "%s: writing 0-length packet", __func__ );
        len = 0;
#ifdef DEBUG
        ssize_t nwritten = write( fd, &len, sizeof(len) );
        XP_ASSERT( nwritten == sizeof(len) );
#endif
        close( fd );
    }

    LOG_RETURN_VOID();
} /* read_pipe_then_close */

void
do_nbs_then_close( CommonGlobals* cGlobals, const TransportProcs* procs )
{
    LOG_FUNC();
    int sockfd = socket( AF_UNIX, SOCK_STREAM, 0 );

    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strcpy( addr.sun_path, cGlobals->params->nbs );

    unlink( cGlobals->params->nbs );
    int err = bind( sockfd, (struct sockaddr*)&addr, sizeof(addr) );
    if ( 0 != err ) {
        XP_LOGF( "%s: bind=>%s", __func__, strerror( errno ) );
        XP_ASSERT( 0 );
    }
    XP_LOGF( "calling listen" );
    err = listen( sockfd, 1 );
    assert( 0 == err );

    struct sockaddr remote;
    socklen_t addrlen = sizeof(remote);
    XP_LOGF( "calling accept" );
    int fd = accept( sockfd, &remote, &addrlen );
    XP_LOGF( "%s: accept=>%d", __func__, fd );
    assert( 0 <= fd );

    /* do stuff here */
    initNoConnStorage( cGlobals );
    handle_messages_from( cGlobals, procs, fd );
    writeNoConnMsgs( cGlobals, fd );

    /* Do I need this?  Will reader get err if I close? */
    unsigned short len = 0;
#ifdef DEBUG
    ssize_t nwritten = 
#endif
        write( fd, &len, sizeof(len) );
    XP_ASSERT( nwritten == sizeof(len) );

    close( fd );
    close( sockfd );
    LOG_RETURN_VOID();
} /* do_nbs_then_close */

#ifdef USE_GLIBLOOP
static gboolean
secondTimerFired( gpointer data )
{
    CommonGlobals* cGlobals = (CommonGlobals*)data;

    /* Undo */
    XWGame* game = &cGlobals->game;
    if ( !!game->server && !!game->board ) {
        XP_U16 undoRatio = cGlobals->params->undoRatio;
        if ( 0 != undoRatio ) {
            if ( (XP_RANDOM() % 100) < undoRatio ) {
                XP_LOGF( "%s: calling server_handleUndo", __func__ );
                if ( server_handleUndo( game->server, 1 ) ) {
                    board_draw( game->board );
                }
            } 
        }
    }

    return TRUE;
}

void
setOneSecondTimer( CommonGlobals* cGlobals )
{
    (void)g_timeout_add_seconds( 1, secondTimerFired, cGlobals );
}
#endif

typedef enum {
    CMD_HELP
    ,CMD_SKIP_GAMEOVER
    ,CMD_SHOW_OTHERSCORES
    ,CMD_HOSTIP
    ,CMD_DICT
#ifdef XWFEATURE_WALKDICT
    ,CMD_TESTDICT
    ,CMD_TESTPRFX
    ,CMD_TESTMINMAX
#endif
    ,CMD_DICTDIR
    ,CMD_PLAYERDICT
    ,CMD_SEED
#ifdef XWFEATURE_DEVID
    ,CMD_DEVID
    ,CMD_RDEVID
    ,CMD_NOANONDEVID
#endif
    ,CMD_GAMESEED
    ,CMD_GAMEFILE
    ,CMD_DBFILE
    ,CMD_SAVEFAIL_PCT
#ifdef USE_SQLITE
    ,CMD_GAMEDB_FILE
    ,CMD_GAMEDB_ID
#endif
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
    ,CMD_SPLITPACKETS
    ,CMD_CHAT
#ifdef XWFEATURE_CROSSHAIRS
    ,CMD_NOCROSSHAIRS
#endif
    ,CMD_ADDPIPE
    ,CMD_ADDNBS
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
    ,CMD_TRADEPCT
#endif
#ifdef USE_GLIBLOOP		/* just because hard to implement otherwise */
    ,CMD_UNDOPCT
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
    { CMD_HELP, false, "help", "print this message" }
    ,{ CMD_SKIP_GAMEOVER, false, "skip-final", "skip final scores display" }
    ,{ CMD_SHOW_OTHERSCORES, false, "show-other", "show robot/remote scores" }
    ,{ CMD_HOSTIP, true, "hostip", "remote host ip address (for direct connect)" }
    ,{ CMD_DICT, true, "game-dict", "dictionary name for game" }
#ifdef XWFEATURE_WALKDICT
    ,{ CMD_TESTDICT, true, "test-dict", "dictionary to be used for iterator test" }
    ,{ CMD_TESTPRFX, true, "test-prefix", "list first word starting with this" }
    ,{ CMD_TESTMINMAX, true, "test-minmax", "M:M -- include only words whose len in range" }
#endif
    ,{ CMD_DICTDIR, true, "dict-dir", "path to dir in which dicts will be sought" }
    ,{ CMD_PLAYERDICT, true, "player-dict", "dictionary name for player (in sequence)" }
    ,{ CMD_SEED, true, "seed", "random seed" }
#ifdef XWFEATURE_DEVID
    ,{ CMD_DEVID, true, "devid", "device ID (for testing GCM stuff)" }
    ,{ CMD_RDEVID, true, "rdevid", "relay's converted device ID (for testing GCM stuff)" }
    ,{CMD_NOANONDEVID, false, "no-anon-devid",
      "override default of using anonymous devid registration when no id provided" }
#endif
    ,{ CMD_GAMESEED, true, "game-seed", "game seed (for relay play)" }
    ,{ CMD_GAMEFILE, true, "file", "file to save to/read from" }
    ,{ CMD_DBFILE, true, "db", "sqlite3 db to store game data" }
    ,{ CMD_SAVEFAIL_PCT, true, "savefail-pct", "How often, at random, does save fail?" }
#ifdef USE_SQLITE
    ,{ CMD_GAMEDB_FILE, true, "game-db-file",
       "sqlite3 file, android format, holding game" }
    ,{ CMD_GAMEDB_ID, true, "game-db-id",
       "id of row of game we want (defaults to first)" }
#endif
    ,{ CMD_NOMMAP, false, "no-mmap", "copy dicts to memory rather than mmap them" }
    ,{ CMD_PRINTHISORY, false, "print-history", "print history on game over" }
    ,{ CMD_SKIPWARNINGS, false, "skip-warnings", "no modals on phonies" }
    ,{ CMD_LOCALPWD, true, "password", "password for user (in sequence)" }
    ,{ CMD_DUPPACKETS, false, "dup-packets", "send two of each to test dropping" }
    ,{ CMD_DROPNTHPACKET, true, "drop-nth-packet", 
       "drop this packet; default 0 (none)" }
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
    ,{ CMD_SPLITPACKETS, true, "split-packets", "send tcp packets in "
       "sections every random MOD <n> seconds to test relay reassembly" }
    ,{ CMD_CHAT, true, "send-chat", "send a chat every <n> seconds" }
#ifdef XWFEATURE_CROSSHAIRS
    ,{ CMD_NOCROSSHAIRS, false, "hide-crosshairs", 
       "don't show crosshairs on board" }
#endif
    ,{ CMD_ADDPIPE, true, "with-pipe", "named pipe to listen on for relay msgs" }
    ,{ CMD_ADDNBS, true, "with-nbs", 
       "nbs socket to listen/reply on for relay msgs" }
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
    ,{ CMD_PHONIES, true, "phonies", 
       "ignore (0, default), warn (1) or lose turn (2)" }
    ,{ CMD_BONUSFILE, true, "bonus-file",
       "provides bonus info: . + * ^ and ! are legal" }
#endif
#ifdef XWFEATURE_BLUETOOTH
    ,{ CMD_BTADDR, true, "btaddr", "bluetooth address of host" }
#endif
#ifdef XWFEATURE_SLOW_ROBOT
    ,{ CMD_SLOWROBOT, true, "slow-robot", "make robot slower to test network" }
    ,{ CMD_TRADEPCT, true, "trade-pct", "what pct of the time should robot trade" }
#endif
#ifdef USE_GLIBLOOP
    ,{ CMD_UNDOPCT, true, "undo-pct",
       "each second, what are the odds of doing an undo" }
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

const XP_UCHAR*
linux_getDevID( LaunchParams* params, DevIDType* typ )
{
    const XP_UCHAR* result = NULL;

    /* commandline takes precedence over stored values */

    if ( !!params->rDevID ) {
        result = params->rDevID;
        *typ = ID_TYPE_RELAY;
    } else if ( !!params->devID ) {
        result = params->devID;
        *typ = ID_TYPE_LINUX;
    } else if ( db_fetch( params->pDb, KEY_RDEVID, params->devIDStore, 
                          sizeof(params->devIDStore) ) ) {
        result = params->devIDStore;
        *typ = ID_TYPE_RELAY;
    } else if ( !params->noAnonDevid ) {
        *typ = ID_TYPE_ANON;
        result = "";
    }
    return result;
}

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
            XP_DEBUGF( "%s: socket returned -1\n", __func__ );
            goto done;
        }

        to_sock.sin_port = htons( addrRec->u.ip_relay.port );
        host = gethostbyname( addrRec->u.ip_relay.hostName );
        if ( NULL == host ) {
            XP_WARNF( "%s: gethostbyname(%s) failed",  __func__, 
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

            struct timeval tv = {0};
            tv.tv_sec = 15;
            setsockopt( sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv) );
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

typedef struct _SendQueueElem {
    XP_U32 id;
    size_t len;
    XP_U8* buf;
} SendQueueElem;

static void
free_elem_proc( gpointer data )
{
    SendQueueElem* elem = (SendQueueElem*)data;
    free( elem->buf );
    free( elem );
}

static bool
send_or_close( CommonGlobals* cGlobals, const XP_U8* buf, size_t len )
{
    size_t nSent = send( cGlobals->socket, buf, len, 0 );
    bool success = len == nSent;
    if ( !success ) {
        close( cGlobals->socket );
        (*cGlobals->socketChanged)( cGlobals->socketChangedClosure,
                                    cGlobals->socket, -1, 
                                    &cGlobals->storage );
        cGlobals->socket = -1;

        /* delete all pending packets since the socket's bad */
        g_slist_free_full( cGlobals->packetQueue, free_elem_proc );
        cGlobals->packetQueue = NULL;
    }
    LOG_RETURNF( "%d", success );
    return success;
}

static gboolean
sendTimerFired( gpointer data )
{
    CommonGlobals* cGlobals = (CommonGlobals*)data;
    if ( !!cGlobals->packetQueue ) {
        guint listLen = g_slist_length( cGlobals->packetQueue );
        assert( 0 < listLen );
        SendQueueElem* elem = (SendQueueElem*)cGlobals->packetQueue->data;
        cGlobals->packetQueue = cGlobals->packetQueue->next;

        XP_LOGF( "%s: sending packet %ld of len %d (%d left)", __func__, 
                 elem->id, elem->len, listLen - 1 );
        bool sent = send_or_close( cGlobals, elem->buf, elem->len );
        free( elem->buf );
        free( elem );

        if ( sent && 1 < listLen ) {
            int when = XP_RANDOM() % (1 + cGlobals->params->splitPackets);
            (void)g_timeout_add_seconds( when, sendTimerFired, cGlobals );
        }
    }

    return FALSE;
}

static bool
send_per_params( const XP_U8* buf, const XP_U16 buflen, 
                 CommonGlobals* cGlobals )
{
    bool success;
    if ( 0 == cGlobals->params->splitPackets ) {
        success = send_or_close( cGlobals, buf, buflen );
    } else {
        for ( int nSent = 0; nSent < buflen;  ) {
            int toSend = buflen / 2;
            if ( toSend > buflen - nSent ) {
                toSend = buflen - nSent;
            }
            SendQueueElem* elem = malloc( sizeof(*elem) );
            elem->id = ++cGlobals->nextPacketID;
            elem->buf = malloc( toSend );
            XP_MEMCPY( elem->buf, &buf[nSent], toSend );
            elem->len = toSend;
            cGlobals->packetQueue = 
                g_slist_append( cGlobals->packetQueue, elem );
            nSent += toSend;
            XP_LOGF( "%s: added packet %ld of len %d", __func__,
                     elem->id, elem->len );
        }
        int when = XP_RANDOM() % (1 + cGlobals->params->splitPackets);
        (void)g_timeout_add_seconds( when, sendTimerFired, cGlobals );
        success = TRUE;
    }
    return success;
}

static XP_S16
linux_tcp_send( CommonGlobals* cGlobals, const XP_U8* buf, XP_U16 buflen, 
                const CommsAddrRec* addrRec )
{
    XP_S16 result = 0;
    if ( !!cGlobals->pDb ) {
        XP_ASSERT( -1 != cGlobals->selRow );
        XP_U16 seed = comms_getChannelSeed( cGlobals->game.comms );
        XP_U32 clientToken = makeClientToken( cGlobals->selRow, seed );
        result = relaycon_send( cGlobals->params, buf, buflen, 
                                clientToken, addrRec );
    } else {
        int sock = cGlobals->socket;
    
        if ( sock == -1 ) {
            XP_LOGF( "%s: socket uninitialized", __func__ );
            sock = linux_init_relay_socket( cGlobals, addrRec );
            if ( sock != -1 ) {
                assert( cGlobals->socket == sock );
                (*cGlobals->socketChanged)( cGlobals->socketChangedClosure, 
                                            -1, sock, &cGlobals->storage );
            }
        }

        if ( sock != -1 ) {
            XP_U16 netLen = htons( buflen );
            XP_U8 tmp[buflen + sizeof(netLen)];
            XP_MEMCPY( &tmp[0], &netLen, sizeof(netLen) );
            XP_MEMCPY( &tmp[sizeof(netLen)], buf, buflen );

            if ( send_per_params( tmp, buflen + sizeof(netLen), cGlobals ) ) {
                result = buflen;
            }
        } else {
            XP_LOGF( "%s: socket still -1", __func__ );
        }
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
linux_send( const XP_U8* buf, XP_U16 buflen, const CommsAddrRec* addrRec,
            XP_U32 XP_UNUSED(gameID), void* closure )
{
    XP_S16 nSent = -1;
    CommonGlobals* cGlobals = (CommonGlobals*)closure;   
    CommsConnType conType;

    if ( !!addrRec ) {
        conType = addrRec->conType;
    } else {
        conType = cGlobals->params->conType;
    }

    if ( 0 ) {
#ifdef XWFEATURE_RELAY
    } else if ( conType == COMMS_CONN_RELAY ) {
        nSent = linux_tcp_send( cGlobals, buf, buflen, addrRec );
        if ( nSent == buflen && cGlobals->params->duplicatePackets ) {
#ifdef DEBUG
            XP_S16 sentAgain = 
#endif
                linux_tcp_send( cGlobals, buf, buflen, addrRec );
            XP_ASSERT( sentAgain == nSent );
        }

#endif
#if defined XWFEATURE_BLUETOOTH
    } else if ( conType == COMMS_CONN_BT ) {
        XP_Bool isServer = comms_getIsServer( cGlobals->game.comms );
        linux_bt_open( cGlobals, isServer );
        nSent = linux_bt_send( buf, buflen, addrRec, cGlobals );
#endif
#if defined XWFEATURE_IP_DIRECT
    } else if ( conType == COMMS_CONN_IP_DIRECT ) {
        CommsAddrRec addr;
        comms_getAddr( cGlobals->game.comms, &addr );
        linux_udp_open( cGlobals, &addr );
        nSent = linux_udp_send( buf, buflen, addrRec, cGlobals );
#endif
#if defined XWFEATURE_SMS
    } else if ( COMMS_CONN_SMS == conType ) {
        CommsAddrRec addr;
        if ( !addrRec ) {
            comms_getAddr( cGlobals->game.comms, &addr );
            addrRec = &addr;
        }
        nSent = linux_sms_send( cGlobals, buf, buflen, 
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

    XP_ASSERT( -1 == cGlobals->socket );

    close( socket );
}

static int
blocking_read( int fd, unsigned char* buf, const int len )
{
    int nRead = -1;
    if ( 0 <= fd && 0 < len ) {
        nRead = 0;
        int tries;
        for ( tries = 5; nRead < len && tries > 0; --tries ) {
            // XP_LOGF( "%s: blocking for %d bytes", __func__, len );
            ssize_t nGot = read( fd, buf + nRead, len - nRead );
            XP_LOGF( "%s: read(fd=%d, len=%d) => %d", __func__, fd, len - nRead, nGot );
            if ( nGot == 0 ) {
                XP_LOGF( "%s: read 0; let's try again (%d more times)", __func__, 
                         tries );
                usleep( 10000 );
            } else if ( nGot < 0 ) {
                XP_LOGF( "read => %d (wanted %d), errno=%d (\"%s\")", nRead, 
                         len - nRead, errno, strerror(errno) );
                break;
            }
            nRead += nGot;
        }

        if ( nRead < len ) {
            nRead = -1;
        }
    }

    XP_LOGF( "%s(fd=%d, sought=%d) => %d", __func__, fd, len, nRead );
    return nRead;
}

int
linux_relay_receive( CommonGlobals* cGlobals, unsigned char* buf, int bufSize )
{
    LOG_FUNC();
    int sock = cGlobals->socket;
    unsigned short tmp;
    ssize_t nRead = blocking_read( sock, (unsigned char*)&tmp, sizeof(tmp) );
    if ( nRead != 2 ) {
        linux_close_socket( cGlobals );
        comms_transportFailed( cGlobals->game.comms );
        nRead = -1;
    } else {
        unsigned short packetSize = ntohs( tmp );
        XP_LOGF( "%s: got packet of size %d", __func__, packetSize );
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
                nRead = blocking_read( sock, buf, packetSize );
                if ( nRead != packetSize ) {
                    nRead = -1;
                } else {
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
        }

        if ( -1 == nRead ) {
            linux_close_socket( cGlobals );
            comms_transportFailed( cGlobals->game.comms );
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
stream_from_msgbuf( CommonGlobals* globals, const unsigned char* bufPtr, 
                    XP_U16 nBytes )
{
    XWStreamCtxt* result;
    result = mem_stream_make( MPPARM(globals->util->mpool)
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
linux_util_informMissing( XW_UtilCtxt* XP_UNUSED(uc), 
                          XP_Bool XP_UNUSED_DBG(isServer), 
                          CommsConnType XP_UNUSED_DBG(conType),
                          XP_U16 XP_UNUSED_DBG(nMissing) )
{
    XP_LOGF( "%s(isServer=%d, conType=%d, nMissing=%d)", 
             __func__, isServer, conType, nMissing );
}

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

void
linuxSetIsServer( CommonGlobals* cGlobals, XP_Bool isServer )
{
    XP_LOGF( "%s(isServer=%d)", __func__, isServer );
    DeviceRole newRole = isServer? SERVER_ISSERVER : SERVER_ISCLIENT;
    cGlobals->params->serverRole = newRole;
    cGlobals->gi->serverRole = newRole;
}

void 
linuxChangeRoles( CommonGlobals* cGlobals )
{
    ServerCtxt* server = cGlobals->game.server;
    server_reset( server, cGlobals->game.comms );
    if ( SERVER_ISCLIENT == cGlobals->gi->serverRole ) {
        XWStreamCtxt* stream =
            mem_stream_make( MPPARM(cGlobals->util->mpool) cGlobals->params->vtMgr,
                             cGlobals, CHANNEL_NONE, sendOnClose );
        server_initClientConnection( server, stream );
    }
    (void)server_do( server );
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
    exit(0);
}

#ifdef XWFEATURE_WALKDICT
//# define PRINT_ALL
static void
testGetNthWord( const DictionaryCtxt* dict, char** XP_UNUSED_DBG(words),
                XP_U16 depth, IndexData* data, XP_U16 min, XP_U16 max )
{
    XP_UCHAR buf[64];
    XP_U32 ii, jj;
    DictIter iter;

    dict_initIter( &iter, dict, min, max );
    XP_U32 half = dict_countWords( &iter, NULL ) / 2;
    XP_U32 interval = half / 100;
    if ( interval == 0 ) {
        ++interval;
    }

    for ( ii = 0, jj = half; ii < half; ii += interval, jj += interval ) {
        if ( dict_getNthWord( &iter, ii, depth, data ) ) {
            dict_wordToString( &iter, buf, VSIZE(buf) );
            XP_ASSERT( 0 == strcmp( buf, words[ii] ) );
# ifdef PRINT_ALL
            XP_LOGF( "%s: word[%ld]: %s", __func__, ii, buf );
# endif
        } else {
            XP_ASSERT( 0 );
        }
        if ( dict_getNthWord( &iter, jj, depth, data ) ) {
            dict_wordToString( &iter, buf, VSIZE(buf) );
            XP_ASSERT( 0 == strcmp( buf, words[jj] ) );
# ifdef PRINT_ALL
            XP_LOGF( "%s: word[%ld]: %s", __func__, jj, buf );
# endif
        } else {
            XP_ASSERT( 0 );
        }
    }
}

static void
walk_dict_test( MPFORMAL const DictionaryCtxt* dict, 
                GSList* testPrefixes, const char* testMinMax )
{
    /* This is just to test that the dict-iterating code works.  The words are
       meant to be printed e.g. in a scrolling dialog on Android. */
    DictIter iter;
    long jj;
    XP_Bool gotOne;

    XP_U16 min, max;
    if ( !testMinMax || !parsePair( testMinMax, &min, &max ) ) {
        min = 0;
        max = MAX_COLS_DICT;
    }

    dict_initIter( &iter, dict, min, max  );
    LengthsArray lens;
    XP_U32 count = dict_countWords( &iter, &lens );

    XP_U32 sum = 0;
    for ( jj = 0; jj < VSIZE(lens.lens); ++jj ) {
        sum += lens.lens[jj];
        XP_LOGF( "%ld words of length %ld", lens.lens[jj], jj );
    }
    XP_ASSERT( sum == count );

    if ( count > 0 ) {
        char** words = g_malloc( count * sizeof(char*) );
        XP_ASSERT( !!words );

        for ( jj = 0, gotOne = dict_firstWord( &iter );
              gotOne;
              gotOne = dict_getNextWord( &iter ) ) {
            XP_ASSERT( dict_getPosition( &iter ) == jj );
            XP_UCHAR buf[64];
            dict_wordToString( &iter, buf, VSIZE(buf) );
# ifdef PRINT_ALL
            fprintf( stderr, "%.6ld: %s\n", jj, buf );
# endif
            if ( !!words ) {
                words[jj] = g_strdup( buf );
            }
            ++jj;
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
        testGetNthWord( dict, words, 0, NULL, min, max );

        XP_U16 depth = 2;
        XP_U16 maxCount = dict_numTileFaces( dict );
        IndexData data;
        data.count = maxCount * maxCount;
        data.indices = XP_MALLOC( mpool,
                                  data.count * depth * sizeof(data.indices[0]) );
        data.prefixes = XP_MALLOC( mpool,
                                   depth * data.count * sizeof(data.prefixes[0]) );

        XP_LOGF( "making index..." );
        dict_makeIndex( &iter, depth, &data );
        XP_LOGF( "DONE making index" );

        data.indices = XP_REALLOC( mpool, data.indices, 
                                   data.count * depth * sizeof(*data.indices) );
        data.prefixes = XP_REALLOC( mpool, data.prefixes,
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
            dict_tilesToString( dict, &prefixes[depth*ii], depth, prfx, 
                                VSIZE(prfx) );
            fprintf( stderr, "%d: index: %ld; prefix: %s; word: %s (prev: %s)\n", 
                     ii, indices[ii], prfx, buf1, buf2 );
        }
#endif

        XP_LOGF( "testing getNth WITH INDEXING" );
        testGetNthWord( dict, words, depth, &data, min, max );

        if ( !!testPrefixes ) {
            int ii;
            guint count = g_slist_length( testPrefixes );
            for ( ii = 0; ii < count; ++ii ) {
                gchar* prefix = (gchar*)g_slist_nth_data( testPrefixes, ii );
                XP_S16 lenMatched = dict_findStartsWith( &iter, prefix );
                if ( 0 <= lenMatched ) {
                    XP_UCHAR buf[32];
                    XP_UCHAR bufPrev[32] = {0};
                    dict_wordToString( &iter, buf, VSIZE(buf) );

                    /* This doesn't work with synonyms like "L-L" for "L·L" */
                    // XP_ASSERT( 0 == strncasecmp( buf, prefix, lenMatched ) );

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
        XP_FREE( mpool, data.indices );
        XP_FREE( mpool, data.prefixes );
    }
    XP_LOGF( "done" );
}

static void
walk_dict_test_all( MPFORMAL const LaunchParams* params, GSList* testDicts, 
                    GSList* testPrefixes, const char* testMinMax )
{
    int ii;
    guint count = g_slist_length( testDicts );
    for ( ii = 0; ii < count; ++ii ) {
        gchar* name = (gchar*)g_slist_nth_data( testDicts, ii );
        DictionaryCtxt* dict = 
            linux_dictionary_make( MPPARM(mpool) params, name,
                                   params->useMmap );
        if ( NULL != dict ) {
            XP_LOGF( "walk_dict_test(%s)", name );
            walk_dict_test( MPPARM(mpool) dict, testPrefixes, testMinMax );
            dict_destroy( dict );
        }
    }
}
#endif

static void
dumpDict( DictionaryCtxt* dict )
{
    DictIter iter;
    dict_initIter( &iter, dict, 0, MAX_COLS_DICT );
    for ( XP_Bool result = dict_firstWord( &iter ); 
          result; 
          result = dict_getNextWord( &iter ) ) {
        XP_UCHAR buf[32];
        dict_wordToString( &iter, buf, VSIZE(buf) );
        fprintf( stdout, "%s\n", buf );
    }
}

static void
trimDictPath( const char* input, char* buf, int bufsiz, char** path, char** dict )
{
    char unlinked[256];
    XP_ASSERT( strlen(input) < VSIZE(unlinked) );
    ssize_t siz = readlink( input, unlinked, VSIZE(unlinked) );
    if ( 0 <= siz ) {
        unlinked[siz] = '\0';
        input = unlinked;
    }

    struct stat statBuf;
    int statResult = stat( input, &statBuf );
    if ( 0 == statResult && S_ISLNK(statBuf.st_mode) ) {
        ssize_t nWritten = readlink( input, buf, bufsiz );
        buf[nWritten] = '\0';
    } else {
        snprintf( buf, bufsiz, "%s", input );
    }

    char* result = strrchr( buf, '/' );
    if ( !!result ) {           /* is is a full path */
        *path = buf;
        *result = '\0';         /* null-terminate it */
        *dict = 1 + result;
    } else {
        *path = NULL;
        *dict = buf;
    }
    char* dot = strrchr( *dict, '.' );
    if ( !!dot && 0 == strcmp(dot, ".xwd") ) {
        *dot = '\0';
    }
    XP_LOGF( "%s => dict: %s; path: %s", __func__, *dict, *path );
}

XP_Bool
getDictPath( const LaunchParams *params, const char* name, 
             char* result, int resultLen )
{
    XP_Bool success = XP_FALSE;
    GSList* iter;
    result[0] = '\0';
    for ( iter = params->dictDirs; !!iter; iter = iter->next ) {
        const char* path = iter->data;
        char buf[256];
        int len = snprintf( buf, VSIZE(buf), "%s/%s.xwd", path, name );
        if ( len < VSIZE(buf) && file_exists( buf ) ) {
            snprintf( result, resultLen, "%s", buf );
            success = XP_TRUE;
            break;
        }
    }
    XP_LOGF( "%s(%s)=>%d", __func__, name, success );
    return success;
}

GSList* 
listDicts( const LaunchParams *params )
{
    GSList* result = NULL;
    GSList* iter = params->dictDirs;
    while ( !!iter ) {
        const gchar *path = iter->data;
        GDir* dir = g_dir_open( path, 0, NULL );
        if ( !!dir ) {
            for ( ; ; ) {
                const gchar* name = g_dir_read_name( dir );
                if ( !name ) {
                    break;
                }
                if ( g_str_has_suffix( name, ".xwd" ) ) {
                    gint len = strlen(name) - 4;
                    result = g_slist_prepend( result, g_strndup( name, len ) );
                }
            }
            g_dir_close( dir );
        }
        iter = iter->next;
    }
    return result;
}

void
setupLinuxUtilCallbacks( XW_UtilCtxt* util )
{
#ifndef XWFEATURE_STANDALONE_ONLY
    util->vtable->m_util_informMissing = linux_util_informMissing;
    util->vtable->m_util_addrChange = linux_util_addrChange;
#endif
}

/* Set up cGlobals->gi and cGlobals->addr based on params fields */
void
initFromParams( CommonGlobals* cGlobals, LaunchParams* params )
{
    LOG_FUNC();
    /* CurGameInfo */
    cGlobals->gi = &params->pgi;

    /* addr */
    CommsAddrRec* addr = &cGlobals->addr;
    XP_MEMSET( addr, 0, sizeof(*addr) );
    if ( 0 ) {
#ifdef XWFEATURE_RELAY
    } else if ( params->conType == COMMS_CONN_RELAY ) {
        addr->conType = COMMS_CONN_RELAY;
        addr->u.ip_relay.ipAddr = 0;       /* ??? */
        addr->u.ip_relay.port = params->connInfo.relay.defaultSendPort;
        addr->u.ip_relay.seeksPublicRoom = 
            params->connInfo.relay.seeksPublicRoom;
        addr->u.ip_relay.advertiseRoom = params->connInfo.relay.advertiseRoom;
        XP_STRNCPY( addr->u.ip_relay.hostName, 
                    params->connInfo.relay.relayName,
                    sizeof(addr->u.ip_relay.hostName) - 1 );
        XP_STRNCPY( addr->u.ip_relay.invite, params->connInfo.relay.invite,
                    sizeof(addr->u.ip_relay.invite) - 1 );
#endif
#ifdef XWFEATURE_SMS
    } else if ( params->conType == COMMS_CONN_SMS ) {
        addr->conType = COMMS_CONN_SMS;
        XP_STRNCPY( addr->u.sms.phone, params->connInfo.sms.serverPhone,
                    sizeof(addr->u.sms.phone) - 1 );
        addr->u.sms.port = params->connInfo.sms.port;
#endif
#ifdef XWFEATURE_BLUETOOTH
    } else if ( params->conType == COMMS_CONN_BT ) {
        addr->conType = COMMS_CONN_BT;
        XP_ASSERT( sizeof(addr->u.bt.btAddr) 
                   >= sizeof(params->connInfo.bt.hostAddr));
        XP_MEMCPY( &addr->u.bt.btAddr, &params->connInfo.bt.hostAddr,
                   sizeof(params->connInfo.bt.hostAddr) );
#endif
    }
}

void
setupUtil( CommonGlobals* cGlobals )
{
    XW_UtilCtxt* util = calloc( 1, sizeof(*util) );
    cGlobals->util = util;
    linux_util_vt_init( MPPARM(cGlobals->params->mpool) util );
    util->gameInfo = cGlobals->gi;
    setupLinuxUtilCallbacks( util );
}

static void
initParams( LaunchParams* params )
{
    memset( params, 0, sizeof(*params) );

    // params->util = calloc( 1, sizeof(*params->util) );
    /* XP_MEMSET( params->util, 0, sizeof(params->util) ); */

#ifdef MEM_DEBUG
    params->mpool = mpool_make();
#endif

    params->vtMgr = make_vtablemgr(MPPARM_NOCOMMA(params->mpool));
    // linux_util_vt_init( MPPARM(params->mpool) params->util );
#ifndef XWFEATURE_STANDALONE_ONLY
    /* params->util->vtable->m_util_informMissing = linux_util_informMissing; */
    /* params->util->vtable->m_util_addrChange = linux_util_addrChange; */
    /* params->util->vtable->m_util_setIsServer = linux_util_setIsServer; */
#endif
}

static void
freeParams( LaunchParams* params )
{
    vtmgr_destroy( MPPARM(params->mpool) params->vtMgr );

    gi_disposePlayerInfo( MPPARM(params->mpool) &params->pgi );
    mpool_destroy( params->mpool );
}

static int
dawg2dict( const LaunchParams* params, GSList* testDicts )
{
    guint count = g_slist_length( testDicts );
    for ( int ii = 0; ii < count; ++ii ) {
        DictionaryCtxt* dict = 
            linux_dictionary_make( MPPARM(params->mpool) params, 
                                   g_slist_nth_data( testDicts, ii ),
                                   params->useMmap );
        if ( NULL != dict ) {
            dumpDict( dict );
            dict_destroy( dict );
        }
    }
    return 0;
}

int
main( int argc, char** argv )
{
    XP_Bool useCurses;
    int opt;
    int totalPlayerCount = 0;
    XP_Bool isServer = XP_FALSE;
    char* portNum = NULL;
    char* hostName = "localhost";
    unsigned int seed = defaultRandomSeed();
    LaunchParams mainParams;
    XP_U16 nPlayerDicts = 0;
    XP_U16 robotCount = 0;
    /* XP_U16 ii; */
#ifdef XWFEATURE_WALKDICT
    GSList* testDicts = NULL;
    GSList* testPrefixes = NULL;
    char* testMinMax = NULL;
#endif
    char dictbuf[256];
    char* dict;
    char* path;

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

    initParams( &mainParams );

    /* defaults */
#ifdef XWFEATURE_RELAY
    mainParams.connInfo.relay.defaultSendPort = DEFAULT_PORT;
    mainParams.connInfo.relay.relayName = "localhost";
    mainParams.connInfo.relay.invite = "INVITE";
#endif
#ifdef XWFEATURE_IP_DIRECT
    mainParams.connInfo.ip.port = DEFAULT_PORT;
    mainParams.connInfo.ip.hostName = "localhost";
#endif
    mainParams.pgi.boardSize = 15;
    mainParams.quitAfter = -1;
    mainParams.sleepOnAnchor = XP_FALSE;
    mainParams.printHistory = XP_FALSE;
    mainParams.undoWhenDone = XP_FALSE;
    mainParams.pgi.timerEnabled = XP_FALSE;
    mainParams.pgi.dictLang = -1;
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

    char* envDictPath = getenv( "XW_DICTDIR" );
    XP_LOGF( "%s: envDictPath=%s", __func__, envDictPath );
    if ( !!envDictPath ) {
        char *saveptr;
        for ( ; ; ) {
            char* path = strtok_r( envDictPath, ":", &saveptr );
            if ( !path ) {
                break;
            }
            mainParams.dictDirs = g_slist_append( mainParams.dictDirs, path );
            envDictPath = NULL;
        }
    }

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
        case CMD_HELP:
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
            // isServer = XP_TRUE; /* implicit */
            break;
#endif
        case CMD_HOSTIP:
            XP_ASSERT( conType == COMMS_CONN_NONE ||
                       conType == COMMS_CONN_IP_DIRECT );
            hostName = optarg;
            conType = COMMS_CONN_IP_DIRECT;
            break;
        case CMD_DICT:
            trimDictPath( optarg, dictbuf, VSIZE(dictbuf), &path, &dict );
            mainParams.pgi.dictName = copyString( mainParams.mpool, dict );
            if ( !path ) {
                path = ".";
            }
            mainParams.dictDirs = g_slist_append( mainParams.dictDirs, path );
            break;
#ifdef XWFEATURE_WALKDICT
        case CMD_TESTDICT:
            testDicts = g_slist_prepend( testDicts, g_strdup(optarg) );
            break;
        case CMD_TESTPRFX:
            testPrefixes = g_slist_prepend( testPrefixes, g_strdup(optarg) );
            break;
        case CMD_TESTMINMAX:
            testMinMax = optarg;
            break;
#endif
        case CMD_DICTDIR:
            mainParams.dictDirs = g_slist_append( mainParams.dictDirs, optarg );
            break;
        case CMD_PLAYERDICT:
            trimDictPath( optarg, dictbuf, VSIZE(dictbuf), &path, &dict );
            mainParams.playerDictNames[nPlayerDicts++] = dict;
            if ( !path ) {
                path = ".";
            }
            mainParams.dictDirs = g_slist_append( mainParams.dictDirs, path );
            break;
        case CMD_SEED:
            seed = atoi(optarg);
            break;
#ifdef XWFEATURE_DEVID
        case CMD_DEVID:
            mainParams.devID = optarg;
            break;
        case CMD_RDEVID:
            mainParams.rDevID = optarg;
            break;
        case CMD_NOANONDEVID:
            mainParams.noAnonDevid = true;
#endif
        case CMD_GAMESEED:
            mainParams.gameSeed = atoi(optarg);
            break;
        case CMD_GAMEFILE:
            mainParams.fileName = optarg;
            break;
        case CMD_DBFILE:
            mainParams.dbName = optarg;
            break;
        case CMD_SAVEFAIL_PCT:
            mainParams.saveFailPct = atoi( optarg );
            break;

#ifdef USE_SQLITE
        case CMD_GAMEDB_FILE:
            /* Android isn't using XWFEATURE_SEARCHLIMIT, and it writes to
               stream, so to read an android DB is to invite mayhem. */
# ifdef XWFEATURE_SEARCHLIMIT
            usage( argv[0], "Don't open android DBs without "
                   "disabling XWFEATURE_SEARCHLIMIT" );
# endif
            mainParams.dbFileName = optarg;
        case CMD_GAMEDB_ID:
            mainParams.dbFileID = atoi(optarg);
            break;
#endif
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
            mainParams.pgi.players[mainParams.nLocalPlayers-1].password
                = (XP_UCHAR*)optarg;
            break;
        case CMD_LOCALSMARTS:
            index = mainParams.pgi.nPlayers - 1;
            XP_ASSERT( LP_IS_ROBOT( &mainParams.pgi.players[index] ) );
            mainParams.pgi.players[index].robotIQ = atoi(optarg);
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
            mainParams.pgi.hintsNotAllowed = XP_TRUE;
            break;
        case CMD_PICKTILESFACEUP:
            mainParams.pgi.allowPickTiles = XP_TRUE;
            break;
        case CMD_PLAYERNAME:
            index = mainParams.pgi.nPlayers++;
            ++mainParams.nLocalPlayers;
            mainParams.pgi.players[index].robotIQ = 0; /* means human */
            mainParams.pgi.players[index].isLocal = XP_TRUE;
            mainParams.pgi.players[index].name = 
                copyString( mainParams.mpool, (XP_UCHAR*)optarg);
            break;
        case CMD_REMOTEPLAYER:
            index = mainParams.pgi.nPlayers++;
            mainParams.pgi.players[index].isLocal = XP_FALSE;
            ++mainParams.info.serverInfo.nRemotePlayers;
            break;
        case CMD_PORT:
            /* could be RELAY or IP_DIRECT or SMS */
            portNum = optarg;
            break;
        case CMD_ROBOTNAME:
            ++robotCount;
            index = mainParams.pgi.nPlayers++;
            ++mainParams.nLocalPlayers;
            mainParams.pgi.players[index].robotIQ = 1; /* real smart by default */
            mainParams.pgi.players[index].isLocal = XP_TRUE;
            mainParams.pgi.players[index].name = 
                copyString( mainParams.mpool, (XP_UCHAR*)optarg);
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
            mainParams.pgi.gameSeconds = atoi(optarg) * 60;
            mainParams.pgi.timerEnabled = XP_TRUE;
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
                mainParams.pgi.phoniesAction = PHONIES_IGNORE;
                break;
            case 1:
                mainParams.pgi.phoniesAction = PHONIES_WARN;
                break;
            case 2:
                mainParams.pgi.phoniesAction = PHONIES_DISALLOW;
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
            mainParams.closeStdin = XP_TRUE;
            break;
        case CMD_QUITAFTER:
            mainParams.quitAfter = atoi(optarg);
            break;
        case CMD_BOARDSIZE:
            mainParams.pgi.boardSize = atoi(optarg);
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
            break;
        case CMD_SPLITPACKETS:
            mainParams.splitPackets = atoi( optarg );
            break;
        case CMD_CHAT:
            mainParams.chatsInterval = atoi(optarg);
            break;
#ifdef XWFEATURE_CROSSHAIRS
        case CMD_NOCROSSHAIRS:
            mainParams.hideCrosshairs = XP_TRUE;
            break;
#endif
        case CMD_ADDPIPE:
            mainParams.pipe = optarg;
            break;
        case CMD_ADDNBS:
            mainParams.nbs = optarg;
            break;
#ifdef XWFEATURE_SLOW_ROBOT
        case CMD_SLOWROBOT:
            if ( !parsePair( optarg, &mainParams.robotThinkMin,
                             &mainParams.robotThinkMax ) ) {
                usage(argv[0], "bad param" );
            }
            break;
        case CMD_TRADEPCT:
            mainParams.robotTradePct = atoi( optarg );
            if ( mainParams.robotTradePct < 0 || mainParams.robotTradePct > 100 ) {
                usage(argv[0], "must be 0 <= n <= 100" );
            }
            break;
#endif

#ifdef USE_GLIBLOOP
	case CMD_UNDOPCT:
            mainParams.undoRatio = atoi( optarg );
            if ( mainParams.undoRatio < 0 || mainParams.undoRatio > 100 ) {
                usage(argv[0], "must be 0 <= n <= 100" );
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

    int result = 0;
    if ( g_str_has_suffix( argv[0], "dawg2dict" ) ) {
        result = dawg2dict( &mainParams, testDicts );
    } else {
        XP_ASSERT( mainParams.pgi.nPlayers == mainParams.nLocalPlayers
                   + mainParams.info.serverInfo.nRemotePlayers );

        if ( isServer ) {
            if ( mainParams.info.serverInfo.nRemotePlayers == 0 ) {
                mainParams.pgi.serverRole = SERVER_STANDALONE;
            } else {
                mainParams.pgi.serverRole = SERVER_ISSERVER;
            }
        } else {
            mainParams.pgi.serverRole = SERVER_ISCLIENT;
        }

        /* sanity checks */
        totalPlayerCount = mainParams.nLocalPlayers 
            + mainParams.info.serverInfo.nRemotePlayers;
        if ( !mainParams.fileName
#ifdef USE_SQLITE
             && !mainParams.dbFileName 
#endif
             ) {
            if ( (totalPlayerCount < 1) || 
                 (totalPlayerCount > MAX_NUM_PLAYERS) ) {
                mainParams.needsNewGame = XP_TRUE;
            }
        }

        if ( !!mainParams.pgi.dictName ) {
            /* char path[256]; */
            /* getDictPath( &mainParams, mainParams.gi.dictName, path, VSIZE(path) ); */
            /* mainParams.dict =  */
            /*     linux_dictionary_make( MPPARM(mainParams.mpool) &mainParams, */
            /*                            mainParams.pgi.dictName, */
            /*                            mainParams.useMmap ); */
            /* XP_ASSERT( !!mainParams.dict ); */
            /* mainParams.pgi.dictLang = dict_getLangCode( mainParams.dict ); */
        } else if ( isServer ) {
#ifdef STUBBED_DICT
            mainParams.dict = 
                make_stubbed_dict( MPPARM_NOCOMMA(mainParams.util->mpool) );
            XP_WARNF( "no dictionary provided: using English stub dict\n" );
            mainParams.pgi.dictLang = dict_getLangCode( mainParams.dict );
#else
            if ( 0 == nPlayerDicts ) {
                mainParams.needsNewGame = XP_TRUE;
            }
#endif
        } else if ( robotCount > 0 ) {
            mainParams.needsNewGame = XP_TRUE;
        }

        if ( 0 < mainParams.info.serverInfo.nRemotePlayers
             && SERVER_STANDALONE == mainParams.pgi.serverRole ) {
            mainParams.needsNewGame = XP_TRUE;
        }

        /* per-player dicts are for local players only.  Assign in the order
           given.  It's an error to give too many, or not to give enough if
           there's no game-dict */
        if ( 0 < nPlayerDicts ) {
            /* XP_U16 nextDict = 0; */
            /* for ( ii = 0; ii < mainParams.gi.nPlayers; ++ii ) { */
            /*     if ( mainParams.gi.players[ii].isLocal ) { */
            /*         const XP_UCHAR* name = mainParams.playerDictNames[nextDict++]; */
            /*         XP_ASSERT( !!name ); */
            /*         mainParams.dicts.dicts[ii] =  */
            /*             linux_dictionary_make( MPPARM(mainParams.util->mpool)  */
            /*                                    &mainParams, name, mainParams.useMmap ); */
            /*     } */
            /* } */
            /* if ( nextDict < nPlayerDicts ) { */
            /*     usage( argv[0], " --player-dict used more times than there are " */
            /*            "local players" ); */
            /* } */
        }

        /* if ( !isServer ) { */
        /*     if ( mainParams.info.serverInfo.nRemotePlayers > 0 ) { */
        /*         mainParams.needsNewGame = XP_TRUE; */
        /*     }	     */
        /* } */
#ifdef XWFEATURE_WALKDICT
        if ( !!testDicts ) {
            walk_dict_test_all( MPPARM(mainParams.mpool) &mainParams, testDicts, 
                                testPrefixes, testMinMax );
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

        // mainParams.util->gameInfo = &mainParams.pgi;

        srandom( seed );	/* init linux random number generator */
        XP_LOGF( "seeded srandom with %d", seed );

        if ( mainParams.closeStdin ) {
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

        /* if ( mainParams.needsNewGame ) { */
        /*     gi_disposePlayerInfo( MPPARM(mainParams.mpool) &mainParams.pgi ); */
        /*     gi_initPlayerInfo( MPPARM(mainParams.mpool) &mainParams.pgi, NULL ); */
        /* } */

        if ( useCurses ) {
            if ( mainParams.needsNewGame ) {
                /* curses doesn't have newgame dialog */
                usage( argv[0], "game params required for curses version" );
            } else {
#if defined PLATFORM_NCURSES
                cursesmain( isServer, &mainParams );
#endif
            }
        } else {
#if defined PLATFORM_GTK
            gtk_init( &argc, &argv );
            gtkmain( &mainParams );
#endif
        }

        freeParams( &mainParams );
    }

    XP_LOGF( "%s exiting main, returning %d", argv[0], result );
    return result;
} /* main */

