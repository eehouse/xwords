/* -*- compile-command: "make MEMDEBUG=TRUE -j3"; -*- */
/* 
 * Copyright 2000-2011 by Eric House (xwords@eehouse.org).  All rights
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

#ifdef TEXT_MODEL
#include <glib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "mvcount.h"

typedef struct _MvCntGlobals {
    CommonGlobals cGlobals;
    GMainLoop* loop;
    GIOChannel* boardPipe;
    GIOChannel* rackPipe;
    XWStreamCtxt* boardStream;
    XWStreamCtxt* rackStream;
    guint rackSrcID;
    guint boardSrcID;

    GArray* move_array;

    XP_UCHAR charBuf[MAX_TRAY_TILES+1];
    XP_U16 nInRack;

    XP_U16 nInLine, lineNo;
    char lineBufs[15][16];
} MvCntGlobals;

static MvCntGlobals* s_globals = NULL;

static void openRackReader( MvCntGlobals* globals );
static void openBoardReader( MvCntGlobals* globals );


static void
handle_sigintterm( int XP_UNUSED_DBG(sig) )
{
    XP_LOGF( "%s(%d)", __func__, sig );
    g_main_quit( s_globals->loop );
}

static void
startCoord( const MoveInfo* move, XP_UCHAR* out, 
            XP_U16 XP_UNUSED_DBG(outLen) )
{
    XP_U8 xx, yy;
    if ( move->isHorizontal ) {
        xx = move->commonCoord;
        yy = move->tiles[0].varCoord;
    } else {
        yy = move->commonCoord;
        xx = move->tiles[0].varCoord;
    }
    XP_ASSERT( outLen >= 3 );
    out[0] = 'A' + xx;
    out[1] = 'a' + yy;
    out[2] = '\0';
}

static void
startSaving( MvCntGlobals* globals )
{
    globals->move_array = g_array_new( FALSE, FALSE, sizeof(XP_UCHAR*) );
}

static gint
compareFunc( gconstpointer a, gconstpointer b)
{
    gchar* stra = *(gchar**)a;
    gchar* strb = *(gchar**)b;
    return strcmp( strb, stra );
}

static void
sortSaved( MvCntGlobals* globals )
{
    g_array_sort( globals->move_array, compareFunc );
}

static void
printSaved( MvCntGlobals* globals )
{
    int ii;
    GArray* move_array = globals->move_array;
    for ( ii = 0; ii < move_array->len; ++ii ) {
        gchar* str = g_array_index( move_array, gchar*, ii );
        fprintf( stdout, "%s\n", str );
        g_free( str );
    }
    (void)g_array_free( move_array, TRUE );
    globals->move_array = NULL;
}

static void
saver( void* closure, XP_U16 score, const MoveInfo* move )
{
    MvCntGlobals* globals = (MvCntGlobals*)closure;
    const DictionaryCtxt* dict = globals->cGlobals.params->dict;
    XP_U16 ii;
    char buf[64];
    XP_UCHAR start[3];
    XP_U16 len;

    startCoord( move, start, sizeof(start) );
    len = snprintf( buf, sizeof(buf), "%.04d:%s%c:", score, 
                    start, move->isHorizontal?'A':'D' );
    for ( ii = 0; ii < move->nTiles; ++ii ) {
        Tile tile = move->tiles[ii].tile;
        XP_Bool isBlank = IS_BLANK( tile );

        if ( isBlank ) {
            tile &= TILE_VALUE_MASK;
        }

        const XP_UCHAR* letter = dict_getTileString( dict, tile );
        XP_UCHAR tmp[4];
        XP_STRNCPY( tmp, letter, sizeof(tmp) );
        if ( isBlank ) {
            XP_LOWERSTR( tmp );
        }
        len += snprintf( buf+len, sizeof(buf)-len, "%s", tmp );
    }

    gchar* str = g_strdup( buf );
    g_array_append_val( globals->move_array, str );
} /* saver */

static void
genMovesForString( MvCntGlobals* globals, const XP_UCHAR* rack )
{
    const DictionaryCtxt* dict = globals->cGlobals.params->dict;
    Tile tiles[MAX_TRAY_TILES];
    XP_U16 len = XP_STRLEN( rack );
    XP_U16 ii;

    for ( ii = 0; ii < len; ++ii ) {
        XP_UCHAR letter[2] = { rack[ii], '\0' };
        if ( '_' == letter[0] ) {
            tiles[ii] = dict_getBlankTile( dict );
        } else {
            tiles[ii] = dict_tileForString( dict, letter );
        }
    }
    startSaving( globals );
    server_genMoveListWith( globals->cGlobals.game.server, 
                            tiles, len, saver, globals );
    sortSaved( globals );
    printSaved( globals );
}

