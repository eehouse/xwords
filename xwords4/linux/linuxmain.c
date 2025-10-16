/* 
 * Copyright 2000 - 2024 by Eric House (xwords@eehouse.org).  All rights
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
#include "mqttcon.h"
#include "device.h"
#include "gamemgr.h"
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
#include "dllist.h"
#include "xwarray.h"
#include "xwmutex.h"
#include "lindmgr.h"
/* #include "commgr.h" */
/* #include "compipe.h" */
#include "memstream.h"
#include "LocalizedStrIncludes.h"

#define DEFAULT_PORT 10997
#define DEFAULT_LISTEN_PORT 4998

// static int blocking_read( int fd, unsigned char* buf, const int len );

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

    stream = mem_stream_make_raw( MPPARM(cGlobals->params->mpool)
                                  cGlobals->params->vtMgr );
    stream_putBytes( stream, buf, statBuf.st_size );
    free( buf );

    return stream;
} /* streamFromFile */

XP_Bool
linux_makeMoveIf( CommonGlobals* cGlobals, XP_Bool tryTrade )
{
    XW_DUtilCtxt* dutil = cGlobals->params->dutil;
    GameRef gr = cGlobals->gr;
    XP_Bool isLocal;
    XP_S16 turn = gr_getCurrentTurn( dutil, gr, NULL_XWE, &isLocal );
    XP_Bool success = 0 <= turn && isLocal;

    if ( success ) {
        gr_selectPlayer( dutil, gr, NULL_XWE, turn, XP_TRUE );
        if ( tryTrade && gr_canTrade( dutil, gr, NULL_XWE ) ) {

            TrayTileSet oldTiles = *gr_getPlayerTiles( dutil, gr, NULL_XWE, turn );
            XP_S16 nTiles = gr_countTilesInPool( dutil, gr, NULL_XWE );
            XP_ASSERT( 0 <= nTiles );
            if ( nTiles < oldTiles.nTiles ) {
                oldTiles.nTiles = nTiles;
            }
            success = gr_commitTrade( dutil, gr, NULL_XWE, &oldTiles, NULL );
        } else {
            XP_Bool ignored;
            if ( gr_canHint( dutil, gr, NULL_XWE )
                 && gr_requestHint( dutil, gr, NULL_XWE,
#ifdef XWFEATURE_SEARCHLIMIT
                                       XP_FALSE,
#endif
                                       XP_FALSE, &ignored ) ) {
                /* nothing to do -- we have a hint */
            } else {
                XP_LOGFF( "unable to find hint; so PASSing" );
            }
            PhoniesConf pc = { .confirmed = XP_TRUE };
            gr_commitTurn( dutil, gr, NULL_XWE, &pc, XP_TRUE, NULL );
        }
    }
    LOG_RETURNF( "%s", boolToStr(success) );
    return success;
}

void
linux_addInvites( CommonGlobals* cGlobals, XP_U16 nRemotes,
                  const CommsAddrRec destAddrs[] )
{
    GameRef gr = cGlobals->gr;

    CommsAddrRec selfAddr;
    XW_DUtilCtxt* dutil = cGlobals->params->dutil;
    gr_getSelfAddr( dutil, gr, NULL_XWE, &selfAddr );
    NetLaunchInfo nli;
    CurGameInfo gi = *cGlobals->gi;
    gi.conTypes = selfAddr._conTypes;
    nli_init( &nli, &gi, &selfAddr, 1, 0 );

    for ( int ii = 0; ii < nRemotes; ++ii ) {
        gr_invite( dutil, gr, NULL_XWE, &nli, &destAddrs[ii], XP_TRUE );
    }
}

XP_Bool
getAndClear( GameChangeEvent evt, GameChangeEvents* gces )
{
    XP_Bool isSet = 0 != (evt & *gces);
    if ( isSet ) {
        *gces &= ~evt;
    }
    return isSet;
}

CommonGlobals*
globalsForUtil( const XW_UtilCtxt* uc, XP_Bool allocMissing )
{
    CommonAppGlobals* cag = getCag( uc );
    CommonGlobals* cGlobals = globalsForGameRef( cag, uc->gr, allocMissing );

    XP_LOGFF( "(" GR_FMT ") => %p", uc->gr, cGlobals );
    return cGlobals;
}

void
forgetGameGlobals( CommonAppGlobals* cag, CommonGlobals* cGlobals )
{
    XP_ASSERT( g_slist_find (cag->globalsList, cGlobals ) );
    cag->globalsList = g_slist_remove( cag->globalsList, cGlobals );
    XP_LOGFF( "(" GR_FMT ", cGlobals=%p); list now %p", cGlobals->gr,
              cGlobals, cag->globalsList );
}

CommonGlobals*
globalsForGameRef( CommonAppGlobals* cag, GameRef gr, XP_Bool allocMissing )
{
    CommonGlobals* found = NULL;
    for ( GSList* iter = cag->globalsList; !!iter && !found; iter = iter->next ) {
        CommonGlobals* one = (CommonGlobals*)iter->data;
        if ( one->gr == gr ) {
            found = one;
        }
    }

    if ( !found && allocMissing ) {
        LaunchParams* params = cag->params;
        CommonGlobals* cGlobals = params->useCurses
            ? allocCursesBoardGlobals()
            : allocGTKBoardGlobals();
        cGlobals->params = params;
        cGlobals->gr = gr;
        XP_ASSERT( cag == params->cag );
        cag->globalsList = g_slist_prepend( cag->globalsList, cGlobals );
        XP_LOGFF( "made new globals: %p", cGlobals );
        found = cGlobals;
    }

    XP_LOGFF( "(" GR_FMT ") => %p", gr, found );
    return found;
}

void
cg_init(CommonGlobals* cGlobals, cg_destructor proc)
{
    XP_ASSERT( 0 == cGlobals->refCount );
#ifdef DEBUG
    cGlobals->creator = pthread_self();
#endif
    cGlobals->refCount = 1;
    cGlobals->destructor = proc;
}

/* This is unused right now. Do I need to ref-count this thing? */
/* CommonGlobals* */
/* _cg_ref(CommonGlobals* cGlobals, const char* proc, int line ) */
/* { */
/*     assertMainThread( cGlobals ); */
/*     ++cGlobals->refCount; */
/*     XP_LOGFF( "cg: %p: refCount now %d (from %s(), line %d)", */
/*               cGlobals, cGlobals->refCount, proc, line ); */
/*     return cGlobals; */
/* } */

