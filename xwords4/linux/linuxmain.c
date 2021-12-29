/* -*- compile-command: "make MEMDEBUG=TRUE -j3"; -*- */
/* 
 * Copyright 2000 - 2020 by Eric House (xwords@eehouse.org).  All rights
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
#include "lindutil.h"
#include "relaycon.h"
#include "mqttcon.h"
#include "smsproto.h"
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
#include "dbgutil.h"
#include "dictiter.h"
#include "gsrcwrap.h"
/* #include "commgr.h" */
/* #include "compipe.h" */
#include "memstream.h"
#include "LocalizedStrIncludes.h"

#define DEFAULT_PORT 10997
#define DEFAULT_LISTEN_PORT 4998

#ifdef MEM_DEBUG
# define MEMPOOL cGlobals->util->mpool,
#else
# define MEMPOOL
#endif

static int blocking_read( int fd, unsigned char* buf, const int len );

XP_Bool
file_exists( const char* fileName )
{
    XP_Bool exists = !!fileName;
    if ( exists ) {
        struct stat statBuf;

        int statResult = stat( fileName, &statBuf );
        // XP_LOGF( "%s(%s)=>%d", __func__, fileName, statResult == 0 );
        exists = statResult == 0;
    }
    return exists;
} /* file_exists */

XWStreamCtxt*
streamFromFile( CommonGlobals* cGlobals, char* name )
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

    stream = mem_stream_make_raw( MPPARM(cGlobals->util->mpool)
                                  cGlobals->params->vtMgr );
    stream_putBytes( stream, buf, statBuf.st_size );
    free( buf );

    return stream;
} /* streamFromFile */

void
tryConnectToServer( CommonGlobals* cGlobals )
{
    (void)server_initClientConnection( cGlobals->game.server, NULL_XWE );
}

void
ensureLocalPlayerNames( LaunchParams* XP_UNUSED_DBG(params), CurGameInfo* gi )
{
    for ( int ii = 0; ii < gi->nPlayers; ++ii ) {
        LocalPlayer* lp = &gi->players[ii];
        if ( lp->isLocal && !lp->name ) {
            replaceStringIfDifferent( params->mpool,
                                      &lp->name, "LPlayer" );
        }
    }
}

#if 0
static bool
canMakeFromGI( const CurGameInfo* gi )
{
    LOG_FUNC();
    bool result = 0 < gi->nPlayers
        && 0 < gi->dictLang
        ;
    bool haveDict = !!gi->dictName;
    bool allHaveDicts = true;
    for ( int ii = 0; result && ii < gi->nPlayers; ++ii ) {
        const LocalPlayer* lp = &gi->players[ii];
        result = !lp->isLocal || (!!lp->name && '\0' != lp->name[0]);
        if ( allHaveDicts ) {
            allHaveDicts = !!lp->dictName;
        }
    }

    result = result && (haveDict || allHaveDicts);

    LOG_RETURNF( "%d", result );
    XP_ASSERT( result );
    return result;
}
#endif

bool
linuxOpenGame( CommonGlobals* cGlobals, const TransportProcs* procs,
               const CommsAddrRec* returnAddrP )
{
    LOG_FUNC();
    XWStreamCtxt* stream = NULL;
    XP_Bool opened = XP_FALSE;

    LaunchParams* params = cGlobals->params;
    if ( !!params->fileName && file_exists( params->fileName ) ) {
        stream = streamFromFile( cGlobals, params->fileName );
#ifdef USE_SQLITE
    } else if ( !!params->dbFileName && file_exists( params->dbFileName ) ) {
        XP_UCHAR buf[32];
        XP_SNPRINTF( buf, sizeof(buf), "%d", params->dbFileID );
        mpool_setTag( MEMPOOL buf );
        stream = streamFromDB( cGlobals );
#endif
    } else if ( !!params->pDb && 0 <= cGlobals->rowid ) {
        stream = mem_stream_make_raw( MPPARM(cGlobals->util->mpool)
                                      params->vtMgr );
        if ( !gdb_loadGame( stream, params->pDb, cGlobals->rowid ) ) {
            stream_destroy( stream, NULL_XWE);
            stream = NULL;
        }
    }

    if ( !!stream ) {
        opened = game_makeFromStream( MEMPOOL NULL_XWE, stream, &cGlobals->game,
                                      cGlobals->gi,
                                      cGlobals->util, cGlobals->draw,
                                      &cGlobals->cp, procs );
        XP_LOGF( "%s: loaded gi at %p", __func__, &cGlobals->gi );
        stream_destroy( stream, NULL_XWE );
    }

    if ( !opened /* && canMakeFromGI( cGlobals->gi )*/ ) {
        opened = XP_TRUE;

        game_makeNewGame( MEMPOOL NULL_XWE, &cGlobals->game, cGlobals->gi,
                          cGlobals->util, cGlobals->draw,
                          &cGlobals->cp, procs
#ifdef SET_GAMESEED
                          , params->gameSeed
#endif
                          );

        bool savedGame = false;
        CommsAddrRec returnAddr = {0};
        if ( !!returnAddrP ) {
            returnAddr = *returnAddrP;
            CommsConnType typ;
            for ( XP_U32 st = 0; addr_iter( &returnAddr, &typ, &st ); ) {
                if ( params->commsDisableds[typ][0] ) {
                    comms_setAddrDisabled( cGlobals->game.comms, typ, XP_FALSE, XP_TRUE );
                }
                if ( params->commsDisableds[typ][1] ) {
                    comms_setAddrDisabled( cGlobals->game.comms, typ, XP_TRUE, XP_TRUE );
                }
                switch( typ ) {
#ifdef XWFEATURE_RELAY
                case COMMS_CONN_RELAY:
                    /* addr.u.ip_relay.ipAddr = 0; */
                    /* addr.u.ip_relay.port = params->connInfo.relay.defaultSendPort; */
                    /* addr.u.ip_relay.seeksPublicRoom = params->connInfo.relay.seeksPublicRoom; */
                    /* addr.u.ip_relay.advertiseRoom = params->connInfo.relay.advertiseRoom; */
                    /* XP_STRNCPY( addr.u.ip_relay.hostName, params->connInfo.relay.relayName, */
                    /*             sizeof(addr.u.ip_relay.hostName) - 1 ); */
                    /* XP_STRNCPY( addr.u.ip_relay.invite, params->connInfo.relay.invite, */
                    /*             sizeof(addr.u.ip_relay.invite) - 1 ); */
                    break;
#endif
#ifdef XWFEATURE_BLUETOOTH
                case COMMS_CONN_BT:
                    XP_ASSERT( sizeof(returnAddr.u.bt.btAddr)
                               >= sizeof(params->connInfo.bt.hostAddr));
                    XP_MEMCPY( &returnAddr.u.bt.btAddr, &params->connInfo.bt.hostAddr,
                               sizeof(params->connInfo.bt.hostAddr) );
                    break;
#endif
#ifdef XWFEATURE_IP_DIRECT
                case COMMS_CONN_IP_DIRECT:
                    XP_STRNCPY( returnAddr.u.ip.hostName_ip, params->connInfo.ip.hostName,
                                sizeof(addr.u.ip.hostName_ip) - 1 );
                    returnAddr.u.ip.port_ip = params->connInfo.ip.port;
                    break;
#endif
#ifdef XWFEATURE_SMS
                case COMMS_CONN_SMS:
                    XP_LOGF( "%s(): SMS is on at least", __func__ );
                    /* No! Don't overwrite what may be a return address with local
                       stuff */
                    /* XP_STRNCPY( addr.u.sms.phone, params->connInfo.sms.phone, */
                    /*             sizeof(addr.u.sms.phone) - 1 ); */
                    /* addr.u.sms.port = params->connInfo.sms.port; */
                    break;
#endif
                default:
                    break;
                }
            }
        }

        setSquareBonuses( cGlobals );

        /* Need to save in order to have a valid selRow for the first send */
        linuxSaveGame( cGlobals );
        savedGame = true;

#ifndef XWFEATURE_STANDALONE_ONLY
        /* If this is to be a relay connected game, tell it so. Otherwise
           let the invitation process and receipt of messages populate
           comms' addressbook */
        if ( cGlobals->gi->serverRole != SERVER_STANDALONE ) {
            if ( addr_hasType( &params->addr, COMMS_CONN_RELAY ) ) {
            
                if ( ! savedGame ) {
                    linuxSaveGame( cGlobals );
                    savedGame = true;
                }
                CommsAddrRec addr = {0};
                comms_getInitialAddr( &addr, params->connInfo.relay.relayName,
                                      params->connInfo.relay.defaultSendPort );
                XP_MEMCPY( addr.u.ip_relay.invite, params->connInfo.relay.invite,
                           1 + XP_STRLEN(params->connInfo.relay.invite) );
                addr.u.ip_relay.seeksPublicRoom = params->connInfo.relay.seeksPublicRoom;
                addr.u.ip_relay.advertiseRoom = params->connInfo.relay.advertiseRoom;
                comms_augmentHostAddr( cGlobals->game.comms, NULL_XWE, &addr ); /* sends stuff */
            }

            if ( addr_hasType( &params->addr, COMMS_CONN_SMS ) ) {
                CommsAddrRec addr = {0};
                addr_addType( &addr, COMMS_CONN_SMS );
                XP_STRCAT( addr.u.sms.phone, params->connInfo.sms.myPhone );
                addr.u.sms.port = params->connInfo.sms.port;
                comms_augmentHostAddr( cGlobals->game.comms, NULL_XWE, &addr );
            }

            if ( addr_hasType( &params->addr, COMMS_CONN_MQTT ) ) {
                CommsAddrRec addr = {0};
                addr_addType( &addr, COMMS_CONN_MQTT );
                addr.u.mqtt.devID = *mqttc_getDevID( params );
                comms_augmentHostAddr( cGlobals->game.comms, NULL_XWE, &addr );
            }
        }

        if ( !!returnAddrP ) {
            /* This may trigger network activity */
            CommsCtxt* comms = cGlobals->game.comms;
            if ( !!comms ) {
                comms_augmentHostAddr( cGlobals->game.comms, NULL_XWE, &returnAddr );
            }
        }
#endif

#ifdef XWFEATURE_SEARCHLIMIT
        cGlobals->gi->allowHintRect = params->allowHintRect;
#endif

        if ( params->needsNewGame && !opened ) {
            XP_ASSERT(0);
            // new_game_impl( globals, XP_FALSE );
        }
    }

    if ( opened ) {
#ifndef XWFEATURE_STANDALONE_ONLY
        DeviceRole serverRole = cGlobals->gi->serverRole;
        XP_LOGF( "%s(): server role: %d", __func__, serverRole );
        if ( /*!!returnAddrP && */serverRole == SERVER_ISCLIENT ) {
            tryConnectToServer( cGlobals );
        }
#endif

#ifndef XWFEATURE_STANDALONE_ONLY
        if ( !!cGlobals->game.comms ) {
            comms_start( cGlobals->game.comms, NULL_XWE );
        }
#endif
        server_do( cGlobals->game.server, NULL_XWE );
        linuxSaveGame( cGlobals );   /* again, to include address etc. */
    }
    return opened;
} /* linuxOpenGame */