static void
checkBoard( const char* buf, XP_U16 len )
{
    int ii, nLines = 0;
    for ( ii = 0; ii < len; ++ii ) {
        if ( 15 == ii % 16 ) {
            if ( '\n' == buf[ii] ) {
                ++nLines;
            } else {
                fprintf( stderr, "badly formatted line %d\n", nLines );
                assert( 0 );
                exit( 1 );
            }
        }
    }
    if ( 15 != nLines ) {
        fprintf( stderr, "expected 15 lines; found %d\n", nLines );
        assert( 0 );
        exit( 1 );
    }
}

static void
loadBoard( MvCntGlobals* globals, const char* board, XP_U16 len )
{
    LOG_FUNC();
    const LaunchParams* params = globals->cGlobals.params;
    XWStreamCtxt* stream;
    
    checkBoard( board, len );

    stream = mem_stream_make( MPPARM(params->util->mpool) params->vtMgr, 
                              globals, CHANNEL_NONE, NULL );
    stream_putBytes( stream, board, len );
    model_setFromTextStream( globals->cGlobals.game.model, stream );
    stream_destroy( stream );
    LOG_RETURN_VOID();
}

static void
loadBoardFrom( MvCntGlobals* globals, const char* boardFile )
{
    FILE* file = fopen( boardFile, "r" );

    if ( NULL == file ) {
        fprintf( stderr, "unable to open %s\n", boardFile );
    } else {
        char buf[16*15];
        size_t nRead = fread( buf, sizeof(buf), 1, file );
        fclose( file );

        if ( 1 != nRead ) {
            fprintf( stderr, "bad read of %s\n", boardFile );
        } else {
            loadBoard( globals, buf, sizeof(buf) );
        }
    }
    
}

static gboolean
rackRead( GIOChannel* source, GIOCondition condition, gpointer data )
{
    int fd = g_io_channel_unix_get_fd( source );
    MvCntGlobals* globals = (MvCntGlobals*)data;
    gboolean go_on = TRUE;

    if ( 0 != (condition & G_IO_IN) ) {
        for ( ; ; ) {
            ssize_t nRead = read( fd, &globals->charBuf[globals->nInRack], 
                                  sizeof(globals->charBuf) - globals->nInRack );
            if ( 0 >= nRead ) {
                break;
            } else {
                globals->nInRack += nRead;
                if ( globals->nInRack > 0 && 
                     globals->charBuf[globals->nInRack-1] == '\n' ) {
                    globals->charBuf[globals->nInRack-1] = '\0';
                    genMovesForString( globals, globals->charBuf );
                    globals->nInRack = 0;
                }
            }
        }
    }

    if ( (condition & G_IO_HUP) != 0 ) {
        XP_LOGF( "%s: G_IO_HUP", __func__ );
        close( fd );
        go_on = FALSE;

        openRackReader( globals );
    } else if ( (condition & G_IO_ERR) != 0 ) {
        XP_LOGF( "%s: G_IO_ERR", __func__ );
        go_on = FALSE;
    }

    return go_on;
} /* rackRead */

static gboolean
boardRead( GIOChannel* source, GIOCondition condition, gpointer data )
{
    gboolean go_on = TRUE;
    XP_LOGF( "got board data!!!" );
    MvCntGlobals* globals = (MvCntGlobals*)data;
    int fd = g_io_channel_unix_get_fd( source );

    if ( 0 != (condition & G_IO_IN) ) {
        for ( ; ; ) {
            char* curBuf = globals->lineBufs[globals->lineNo];
            int toRead = sizeof(globals->lineBufs[0])-globals->nInLine;
            XP_LOGF( "looking for %d bytes", toRead );
            ssize_t nRead = read( fd, &curBuf[globals->nInLine], toRead );
            if ( 0 >= nRead ) {
                break;
            } else {
                XP_LOGF( "%s: read %d bytes", __func__, nRead );
                globals->nInLine += nRead;
                if ( globals->nInLine == sizeof(globals->lineBufs[0]) ) {
                    if ( curBuf[globals->nInLine-1] != '\n' ) {
                        fprintf( stderr, "badly formed line %d", 
                                 globals->lineNo );
                        assert( 0 );
                        exit( 1 );
                    } else {
                        if ( ++globals->lineNo == VSIZE(globals->lineBufs) ) {
                            loadBoard( globals, globals->lineBufs[0], 
                                       sizeof(globals->lineBufs) );
                            globals->lineNo = 0;
                        }
                        globals->nInLine = 0;
                    }
                }
            }
        }
    }

    if ( (condition & G_IO_HUP) != 0 ) {
        XP_LOGF( "G_IO_HUP" );
        close( fd );
        go_on = FALSE;
        openBoardReader( globals );

    } else if ( (condition & G_IO_ERR) != 0 ) {
        XP_LOGF( "G_IO_ERR" );
        go_on = FALSE;
    }

    return go_on;
} /* boardRead */