void
_cg_unref( CommonGlobals* cGlobals, const char* XP_UNUSED_DBG(proc),
           int XP_UNUSED_DBG(line) )
{
    LOG_FUNC();
    XP_LOGFF( "cg: %p: refCount %d", cGlobals, cGlobals->refCount );
    if ( cGlobals->refCount <= 0 ) {
        XP_LOGFF( "cg: %p: refCount too low!! (from %s(), line %d)",
                  cGlobals, proc, line );
        XP_ASSERT( 0 );
    }
    --cGlobals->refCount;
    XP_LOGFF( "cg: %p: refCount now %d (from %s(), line %d)",
              cGlobals, cGlobals->refCount, proc, line );
    if ( 0 == cGlobals->refCount ) {
        (*cGlobals->destructor)(cGlobals);
    }
}

bool
linuxOpenGame( CommonGlobals* cGlobals )
{
    XWStreamCtxt* stream = NULL;
    XP_Bool opened = XP_FALSE;

    LaunchParams* params = cGlobals->params;
    if ( !!params->fileName && file_exists( params->fileName ) ) {
        stream = streamFromFile( cGlobals, params->fileName );
#ifdef USE_SQLITE
    } else if ( !!params->dbFileName && file_exists( params->dbFileName ) ) {
        /* XP_UCHAR buf[32]; */
        /* XP_SNPRINTF( buf, sizeof(buf), "%d", params->dbFileID ); */
        /* mpool_setTag( mpool buf ); */
        stream = streamFromDB( cGlobals );
        XP_ASSERT(0);
#endif
#ifdef XWFEATURE_DEVICE_STORES
    } else if ( !!cGlobals->gr ) {
        opened = XP_TRUE;
#else
    } else if ( !!params->pDb && 0 <= cGlobals->rowid ) {
        stream = mem_stream_make_raw( MPPARM(cGlobals->util->mpool)
                                      params->vtMgr );
        if ( !gdb_loadGame( stream, params->pDb, cGlobals->rowid ) ) {
            stream_destroy( stream );
            stream = NULL;
        }
#endif
    }

    if ( !!stream ) {
        cGlobals->gr = dvc_makeFromStream( params->dutil, NULL_XWE, stream,
                                                cGlobals->gi, NULL,
                                                cGlobals->draw, &cGlobals->cp );
        LOG_GI( cGlobals->gi, __func__ );
        stream_destroy( stream );
    }

    if ( !opened /* && canMakeFromGI( cGlobals->gi )*/ ) {
        opened = XP_TRUE;
        XP_ASSERT(0);
        /* cGlobals->gr = gmgr_makeNewGame( params->dutil, NULL_XWE, cGlobals->gi, */
        /*                                       NULL, cGlobals->util, cGlobals->draw, */
        /*                                       &cGlobals->cp ); */
#ifdef XWFEATURE_RELAY
        bool savedGame = false;
#endif

        /* Need to save in order to have a valid selRow for the first send */
        linuxSaveGame( cGlobals );
#ifdef XWFEATURE_RELAY
        savedGame = true;
#endif

#ifdef XWFEATURE_SEARCHLIMIT
        XP_ASSERT( !params->allowHintRect ); /* otherwise handle this!! */
        // cGlobals->gi->allowHintRect = params->allowHintRect;
#endif
        if ( params->needsNewGame && !opened ) {
            XP_ASSERT(0);
            // new_game_impl( globals, XP_FALSE );
        }
    }

    if ( opened ) {
        XP_LOGFF( "server role: %d", cGlobals->gi->deviceRole );
        // linuxSaveGame( cGlobals );   /* again, to include address etc. */
    }
    LOG_RETURNF( "%s", boolToStr(opened) );
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
                stream = mem_stream_make_raw( MPPARM(cGlobals->params->mpool)
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

#ifdef XWFEATURE_RELAY
gint
requestMsgsIdle( gpointer data )
{
    CommonGlobals* cGlobals = (CommonGlobals*)data;
    XP_UCHAR devIDBuf[64] = {};
    gdb_fetch_safe( cGlobals->params->pDb, KEY_RDEVID, NULL, devIDBuf,
                    sizeof(devIDBuf) );
    if ( '\0' != devIDBuf[0] ) {
        relaycon_requestMsgs( cGlobals->params, devIDBuf );
    } else {
        XP_LOGF( "%s: not requesting messages as don't have relay id", __func__ );
    }
    return 0;                   /* don't run again */
}
#endif

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
catAndClose( XWStreamCtxt* stream )
{
    XP_U16 nBytes;
    char* buffer;

    nBytes = stream_getSize( stream );
    buffer = malloc( nBytes + 1 );
    stream_getBytes( stream, buffer, nBytes );
    buffer[nBytes] = '\0';

    fprintf( stderr, "%s", buffer );

    free( buffer );
    stream_destroy( stream );
} /* catOnClose */

void
catGameHistory( LaunchParams* params, GameRef gr )
{
    XW_DUtilCtxt* dutil = params->dutil;
    XP_Bool gameOver = gr_getGameIsOver( dutil, gr, NULL_XWE );
    XWStreamCtxt* stream = 
        mem_stream_make( MPPARM(dutil->mpool) params->vtMgr, CHANNEL_NONE );
    gr_writeGameHistory( dutil, gr, NULL_XWE, stream, gameOver );
    stream_putU8( stream, '\n' );
    catAndClose( stream );
} /* catGameHistory */

void
catFinalScores( LaunchParams* params, GameRef gr, XP_S16 quitter )
{
    XW_DUtilCtxt* dutil = params->dutil;
    const CurGameInfo* gi = gr_getGI( dutil, gr, NULL_XWE );
    XP_ASSERT( quitter < gi->nPlayers );

    XWStreamCtxt* stream = mem_stream_make( MPPARM(params->mpool)
                                            params->vtMgr,
                                            CHANNEL_NONE );
    if ( -1 != quitter ) {
        XP_UCHAR buf[128];
        XP_SNPRINTF( buf, VSIZE(buf), "Player %s resigned\n",
                     gi->players[quitter].name );
        stream_catString( stream, buf );
    }
    gr_writeFinalScores( dutil, gr, NULL_XWE, stream );
    stream_putU8( stream, '\n' );
    catAndClose( stream );
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

#ifndef XWFEATURE_DEVICE_STORES
void
linuxSaveGame( CommonGlobals* cGlobals )
{
    sqlite3* pDb = cGlobals->params->pDb;
    if ( !!cGlobals->gr &&
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
            MemStreamCloseCallback onClose = !!pDb? gdb_write : writeToFile;
            XWStreamCtxt* outStream =
                mem_stream_make_sized( MPPARM(cGlobals->util->mpool)
                                       cGlobals->params->vtMgr, 
                                       cGlobals->lastStreamSize,
                                       0, onClose );

            game_saveToStream( cGlobals->gr, cGlobals->gi, outStream,
                               ++cGlobals->curSaveToken );
            cGlobals->lastStreamSize = stream_getSize( outStream );
            stream_destroy( outStream );

            gr_saveSucceeded( dutil, cGlobals->gr, NULL_XWE, cGlobals->curSaveToken );

            /* if ( !!pDb ) { */
            /*     gdb_summarize( cGlobals ); */
            /* } */
            XP_LOGFF( "saved" );
        } else {
            XP_LOGFF( "simulating save failure" );
        }
    }
} /* linuxSaveGame */
#endif

#if 0
static void
handle_messages_from( CommonGlobals* cGlobals, const TransportProcs* procs,
                      int fdin )
{
    XP_USE( cGlobals );
    XP_USE( procs );
    XP_USE( fdin );
#if 0
    LOG_FUNC();
    LaunchParams* params = cGlobals->params;
    XWStreamCtxt* stream = streamFromFile( cGlobals, params->fileName );

    XP_ASSERT( !cGlobals->gr );
    XP_Bool success = game_makeFromStream( MPPARM(cGlobals->util->mpool) 
                                             NULL_XWE, stream, cGlobals->gi,
                                             &cGlobals->gr,
                                             cGlobals->util, NULL /*draw*/,
                                             &cGlobals->cp, procs );
    XP_ASSERT( success );
    stream_destroy( stream );

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
        (void)game_receiveMessage( cGlobals->gr, NULL_XWE, stream, NULL );
        stream_destroy( stream );
    }

    LOG_RETURN_VOID();
#endif
} /* handle_messages_from */
#endif

typedef enum {
    CMD_HELP
    ,CMD_SKIP_GAMEOVER
    ,CMD_NO_SHOW_OTHERSCORES
    ,CMD_SKIP_MQTT
    ,CMD_SKIP_BT
    ,CMD_HOSTIP
    ,CMD_HOSTPORT
    ,CMD_MYPORT
    ,CMD_DICT
#ifdef XWFEATURE_WALKDICT
    ,CMD_TESTDICT
    ,CMD_TESTPRFX
    ,CMD_TESTMINMAX
#endif
#ifdef XWFEATURE_TESTSORT
    ,CMD_SORTDICT
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
    ,CMD_SHOWGAMES
    ,CMD_PRINTHISORY
    ,CMD_SKIPWARNINGS
    ,CMD_LOCALPWD
    ,CMD_DUPPACKETS
    ,CMD_DROPNTHPACKET
    ,CMD_NOHINTS
    ,CMD_PICKTILESFACEUP
    ,CMD_LOCALNAME
    ,CMD_PLAYERNAME
    ,CMD_REMOTEPLAYER
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
    ,CMD_SKIP_ERRS
    ,CMD_NOCLOSESTDIN
    ,CMD_QUITAFTER
    ,CMD_BOARDSIZE
    ,CMD_TRAYSIZE
    ,CMD_DUP_MODE
    ,CMD_HIDEVALUES
    ,CMD_SKIPCONFIRM
    ,CMD_VERTICALSCORE
    ,CMD_NOPEEK
    ,CMD_CHAT
#ifdef XWFEATURE_RELAY
    ,CMD_SPLITPACKETS
    ,CMD_RELAY_PORT
    ,CMD_USEUDP
    ,CMD_NOUDP
    ,CMD_USEHTTP
    ,CMD_NOHTTPAUTO
    ,CMD_DROPSENDRELAY
    ,CMD_DROPRCVRELAY
#endif
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
    ,CMD_WITHOUT_MQTT
    ,CMD_MQTTHOST
    ,CMD_MQTTPORT

    ,CMD_INVITEE_MQTTDEVID
    ,CMD_INVITEE_COUNTS
#ifdef XWFEATURE_RELAY
    ,CMD_ROOMNAME
    ,CMD_ADVERTISEROOM
    ,CMD_JOINADVERTISED
    ,CMD_INVITEE_RELAYID
#endif
    ,CMD_PHONIES
    ,CMD_BONUSFILE
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
    ,CMD_REMATCH_ON_OVER
    ,CMD_STATUS_SOCKET_NAME
    ,CMD_CMDS_SOCKET_NAME
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
    ,{ CMD_NO_SHOW_OTHERSCORES, false, "no-show-other", "Don't show robot/remote scores" }
    ,{ CMD_SKIP_MQTT, false, "skip-mqtt-add", "Do not add MQTT to games as they connect" }
    ,{ CMD_SKIP_BT, false, "disable-bt", "Don't support or offer bluetooth" }
    ,{ CMD_HOSTIP, true, "host-ip", "remote host ip address (for direct connect)" }
    ,{ CMD_HOSTPORT, true, "host-port", "remote host ip address (for direct connect)" }
    ,{ CMD_MYPORT, true, "my-port", "remote host ip address (for direct connect)" }
    ,{ CMD_DICT, true, "game-dict", "dictionary name for game" }
#ifdef XWFEATURE_WALKDICT
    ,{ CMD_TESTDICT, true, "test-dict", "dictionary to be used for iterator test" }
    ,{ CMD_TESTPRFX, true, "test-prefix", "list first word starting with this" }
    ,{ CMD_TESTMINMAX, true, "test-minmax", "M:M -- include only words whose len in range" }
#endif
#ifdef XWFEATURE_TESTSORT
    ,{ CMD_SORTDICT, true, "sort-dict", "dictionary to be used for sorting test" }
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
    ,{ CMD_SHOWGAMES, false, "show-games", "open games created in response to invitations"}
    ,{ CMD_PRINTHISORY, false, "print-history", "print history on game over" }
    ,{ CMD_SKIPWARNINGS, false, "skip-warnings", "no modals on phonies" }
    ,{ CMD_LOCALPWD, true, "password", "password for user (in sequence)" }
    ,{ CMD_DUPPACKETS, false, "dup-packets", "send two of each to test dropping" }
    ,{ CMD_DROPNTHPACKET, true, "drop-nth-packet", 
       "drop this packet; default 0 (none)" }
    ,{ CMD_NOHINTS, false, "no-hints", "disallow hints" }
    ,{ CMD_PICKTILESFACEUP, false, "pick-face-up", "allow to pick tiles" }
    ,{ CMD_LOCALNAME, true, "localName", "name given all local players" }
    ,{ CMD_PLAYERNAME, true, "name", "name of local, non-robot player" }
    ,{ CMD_REMOTEPLAYER, false, "remote-player", "add an expected player" }
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
    ,{ CMD_SKIP_ERRS, false, "skip-user-errors", "don't show user errors like Not your turn" }
    ,{ CMD_NOCLOSESTDIN, false, "no-close-stdin", "do not close stdin on start" }
    ,{ CMD_QUITAFTER, true, "quit-after", "exit <n> seconds after game's done" }
    ,{ CMD_BOARDSIZE, true, "board-size", "board is <n> by <n> cells" }
    ,{ CMD_TRAYSIZE, true, "tray-size", "<n> tiles per tray (7-9 are legal)" }
    ,{ CMD_DUP_MODE, false, "duplicate-mode", "play in duplicate mode" }
    ,{ CMD_HIDEVALUES, false, "hide-values", "show letters, not nums, on tiles" }
    ,{ CMD_SKIPCONFIRM, false, "skip-confirm", "don't confirm before commit" }
    ,{ CMD_VERTICALSCORE, false, "vertical", "scoreboard is vertical" }
    ,{ CMD_NOPEEK, false, "no-peek", "disallow scoreboard tap changing player" }
    ,{ CMD_CHAT, true, "send-chat", "send a chat every <n> seconds" }
#ifdef XWFEATURE_RELAY
    ,{ CMD_SPLITPACKETS, true, "split-packets", "send tcp packets in "
       "sections every random MOD <n> seconds to test relay reassembly" }
    ,{ CMD_RELAY_PORT, true, "relay-port", "port to connect to on relay" }
    ,{ CMD_USEUDP, false, "use-udp", "connect to relay new-style, via udp not tcp (on by default)" }
    ,{ CMD_NOUDP, false, "no-use-udp", "connect to relay old-style, via tcp not udp" }
    ,{ CMD_USEHTTP, false, "use-http", "use relay's new http interfaces rather than sockets" }
    ,{ CMD_NOHTTPAUTO, false, "no-http-auto", "When http's on, don't periodically connect to relay (manual only)" }
    ,{ CMD_DROPSENDRELAY, false, "drop-send-relay", "start new games with relay send disabled" }
    ,{ CMD_DROPRCVRELAY, false, "drop-receive-relay", "start new games with relay receive disabled" }
#endif
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
    ,{ CMD_WITHOUT_MQTT, false, "without-mqtt", "disable connecting via mqtt (which is on by default)" }
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
    ,{ CMD_INVITEE_RELAYID, true, "invitee-relayid", "relayID to send any invitation to" }
#endif
    ,{ CMD_PHONIES, true, "phonies", 
       "ignore (0, default), warn (1), lose turn (2), or refuse to commit (3)" }
    ,{ CMD_BONUSFILE, true, "bonus-file",
       "provides bonus info: . + * ^ and ! are legal" }
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

    ,{ CMD_REMATCH_ON_OVER, false, "rematch-when-done", "Rematch games if they end" }
    ,{ CMD_STATUS_SOCKET_NAME, true, "status-socket-name",
       "Unix domain socket to which to write status" },
    { CMD_CMDS_SOCKET_NAME, true, "cmd-socket-name", "Unix domain socket on which to listen for commands"},
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
    GameRef gr = cGlobals->gr;
    BoardObjectType nxt = OBJ_NONE;
    XP_U16 curIndex = 0;

    XW_DUtilCtxt* dutil = cGlobals->params->dutil;
    BoardObjectType cur = gr_getFocusOwner( dutil, gr, NULL_XWE );
    if ( cur == OBJ_NONE ) {
        cur = order[0];
    }
    for ( int ii = 0; ii < 3; ++ii ) {
        if ( cur == order[ii] ) {
            curIndex = ii;
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
    XP_Bool result = gr_focusChanged( dutil, gr, NULL_XWE, nxt,
                                      XP_TRUE );

    if ( !!nxtP ) {
        *nxtP = nxt;
    }

    return result;
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

#ifdef XWFEATURE_RELAY
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
#endif

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

void
cpFromLP( CommonPrefs* cp, const LaunchParams* params )
{
    cp->showBoardArrow = XP_TRUE;
    cp->hideTileValues = params->hideValues;
    cp->skipMQTTAdd = params->skipMQTTAdd;
    cp->skipCommitConfirm = params->skipCommitConfirm;
    cp->sortNewTiles = params->sortNewTiles;
    cp->showColors = params->showColors;
    cp->allowPeek = params->allowPeek;
    cp->showRobotScores = params->showRobotScores;
    XP_LOGFF( "showRobotScores: %s", boolToStr(cp->showRobotScores) );
#ifdef XWFEATURE_SLOW_ROBOT
    cp->robotThinkMin = params->robotThinkMin;
    cp->robotThinkMax = params->robotThinkMax;
    cp->robotTradePct = params->robotTradePct;
#endif
#ifdef XWFEATURE_ROBOTPHONIES
    cp->makePhonyPct = params->makePhonyPct;
#endif
#ifdef XWFEATURE_CROSSHAIRS
    cp->hideCrosshairs = params->hideCrosshairs;
#endif
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

            struct timeval tv = {};
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
        gchar* sum = g_compute_checksum_for_data( G_CHECKSUM_MD5, buf, len );
        XP_LOGF( "%s: sent %zd bytes with sum %s", __func__, len, sum );
        g_free( sum );
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
            gchar* sum = g_compute_checksum_for_data( G_CHECKSUM_MD5, buf, nBytes );
            XP_LOGF( "%s: got %d bytes with sum %s", __func__, nBytes, sum );
            g_free( sum );

            XWStreamCtxt* inboundS;
            XP_Bool redraw = XP_FALSE;
            
            inboundS = stream_from_msgbuf( cGlobals, buf, nBytes );
            if ( !!inboundS ) {
                CommsAddrRec addr = {};
                addr_addType( &addr, COMMS_CONN_RELAY );
                redraw = game_receiveMessage( &cGlobals->game, NULL_XWE, inboundS, &addr );

                stream_destroy( inboundS );
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

#if 0
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
#endif

#ifdef XWFEATURE_RELAY
void
linux_close_socket( CommonGlobals* cGlobals )
{
    LOG_FUNC();
    close( cGlobals->relaySocket );
    cGlobals->relaySocket = -1;
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
stream_from_msgbuf( CommonGlobals* cGlobals, const unsigned char* bufPtr, 
                    XP_U16 nBytes )
{
    XWStreamCtxt* result;
    result = mem_stream_make_raw( MPPARM(cGlobals->params->mpool)
                                  cGlobals->params->vtMgr );
    stream_putBytes( result, bufPtr, nBytes );

    return result;
} /* stream_from_msgbuf */

XP_Bool
linuxFireTimer( CommonGlobals* cGlobals, XWTimerReason why )
{
    TimerInfo* tip = &cGlobals->timerInfo[why];
    UtilTimerProc proc = tip->proc;
    XP_Bool draw = false;

    tip->proc = NULL;

    if ( !!proc ) {
        draw = (*proc)( tip->closure, NULL_XWE, why );
    } else {
        XP_LOGF( "%s: skipping timer %d; cancelled?", __func__, why );
    }
    return draw;
} /* linuxFireTimer */

/* static void */
/* linux_util_informMissing( XW_UtilCtxt* XP_UNUSED(uc), XWEnv XP_UNUSED(xwe), */
/*                           XP_Bool XP_UNUSED_DBG(isServer), */
/*                           const CommsAddrRec* XP_UNUSED(hostAddr), */
/*                           const CommsAddrRec* XP_UNUSED_DBG(selfAddr), */
/*                           XP_U16 XP_UNUSED_DBG(nDevs), */
/*                           XP_U16 XP_UNUSED_DBG(nMissing), */
/*                           XP_U16 XP_UNUSED_DBG(nInvited), */
/*                           XP_Bool XP_UNUSED_DBG(fromRematch) ) */
/* { */
/*     XP_LOGFF( "(isServer=%d, addr=%p, nDevs=%d, nMissing=%d, " */
/*               "nInvited=%d, fromRematch=%s)", isServer, selfAddr, */
/*               nDevs, nMissing, nInvited, boolToStr(fromRematch) ); */
/* } */

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
    PatDesc descs[3] = {};
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

    DictIter* iter = di_makeIter( dict, dimmp, strPats, nStrPats,
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
    /*     XP_UCHAR bufPrev[32] = {}; */
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
                XP_UCHAR buf2[64] = {};
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
            linux_dictionary_make( MPPARM(mpool) params, name,
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

void
setupLinuxUtilCallbacks( XW_UtilCtxt* util, XP_Bool useCurses )
{
#define SET_PROC(NAM) util->vtable->m_util_##NAM = linux_util_##NAM
    SET_PROC(formatPauseHistory);
#undef SET_PROC
    if ( useCurses ) {
        cb_setupUtilCallbacks( util );
    } else {
        setupGtkUtilCallbacks( util );
    }
}

/* void */
/* assertDrawCallbacksSet( const DrawCtxVTable* vtable ) */
/* { */
/*     bool allSet = true; */
/*     void(**proc)() = (void(**)())vtable; */
/*     for ( int ii = 0; ii < sizeof(*vtable)/sizeof(*proc); ++ii ) { */
/*         if ( !*proc ) { */
/*             XP_LOGF( "%s(): null ptr at index %d", __func__, ii ); */
/*             allSet = false; */
/*         } */
/*         ++proc; */
/*     } */
/*     XP_USE(allSet); */
/*     XP_ASSERT( allSet ); */
/* } */

static void
initParams( LaunchParams* params )
{
    memset( params, 0, sizeof(*params) );

#ifdef MEM_DEBUG
    params->mpool = mpool_make(__func__);
#endif

    params->vtMgr = make_vtablemgr(MPPARM_NOCOMMA(params->mpool));

    params->dutil = linux_dutils_init( MPPARM(params->mpool) params->vtMgr,
                                       params );
}

static void
testStreams( LaunchParams* params )
{
    XP_USE(params);
#if 0
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
        XP_USE(num);
        XP_LOGFF( "compariing num[%d]: %d with %d", ii, nums[ii], num );
        XP_ASSERT( num == nums[ii] );
    }

    stream_destroy( stream );
    XP_LOGFF( "OK!!" );
#endif
}

static void
testPhonies( LaunchParams* params )
{
    XW_DUtilCtxt* dutil = params->dutil;
    dvc_addLegalPhony( dutil, NULL_XWE, "en", "QI" );
    dvc_addLegalPhony( dutil, NULL_XWE, "de", "PUTZ" );
    XP_ASSERT( dvc_isLegalPhony( dutil, NULL_XWE, "en", "QI" ) );
    XP_ASSERT( !dvc_isLegalPhony( dutil, NULL_XWE, "fr", "QI" ) );
    XP_ASSERT( !dvc_isLegalPhony( dutil, NULL_XWE, "de", "QI" ) );
    XP_ASSERT( dvc_haveLegalPhonies( dutil, NULL_XWE ) );
    dvc_clearLegalPhony( dutil, NULL_XWE, "en", "QI" );
    XP_ASSERT( !dvc_isLegalPhony( dutil, NULL_XWE, "en", "QI" ) );
}

static void
freeParams( LaunchParams* params )
{
    linux_dutils_free( &params->dutil );
    vtmgr_destroy( MPPARM(params->mpool) params->vtMgr );

    gdb_close( params->pDb );
    params->pDb = NULL;

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
            linux_dictionary_make( MPPARM(params->mpool) params,
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
            dict_unref( dict, NULL_XWE );
        }
    }
    return result;
}
#endif

void
makeSelfAddress( CommsAddrRec* selfAddr, LaunchParams* params )
{
    XP_MEMSET( selfAddr, 0, sizeof(*selfAddr) );

    CommsConnType typ;
    for ( XP_U32 state = 0; types_iter( params->conTypes, &typ, &state ); ) {
        XP_LOGFF( "got type: %s", ConnType2Str(typ) );
        addr_addType( selfAddr, typ );
        switch ( typ ) {
        case COMMS_CONN_MQTT:
            dvc_getMQTTDevID( params->dutil, NULL_XWE, &selfAddr->u.mqtt.devID );
            XP_ASSERT( 0 != selfAddr->u.mqtt.devID );
            break;
        case COMMS_CONN_SMS:
            XP_ASSERT( !!params->connInfo.sms.myPhone[0] );
            XP_STRCAT( selfAddr->u.sms.phone, params->connInfo.sms.myPhone );
            XP_ASSERT( 1 == params->connInfo.sms.port ); /* It's ignored, but keep it 1 */
            selfAddr->u.sms.port = params->connInfo.sms.port;
            break;
        case COMMS_CONN_BT: {
            BTHostPair hp;
            lbt_setToSelf( params, &hp );
            addr_addBT( selfAddr, hp.hostName, hp.btAddr.chars );
        }
            break;
        default:
            XP_ASSERT(0);
        }
    }
}

RematchOrder
roFromStr(const char* rematchOrder )
{
    RematchOrder result;
    struct {
        char* str;
        RematchOrder ro;
    } vals [] = {
        { "same", RO_SAME },
        { "low_score_first", RO_LOW_SCORE_FIRST },
        { "high_score_first", RO_HIGH_SCORE_FIRST },
        { "juggle", RO_JUGGLE },
#ifdef XWFEATURE_RO_BYNAME
        { "by_name", RO_BY_NAME },
#endif
    };
    for ( int ii = 0; ii < VSIZE(vals); ++ii ) {
        if ( 0 == strcmp( rematchOrder, vals[ii].str ) ) {
            result = vals[ii].ro;
            break;
        }
    }
    XP_LOGFF( "(%s) => %d", rematchOrder, result );
    return result;
}

void
assertMainThread(const CommonGlobals* XP_UNUSED_DBG(cGlobals) )
{
#ifdef DEBUG
    if ( !!cGlobals && cGlobals->creator != pthread_self() ) {
        XP_LOGFF( "cGlobals: %p", cGlobals );
        XP_ASSERT( 0 );
    }
#endif
}

static void
writeStatus( const char* statusSocket, const char* dbName )
{
    int sock = socket( AF_UNIX, SOCK_DGRAM, 0 );

    DevSummary ds = {};
    sqlite3* pDb = gdb_open( dbName );
    gdb_getSummary( pDb, &ds );
    gdb_close( pDb );

    struct sockaddr_un addr = {};
    addr.sun_family = AF_UNIX;
    strncpy( addr.sun_path, statusSocket, sizeof(addr.sun_path) - 1);
    int err = connect( sock, (const struct sockaddr *) &addr, sizeof(addr));
    if ( !err ) {
        dprintf( sock, "{\"allDone\":%s, \"nTiles\":%d, \"nGames\":%d}",
                 boolToStr(ds.allDone), ds.nTiles, ds.nGames );
        close( sock );
    } else {
        XP_LOGFF( "error connecting: %d/%s", errno, strerror(errno) );
    }
}

#if 1
typedef struct TestThing {
    DLHead links;
    char* name;
} TestThing;

static int
compByName( const DLHead* dl1, const DLHead* dl2 )
{
    return strcmp( ((TestThing*)dl1)->name, ((TestThing*)dl2)->name );
}

static int
compByNameRev( const DLHead* dl1, const DLHead* dl2 )
{
    return strcmp( ((TestThing*)dl2)->name, ((TestThing*)dl1)->name );
}

static ForEachAct
mapProc( const DLHead* XP_UNUSED_DBG(dl), void* XP_UNUSED(closure))
{
    XP_LOGFF( "name: %s", ((TestThing*)dl)->name );
    return FEA_OK;
}

static TestThing*
removeAndMap( TestThing* list, TestThing* node )
{
    XP_LOGFF( "removing %s", node->name );
    list = (TestThing*)dll_remove( &list->links, &node->links );
    dll_map( &list->links, mapProc, NULL, NULL );
    return list;
}

static void
testDLL()
{
    TestThing* list = NULL;

    TestThing tss[] = {
        {.name = "Brynn"},
        {.name = "Ariela"},
        {.name = "Kati"},
        {.name = "Eric"},
    };
    for ( int ii = 0; ii < VSIZE(tss); ++ii ) {
        list = (TestThing*)dll_insert( &list->links, &tss[ii].links, compByName );
    }
    dll_map( &list->links, mapProc, NULL, NULL );

    list = (TestThing*)dll_sort( &list->links, compByNameRev );
    dll_map( &list->links, mapProc, NULL, NULL );

    list = removeAndMap( list, &tss[0] );
    list = removeAndMap( list, &tss[2] );
    list = removeAndMap( list, &tss[3] );
    list = removeAndMap( list, &tss[1] );
    XP_ASSERT( !list );
}
#else
# define testDLL()
#endif

#ifdef XWFEATURE_TESTSORT
typedef struct _SortTestElem {
    DLHead link;
    XP_UCHAR buf[32];           /* the word */
} SortTestElem;

#if 0
static int
compAlpha(const void* dl1, const void* dl2)
{
    gchar* elem1 = (gchar*)dl1;
    gchar* elem2 = (gchar*)dl2;
    int result = strcmp(elem1, elem2);
    // XP_LOGFF( "(%s, %s) => %d", elem1, elem2, result );
    return result;
}
#endif

static int
compLenAlpha(const void* dl1, const void* dl2,
             XWEnv XP_UNUSED(xwe), void* XP_UNUSED(closure))
{
    gchar* elem1 = (gchar*)dl1;
    int len1 = strlen(elem1);
    gchar* elem2 = (gchar*)dl2;
    int len2 = strlen(elem2);
    int result = len1 - len2;
    if ( 0 == result ) {
        result = strcmp(elem1, elem2);
    }
    // XP_LOGFF( "(%s, %s) => %d", elem1, elem2, result );
    return result;
}

static ForEachAct
printProc( void* XP_UNUSED_DBG(elem), void* closure, XWEnv XP_UNUSED(xwe) )
{
#ifdef DEBUG
    gchar* ste = (gchar*)elem;
#endif
    int* counter = (int*)closure;
    ++*counter;
    XP_LOGFF( "word %d: %s", *counter, ste );
    return FEA_OK;
}

static void
printList( XWArray* array )
{
    int counter = 0;
    arr_map( array, NULL_XWE, printProc, &counter );
}

static void
disposeProc( void* elem, void* XP_UNUSED(closure) )
{
    g_free( elem );
}

#if 1
static gchar*
addWord( XWArray* array, const gchar* word )
{
    XP_LOGFF( "adding %s", word );
    gchar* str = g_strdup(word);
    arr_insert( array, NULL_XWE, str );
    printList( array );
    return str;
}
#endif

static XP_Bool
testSort( LaunchParams* params )
{
    XP_Bool success = !!params->sortDict;
    if ( success ) {
        XWArray* array = arr_make( params->mpool, compLenAlpha, NULL );
#if 1
        addWord( array, "dd" );
        gchar* saveMe = addWord( array, "bb" );
        addWord( array, "aa" );
        addWord( array, "ee" );
        addWord( array, "cc" );

        arr_remove( array, NULL_XWE, saveMe );
        XP_LOGFF( "removed %s", saveMe );
        printList( array );
#else
        XP_LOGFF( "(sortdict: %s)", params->sortDict );
        DictionaryCtxt* dict =
            linux_dictionary_make( MPPARM(params->mpool) params,
                                   params->sortDict, params->useMmap );
        XP_ASSERT( !!dict );

        DictIter* iter = di_makeIter( dict, NULL_XWE, NULL, NULL, 0, NULL, 0 );
        XP_ASSERT( !!iter );

        XP_Bool success;
        XP_U32 ii = 0;
        for ( success = di_firstWord( iter ); success; success = di_getNextWord( iter ) ) {
            XP_UCHAR buf[32];
            di_wordToString( iter, buf, VSIZE(buf), NULL );
            gchar* word = g_strdup(buf);
            ++ii;
            arr_insert( array, word );
        }
        // printList(array);

        arr_setSort( array, compAlpha );
        arr_setSort( array, compLenAlpha );

        di_freeIter( iter, NULL_XWE );
        dict_unref( dict, NULL_XWE );
#endif
        printList(array);
        arr_removeAll( array, disposeProc, NULL );

        arr_destroy( array );
    }
    return success;
/* ======= */
/* static void */
/* testSort( LaunchParams* params ) */
/* { */
/*     DLHead* list = NULL; */

/*     XP_LOGFF( "(sortdict: %s)", params->sortDict ); */
/*     DictionaryCtxt* dict = */
/*         linux_dictionary_make( MPPARM(params->mpool) NULL_XWE, params, params->sortDict, */
/*                                params->useMmap ); */
/*     XP_ASSERT( !!dict ); */

/*     DictIter* iter = di_makeIter( dict, NULL_XWE, NULL, NULL, 0, NULL, 0 ); */
/*     XP_ASSERT( !!iter ); */

/*     XP_Bool success; */
/*     XP_U32 ii = 0; */
/*     for ( success = di_firstWord( iter ); success; success = di_getNextWord( iter ) ) { */
/*         SortTestElem* elem = calloc( 1, sizeof(*elem) ); */
/*         di_wordToString( iter, elem->buf, VSIZE(elem->buf), NULL ); */
/*         ++ii; */
/*         // list = dll_insert(list, &elem->link, NULL ); */
/*         list = dll_insert(list, &elem->link, compLenAlpha ); */
/*     } */

/*     // list = dll_sort( list, compLenAlpha ); */
/*     printList(list); */

/*     di_freeIter( iter, NULL_XWE ); */
/*     dll_removeAll( list, disposeProc, NULL ); */
/* >>>>>>> a9173b96e (add test of sorting in my list.) */
}
#endif

static void
onDictAdded( void* closure, const XP_UCHAR* dictName )
{
    XP_LOGFF( "%s", dictName );
    LaunchParams* params = (LaunchParams*)closure;
    XP_UCHAR shortName[64];
    stripExtn( dictName, shortName, VSIZE(shortName) );
    dvc_onDictAdded( params->dutil, NULL_XWE, shortName );
}

static void
onDictGone( void* closure, const XP_UCHAR* dictName )
{
    XP_LOGFF( "(%s)", dictName );
    LaunchParams* params = (LaunchParams*)closure;
    XP_UCHAR shortName[64];
    stripExtn( dictName, shortName, VSIZE(shortName) );
    dvc_onDictRemoved( params->dutil, NULL_XWE, shortName );
}

int
main( int argc, char** argv )
{
    XP_LOGFF( "%s starting; ptr size: %zu", argv[0], sizeof(argv) );

    testDLL();
    // mtx_crashToTest();
    // return 0;

    int opt;
    int totalPlayerCount = 0;
    XP_Bool isServer = XP_FALSE;
    // char* portNum = NULL;
    // char* hostName = "localhost";
    unsigned int seed = makeRandomInt();
    LaunchParams mainParams = {};
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

    setlocale(LC_ALL, "");

    XP_LOGFF( "pid = %d", getpid() );
#ifdef DEBUG
    syslog( LOG_DEBUG, "main started: pid = %d", getpid() );
#endif

#ifdef DEBUG
    {
        for ( int ii = 0; ii < argc; ++ii ) {
            XP_LOGFF( "arg[%d]: %s", ii, argv[ii] );
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
    mainParams.connInfo.mqtt.hostName = "localhost";
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
    mainParams.showRobotScores = XP_TRUE;
    mainParams.useMmap = XP_TRUE;
    mainParams.useUdp = true;
    mainParams.dbName = "xwgames.sqldb";
    mainParams.cursesListWinHt = 10;
    types_addType( &mainParams.conTypes, COMMS_CONN_MQTT );

    mainParams.ldm = ldm_init( onDictAdded, onDictGone, &mainParams );
    ldm_addDir( mainParams.ldm, "./" );

    if ( file_exists( "./dict.xwd" ) )  {
        trimDictPath( "./dict.xwd", dictbuf, VSIZE(dictbuf), &path, &dict );
        str2ChrArray( mainParams.pgi.dictName, dict );
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
            ldm_addDir( mainParams.ldm, path );
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
    const char* statusSocket = NULL;
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
        case CMD_NO_SHOW_OTHERSCORES:
            mainParams.showRobotScores = XP_FALSE;
            break;
        case CMD_SKIP_BT:
            mainParams.disableBT = XP_TRUE;
            break;
#ifdef XWFEATURE_RELAY
        case CMD_ROOMNAME:
            mainParams.connInfo.relay.invite = optarg;
            addr_addType( &mainParams.addr, COMMS_CONN_RELAY );
            // isServer = XP_TRUE; /* implicit */
            break;
#endif
#ifdef XWFEATURE_DIRECTIP
        case CMD_HOSTIP:
            mainParams.connInfo.ip.hostName = optarg;
            types_addType( &mainParams.conTypes, COMMS_CONN_IP_DIRECT );
            break;
        case CMD_HOSTPORT:
            mainParams.connInfo.ip.hostPort = atoi(optarg);
            types_addType( &mainParams.conTypes, COMMS_CONN_IP_DIRECT );
            break;
        case CMD_MYPORT:
            mainParams.connInfo.ip.myPort = atoi(optarg);
            types_addType( &mainParams.conTypes, COMMS_CONN_IP_DIRECT );
            break;
#endif
        case CMD_DICT:
            trimDictPath( optarg, dictbuf, VSIZE(dictbuf), &path, &dict );
            str2ChrArray( mainParams.pgi.dictName, dict );
            if ( !path ) {
                path = ".";
            }
            XP_LOGFF( "appending dict path: %s", path );
            ldm_addDir( mainParams.ldm, path );
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
#ifdef XWFEATURE_TESTSORT
        case CMD_SORTDICT:
            mainParams.sortDict = optarg;
            XP_LOGFF( "set testdict: %s/%s", optarg, mainParams.sortDict );
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
            ldm_addDir( mainParams.ldm, optarg );
            break;
        case CMD_PLAYERDICT:
            trimDictPath( optarg, dictbuf, VSIZE(dictbuf), &path, &dict );
            mainParams.playerDictNames[nPlayerDicts++] = dict;
            if ( !path ) {
                path = ".";
            }
            ldm_addDir( mainParams.ldm, path );
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
        case CMD_SHOWGAMES:
            mainParams.showGames = XP_TRUE;
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
            str2ChrArray(mainParams.pgi.players[mainParams.nLocalPlayers-1].password, optarg );
            break;
        case CMD_LOCALSMARTS:
            index = mainParams.pgi.nPlayers - 1;
            XP_ASSERT( LP_IS_ROBOT( &mainParams.pgi.players[index] ) );
            mainParams.pgi.players[index].robotIQ = atoi(optarg);
            break;
#ifdef XWFEATURE_SMS
        case CMD_SMSNUMBER:		/* SMS phone number */
            mainParams.connInfo.sms.myPhone = optarg;
            types_addType( &mainParams.conTypes, COMMS_CONN_SMS );
            break;
        case CMD_INVITEE_SMSNUMBER:
            mainParams.connInfo.sms.inviteePhones =
                g_slist_append( mainParams.connInfo.sms.inviteePhones, optarg );
            types_addType( &mainParams.conTypes, COMMS_CONN_SMS );
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
            types_addType( &mainParams.conTypes, COMMS_CONN_SMS );
            break;
#endif
        case CMD_WITHOUT_MQTT:
        case CMD_SKIP_MQTT:
            /* These should be joined */
            mainParams.skipMQTTAdd = XP_TRUE;
            types_rmType( &mainParams.conTypes, COMMS_CONN_MQTT );
            break;
        case CMD_MQTTHOST:
            mainParams.connInfo.mqtt.hostName = optarg;
            break;
        case CMD_MQTTPORT:
            mainParams.connInfo.mqtt.port = atoi(optarg);
            break;
        case CMD_INVITEE_MQTTDEVID:
            XP_ASSERT( 16 == strlen(optarg) );
            mainParams.connInfo.mqtt.inviteeDevIDs =
                g_slist_append( mainParams.connInfo.mqtt.inviteeDevIDs, optarg );
            types_addType( &mainParams.conTypes, COMMS_CONN_MQTT );
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
        case CMD_LOCALNAME:
            mainParams.localName = optarg;
            break;
        case CMD_PLAYERNAME:
            index = mainParams.pgi.nPlayers++;
            XP_ASSERT( index < MAX_NUM_PLAYERS );
            ++mainParams.nLocalPlayers;
            mainParams.pgi.players[index].robotIQ = 0; /* means human */
            mainParams.pgi.players[index].isLocal = XP_TRUE;
            XP_ASSERT( !mainParams.pgi.players[index].name[0] );
            str2ChrArray(mainParams.pgi.players[index].name, optarg);
            break;
        case CMD_REMOTEPLAYER:
            index = mainParams.pgi.nPlayers++;
            XP_ASSERT( index < MAX_NUM_PLAYERS );
            mainParams.pgi.players[index].isLocal = XP_FALSE;
            ++mainParams.info.serverInfo.nRemotePlayers;
            break;
        case CMD_ROBOTNAME:
            ++robotCount;
            index = mainParams.pgi.nPlayers++;
            XP_ASSERT( index < MAX_NUM_PLAYERS );
            ++mainParams.nLocalPlayers;
            mainParams.pgi.players[index].robotIQ = 1; /* real smart by default */
            mainParams.pgi.players[index].isLocal = XP_TRUE;
            XP_ASSERT( !mainParams.pgi.players[index].name[0] );
            str2ChrArray( mainParams.pgi.players[index].name, optarg );
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
#ifdef XWFEATURE_RELAY
        case CMD_RELAY_PORT:
            addr_addType( &mainParams.addr, COMMS_CONN_RELAY );
            mainParams.connInfo.relay.defaultSendPort = atoi( optarg );
            break;

        case CMD_HOSTNAME:
            /* mainParams.info.clientInfo.serverName =  */
            addr_addType( &mainParams.addr, COMMS_CONN_RELAY );
            mainParams.connInfo.relay.relayName = optarg;
            break;
        case CMD_ADVERTISEROOM:
            mainParams.connInfo.relay.advertiseRoom = true;
            break;
        case CMD_JOINADVERTISED:
            mainParams.connInfo.relay.seeksPublicRoom = true;
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
        case CMD_CLOSESTDIN:
            mainParams.closeStdin = XP_TRUE;
            break;
        case CMD_SKIP_ERRS:
            mainParams.skipUserErrs = XP_TRUE;
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
            types_addType( &mainParams.conTypes, COMMS_CONN_BT );
            mainParams.connInfo.bt.btaddr = optarg;
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
        case CMD_CHAT:
            mainParams.chatsInterval = atoi(optarg);
            break;
#ifdef XWFEATURE_RELAY
        case CMD_SPLITPACKETS:
            mainParams.splitPackets = atoi( optarg );
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
#endif
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

        case CMD_REMATCH_ON_OVER:
            mainParams.rematchOnDone = XP_TRUE;
            break;

        case CMD_STATUS_SOCKET_NAME:
            statusSocket = optarg;
            break;

        case CMD_CMDS_SOCKET_NAME:
            mainParams.cmdsSocket = optarg;
            break;

        default:
            done = true;
            break;
        }
    }

#ifdef XWFEATURE_TESTSORT
    if ( testSort( &mainParams ) ) {
        exit(0);
    }
#endif

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
            mainParams.pgi.deviceRole = ROLE_STANDALONE;
        } else if ( isServer ) {
            if ( mainParams.info.serverInfo.nRemotePlayers > 0 ) {
                mainParams.pgi.deviceRole = ROLE_ISHOST;
            }
        } else {
            mainParams.pgi.deviceRole = ROLE_ISGUEST;
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

        if ( !!mainParams.pgi.dictName[0] ) {
            /* char path[256]; */
            /* getDictPath( &mainParams, mainParams.gi.dictName, path, VSIZE(path) ); */
            DictionaryCtxt* dict =
                linux_dictionary_make( MPPARM(mainParams.mpool) &mainParams,
                                       mainParams.pgi.dictName,
                                       mainParams.useMmap );
            XP_ASSERT( !!dict );
            XP_STRNCPY( mainParams.pgi.isoCodeStr, dict_getISOCode( dict ),
                        VSIZE(mainParams.pgi.isoCodeStr) );
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
             && ROLE_STANDALONE == mainParams.pgi.deviceRole ) {
            mainParams.needsNewGame = XP_TRUE;
        }

        /* per-player dicts are for local players only.  Assign in the order
           given.  It's an error to give too many, or not to give enough if
           there's no game-dict */
        if ( 0 < nPlayerDicts ) {
            XP_ASSERT(0);       /* fix me */
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

#ifdef XWFEATURE_WALKDICT
        if ( !!testDicts ) {
            walk_dict_test_all( MPPARM(mainParams.mpool) &mainParams, testDicts, testPrefixes );
            exit( 0 );
        }
#endif

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
                mainParams.deviceRole = ROLE_STANDALONE;
            } else {
                mainParams.deviceRole = ROLE_ISHOST;
            }	    
        } else {
            mainParams.deviceRole = ROLE_ISGUEST;
        }

        XP_ASSERT( !!mainParams.dbName );
        mainParams.pDb = gdb_open( mainParams.dbName );

        dvc_init( mainParams.dutil, NULL_XWE );
        testPhonies( &mainParams );
        lbt_init( &mainParams );

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

        lbt_destroy( &mainParams );
        freeParams( &mainParams );
    }

    free( longopts );
    ldm_destroy( mainParams.ldm );

    gsw_logIdles();

    if ( !!statusSocket ) {
        writeStatus( statusSocket, mainParams.dbName );
    }

    mempool_dbg_checkall();
    
    XP_LOGFF( "%s exiting, returning %d", argv[0], result );
    return result;
} /* main */