#ifdef USE_SQLITE
XWStreamCtxt*
streamFromDB( CommonGlobals* cGlobals )
{
    LOG_FUNC();
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
                stream = mem_stream_make_raw( MPPARM(cGlobals->util->mpool)
                                              params->vtMgr  );
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

void
gameGotBuf( CommonGlobals* cGlobals, XP_Bool hasDraw, const XP_U8* buf, 
            XP_U16 len, const CommsAddrRec* from )
{
    XP_LOGF( "%s(hasDraw=%d)", __func__, hasDraw );
    XP_Bool redraw = XP_FALSE;
    XWGame* game = &cGlobals->game;
    XWStreamCtxt* stream = stream_from_msgbuf( cGlobals, buf, len );
    if ( !!stream ) {
        redraw = game_receiveMessage( game, NULL_XWE, stream, from );
        if ( redraw ) {
            linuxSaveGame( cGlobals );
        }
        stream_destroy( stream, NULL_XWE );

        /* if there's something to draw resulting from the message, we
           need to give the main loop time to reflect that on the screen
           before giving the server another shot.  So just call the idle
           proc. */
        if ( hasDraw && redraw ) {
            util_requestTime( cGlobals->util, NULL_XWE );
        } else {
            for ( int ii = 0; ii < 4; ++ii ) {
                redraw = server_do( game->server, NULL_XWE ) || redraw;
            }
        }
        if ( hasDraw && redraw ) {
            board_draw( game->board, NULL_XWE );
        }
    }
}

gint
requestMsgsIdle( gpointer data )
{
    CommonGlobals* cGlobals = (CommonGlobals*)data;
    XP_UCHAR devIDBuf[64] = {0};
    gdb_fetch_safe( cGlobals->params->pDb, KEY_RDEVID, NULL, devIDBuf,
                    sizeof(devIDBuf) );
    if ( '\0' != devIDBuf[0] ) {
        relaycon_requestMsgs( cGlobals->params, devIDBuf );
    } else {
        XP_LOGF( "%s: not requesting messages as don't have relay id", __func__ );
    }
    return 0;                   /* don't run again */
}

void
writeToFile( XWStreamCtxt* stream, XWEnv XP_UNUSED(xwe), void* closure )
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
catOnClose( XWStreamCtxt* stream, XWEnv XP_UNUSED(xwe),
            void* XP_UNUSED(closure) )
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
sendOnClose( XWStreamCtxt* stream, XWEnv XP_UNUSED(xwe), void* closure )
{
    CommonGlobals* cGlobals = (CommonGlobals*)closure;
    XP_LOGF( "%s called with msg of len %d", __func__, stream_getSize(stream) );
    (void)comms_send( cGlobals->game.comms, NULL_XWE, stream );
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
        model_writeGameHistory( cGlobals->game.model, NULL_XWE, stream,
                                cGlobals->game.server, gameOver );
        stream_putU8( stream, '\n' );
        stream_destroy( stream, NULL_XWE );
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
    server_writeFinalScores( cGlobals->game.server, NULL_XWE, stream );
    stream_putU8( stream, '\n' );
    stream_destroy( stream, NULL_XWE );
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
linuxSaveGame( CommonGlobals* cGlobals )
{
    LOG_FUNC();
    sqlite3* pDb = cGlobals->params->pDb;
    if ( !!cGlobals->game.model &&
         (!!cGlobals->params->fileName || !!pDb) ) {
        XP_Bool doSave = XP_TRUE;
        XP_Bool newGame = !file_exists( cGlobals->params->fileName )
            || -1 == cGlobals->rowid;
        /* don't fail to save first time!  */
        if ( 0 < cGlobals->params->saveFailPct && !newGame ) {
            XP_U16 pct = XP_RANDOM() % 100;
            doSave = pct >= cGlobals->params->saveFailPct;
        }

        if ( doSave ) {
            if ( !!pDb ) {
                gdb_summarize( cGlobals );
            }

            XWStreamCtxt* outStream;
            MemStreamCloseCallback onClose = !!pDb? gdb_write : writeToFile;
            outStream = 
                mem_stream_make_sized( MPPARM(cGlobals->util->mpool)
                                       cGlobals->params->vtMgr, 
                                       cGlobals->lastStreamSize,
                                       cGlobals, 0, onClose );
            stream_open( outStream );

            game_saveToStream( &cGlobals->game, NULL_XWE, cGlobals->gi,
                               outStream, ++cGlobals->curSaveToken );
            cGlobals->lastStreamSize = stream_getSize( outStream );
            stream_destroy( outStream, NULL_XWE );

            game_saveSucceeded( &cGlobals->game, NULL_XWE, cGlobals->curSaveToken );
            XP_LOGF( "%s: saved", __func__ );

        } else {
            XP_LOGF( "%s: simulating save failure", __func__ );
        }
    }
} /* linuxSaveGame */

static void
handle_messages_from( CommonGlobals* cGlobals, const TransportProcs* procs,
                      int fdin )
{
    LOG_FUNC();
    LaunchParams* params = cGlobals->params;
    XWStreamCtxt* stream = streamFromFile( cGlobals, params->fileName );

#ifdef DEBUG
    XP_Bool opened = 
#endif
        game_makeFromStream( MPPARM(cGlobals->util->mpool) 
                             NULL_XWE, stream, &cGlobals->game, cGlobals->gi,
                             cGlobals->util, NULL /*draw*/,
                             &cGlobals->cp, procs );
    XP_ASSERT( opened );
    stream_destroy( stream, NULL_XWE );

    unsigned short len;
    for ( ; ; ) {
        ssize_t nRead = blocking_read( fdin, (unsigned char*)&len, 
                                       sizeof(len) );
        if ( nRead != sizeof(len) ) {
            XP_LOGF( "%s: 1: unexpected nRead: %zd", __func__, nRead );
            break;
        }
        len = ntohs( len );
        if ( 0 == len ) {
            break;
        }
        unsigned char buf[len];
        nRead = blocking_read( fdin, buf, len );
        if ( nRead != len ) {
            XP_LOGF( "%s: 2: unexpected nRead: %zd", __func__, nRead );
            break;
        }
        stream = mem_stream_make_raw( MPPARM(cGlobals->util->mpool)
                                      params->vtMgr );
        stream_putBytes( stream, buf, len );
        (void)game_receiveMessage( &cGlobals->game, NULL_XWE, stream, NULL );
        stream_destroy( stream, NULL_XWE );
    }

    LOG_RETURN_VOID();
} /* handle_messages_from */

void
read_pipe_then_close( CommonGlobals* cGlobals, const TransportProcs* procs )
{
    LOG_FUNC();
    LaunchParams* params = cGlobals->params;
    XWStreamCtxt* stream = streamFromFile( cGlobals, params->fileName );

#ifdef DEBUG
    XP_Bool opened = 
#endif
        game_makeFromStream( MPPARM(cGlobals->util->mpool) 
                             NULL_XWE, stream, &cGlobals->game,
                             cGlobals->gi,
                             cGlobals->util, NULL /*draw*/,
                             &cGlobals->cp, procs );
    XP_ASSERT( opened );
    stream_destroy( stream, NULL_XWE );

    int fd = open( params->pipe, O_RDWR );
    XP_ASSERT( fd >= 0 );
    if ( fd >= 0 ) {
        unsigned short len;
        for ( ; ; ) {
            ssize_t nRead = blocking_read( fd, (unsigned char*)&len, 
                                           sizeof(len) );
            if ( nRead != sizeof(len) ) {
                XP_LOGF( "%s: 1: unexpected nRead: %zd", __func__, nRead );
                break;
            }
            len = ntohs( len );
            if ( 0 == len ) {
                break;
            }
            unsigned char buf[len];
            nRead = blocking_read( fd, buf, len );
            if ( nRead != len ) {
                XP_LOGF( "%s: 2: unexpected nRead: %zd", __func__, nRead );
                break;
            }
            stream = mem_stream_make_raw( MPPARM(cGlobals->util->mpool)
                                          params->vtMgr );
            stream_putBytes( stream, buf, len );
            (void)game_receiveMessage( &cGlobals->game, NULL_XWE, stream, NULL );
            stream_destroy( stream, NULL_XWE );
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
            if ( (XP_RANDOM() % 1000) < undoRatio ) {
                XP_LOGFF( "calling server_handleUndo()" );
                if ( server_handleUndo( game->server, NULL_XWE, 1 ) ) {
                    board_draw( game->board, NULL_XWE );
                }
            } 
        }
    }

    return TRUE;
}

void
setOneSecondTimer( CommonGlobals* cGlobals )
{
    guint id = g_timeout_add_seconds( 1, secondTimerFired, cGlobals );
    cGlobals->secondsTimerID = id;
}

void
clearOneSecondTimer( CommonGlobals* cGlobals )
{
    g_source_remove( cGlobals->secondsTimerID );
}
#endif

typedef enum {
    CMD_HELP
    ,CMD_SKIP_GAMEOVER
    ,CMD_SHOW_OTHERSCORES
    ,CMD_HOSTIP
    ,CMD_HOSTPORT
    ,CMD_MYPORT
    ,CMD_DICT
#ifdef XWFEATURE_WALKDICT
    ,CMD_TESTDICT
    ,CMD_TESTPRFX
    ,CMD_TESTMINMAX
#endif
    ,CMD_DELIM
#ifdef XWFEATURE_TESTPATSTR
    ,CMD_TESTPAT
    ,CMD_TESTSTR
#endif
    ,CMD_TESTSTARTSW
    ,CMD_TESTCONTAINS
    ,CMD_TESTENDS
    ,CMD_DICTDIR
    ,CMD_PLAYERDICT
    ,CMD_SEED
#ifdef XWFEATURE_DEVID
    ,CMD_LDEVID
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
    ,CMD_RELAY_PORT
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
    ,CMD_NOCLOSESTDIN
    ,CMD_QUITAFTER
    ,CMD_BOARDSIZE
    ,CMD_TRAYSIZE
    ,CMD_DUP_MODE
    ,CMD_HIDEVALUES
    ,CMD_SKIPCONFIRM
    ,CMD_VERTICALSCORE
    ,CMD_NOPEEK
    ,CMD_SPLITPACKETS
    ,CMD_CHAT
    ,CMD_USEUDP
    ,CMD_NOUDP
    ,CMD_USEHTTP
    ,CMD_NOHTTPAUTO
    ,CMD_DROPSENDRELAY
    ,CMD_DROPRCVRELAY
    ,CMD_DROPSENDSMS
    ,CMD_SMSFAILPCT
    ,CMD_DROPRCVSMS
    ,CMD_FORCECHANNEL
    ,CMD_FORCE_GAME
    ,CMD_FORCE_INVITE

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
    ,CMD_INVITEE_SMSNUMBER
    ,CMD_SMSPORT
#endif
    ,CMD_WITHMQTT
    ,CMD_MQTTHOST
    ,CMD_MQTTPORT

    ,CMD_INVITEE_MQTTDEVID
    ,CMD_INVITEE_COUNTS
#ifdef XWFEATURE_RELAY
    ,CMD_ROOMNAME
    ,CMD_ADVERTISEROOM
    ,CMD_JOINADVERTISED
    ,CMD_PHONIES
    ,CMD_BONUSFILE
    ,CMD_INVITEE_RELAYID
#endif
#ifdef XWFEATURE_BLUETOOTH
    ,CMD_BTADDR
#endif
#ifdef XWFEATURE_SLOW_ROBOT
    ,CMD_SLOWROBOT
    ,CMD_TRADEPCT
#endif
#ifdef XWFEATURE_ROBOTPHONIES
    ,CMD_MAKE_PHONY_PCT
#endif
#ifdef USE_GLIBLOOP		/* just because hard to implement otherwise */
    ,CMD_UNDOPCT
#endif
#if defined PLATFORM_GTK && defined PLATFORM_NCURSES
    ,CMD_GTK
    ,CMD_CURSES
    ,CMD_CURSES_LIST_HT
#endif
#if defined PLATFORM_GTK
    ,CMD_ASKNEWGAME
    ,CMD_NHIDDENROWS
#endif
    ,CMD_ASKTIME
    ,CMD_SMSTEST
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
    ,{ CMD_HOSTIP, true, "host-ip", "remote host ip address (for direct connect)" }
    ,{ CMD_HOSTPORT, true, "host-port", "remote host ip address (for direct connect)" }
    ,{ CMD_MYPORT, true, "my-port", "remote host ip address (for direct connect)" }
    ,{ CMD_DICT, true, "game-dict", "dictionary name for game" }
#ifdef XWFEATURE_WALKDICT
    ,{ CMD_TESTDICT, true, "test-dict", "dictionary to be used for iterator test" }
    ,{ CMD_TESTPRFX, true, "test-prefix", "list first word starting with this" }
    ,{ CMD_TESTMINMAX, true, "test-minmax", "M:M -- include only words whose len in range" }
#endif
    ,{ CMD_DELIM, true, "test-delim", "string (should be one char) printed between tile faces" }
#ifdef XWFEATURE_TESTPATSTR
    ,{ CMD_TESTPAT, true, "test-pat", "pattern: e.g. 'ABC' or 'A[+BC]_{2,5}' (can repeat)" }
    ,{ CMD_TESTSTR, true, "test-string",
       "string to be tested against test-pat; exit with non-0 if doesn't match" }
#endif
    ,{CMD_TESTSTARTSW, true, "test-startsw", "use as 'start-with' for pattern"}
    ,{CMD_TESTCONTAINS, true, "test-contains", "use as 'contains' for pattern"}
    ,{CMD_TESTENDS, true, "test-endsw", "use as 'ends-with' for pattern"}

    ,{ CMD_DICTDIR, true, "dict-dir", "path to dir in which dicts will be sought" }
    ,{ CMD_PLAYERDICT, true, "player-dict", "dictionary name for player (in sequence)" }
    ,{ CMD_SEED, true, "seed", "random seed" }
#ifdef XWFEATURE_DEVID
    ,{ CMD_LDEVID, true, "ldevid", "local device ID (for testing GCM stuff)" }
    ,{ CMD_NOANONDEVID, false, "no-anon-devid",
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
    ,{ CMD_RELAY_PORT, true, "relay-port", "port to connect to on relay" }
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
    ,{ CMD_NOCLOSESTDIN, false, "no-close-stdin", "do not close stdin on start" }
    ,{ CMD_QUITAFTER, true, "quit-after", "exit <n> seconds after game's done" }
    ,{ CMD_BOARDSIZE, true, "board-size", "board is <n> by <n> cells" }
    ,{ CMD_TRAYSIZE, true, "tray-size", "<n> tiles per tray (7-9 are legal)" }
    ,{ CMD_DUP_MODE, false, "duplicate-mode", "play in duplicate mode" }
    ,{ CMD_HIDEVALUES, false, "hide-values", "show letters, not nums, on tiles" }
    ,{ CMD_SKIPCONFIRM, false, "skip-confirm", "don't confirm before commit" }
    ,{ CMD_VERTICALSCORE, false, "vertical", "scoreboard is vertical" }
    ,{ CMD_NOPEEK, false, "no-peek", "disallow scoreboard tap changing player" }
    ,{ CMD_SPLITPACKETS, true, "split-packets", "send tcp packets in "
       "sections every random MOD <n> seconds to test relay reassembly" }
    ,{ CMD_CHAT, true, "send-chat", "send a chat every <n> seconds" }
    ,{ CMD_USEUDP, false, "use-udp", "connect to relay new-style, via udp not tcp (on by default)" }
    ,{ CMD_NOUDP, false, "no-use-udp", "connect to relay old-style, via tcp not udp" }
    ,{ CMD_USEHTTP, false, "use-http", "use relay's new http interfaces rather than sockets" }
    ,{ CMD_NOHTTPAUTO, false, "no-http-auto", "When http's on, don't periodically connect to relay (manual only)" }

    ,{ CMD_DROPSENDRELAY, false, "drop-send-relay", "start new games with relay send disabled" }
    ,{ CMD_DROPRCVRELAY, false, "drop-receive-relay", "start new games with relay receive disabled" }
    ,{ CMD_DROPSENDSMS, false, "drop-send-sms", "start new games with sms send disabled" }
    ,{ CMD_SMSFAILPCT, true, "sms-fail-pct", "percent of sms sends, randomly chosen, never arrive" }
    ,{ CMD_DROPRCVSMS, false, "drop-receive-sms", "start new games with sms receive disabled" }
    ,{ CMD_FORCECHANNEL, true, "force-channel", "force (clients) to use this hostid/channel" }
    ,{ CMD_FORCE_GAME, false, "force-game", "if there's no game on launch, create one" }
    ,{ CMD_FORCE_INVITE, false, "force-invite", "if we can, send an invitation by relay or sms" }

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
    ,{ CMD_SMSNUMBER, true, "sms-number", "this devices's sms phone number" }
    ,{ CMD_INVITEE_SMSNUMBER, true, "invitee-sms-number", "number to send any invitation to" }
    ,{ CMD_SMSPORT, true, "sms-port", "this devices's sms port" }
#endif
    ,{ CMD_WITHMQTT, false, "with-mqtt", "enable connecting via mqtt" }
    ,{ CMD_MQTTHOST, true, "mqtt-host", "server mosquitto is running on" }
    ,{ CMD_MQTTPORT, true, "mqtt-port", "port mosquitto is listening on" }
    ,{ CMD_INVITEE_MQTTDEVID, true, "invitee-mqtt-devid", "upper-case hex devID to send any invitation to" }
    ,{ CMD_INVITEE_COUNTS, true, "invitee-counts",
       "When invitations sent, how many on each device? e.g. \"1:2\" for a "
       "three-dev game with two players on second guest" }
#ifdef XWFEATURE_RELAY
    ,{ CMD_ROOMNAME, true, "room", "name of room on relay" }
    ,{ CMD_ADVERTISEROOM, false, "make-public", "make room public on relay" }
    ,{ CMD_JOINADVERTISED, false, "join-public", "look for a public room" }
    ,{ CMD_PHONIES, true, "phonies", 
       "ignore (0, default), warn (1), lose turn (2), or refuse to commit (3)" }
    ,{ CMD_BONUSFILE, true, "bonus-file",
       "provides bonus info: . + * ^ and ! are legal" }
    ,{ CMD_INVITEE_RELAYID, true, "invitee-relayid", "relayID to send any invitation to" }
#endif
#ifdef XWFEATURE_BLUETOOTH
    ,{ CMD_BTADDR, true, "btaddr", "bluetooth address of host" }
#endif
#ifdef XWFEATURE_SLOW_ROBOT
    ,{ CMD_SLOWROBOT, true, "slow-robot", "make robot slower to test network" }
    ,{ CMD_TRADEPCT, true, "trade-pct", "what pct of the time should robot trade" }
#endif
#ifdef XWFEATURE_ROBOTPHONIES
    ,{ CMD_MAKE_PHONY_PCT, true, "make-phony-pct",
       "what pct of the time should robot play a bad word" }
#endif
#ifdef USE_GLIBLOOP
    ,{ CMD_UNDOPCT, true, "undo-pct",
       "each second, what are the odds of doing an undo" }
#endif
#if defined PLATFORM_GTK && defined PLATFORM_NCURSES
    ,{ CMD_GTK, false, "gtk", "use GTK for display" }
    ,{ CMD_CURSES, false, "curses", "use curses for display" }
    ,{ CMD_CURSES_LIST_HT, true, "curses-list-ht", "how many cols tall is the games list" }
#endif
#if defined PLATFORM_GTK
    ,{ CMD_ASKNEWGAME, false, "ask-new", "put up ui for new game params" }
    ,{ CMD_NHIDDENROWS, true, "hide-rows", "number of rows obscured by tray" }
    ,{ CMD_ASKTIME, true, "ask-timeout", 
       "Wait this many ms before cancelling dialog (default 500 ms; 0 means forever)" }
#endif
    ,{ CMD_SMSTEST, false, "run-sms-test", "Run smsproto_runTests() on startup"}
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
    const char* param = "<param>";
    int ii;
    if ( msg != NULL ) {
        fprintf( stderr, "Error: %s\n\n", msg );
    }
    fprintf( stderr, "usage: %s \n", appName );

    int maxWidth = 0;
    for ( ii = 0; ii < VSIZE(CmdInfoRecs); ++ii ) {
        const CmdInfoRec* rec = &CmdInfoRecs[ii];
        int width = strlen(rec->param) + 1;
        if ( rec->hasArg ) {
            width += strlen(param) + 1;
        }
        if ( width > maxWidth ) {
            maxWidth = width;
        }
    }

    for ( ii = 0; ii < VSIZE(CmdInfoRecs); ++ii ) {
        const CmdInfoRec* rec = &CmdInfoRecs[ii];
        char buf[120];
        snprintf( buf, sizeof(buf), "--%s %s", rec->param, 
                 (rec->hasArg ? param : "") );
        fprintf( stderr, "  %-*s # %s\n", maxWidth, buf, rec->hint );
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
    handled = board_focusChanged( board, NULL_XWE, nxt, XP_TRUE );

    if ( !!nxtP ) {
        *nxtP = nxt;
    }

    return handled;
} /* linShiftFocus */
#endif

const XP_U32
linux_getDevIDRelay( LaunchParams* params )
{
    XP_U32 result = 0;
    gchar buf[32];
    if ( gdb_fetch_safe( params->pDb, KEY_RDEVID, NULL, buf, sizeof(buf) ) ) {
        sscanf( buf, "%X", &result );
        /* XP_LOGF( "%s(): %s => %x", __func__, buf, result ); */
    }
    /* LOG_RETURNF( "%d", result ); */
    return result;
}

XP_U32
linux_getCurSeconds()
{
     return (XP_U32)time(NULL);//tv.tv_sec;
}

const XP_UCHAR*
linux_getDevID( LaunchParams* params, DevIDType* typ )
{
    const XP_UCHAR* result = NULL;

    /* commandline takes precedence over stored values */

    if ( !!params->lDevID ) {
        result = params->lDevID;
        *typ = ID_TYPE_LINUX;
    } else if ( gdb_fetch_safe( params->pDb, KEY_RDEVID, NULL, params->devIDStore,
                                sizeof(params->devIDStore) ) ) {
        result = params->devIDStore;
        *typ = '\0' == result[0] ? ID_TYPE_ANON : ID_TYPE_RELAY;
    } else if ( gdb_fetch_safe( params->pDb, KEY_LDEVID, NULL, params->devIDStore,
                                sizeof(params->devIDStore) ) ) {
        result = params->devIDStore;
        *typ = '\0' == result[0] ? ID_TYPE_ANON : ID_TYPE_LINUX;
    } else if ( !params->noAnonDevid ) {
        *typ = ID_TYPE_ANON;
        result = "";
    }
    return result;
}

void
linux_doInitialReg( LaunchParams* params, XP_Bool idIsNew )
{
    gchar rDevIDBuf[64];
    if ( !gdb_fetch_safe( params->pDb, KEY_RDEVID, NULL, rDevIDBuf,
                          sizeof(rDevIDBuf) ) ) {
        rDevIDBuf[0] = '\0';
    }
    DevIDType typ = ID_TYPE_NONE;
    const XP_UCHAR* devID = NULL;
    if ( idIsNew || '\0' == rDevIDBuf[0] ) {
        devID = linux_getDevID( params, &typ );
    }
    relaycon_reg( params, rDevIDBuf, typ, devID );
}

XP_Bool
linux_setupDevidParams( LaunchParams* params )
{
    XP_Bool idIsNew = XP_TRUE;
    gchar oldLDevID[256];
    if ( gdb_fetch_safe( params->pDb, KEY_LDEVID, NULL, oldLDevID, sizeof(oldLDevID) )
         && (!params->lDevID || 0 == strcmp( oldLDevID, params->lDevID )) ) {
        idIsNew = XP_FALSE;
    } else {
        const XP_UCHAR* lDevID = params->lDevID;
        if ( NULL == lDevID ) {
            lDevID = "";        /* we'll call this ANONYMOUS */
        }
        gdb_store( params->pDb, KEY_LDEVID, lDevID );
    }
    return idIsNew;
}

XP_Bool
parseSMSParams( LaunchParams* params, gchar** myPhone, XP_U16* myPort )
{
    gchar buf[32];
    const XP_UCHAR* phone = params->connInfo.sms.myPhone;
    if ( !!phone ) {
        gdb_store( params->pDb, KEY_SMSPHONE, phone );
        *myPhone = g_strdup( phone );
    } else if ( !phone && gdb_fetch_safe( params->pDb, KEY_SMSPHONE, NULL,
                                          buf, VSIZE(buf) ) ) {
        params->connInfo.sms.myPhone = *myPhone = g_strdup(buf);
    } else {
        *myPhone = NULL;
    }

    *myPort = params->connInfo.sms.port;
    gchar portbuf[8];
    if ( 0 < *myPort ) {
        sprintf( portbuf, "%d", *myPort );
        gdb_store( params->pDb, KEY_SMSPORT, portbuf );
    } else if ( gdb_fetch_safe( params->pDb, KEY_SMSPORT, NULL, portbuf,
                                VSIZE(portbuf) ) ) {
        params->connInfo.sms.port = *myPort = atoi( portbuf );
    }
    return NULL != *myPhone && 0 < *myPort;
}

#ifdef XWFEATURE_RELAY
static int
linux_init_relay_socket( CommonGlobals* cGlobals, const CommsAddrRec* addrRec )
{
    XP_ASSERT( !cGlobals->params->useHTTP );
    struct sockaddr_in to_sock;
    struct hostent* host;
    int sock = cGlobals->relaySocket;
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
            cGlobals->relaySocket = sock;
            XP_LOGF( "%s: connected new socket %d to relay", __func__, sock );

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
    size_t nSent = send( cGlobals->relaySocket, buf, len, 0 );
    bool success = len == nSent;
    if ( success ) {
#ifdef COMMS_CHECKSUM
        gchar* sum = g_compute_checksum_for_data( G_CHECKSUM_MD5, buf, len );
        XP_LOGF( "%s: sent %zd bytes with sum %s", __func__, len, sum );
        g_free( sum );
#endif        
    } else {
        close( cGlobals->relaySocket );
        cGlobals->relaySocket = -1;

        /* delete all pending packets since the socket's bad */
        for ( GSList* iter = cGlobals->packetQueue; !!iter; iter = iter->next ) {
            free_elem_proc( iter->data );
        }
        g_slist_free( cGlobals->packetQueue );
        cGlobals->packetQueue = NULL;
    }
    LOG_RETURNF( "%s", boolToStr(success) );
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

        XP_LOGF( "%s: sending packet %d of len %zd (%d left)", __func__, 
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
            XP_LOGF( "%s: added packet %d of len %zd", __func__,
                     elem->id, elem->len );
        }
        int when = XP_RANDOM() % (1 + cGlobals->params->splitPackets);
        (void)g_timeout_add_seconds( when, sendTimerFired, cGlobals );
        success = TRUE;
    }
    return success;
}

static gboolean
linux_relay_ioproc( GIOChannel* source, GIOCondition condition, gpointer data )
{
    gboolean keep = TRUE;
    if ( 0 != ((G_IO_HUP|G_IO_ERR|G_IO_NVAL) & condition) ) {
        XP_LOGF( "%s: got error condition; returning FALSE", __func__ );
        keep = FALSE;
    } else if ( 0 != (G_IO_IN & condition) ) {
        CommonGlobals* cGlobals = (CommonGlobals*)data;
        unsigned char buf[1024];
        int sock = g_io_channel_unix_get_fd( source );
        if ( cGlobals->relaySocket != sock ) {
            XP_LOGF( "%s: changing relaySocket from %d to %d", __func__,
                     cGlobals->relaySocket, sock );
            cGlobals->relaySocket = sock;
        }
        int nBytes = linux_relay_receive( cGlobals, sock, buf, sizeof(buf) );

        if ( nBytes != -1 ) {
#ifdef COMMS_CHECKSUM
            gchar* sum = g_compute_checksum_for_data( G_CHECKSUM_MD5, buf, nBytes );
            XP_LOGF( "%s: got %d bytes with sum %s", __func__, nBytes, sum );
            g_free( sum );
#endif        

            XWStreamCtxt* inboundS;
            XP_Bool redraw = XP_FALSE;
            
            inboundS = stream_from_msgbuf( cGlobals, buf, nBytes );
            if ( !!inboundS ) {
                CommsAddrRec addr = {0};
                addr_addType( &addr, COMMS_CONN_RELAY );
                redraw = game_receiveMessage( &cGlobals->game, NULL_XWE, inboundS, &addr );

                stream_destroy( inboundS, NULL_XWE );
            }
                
            /* if there's something to draw resulting from the
               message, we need to give the main loop time to reflect
               that on the screen before giving the server another
               shot.  So just call the idle proc. */
            if ( redraw ) {
                util_requestTime( cGlobals->util, NULL_XWE );
            }
        }
    }
    return keep;
}

static XP_S16
linux_relay_send( CommonGlobals* cGlobals, const XP_U8* buf, XP_U16 buflen, 
                const CommsAddrRec* addrRec )
{
    XP_S16 result = 0;
    if ( cGlobals->params->useUdp ) {
        XP_ASSERT( -1 != cGlobals->rowid );
        XP_U16 seed = comms_getChannelSeed( cGlobals->game.comms );
        XP_U32 clientToken = makeClientToken( cGlobals->rowid, seed );
        result = relaycon_send( cGlobals->params, buf, buflen, 
                                clientToken, addrRec );
    } else {
        XP_ASSERT( !cGlobals->params->useHTTP );
        int sock = cGlobals->relaySocket;
    
        if ( sock == -1 ) {
            XP_LOGF( "%s: socket uninitialized", __func__ );
            sock = linux_init_relay_socket( cGlobals, addrRec );
            ADD_SOCKET( cGlobals, sock, linux_relay_ioproc );
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
} /* linux_relay_send */
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
linux_reset( XWEnv xwe, void* closure )
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
linux_send( XWEnv XP_UNUSED(xwe), const XP_U8* buf, XP_U16 buflen,
            const XP_UCHAR* msgNo, const CommsAddrRec* addrRec, CommsConnType conType,
            XP_U32 gameID, void* closure )
{
    XP_LOGF( "%s(mid=%s)", __func__, msgNo );
    XP_S16 nSent = -1;
    CommonGlobals* cGlobals = (CommonGlobals*)closure;   

    /* if ( !!addrRec ) { */
    /*     conType = addr_getType( addrRec ); */
    /* } else { */
    /*     conType = addr_getType( &cGlobals->params->addr ); */
    /* } */

    switch ( conType ) {
#ifdef XWFEATURE_RELAY
    case COMMS_CONN_RELAY:
        nSent = linux_relay_send( cGlobals, buf, buflen, addrRec );
        if ( nSent == buflen && cGlobals->params->duplicatePackets ) {
#ifdef DEBUG
            XP_S16 sentAgain = 
#endif
                linux_relay_send( cGlobals, buf, buflen, addrRec );
            XP_ASSERT( sentAgain == nSent );
        }
        break;
#endif
#if defined XWFEATURE_BLUETOOTH
    case COMMS_CONN_BT: {
        XP_Bool isServer = game_getIsServer( &cGlobals->game );
        linux_bt_open( cGlobals, isServer );
        nSent = linux_bt_send( buf, buflen, addrRec, cGlobals );
    }
        break;
#endif
#if defined XWFEATURE_IP_DIRECT || defined XWFEATURE_DIRECTIP
    case COMMS_CONN_IP_DIRECT: {
        CommsAddrRec addr;
        comms_getAddr( cGlobals->game.comms, &addr );
        XP_LOGF( "%s: given %d bytes to send via IP_DIRECT -- which isn't implemented", 
                 __func__, buflen );
        // linux_udp_open( cGlobals, &addr );
        // nSent = linux_udp_send( buf, buflen, addrRec, cGlobals );
    }
        break;
#endif
#if defined XWFEATURE_SMS
    case COMMS_CONN_SMS: {
        CommsAddrRec addr;
        if ( !addrRec ) {
            comms_getAddr( cGlobals->game.comms, &addr );
            addrRec = &addr;
        }

        // use serverphone if I'm a client, else hope one's provided (this is
        // a reply)
        nSent = linux_sms_send( cGlobals->params, buf, buflen, msgNo,
                                addrRec->u.sms.phone, addrRec->u.sms.port,
                                gameID );
    }
        break;
#endif

    case COMMS_CONN_MQTT:
        nSent = mqttc_send( cGlobals->params, gameID, buf, buflen, &addrRec->u.mqtt.devID );
        break;

    case COMMS_CONN_NFC:
        XP_LOGFF( "I don't do nfc! Should be filtering it on invitation receipt" );
        break;

    default:
        XP_ASSERT(0);
    }
    return nSent;
} /* linux_send */

#ifdef XWFEATURE_RELAY
void
linux_close_socket( CommonGlobals* cGlobals )
{
    LOG_FUNC();
    close( cGlobals->relaySocket );
    cGlobals->relaySocket = -1;
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
            XP_LOGF( "%s: read(fd=%d, len=%d) => %zd", __func__, fd, 
                     len - nRead, nGot );
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
linux_relay_receive( CommonGlobals* cGlobals, int sock, unsigned char* buf, int bufSize )
{
    LOG_FUNC();
    ssize_t nRead = -1;
    if ( 0 <= sock ) {
        unsigned short tmp;
        nRead = blocking_read( sock, (unsigned char*)&tmp, sizeof(tmp) );
        if ( nRead != 2 ) {
            linux_close_socket( cGlobals );

            comms_transportFailed( cGlobals->game.comms, NULL_XWE, COMMS_CONN_RELAY );
            nRead = -1;
        } else {
            unsigned short packetSize = ntohs( tmp );
            XP_LOGF( "%s: got packet of size %d", __func__, packetSize );
            if ( packetSize > bufSize ) {
                XP_LOGF( "%s: packet size %d TOO LARGE; closing socket", __func__, packetSize );
                nRead = -1;
            } else {
                nRead = blocking_read( sock, buf, packetSize );
            }
            if ( nRead == packetSize ) {
                LaunchParams* params = cGlobals->params;
                ++params->nPacketsRcvd;
                if ( params->dropNthRcvd == 0 ) {
                    /* do nothing */
                } else if ( params->dropNthRcvd > 0 ) {
                    if ( params->nPacketsRcvd == params->dropNthRcvd ) {
                        XP_LOGF( "%s: dropping %dth packet per "
                                 "--drop-nth-packet",
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
                                XP_LOGF( "%s: dropping %dth packet per "
                                         "--drop-nth-packet",
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
                comms_transportFailed( cGlobals->game.comms, NULL_XWE, COMMS_CONN_RELAY );
            }
        }
    }
    XP_LOGF( "%s=>%zd", __func__, nRead );
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
    result = mem_stream_make_raw( MPPARM(globals->util->mpool)
                                  globals->params->vtMgr );
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
linux_util_makeStreamFromAddr( XW_UtilCtxt* uctx, XWEnv XP_UNUSED(xwe), XP_U16 channelNo )
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
        draw = (*proc)( closure, NULL_XWE, why );
    } else {
        XP_LOGF( "%s: skipping timer %d; cancelled?", __func__, why );
    }
    return draw;
} /* linuxFireTimer */

#ifndef XWFEATURE_STANDALONE_ONLY
static void
linux_util_informMissing( XW_UtilCtxt* XP_UNUSED(uc), XWEnv XP_UNUSED(xwe),
                          XP_Bool XP_UNUSED_DBG(isServer), 
                          const CommsAddrRec* XP_UNUSED_DBG(addr),
                          XP_U16 XP_UNUSED_DBG(nDevs),
                          XP_U16 XP_UNUSED_DBG(nMissing) )
{
    XP_LOGF( "%s(isServer=%d, addr=%p, nDevs=%d, nMissing=%d)", 
             __func__, isServer, addr, nDevs, nMissing );
}

static void
linux_util_addrChange( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe),
                       const CommsAddrRec* XP_UNUSED(oldAddr),
                       const CommsAddrRec* newAddr )
{
    CommonGlobals* cGlobals = (CommonGlobals*)uc->closure;
    CommsConnType typ;
    for ( XP_U32 st = 0; addr_iter( newAddr, &typ, &st ); ) {
        switch ( typ ) {
#ifdef XWFEATURE_BLUETOOTH
        case COMMS_CONN_BT: {
            XP_Bool isServer = game_getIsServer( &cGlobals->game );
            linux_bt_open( cGlobals, isServer );
        }
            break;
#endif
#ifdef XWFEATURE_IP_DIRECT
        case COMMS_CONN_IP_DIRECT:
            linux_udp_open( cGlobals, newAddr );
            break;
#endif
#ifdef XWFEATURE_SMS
        case COMMS_CONN_SMS:
            /* nothing to do??? */
            // XP_ASSERT(0);
            // linux_sms_init( cGlobals, newAddr );
            break;
#endif
        default:
            // XP_ASSERT(0);
            break;
        }
    }
}

#endif

unsigned int
makeRandomInt()
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
            if ( intmin <= intmax ) {
                *min = intmin;
                *max = intmax;
                success = true;
            } else {
                XP_LOGFF( "bad len params: %d <= %d expected", intmin, intmax );
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

typedef struct _FTD {
    PatDesc* desc;
    XP_Bool called;
} FTD;

static XP_Bool
onFoundTiles2( void* closure, const Tile* tiles, int nTiles )
{
    FTD* data = (FTD*)closure;
    if ( data->called ) {
        XP_LOGFF( "ERROR: called more than once; Hungarian case???" );
    } else if ( nTiles <= VSIZE(data->desc->tiles) ) {
        data->called = XP_TRUE;
        data->desc->nTiles = nTiles;
        XP_MEMCPY( &data->desc->tiles[0], tiles, nTiles * sizeof(tiles[0]) );
    }
    return XP_TRUE;
}

#ifdef XWFEATURE_WALKDICT
static void
getPat( const DictionaryCtxt* dict, const XP_UCHAR* str, PatDesc* desc )
{
    if ( !!str && '\0' != str[0] ) {
        FTD data = { .desc = desc, };
        dict_tilesForString( dict, str, 0, onFoundTiles2, &data );
    }
}

static DictIter*
patsParamsToIter( const LaunchParams* params, const DictionaryCtxt* dict )
{
    const XP_UCHAR** strPats = NULL;
    const XP_UCHAR* _strPats[4];
    XP_U16 nStrPats = 0;
    PatDesc descs[3] = {0};
    XP_U16 nPatDescs = 0;

    if ( !!params->iterTestPats ) {
        nStrPats = g_slist_length( params->iterTestPats );
        strPats = &_strPats[0];
        GSList* iter;
        int ii;
        for ( ii = 0, iter = params->iterTestPats;
              !!iter && ii < nStrPats;
              ++ii, iter = iter->next ) {
            strPats[ii] = iter->data;
        }
    } else if ( !!params->patStartW || !!params->patContains || !!params->patEndsW ) {
        getPat( dict, params->patStartW, &descs[0] );
        getPat( dict, params->patContains, &descs[1] );
        getPat( dict, params->patEndsW, &descs[2] );
        nPatDescs = 3;
        /* and what about the boolean? */
    }

    DIMinMax dimm;
    DIMinMax* dimmp = NULL;
    if ( !!params->testMinMax && parsePair( params->testMinMax, &dimm.min, &dimm.max ) ) {
        dimmp = &dimm;
    }

    DictIter* iter = di_makeIter( dict, NULL_XWE, dimmp, strPats, nStrPats,
                                  0 == nPatDescs ? NULL : descs, nPatDescs );
    if ( !iter ) {
        XP_LOGFF( "Unable to build iter" );
    }
    return iter;
}

# define PRINT_ALL
static void
testGetNthWord( const LaunchParams* params, const DictionaryCtxt* dict,
                char** XP_UNUSED_DBG(words), XP_U16 depth,
                const IndexData* data  )
{
    DictIter* iter = patsParamsToIter( params, dict );
    if ( !!iter ) {
        XP_U32 half = di_countWords( iter, NULL ) / 2;
        XP_U32 interval = half / 100;
        const XP_UCHAR* delim = params->dumpDelim; /* NULL is ok */
        if ( interval == 0 ) {
            ++interval;
        }

        XP_UCHAR buf[64];
        int ii, jj;
        for ( ii = 0, jj = half; ii < half; ii += interval, jj += interval ) {
            if ( di_getNthWord( iter, NULL_XWE, ii, depth, data ) ) {
                XP_UCHAR buf[64];
                di_wordToString( iter, buf, VSIZE(buf), delim );
                XP_ASSERT( 0 == strcmp( buf, words[ii] ) );
# ifdef PRINT_ALL
                XP_LOGFF( "word[%d]: %s", ii, buf );
# endif
            } else {
                XP_ASSERT( 0 );
            }
            if ( di_getNthWord( iter, NULL_XWE, jj, depth, data ) ) {
                di_wordToString( iter, buf, VSIZE(buf), delim );
                XP_ASSERT( 0 == strcmp( buf, words[jj] ) );
# ifdef PRINT_ALL
                XP_LOGFF( "word[%d]: %s", jj, buf );
# endif
            } else {
                XP_ASSERT( 0 );
            }
        }
        di_freeIter( iter, NULL_XWE );
    }
}

typedef struct _FTData {
    DictIter* iter;
    IndexData* data;
    char** words;
    gchar* prefix;
    XP_U16 depth;
} FTData;

static XP_Bool
onFoundTiles( void* XP_UNUSED(closure), const Tile* XP_UNUSED(tiles),
              int XP_UNUSED(nTiles) )
{
    XP_LOGFF( "Not doing anything as di_findStartsWith is gone" );
    /* FTData* ftp = (FTData*)closure; */
    /* XP_S16 lenMatched = di_findStartsWith( ftp->iter, tiles, nTiles ); */
    /* if ( 0 <= lenMatched ) { */
    /*     XP_UCHAR buf[32]; */
    /*     XP_UCHAR bufPrev[32] = {0}; */
    /*     di_wordToString( ftp->iter, buf, VSIZE(buf), "." ); */

    /*     /\* This doesn't work with synonyms like "L-L" for "L·L" *\/ */
    /*     // XP_ASSERT( 0 == strncasecmp( buf, prefix, lenMatched ) ); */

    /*     DictPosition pos = di_getPosition( ftp->iter ); */
    /*     XP_ASSERT( 0 == strcmp( buf, ftp->words[pos] ) ); */
    /*     if ( pos > 0 ) { */
    /*         if ( !di_getNthWord( ftp->iter, pos-1, ftp->depth, ftp->data ) ) { */
    /*             XP_ASSERT( 0 ); */
    /*         } */
    /*         di_wordToString( ftp->iter, bufPrev, VSIZE(bufPrev), "." ); */
    /*         XP_ASSERT( 0 == strcmp( bufPrev, ftp->words[pos-1] ) ); */
    /*     } */
    /*     XP_LOGF( "di_getStartsWith(%s) => %s (prev=%s)", */
    /*              ftp->prefix, buf, bufPrev ); */
    /* } else { */
    /*     XP_LOGFF( "nothing starts with %s", ftp->prefix ); */
    /* } */
    return XP_TRUE;
}

/** walk_dict_test()
 *
 * This is just to test that the dict-iterating code works.  The words are
 * meant to be printed e.g. in a scrolling dialog on Android.
*/
static void
walk_dict_test( MPFORMAL const LaunchParams* params, const DictionaryCtxt* dict,
                GSList* testPrefixes )
{

    DictIter* iter = patsParamsToIter( params, dict );
    if ( !!iter ) {
        LengthsArray lens;
        XP_U32 count = di_countWords( iter, &lens );

        XP_U32 sum = 0;
        for ( long ii = 0; ii < VSIZE(lens.lens); ++ii ) {
            XP_LOGF( "%d words of length %ld", lens.lens[ii], ii );
            sum += lens.lens[ii];
        }
        XP_ASSERT( sum == count );

        if ( count > 0 ) {
            const XP_UCHAR* delim = params->dumpDelim;
            XP_Bool gotOne;
            long jj;
            char** words = g_malloc( count * sizeof(char*) );
            XP_ASSERT( !!words );

            for ( jj = 0, gotOne = di_firstWord( iter );
                  gotOne;
                  gotOne = di_getNextWord( iter ) ) {
                XP_ASSERT( di_getPosition( iter ) == jj );
                XP_UCHAR buf[64];
                di_wordToString( iter, buf, VSIZE(buf), delim );
# ifdef PRINT_ALL
                fprintf( stderr, "%.6ld: %s\n", jj, buf );
# endif
                if ( !!words ) {
                    words[jj] = g_strdup( buf );
                }
                ++jj;
            }
            XP_ASSERT( count == jj );

            XP_LOGFF( "comparing runs in both directions" );
            for ( jj = 0, gotOne = di_lastWord( iter );
                  gotOne;
                  ++jj, gotOne = di_getPrevWord( iter ) ) {
                XP_ASSERT( di_getPosition(iter) == count-jj-1 );
                XP_UCHAR buf[64];
                di_wordToString( iter, buf, VSIZE(buf), delim );
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
            XP_LOGFF( "FINISHED comparing runs in both directions" );

            XP_LOGFF( "testing getNth" );
            testGetNthWord( params, dict, words, 0, NULL );
            XP_LOGFF( "FINISHED testing getNth" );

            XP_U16 depth = 2;
            XP_U16 maxCount = dict_numTileFaces( dict );
            IndexData data;
            data.count = maxCount * maxCount; /* squared because depth == 2! */
            data.indices = XP_MALLOC( mpool,
                                      data.count * depth * sizeof(data.indices[0]) );
            data.prefixes = XP_MALLOC( mpool,
                                       depth * data.count * sizeof(data.prefixes[0]) );

            XP_LOGF( "making index..." );
            di_makeIndex( iter, depth, &data );
            XP_LOGF( "DONE making index (have %d indices)", data.count );

            /* Resize 'em in case not all slots filled */
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
                dict_wordToString( dict, &word, buf1, VSIZE(buf1), delim );
                XP_UCHAR buf2[64] = {0};
                if ( ii > 0 && dict_getNthWord( dict, &word, indices[ii]-1 ) ) {
                    dict_wordToString( dict, &word, buf2, VSIZE(buf2), delim );
                }
                char prfx[8];
                dict_tilesToString( dict, &prefixes[depth*ii], depth, prfx, 
                                    VSIZE(prfx), NULL );
                fprintf( stderr, "%d: index: %ld; prefix: %s; word: %s (prev: %s)\n", 
                         ii, indices[ii], prfx, buf1, buf2 );
            }
#endif

            XP_LOGFF( "testing getNth WITH INDEXING" );
            testGetNthWord( params, dict, words, depth, &data );
            XP_LOGFF( "DONE testing getNth WITH INDEXING" );

            if ( !!testPrefixes ) {
                int ii;
                guint count = g_slist_length( testPrefixes );
                for ( ii = 0; ii < count; ++ii ) {
                    gchar* prefix = (gchar*)g_slist_nth_data( testPrefixes, ii );
                    XP_LOGFF( "prefix %d: %s", ii, prefix );

                    FTData foundTilesData = { .iter = iter, .words = words,
                                              .depth = depth, .data = &data,
                                              .prefix = prefix, };
                    dict_tilesForString( dict, prefix, 0, onFoundTiles, &foundTilesData );
                }
            }
            XP_FREE( mpool, data.indices );
            XP_FREE( mpool, data.prefixes );
        }
        di_freeIter( iter, NULL_XWE );
    }
    XP_LOGFF( "done" );
}

static void
walk_dict_test_all( MPFORMAL const LaunchParams* params, GSList* testDicts, 
                    GSList* testPrefixes )
{
    int ii;
    guint count = g_slist_length( testDicts );
    for ( ii = 0; ii < count; ++ii ) {
        gchar* name = (gchar*)g_slist_nth_data( testDicts, ii );
        DictionaryCtxt* dict = 
            linux_dictionary_make( MPPARM(mpool) NULL_XWE, params, name,
                                   params->useMmap );
        if ( NULL != dict ) {
            XP_LOGF( "walk_dict_test(%s)", name );
            walk_dict_test( MPPARM(mpool) params, dict, testPrefixes );
            dict_unref( dict, NULL_XWE );
        }
    }
}
#endif

static void
dumpDict( const LaunchParams* params, DictionaryCtxt* dict )
{
    DictIter* iter = patsParamsToIter( params, dict );
    if ( !!iter ) {
        const XP_UCHAR* delim = params->dumpDelim; /* NULL is ok */
        for ( XP_Bool result = di_firstWord( iter );
              result;
              result = di_getNextWord( iter ) ) {
            XP_UCHAR buf[32];
            di_wordToString( iter, buf, VSIZE(buf), delim );
            fprintf( stdout, "%s\n", buf );
        }
        di_freeIter( iter, NULL_XWE );
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
    for ( iter = params->dictDirs; !!iter && !success; iter = iter->next ) {
        const char* path = iter->data;

        for ( bool firstPass = true; ; firstPass = false ) {
            char buf[256];
            int len = snprintf( buf, VSIZE(buf), "%s/%s%s", path, name,
                                firstPass ? "" : ".xwd" );
            XP_ASSERT( len < VSIZE(buf) );
            if ( len < VSIZE(buf) && file_exists( buf ) ) {
                snprintf( result, resultLen, "%s", buf );
                success = XP_TRUE;
                break;
            } else {
                XP_LOGFF( "nothing found at %s", buf );
                if ( !firstPass ) {
                    break;
                }
            }
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

static void
linux_util_formatPauseHistory( XW_UtilCtxt* XP_UNUSED(uc), XWEnv XP_UNUSED(xwe),
                               XWStreamCtxt* stream,
                               DupPauseType typ, XP_S16 turn,
                               XP_U32 whenPrev, XP_U32 whenCur, const XP_UCHAR* msg )
{
    XP_UCHAR buf[128];
    if ( UNPAUSED == typ ) {
        XP_SNPRINTF( buf, VSIZE(buf), "Game unpaused by player %d after %d seconds; msg: %s",
                     turn, whenCur - whenPrev, msg );
    } else {
        if ( AUTOPAUSED == typ ) {
            XP_SNPRINTF( buf, VSIZE(buf), "%s", "Game auto-paused" );
        } else {
            XP_SNPRINTF( buf, VSIZE(buf), "Game paused by player %d; msg: %s",
                         turn, msg );
        }
    }
    stream_catString( stream, buf );
}

static void
cancelTimer( CommonGlobals* cGlobals, XWTimerReason why )
{
    guint src = cGlobals->timerSources[why-1];
    if ( src != 0 ) {
        g_source_remove( src );
        cGlobals->timerSources[why-1] = 0;
    }
} /* cancelTimer */

void
cancelTimers( CommonGlobals* cGlobals )
{
    /* There is no 0. */
    for ( XWTimerReason why = 1; why < NUM_TIMERS_PLUS_ONE; ++why ) {
        cancelTimer( cGlobals, why );
    }
}

static gint
dup_timer_func( gpointer data )
{
    CommonGlobals* cGlobals = (CommonGlobals*)data;

    if ( linuxFireTimer( cGlobals, TIMER_DUP_TIMERCHECK ) ) {
        board_draw( cGlobals->game.board, NULL_XWE );
    }

    return XP_FALSE;
} /* score_timer_func */

static gint
score_timer_func( gpointer data )
{
    CommonGlobals* cGlobals = (CommonGlobals*)data;

    if ( linuxFireTimer( cGlobals, TIMER_TIMERTICK ) ) {
        board_draw( cGlobals->game.board, NULL_XWE );
    }

    return XP_FALSE;
} /* score_timer_func */

#ifndef XWFEATURE_STANDALONE_ONLY
static gint
comms_timer_func( gpointer data )
{
    GtkGameGlobals* globals = (GtkGameGlobals*)data;

    if ( linuxFireTimer( &globals->cGlobals, TIMER_COMMS ) ) {
        board_draw( globals->cGlobals.game.board, NULL_XWE );
    }

    return (gint)0;
}
#endif

static gint
pen_timer_func( gpointer data )
{
    CommonGlobals* cGlobals = (CommonGlobals*)data;

    if ( linuxFireTimer( cGlobals, TIMER_PENDOWN ) ) {
        board_draw( cGlobals->game.board, NULL_XWE );
    }

    return XP_FALSE;
} /* pen_timer_func */

#ifdef XWFEATURE_SLOW_ROBOT
static gint
slowrob_timer_func( gpointer data )
{
    CommonGlobals* cGlobals = (CommonGlobals*)data;

    if ( linuxFireTimer( cGlobals, TIMER_SLOWROBOT ) ) {
        board_draw( cGlobals->game.board, NULL_XWE );
    }

    return (gint)0;
}
#endif

static void
linux_util_setTimer( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe), XWTimerReason why,
                     XP_U16 XP_UNUSED_STANDALONE(when),
                     XWTimerProc proc, void* closure )
{
    CommonGlobals* cGlobals = (CommonGlobals*)uc->closure;
    guint newSrc;

    cancelTimer( cGlobals, why );

    switch( why ) {
    case TIMER_PENDOWN:
        if ( 0 != cGlobals->timerSources[why-1] ) {
            g_source_remove( cGlobals->timerSources[why-1] );
        }
        newSrc = g_timeout_add( 1000, pen_timer_func, cGlobals );
        break;
    case TIMER_TIMERTICK:
        /* one second */
        cGlobals->scoreTimerInterval = 100 * 10000;

        (void)gettimeofday( &cGlobals->scoreTv, NULL );

        newSrc = g_timeout_add( 1000, score_timer_func, cGlobals );
        break;

    case TIMER_DUP_TIMERCHECK:
        newSrc = g_timeout_add( 1000 * when, dup_timer_func, cGlobals );
        break;

#ifndef XWFEATURE_STANDALONE_ONLY
    case TIMER_COMMS:
        newSrc = g_timeout_add( 1000 * when, comms_timer_func, cGlobals );
        break;
#endif
#ifdef XWFEATURE_SLOW_ROBOT
    case TIMER_SLOWROBOT:
        newSrc = g_timeout_add( 1000 * when, slowrob_timer_func, cGlobals );
        break;
#endif
    default:
        XP_ASSERT( 0 );
    }

    cGlobals->timerInfo[why].proc = proc;
    cGlobals->timerInfo[why].closure = closure;
    XP_ASSERT( newSrc != 0 );
    cGlobals->timerSources[why-1] = newSrc;
} /* linux_util_setTimer */

static void
linux_util_clearTimer( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe), XWTimerReason why )
{
    CommonGlobals* cGlobals = (CommonGlobals*)uc->closure;
    cGlobals->timerInfo[why].proc = NULL;
}

static gint
idle_func( gpointer data )
{
    CommonGlobals* cGlobals = (CommonGlobals*)data;

    /* remove before calling server_do.  If server_do puts up a dialog that
       calls gtk_main, then this idle proc will also apply to that event loop
       and bad things can happen.  So kill the idle proc asap. */
    g_source_remove( cGlobals->idleID );
    cGlobals->idleID = 0;        /* 0 is illegal event source ID */

    ServerCtxt* server = cGlobals->game.server;
    if ( !!server && server_do( server, NULL_XWE ) ) {
        if ( !!cGlobals->game.board ) {
            board_draw( cGlobals->game.board, NULL_XWE );
        }
    }
    return 0; /* 0 will stop it from being called again */
} /* idle_func */

static void
linux_util_requestTime( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe) )
{
    CommonGlobals* cGlobals = (CommonGlobals*)uc->closure;
    cGlobals->idleID = g_idle_add( idle_func, cGlobals );
} /* gtk_util_requestTime */

void
setupLinuxUtilCallbacks( XW_UtilCtxt* util )
{
#define SET_PROC(NAM) util->vtable->m_util_##NAM = linux_util_##NAM
#ifndef XWFEATURE_STANDALONE_ONLY
    SET_PROC(informMissing);
    SET_PROC(addrChange);
#endif
    SET_PROC(formatPauseHistory);
    SET_PROC(setTimer);
    SET_PROC(clearTimer);
    SET_PROC(requestTime);
#undef SET_PROC
}

void
assertDrawCallbacksSet( const DrawCtxVTable* vtable )
{
    bool allSet = true;
    void(**proc)() = (void(**)())vtable;
    for ( int ii = 0; ii < sizeof(*vtable)/sizeof(*proc); ++ii ) {
        if ( !*proc ) {
            XP_LOGF( "%s(): null ptr at index %d", __func__, ii );
            allSet = false;
        }
        ++proc;
    }
    XP_USE(allSet);
    XP_ASSERT( allSet );
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

void
disposeUtil( CommonGlobals* cGlobals )
{
    linux_util_vt_destroy( cGlobals->util );
}

static void
initParams( LaunchParams* params )
{
    memset( params, 0, sizeof(*params) );

    // params->util = calloc( 1, sizeof(*params->util) );
    /* XP_MEMSET( params->util, 0, sizeof(params->util) ); */

#ifdef MEM_DEBUG
    params->mpool = mpool_make(NULL);
#endif

    params->vtMgr = make_vtablemgr(MPPARM_NOCOMMA(params->mpool));
    params->dictMgr = dmgr_make( MPPARM_NOCOMMA(params->mpool) );

    // linux_util_vt_init( MPPARM(params->mpool) params->util );
#ifndef XWFEATURE_STANDALONE_ONLY
    /* params->util->vtable->m_util_informMissing = linux_util_informMissing; */
    /* params->util->vtable->m_util_addrChange = linux_util_addrChange; */
    /* params->util->vtable->m_util_setIsServer = linux_util_setIsServer; */
#endif

    params->dutil = linux_dutils_init( MPPARM(params->mpool) params->vtMgr, params );
}

static void
testStreams( LaunchParams* params )
{
    XWStreamCtxt* stream = mem_stream_make_raw( MPPARM(params->dutil->mpool)
                                                params->vtMgr );

    XP_U32 nums[] = { 1, 4, 8, 200,
                      makeRandomInt(),
                      makeRandomInt(),
                      makeRandomInt(),
                      makeRandomInt(),
                      makeRandomInt(),
                      makeRandomInt(),
    };

    for ( int ii = 0; ii < VSIZE(nums); ++ii ) {
        stream_putU32VL( stream, nums[ii] );
        XP_LOGFF( "put num[%d]: %d", ii, nums[ii] );
    }

    for ( int ii = 0; ii < VSIZE(nums); ++ii ) {
        XP_U32 num = stream_getU32VL( stream );
        XP_LOGFF( "compariing num[%d]: %d with %d", ii, nums[ii], num );
        XP_ASSERT( num == nums[ii] );
    }

    stream_destroy( stream, NULL );
    XP_LOGFF( "OK!!" );
}

static void
freeParams( LaunchParams* params )
{
    gdb_close( params->pDb );
    params->pDb = NULL;
    
    vtmgr_destroy( MPPARM(params->mpool) params->vtMgr );
    linux_dutils_free( &params->dutil );
    dmgr_destroy( params->dictMgr, NULL_XWE );

    gi_disposePlayerInfo( MPPARM(params->mpool) &params->pgi );
    mpool_destroy( params->mpool );
}

static int
dawg2dict( const LaunchParams* params, GSList* testDicts )
{
    guint count = g_slist_length( testDicts );
    for ( int ii = 0; ii < count; ++ii ) {
        DictionaryCtxt* dict = 
            linux_dictionary_make( MPPARM(params->mpool) NULL_XWE, params,
                                   g_slist_nth_data( testDicts, ii ),
                                   params->useMmap );
        if ( NULL != dict ) {
            dumpDict( params, dict );
            dict_unref( dict, NULL_XWE );
        }
    }
    return 0;
}

#ifdef XWFEATURE_TESTPATSTR
static int
testOneString( const LaunchParams* params, GSList* testDicts )
{
    int result = 0;
    guint count = g_slist_length( testDicts );
    for ( int ii = 0; 0 == result && ii < count; ++ii ) {
        DictionaryCtxt* dict =
            linux_dictionary_make( MPPARM(params->mpool) NULL_XWE, params,
                                   g_slist_nth_data( testDicts, ii ),
                                   params->useMmap );
        if ( NULL != dict ) {
            DictIter* iter = patsParamsToIter( params, dict );
            if ( !!iter ) {
                if ( ! di_stringMatches( iter, params->iterTestPatStr ) ) {
                    result = 1;
                }
                di_freeIter( iter, NULL_XWE );
            }
        }
    }
    return result;
}
#endif

int
main( int argc, char** argv )
{
    XP_LOGFF( "%s starting; ptr size: %zu", argv[0], sizeof(argv) );

    int opt;
    int totalPlayerCount = 0;
    XP_Bool isServer = XP_FALSE;
    // char* portNum = NULL;
    // char* hostName = "localhost";
    unsigned int seed = makeRandomInt();
    LaunchParams mainParams = {0};
    XP_U16 nPlayerDicts = 0;
    XP_U16 robotCount = 0;
    /* XP_U16 ii; */
#ifdef XWFEATURE_WALKDICT
    GSList* testDicts = NULL;
    GSList* testPrefixes = NULL;
#endif
    char dictbuf[256];
    char* dict;
    char* path;

    /* install a no-op signal handler.  Later curses- or gtk-specific code
       will install one that does the right thing in that context */

    struct sigaction act = { .sa_handler = tmp_noop_sigintterm };
    sigaction( SIGINT, &act, NULL );
    sigaction( SIGTERM, &act, NULL );
    
    // CommsConnType conType = COMMS_CONN_NONE;
#ifdef XWFEATURE_SMS
    // char* phone = NULL;
#endif
#ifdef XWFEATURE_BLUETOOTH
    const char* btaddr = NULL;
#endif

    setlocale(LC_ALL, "");

    XP_LOGFF( "pid = %d", getpid() );
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
    testStreams( &mainParams );

    /* defaults */
    for ( int ii = 0; ii < VSIZE(mainParams.connInfo.inviteeCounts); ++ii ) {
        mainParams.connInfo.inviteeCounts[ii] = 1;
    }
#ifdef XWFEATURE_RELAY
    mainParams.connInfo.relay.defaultSendPort = DEFAULT_PORT;
    mainParams.connInfo.relay.relayName = "localhost";
    mainParams.connInfo.relay.invite = "INVITE";
#endif
#ifdef XWFEATURE_IP_DIRECT
    mainParams.connInfo.ip.port = DEFAULT_PORT;
    mainParams.connInfo.ip.hostName = "localhost";
#endif
    mainParams.connInfo.mqtt.hostName = "eehouse.org";
    mainParams.connInfo.mqtt.port = 1883;
#ifdef XWFEATURE_SMS
    mainParams.connInfo.sms.port = 1;
#endif
    mainParams.pgi.boardSize = 15;
    mainParams.pgi.traySize = 7;
    mainParams.pgi.bingoMin = 7;
    mainParams.quitAfter = -1;
    mainParams.sleepOnAnchor = XP_FALSE;
    mainParams.printHistory = XP_FALSE;
    mainParams.undoWhenDone = XP_FALSE;
    mainParams.pgi.timerEnabled = XP_FALSE;
    mainParams.pgi.dictLang = -1;
    mainParams.noHeartbeat = XP_FALSE;
    mainParams.nHidden = 0;
    mainParams.needsNewGame = XP_FALSE;
    mainParams.askTimeout = 500;
#ifdef XWFEATURE_SEARCHLIMIT
    mainParams.allowHintRect = XP_FALSE;
#endif
    mainParams.skipCommitConfirm = XP_FALSE;
    mainParams.showColors = XP_TRUE;
    mainParams.allowPeek = XP_TRUE;
    mainParams.showRobotScores = XP_FALSE;
    mainParams.useMmap = XP_TRUE;
    mainParams.useUdp = true;
    mainParams.dbName = "xwgames.sqldb";
    mainParams.cursesListWinHt = 5;

    if ( file_exists( "./dict.xwd" ) )  {
        trimDictPath( "./dict.xwd", dictbuf, VSIZE(dictbuf), &path, &dict );
        mainParams.pgi.dictName = copyString( mainParams.mpool, dict );
    }

    char* envDictPath = getenv( "XW_DICTDIR" );
    XP_LOGFF( "envDictPath=%s", envDictPath );
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
    mainParams.useCurses = XP_FALSE;
#else  /* curses is the default if GTK isn't available */
    mainParams.useCurses = XP_TRUE;
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
            mainParams.connInfo.relay.invite = optarg;
            addr_addType( &mainParams.addr, COMMS_CONN_RELAY );
            // isServer = XP_TRUE; /* implicit */
            break;
#endif
        case CMD_HOSTIP:
            mainParams.connInfo.ip.hostName = optarg;
            addr_addType( &mainParams.addr, COMMS_CONN_IP_DIRECT );
            break;
        case CMD_HOSTPORT:
            mainParams.connInfo.ip.hostPort = atoi(optarg);
            addr_addType( &mainParams.addr, COMMS_CONN_IP_DIRECT );
            break;
        case CMD_MYPORT:
            mainParams.connInfo.ip.myPort = atoi(optarg);
            addr_addType( &mainParams.addr, COMMS_CONN_IP_DIRECT );
            break;
        case CMD_DICT:
            trimDictPath( optarg, dictbuf, VSIZE(dictbuf), &path, &dict );
            replaceStringIfDifferent( mainParams.mpool, &mainParams.pgi.dictName,
                                      dict );
            if ( !path ) {
                path = ".";
            }
            XP_LOGFF( "appending dict path: %s", path );
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
            mainParams.testMinMax = optarg;
            break;
#endif
        case CMD_DELIM:
            mainParams.dumpDelim = optarg;
            break;
#ifdef XWFEATURE_TESTPATSTR
        case CMD_TESTPAT:
            mainParams.iterTestPats = g_slist_append( mainParams.iterTestPats, optarg );
            break;
        case CMD_TESTSTR:
            mainParams.iterTestPatStr = optarg;
            break;
#endif
        case CMD_TESTSTARTSW:
            mainParams.patStartW = optarg;
            break;
        case CMD_TESTCONTAINS:
            mainParams.patContains = optarg;
            break;
        case CMD_TESTENDS:
            mainParams.patEndsW = optarg;
            break;

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
        case CMD_LDEVID:
            mainParams.lDevID = optarg;
            break;
        case CMD_NOANONDEVID:
            mainParams.noAnonDevid = true;
            break;
#endif
        case CMD_GAMESEED:
            mainParams.gameSeed = atoi(optarg);
            break;
        case CMD_GAMEFILE:
            mainParams.fileName = optarg;
            mainParams.dbName = NULL; /* clear the default */
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
            mainParams.dbName = NULL;
            break;
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
            mainParams.connInfo.sms.myPhone = optarg;
            addr_addType( &mainParams.addr, COMMS_CONN_SMS );
            break;
        case CMD_INVITEE_SMSNUMBER:
            mainParams.connInfo.sms.inviteePhones =
                g_slist_append( mainParams.connInfo.sms.inviteePhones, optarg );
            addr_addType( &mainParams.addr, COMMS_CONN_SMS );
            break;
        case CMD_INVITEE_COUNTS: {
            gchar** strs = g_strsplit( optarg, ":", -1 );
            for ( int ii = 0;
                  !!strs[ii] && ii < VSIZE(mainParams.connInfo.inviteeCounts);
                  ++ii ) {
                mainParams.connInfo.inviteeCounts[ii] = atoi(strs[ii]);
            }
            g_strfreev( strs );
        }
            break;
        case CMD_SMSPORT:
            mainParams.connInfo.sms.port = atoi(optarg);
            addr_addType( &mainParams.addr, COMMS_CONN_SMS );
            break;
#endif
        case CMD_WITHMQTT:
            addr_addType( &mainParams.addr, COMMS_CONN_MQTT );
            break;
        case CMD_MQTTHOST:
            mainParams.connInfo.mqtt.hostName = optarg;
            break;
        case CMD_MQTTPORT:
            mainParams.connInfo.mqtt.port = atoi(optarg);
            break;
        case CMD_INVITEE_MQTTDEVID:
            mainParams.connInfo.mqtt.inviteeDevIDs =
                g_slist_append( mainParams.connInfo.mqtt.inviteeDevIDs, optarg );
            addr_addType( &mainParams.addr, COMMS_CONN_MQTT );
            break;
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
            XP_ASSERT( index < MAX_NUM_PLAYERS );
            ++mainParams.nLocalPlayers;
            mainParams.pgi.players[index].robotIQ = 0; /* means human */
            mainParams.pgi.players[index].isLocal = XP_TRUE;
            mainParams.pgi.players[index].name = 
                copyString( mainParams.mpool, (XP_UCHAR*)optarg);
            break;
        case CMD_REMOTEPLAYER:
            index = mainParams.pgi.nPlayers++;
            XP_ASSERT( index < MAX_NUM_PLAYERS );
            mainParams.pgi.players[index].isLocal = XP_FALSE;
            ++mainParams.info.serverInfo.nRemotePlayers;
            break;
        case CMD_RELAY_PORT:
            addr_addType( &mainParams.addr, COMMS_CONN_RELAY );
            mainParams.connInfo.relay.defaultSendPort = atoi( optarg );
            break;
        case CMD_ROBOTNAME:
            ++robotCount;
            index = mainParams.pgi.nPlayers++;
            XP_ASSERT( index < MAX_NUM_PLAYERS );
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
            addr_addType( &mainParams.addr, COMMS_CONN_RELAY );
            mainParams.connInfo.relay.relayName = optarg;
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
            case 3:
                mainParams.pgi.phoniesAction = PHONIES_BLOCK;
                break;
            default:
                usage( argv[0], "phonies takes 0 or 1 or 2 or 3" );
            }
            break;
        case CMD_BONUSFILE:
            mainParams.bonusFile = optarg;
            break;
        case CMD_INVITEE_RELAYID: {
            uint64_t* ptr = g_malloc( sizeof(*ptr) );
            *ptr = (uint64_t)atoi(optarg);
            mainParams.connInfo.relay.inviteeRelayIDs =
                g_slist_append(mainParams.connInfo.relay.inviteeRelayIDs, ptr );
            addr_addType( &mainParams.addr, COMMS_CONN_RELAY );
        }
            break;
#endif
        case CMD_CLOSESTDIN:
            mainParams.closeStdin = XP_TRUE;
            break;
        case CMD_NOCLOSESTDIN:
            mainParams.closeStdin = XP_FALSE;
            break;
        case CMD_QUITAFTER:
            mainParams.quitAfter = atoi(optarg);
            break;
        case CMD_BOARDSIZE:
            mainParams.pgi.boardSize = atoi(optarg);
            break;
        case CMD_TRAYSIZE:
            mainParams.pgi.traySize = atoi(optarg);
            XP_ASSERT( MIN_TRAY_TILES <= mainParams.pgi.traySize
                       && mainParams.pgi.traySize <= MAX_TRAY_TILES );
            break;
        case CMD_DUP_MODE:
            mainParams.pgi.inDuplicateMode = XP_TRUE;
            break;
#ifdef XWFEATURE_BLUETOOTH
        case CMD_BTADDR:
            addr_addType( &mainParams.addr, COMMS_CONN_BT );
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
        case CMD_USEUDP:
            mainParams.useUdp = true;
            break;
        case CMD_NOUDP:
            mainParams.useUdp = false;
            break;
        case CMD_USEHTTP:
            mainParams.useHTTP = true;
            break;
        case CMD_NOHTTPAUTO:
            mainParams.noHTTPAuto = true;
            break;

        case CMD_DROPSENDRELAY:
            mainParams.commsDisableds[COMMS_CONN_RELAY][1] = XP_TRUE;
            break;
        case CMD_DROPRCVRELAY:
            mainParams.commsDisableds[COMMS_CONN_RELAY][0] = XP_TRUE;
            break;
        case CMD_DROPSENDSMS:
            mainParams.commsDisableds[COMMS_CONN_SMS][1] = XP_TRUE;
            break;
        case CMD_SMSFAILPCT:
            mainParams.smsSendFailPct = atoi(optarg);
            XP_ASSERT( mainParams.smsSendFailPct >= 0 && mainParams.smsSendFailPct <= 100 );
            break;
        case CMD_DROPRCVSMS:
            mainParams.commsDisableds[COMMS_CONN_SMS][0] = XP_TRUE;
            break;
        case CMD_FORCECHANNEL:
            mainParams.pgi.forceChannel = atoi( optarg );
            break;

        case CMD_FORCE_GAME:
            mainParams.forceNewGame = true;
            break;

        case CMD_FORCE_INVITE:
            mainParams.forceInvite = true;
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
#ifdef XWFEATURE_ROBOTPHONIES
        case CMD_MAKE_PHONY_PCT:
            mainParams.makePhonyPct = atoi( optarg );
            if ( mainParams.makePhonyPct < 0 || mainParams.makePhonyPct > 100 ) {
                usage(argv[0], "must be 0 <= n <= 100" );
            }
            break;
#endif

#ifdef USE_GLIBLOOP
	case CMD_UNDOPCT:
            mainParams.undoRatio = atoi( optarg );
            if ( mainParams.undoRatio < 0 || mainParams.undoRatio > 1000 ) {
                usage(argv[0], "must be 0 <= n <= 1000" );
            }
	    break;
#endif

#if defined PLATFORM_GTK && defined PLATFORM_NCURSES
        case CMD_GTK:
            mainParams.useCurses = XP_FALSE;
            break;
        case CMD_CURSES:
            mainParams.useCurses = XP_TRUE;
            break;
        case CMD_CURSES_LIST_HT:
            mainParams.cursesListWinHt = atoi(optarg);
            break;
#endif
#if defined PLATFORM_GTK
        case CMD_ASKNEWGAME:
            mainParams.askNewGame = XP_TRUE;
            break;
        case CMD_NHIDDENROWS:
            mainParams.nHidden = atoi(optarg);
            break;
        case CMD_ASKTIME:
            mainParams.askTimeout = atoi(optarg);
            break;
#endif
        case CMD_SMSTEST:
            mainParams.runSMSTest = XP_TRUE;
            break;

        default:
            done = true;
            break;
        }
    }

    /* add cur dir if dict search dir path is empty */
    if ( !mainParams.dictDirs ) {
        mainParams.dictDirs = g_slist_append( mainParams.dictDirs, "./" );
    }

    int result = 0;
    if ( g_str_has_suffix( argv[0], "dawg2dict" ) ) {
        result = dawg2dict( &mainParams, testDicts );
#ifdef XWFEATURE_TESTPATSTR
    } else if ( !!mainParams.iterTestPatStr ) {
        result = testOneString( &mainParams, testDicts );
#endif
    } else {
        XP_ASSERT( mainParams.pgi.nPlayers == mainParams.nLocalPlayers
                   + mainParams.info.serverInfo.nRemotePlayers );

        if ( mainParams.info.serverInfo.nRemotePlayers == 0 ) {
            mainParams.pgi.serverRole = SERVER_STANDALONE;
        } else if ( isServer ) {
            if ( mainParams.info.serverInfo.nRemotePlayers > 0 ) {
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
            DictionaryCtxt* dict =
                linux_dictionary_make( MPPARM(mainParams.mpool) NULL_XWE, &mainParams,
                                       mainParams.pgi.dictName,
                                       mainParams.useMmap );
            XP_ASSERT( !!dict );
            mainParams.pgi.dictLang = dict_getLangCode( dict );
            XP_LOGFF( "set lang code: %d", mainParams.pgi.dictLang );
            dict_unref( dict, NULL_XWE );
        } else if ( isServer ) {
#ifdef STUBBED_DICT
            foo
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
            walk_dict_test_all( MPPARM(mainParams.mpool) &mainParams, testDicts, testPrefixes );
            exit( 0 );
        }
#endif
        CommsConnType typ;
        for ( XP_U32 st = 0; addr_iter( &mainParams.addr, &typ, &st ); ) {
            switch ( typ ) {
#ifdef XWFEATURE_BLUETOOTH
            case COMMS_CONN_BT: {
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
            }
                break;
#endif
/* #ifdef XWFEATURE_SMS */
/*             case COMMS_CONN_SMS: */
/*                 XP_MEMCPY( &mainParams.connInfo.sms.myPhone, sms-phone */
/*                 const char* serverPhone; */
/*                 int port; */


/*                 break; */
/* #endif */
            default:
                break;
            }
        }
        // addr_setType( &mainParams.addr, conType );

        /*     mainParams.pipe = linuxCommPipeCtxtMake( isServer ); */

        /*     mainParams.util->vtable->m_util_makeStreamFromAddr =  */
        /* 	linux_util_makeStreamFromAddr; */

        // mainParams.util->gameInfo = &mainParams.pgi;

        srandom( seed );	/* init linux random number generator */
        XP_LOGFF( "seeded srandom with %d", seed );

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

        XP_ASSERT( !!mainParams.dbName );
        mainParams.pDb = gdb_open( mainParams.dbName );
        
        if ( mainParams.useCurses ) {
            /* if ( mainParams.needsNewGame ) { */
            /*     /\* curses doesn't have newgame dialog *\/ */
            /*     usage( argv[0], "game params required for curses version, e.g. --name Eric --room MyRoom" */
            /*            " --remote-player --dict-dir ../ --game-dict CollegeEng_2to8.xwd"); */
            /* } else { */
#if defined PLATFORM_NCURSES
            cursesmain( isServer, &mainParams );
#endif
            /* } */
        } else {
#if defined PLATFORM_GTK
            gtk_init( &argc, &argv );
            gtkmain( &mainParams );
#endif
        }

        freeParams( &mainParams );
    }

    free( longopts );
    g_slist_free( mainParams.dictDirs );

    gsw_logIdles();

    XP_LOGFF( "%s exiting, returning %d", argv[0], result );
    return result;
} /* main */