static guint
makeChannel( MvCntGlobals*globals, const char* path, GIOFunc func )
{
    int fd = open( path, O_RDONLY | O_NONBLOCK );
    GIOChannel* channel = g_io_channel_unix_new( fd );
    XP_ASSERT( !!channel );
    XP_LOGF( "got channel back" );

    guint srcID = g_io_add_watch( channel, G_IO_IN | G_IO_ERR | G_IO_HUP,
                                  func, globals );
    return srcID;
}

static void 
openRackReader( MvCntGlobals* globals )
{
    const LaunchParams* params = globals->cGlobals.params;
    globals->rackSrcID = makeChannel( globals, params->rackPipe, rackRead );
}

static void 
openBoardReader( MvCntGlobals* globals )
{
    const LaunchParams* params = globals->cGlobals.params;
    globals->boardSrcID = makeChannel( globals, params->boardPipe, 
                                       boardRead );
}

static void
makeGame( MvCntGlobals* globals )
{
    game_makeNewGame( MPPARM(globals->cGlobals.params->util->mpool) 
                      &globals->cGlobals.game, &globals->cGlobals.params->gi, 
                      globals->cGlobals.params->util, NULL, /* no drawing */
                      NULL, NULL );
    model_setDictionary( globals->cGlobals.game.model, 
                         globals->cGlobals.params->dict );
}

static void
cleanup( MvCntGlobals* globals )
{
    game_dispose( &globals->cGlobals.game );
    gi_disposePlayerInfo( MPPARM(globals->cGlobals.params->util->mpool)
                          &globals->cGlobals.params->gi );
}

static XP_Bool
mvc_util_hiliteCell( XW_UtilCtxt* XP_UNUSED(uc), XP_U16 XP_UNUSED(col), 
                     XP_U16 XP_UNUSED(row) )
{
    return XP_TRUE;
}

static XP_Bool
mvc_util_engineProgressCallback( XW_UtilCtxt* XP_UNUSED(uc) )
{
    return XP_TRUE;
}

static void
initUtils( XW_UtilCtxt* util )
{
    util->vtable->m_util_hiliteCell = mvc_util_hiliteCell;
    util->vtable->m_util_engineProgressCallback = 
        mvc_util_engineProgressCallback;
}

int
movecount_main( LaunchParams* params )
{
    MvCntGlobals globals;
    XP_MEMSET( &globals, 0, sizeof(globals) );
    globals.cGlobals.params = params;
    s_globals = &globals;
    params->util->closure = &globals;

    struct sigaction act = { .sa_handler = handle_sigintterm };
    sigaction( SIGINT, &act, NULL );
    sigaction( SIGTERM, &act, NULL );

    initUtils( params->util );

    /* globals.boardStream = mem_stream_make( MPPARM(params->util->mpool)  */
    /*                                        params->vtMgr,  */
    /*                                        &globals, CHANNEL_NONE, NULL ); */
    /* globals.rackStream = mem_stream_make( MPPARM(params->util->mpool)  */
    /*                                       params->vtMgr,  */
    /*                                       &globals, CHANNEL_NONE, NULL ); */

    makeGame( &globals );

    globals.loop = g_main_loop_new( NULL, FALSE );

    if ( params->boardFile ) {
        loadBoardFrom( &globals, params->boardFile );
    }
    if ( params->rackString ) {
        genMovesForString( &globals, params->rackString );
    }

    if ( !!params->rackPipe ) {
        openRackReader( &globals );
    }
    if ( params->boardPipe ) {
        openBoardReader( &globals );
    }

    if ( !!globals.rackSrcID || !!params->boardPipe ) {
        g_main_loop_run( globals.loop );
    }
    
    // dict_destroy( params->dict );
    cleanup( &globals );
    stream_destroy( globals.boardStream );
    stream_destroy( globals.rackStream );

    return 0;
}

#endif
