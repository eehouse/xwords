/* -*-mode: C; fill-column: 78; c-basic-offset: 4;  compile-command: "make MEMDEBUG=TRUE"; -*- */
/* 
 * Copyright 2000-2003 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#ifdef PLATFORM_GTK

#include <gtk/gtk.h>
#include <stdio.h>
#include <stdarg.h>

#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <unistd.h>
#include <sys/stat.h>
#include <ctype.h>
#include <gdk/gdkkeysyms.h>
#include <errno.h>
#ifndef CLIENT_ONLY
/*  # include <prc.h> */
#endif
#include <sys/types.h>
#include <unistd.h>

#include "main.h"
#include "linuxmain.h"
#include "linuxbt.h"
/* #include "gtkmain.h" */

#include "draw.h"
#include "game.h"
#include "gtkask.h"
#include "gtknewgame.h"
#include "gtkletterask.h"
#include "gtkpasswdask.h"
#include "gtkntilesask.h"
/* #include "undo.h" */
#include "gtkdraw.h"
#include "memstream.h"
#include "filestream.h"

/* static guint gtkSetupClientSocket( GtkAppGlobals* globals, int sock ); */
static void sendOnClose( XWStreamCtxt* stream, void* closure );
static XP_Bool file_exists( const char* fileName );
static void gtkListenOnSocket( GtkAppGlobals* globals, int newSock );
static void setCtrlsForTray( GtkAppGlobals* globals );
static void printFinalScores( GtkAppGlobals* globals );

#define TRAY_HT_ROWS 3

#if 0
static XWStreamCtxt*
lookupClientStream( GtkAppGlobals* globals, int sock ) 
{
    short i;
    for ( i = 0; i < MAX_NUM_PLAYERS; ++i ) {
        ClientStreamRec* rec = &globals->clientRecs[i];
        if ( rec->sock == sock ) {
            XP_ASSERT( rec->stream != NULL );
            return rec->stream;
        }
    }
    XP_ASSERT( i < MAX_NUM_PLAYERS );
    return NULL;
} /* lookupClientStream */

static void
rememberClient( GtkAppGlobals* globals, guint key, int sock, 
		XWStreamCtxt* stream )
{
    short i;
    for ( i = 0; i < MAX_NUM_PLAYERS; ++i ) {
        ClientStreamRec* rec = &globals->clientRecs[i];
        if ( rec->stream == NULL ) {
            XP_ASSERT( stream != NULL );
            rec->stream = stream;
            rec->key = key;
            rec->sock = sock;
            break;
        }
    }
    XP_ASSERT( i < MAX_NUM_PLAYERS );
} /* rememberClient */
#endif

static gint
button_press_event( GtkWidget* XP_UNUSED(widget), GdkEventButton *event,
                    GtkAppGlobals* globals )
{
    XP_Bool redraw, handled;

    globals->mouseDown = XP_TRUE;

    redraw = board_handlePenDown( globals->cGlobals.game.board, 
                                  event->x, event->y, &handled );
    if ( redraw ) {
        board_draw( globals->cGlobals.game.board );
    }
    return 1;
} /* button_press_event */

static gint
motion_notify_event( GtkWidget* XP_UNUSED(widget), GdkEventMotion *event,
                     GtkAppGlobals* globals )
{
    XP_Bool handled;

    if ( globals->mouseDown ) {
        handled = board_handlePenMove( globals->cGlobals.game.board, event->x, 
                                       event->y );
        if ( handled ) {
            board_draw( globals->cGlobals.game.board );
        }
    } else {
        handled = XP_FALSE;
    }

    return handled;
} /* motion_notify_event */

static gint
button_release_event( GtkWidget* XP_UNUSED(widget), GdkEventMotion *event,
                      GtkAppGlobals* globals )
{
    XP_Bool redraw;

    if ( globals->mouseDown ) {
        redraw = board_handlePenUp( globals->cGlobals.game.board, 
                                    event->x, 
                                    event->y );
        if ( redraw ) {
            board_draw( globals->cGlobals.game.board );
        }
        globals->mouseDown = XP_FALSE;
    }
    return 1;
} /* button_release_event */

#ifdef KEY_SUPPORT
static XP_Key
evtToXPKey( GdkEventKey* event, XP_Bool* movesCursorP )
{
    XP_Key xpkey = XP_KEY_NONE;
    XP_Bool movesCursor = XP_FALSE;
    guint keyval = event->keyval;

    switch( keyval ) {
#ifdef KEYBOARD_NAV
    case GDK_Return:
        xpkey = XP_RETURN_KEY;
        break;
    case GDK_space:
        xpkey = XP_RAISEFOCUS_KEY;
        break;

    case GDK_Left:
        xpkey = XP_CURSOR_KEY_LEFT;
        movesCursor = XP_TRUE;
        break;
    case GDK_Right:
        xpkey = XP_CURSOR_KEY_RIGHT;
        movesCursor = XP_TRUE;
        break;
    case GDK_Up:
        xpkey = XP_CURSOR_KEY_UP;
        movesCursor = XP_TRUE;
        break;
    case GDK_Down:
        xpkey = XP_CURSOR_KEY_DOWN;
        movesCursor = XP_TRUE;
        break;
#endif
    case GDK_BackSpace:
        XP_LOGF( "... it's a DEL" );
        xpkey = XP_CURSOR_KEY_DEL;
        break;
    default:
        keyval = keyval & 0x00FF; /* mask out gtk stuff */
        if ( isalpha( keyval ) ) {
            xpkey = toupper(keyval);
            break;
        }
    }
    *movesCursorP = movesCursor;
    return xpkey;
} /* evtToXPKey */

#ifdef KEYBOARD_NAV
static gint
key_press_event( GtkWidget* XP_UNUSED(widget), GdkEventKey* event,
                 GtkAppGlobals* globals )
{
    XP_Bool handled = XP_FALSE;
    XP_Bool movesCursor;
    XP_Key xpkey = evtToXPKey( event, &movesCursor );

    if ( xpkey != XP_KEY_NONE ) {
        XP_Bool draw = globals->keyDown ?
            board_handleKeyRepeat( globals->cGlobals.game.board, xpkey, 
                                   &handled )
            : board_handleKeyDown( globals->cGlobals.game.board, xpkey,
                                   &handled );
        if ( draw ) {
            board_draw( globals->cGlobals.game.board );
        }
    }
    globals->keyDown = XP_TRUE;
    return 1;
}
#endif

static gint
key_release_event( GtkWidget* XP_UNUSED(widget), GdkEventKey* event,
                   GtkAppGlobals* globals )
{
    XP_Bool handled = XP_FALSE;
    XP_Bool movesCursor;
    XP_Key xpkey = evtToXPKey( event, &movesCursor );

    if ( xpkey != XP_KEY_NONE ) {
        XP_Bool draw;
        draw = board_handleKeyUp( globals->cGlobals.game.board, xpkey, 
                                  &handled );
#ifdef KEYBOARD_NAV
        if ( movesCursor && !handled ) {
            BoardObjectType order[] = { OBJ_SCORE, OBJ_BOARD, OBJ_TRAY };
            draw = linShiftFocus( &globals->cGlobals, xpkey, order,
                                  NULL ) || draw;
        }
#endif
        if ( draw ) {
            board_draw( globals->cGlobals.game.board );
        }
    }

/*     XP_ASSERT( globals->keyDown ); */
    globals->keyDown = XP_FALSE;

    return handled? 1 : 0;        /* gtk will do something with the key if 0 returned  */
} /* key_release_event */
#endif

#ifdef MEM_DEBUG
# define MEMPOOL globals->cGlobals.params->util->mpool,
#else
# define MEMPOOL
#endif

static XWStreamCtxt*
streamFromFile( GtkAppGlobals* globals, char* name )
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

    stream = mem_stream_make( MEMPOOL 
                              globals->cGlobals.params->vtMgr, 
                              globals, CHANNEL_NONE, NULL );
    stream_putBytes( stream, buf, statBuf.st_size );
    free( buf );

    return stream;
} /* streamFromFile */

static void
createOrLoadObjects( GtkAppGlobals* globals )
{
    XWStreamCtxt* stream = NULL;
    XP_Bool opened = XP_FALSE;

    Connectedness serverRole = globals->cGlobals.params->serverRole;
    XP_Bool isServer = serverRole != SERVER_ISCLIENT;
    LaunchParams* params = globals->cGlobals.params;

    globals->draw = (GtkDrawCtx*)gtkDrawCtxtMake( globals->drawing_area,
                                                  globals );

    if ( !!params->fileName && file_exists( params->fileName ) ) {

        stream = streamFromFile( globals, params->fileName );

        opened = game_makeFromStream( MEMPOOL stream, &globals->cGlobals.game, 
                                      &globals->cGlobals.params->gi, 
                                      params->dict, params->util, 
                                      (DrawCtx*)globals->draw, 
                                      &globals->cp,
                                      linux_send, globals );
        
        stream_destroy( stream );
    }

    if ( !opened ) {
        XP_U16 gameID;
        CommsAddrRec addr;

        addr.conType = params->conType;

        gameID = (XP_U16)util_getCurSeconds( globals->cGlobals.params->util );

#ifdef XWFEATURE_RELAY
        if ( addr.conType == COMMS_CONN_RELAY ) {
            XP_ASSERT( !!params->connInfo.relay.relayName );
            globals->cGlobals.defaultServerName
                = params->connInfo.relay.relayName;
        }
#endif

        params->gi.gameID = util_getCurSeconds(globals->cGlobals.params->util);
        XP_STATUSF( "grabbed gameID: %ld\n", params->gi.gameID );

        game_makeNewGame( MEMPOOL &globals->cGlobals.game, &params->gi,
                          params->util, (DrawCtx*)globals->draw,
                          gameID, &globals->cp, linux_send, globals );

        addr.conType = params->conType;
        if ( 0 ) {
#ifdef XWFEATURE_RELAY
        } else if ( addr.conType == COMMS_CONN_RELAY ) {
            addr.u.ip_relay.ipAddr = 0;
            addr.u.ip_relay.port = params->connInfo.relay.defaultSendPort;
            XP_STRNCPY( addr.u.ip_relay.hostName, params->connInfo.relay.relayName,
                        sizeof(addr.u.ip_relay.hostName) - 1 );
            XP_STRNCPY( addr.u.ip_relay.cookie, params->connInfo.relay.cookie,
                        sizeof(addr.u.ip_relay.cookie) - 1 );
#endif
#ifdef XWFEATURE_BLUETOOTH
        } else if ( addr.conType == COMMS_CONN_BT ) {
            XP_ASSERT( sizeof(addr.u.bt.btAddr) 
                       >= sizeof(params->connInfo.bt.hostAddr));
            XP_MEMCPY( &addr.u.bt.btAddr, &params->connInfo.bt.hostAddr,
                       sizeof(params->connInfo.bt.hostAddr) );
#endif
        }

        /* This may trigger network activity */
        if ( !!globals->cGlobals.game.comms ) {
            comms_setAddr( globals->cGlobals.game.comms, &addr );
        }

        model_setDictionary( globals->cGlobals.game.model, params->dict );

        /*         params->gi.phoniesAction = PHONIES_DISALLOW; */
#ifdef XWFEATURE_SEARCHLIMIT
        params->gi.allowHintRect = XP_TRUE;
#endif

        if ( !isServer ) {
            XWStreamCtxt* stream = 
                mem_stream_make( MEMPOOL params->vtMgr, globals, CHANNEL_NONE,
                                 sendOnClose );
            server_initClientConnection( globals->cGlobals.game.server, 
                                         stream );
        }
    }

    server_do( globals->cGlobals.game.server );

} /* createOrLoadObjects */

/* Create a new backing pixmap of the appropriate size and set up contxt to
 * draw using that size.
 */
static gboolean
configure_event( GtkWidget* widget, GdkEventConfigure* XP_UNUSED(event),
                 GtkAppGlobals* globals )
{
    short width, height, leftMargin, topMargin;
    short timerLeft, timerTop;
    gint hscale, vscale;
    gint trayTop;
    gint boardTop = 0;

    if ( globals->draw == NULL ) {
        createOrLoadObjects( globals );
    }

    width = widget->allocation.width - (RIGHT_MARGIN + BOARD_LEFT_MARGIN);
    if ( globals->cGlobals.params->verticalScore ) {
        width -= VERT_SCORE_WIDTH;
    }
    height = widget->allocation.height - (TOP_MARGIN + BOTTOM_MARGIN)
        - MIN_TRAY_SCALEV - BOTTOM_MARGIN;

    hscale = width / NUM_COLS;
    vscale = (height / (NUM_ROWS + 2)); /* makd tray height 2x cell height */

    leftMargin = (width - (hscale*NUM_COLS)) / 2;
    topMargin = (height - (vscale*(NUM_ROWS*2))) / 2;

    if ( !globals->cGlobals.params->verticalScore ) {
        boardTop += HOR_SCORE_HEIGHT;
    }

    trayTop = boardTop + (vscale * NUM_ROWS);
    /* move tray up if part of board's meant to be hidden */
    trayTop -= vscale * globals->cGlobals.params->nHidden;
    board_setPos( globals->cGlobals.game.board, BOARD_LEFT, boardTop,
                  XP_FALSE );
    board_setScale( globals->cGlobals.game.board, hscale, vscale );
    board_setShowColors( globals->cGlobals.game.board, XP_TRUE );
    globals->gridOn = XP_TRUE;

    timerTop = TIMER_TOP;
    if ( globals->cGlobals.params->verticalScore ) {
        timerLeft = BOARD_LEFT + (hscale*NUM_COLS) + 1;
        board_setScoreboardLoc( globals->cGlobals.game.board, 
                                timerLeft,
                                VERT_SCORE_TOP,
                                VERT_SCORE_WIDTH, 
                                vscale*NUM_COLS,
                                XP_FALSE );

    } else {
        timerLeft = BOARD_LEFT + (hscale*NUM_COLS) - TIMER_WIDTH;
        board_setScoreboardLoc( globals->cGlobals.game.board, 
                                BOARD_LEFT, HOR_SCORE_TOP,
                                timerLeft-BOARD_LEFT,
                                HOR_SCORE_HEIGHT, 
                                XP_TRUE );

    }

    board_setTimerLoc( globals->cGlobals.game.board, timerLeft, timerTop,
                       TIMER_WIDTH, HOR_SCORE_HEIGHT );

    board_setTrayLoc( globals->cGlobals.game.board, TRAY_LEFT, trayTop, 
                      hscale * NUM_COLS, vscale * TRAY_HT_ROWS, 
                      GTK_DIVIDER_WIDTH );

    setCtrlsForTray( globals );
    
    board_invalAll( globals->cGlobals.game.board );

    return TRUE;
} /* configure_event */

/* Redraw the screen from the backing pixmap */
static gint
expose_event( GtkWidget* XP_UNUSED(widget),
              GdkEventExpose* XP_UNUSED(event),
              GtkAppGlobals* globals )
{
    /*
    gdk_draw_rectangle( widget->window,//((GtkDrawCtx*)globals->draw)->pixmap,
			widget->style->white_gc,
			TRUE,
			0, 0,
			widget->allocation.width,
			widget->allocation.height+widget->allocation.y );
    */
    /* I want to inval only the area that's exposed, but the rect is always
       empty, even when clearly shouldn't be.  Need to investigate.  Until
       fixed, use board_invalAll to ensure board is drawn.*/
/*     board_invalRect( globals->cGlobals.game.board, (XP_Rect*)&event->area ); */

    board_invalAll( globals->cGlobals.game.board );
    board_draw( globals->cGlobals.game.board );
    
/*     gdk_draw_pixmap( widget->window, */
/* 		     widget->style->fg_gc[GTK_WIDGET_STATE (widget)], */
/* 		     ((GtkDrawCtx*)globals->draw)->pixmap, */
/* 		     event->area.x, event->area.y, */
/* 		     event->area.x, event->area.y, */
/* 		     event->area.width, event->area.height ); */

    return FALSE;
} /* expose_event */

#if 0
static gint
handle_client_event( GtkWidget *widget, GdkEventClient *event,
		     GtkAppGlobals* globals )
{
    XP_LOGF( "handle_client_event called: event->type = " );
    if ( event->type == GDK_CLIENT_EVENT ) {
	XP_LOGF( "GDK_CLIENT_EVENT" );
	return 1;
    } else {
	XP_LOGF( "%d", event->type );	
	return 0;
    }
} /* handle_client_event */
#endif

static void
writeToFile( XWStreamCtxt* stream, void* closure )
{
    void* buf;
    FILE* file;
    XP_U16 len;
    GtkAppGlobals* globals = (GtkAppGlobals*)closure;

    len = stream_getSize( stream );
    buf = malloc( len );
    stream_getBytes( stream, buf, len );

    file = fopen( globals->cGlobals.params->fileName, "w" );
    fwrite( buf, 1, len, file );
    fclose( file );

    free( buf );
} /* writeToFile */

static void
quit( void* XP_UNUSED(dunno), GtkAppGlobals* globals )
{
    if ( !!globals->cGlobals.params->fileName ) {
        XWStreamCtxt* outStream;

        outStream = mem_stream_make( MEMPOOL globals->cGlobals.params->vtMgr, 
                                     globals, 0, writeToFile );
        stream_open( outStream );

        game_saveToStream( &globals->cGlobals.game, 
                           &globals->cGlobals.params->gi, 
                           outStream );

        stream_destroy( outStream );
    }

    game_dispose( &globals->cGlobals.game ); /* takes care of the dict */
    gi_disposePlayerInfo( MEMPOOL &globals->cGlobals.params->gi );

#ifdef XWFEATURE_BLUETOOTH
    linux_bt_close( &globals->cGlobals );
#endif
    vtmgr_destroy( MEMPOOL globals->cGlobals.params->vtMgr );
    
    mpool_destroy( globals->cGlobals.params->util->mpool );

    gtk_exit( 0 );
} /* quit */

GtkWidget*
makeAddSubmenu( GtkWidget* menubar, gchar* label )
{
    GtkWidget* submenu;
    GtkWidget* item;

    item = gtk_menu_item_new_with_label( label );
    gtk_menu_bar_append( GTK_MENU_BAR(menubar), item );
    
    submenu = gtk_menu_new();
    gtk_menu_item_set_submenu( GTK_MENU_ITEM(item), submenu );

    gtk_widget_show(item);

    return submenu;
} /* makeAddSubmenu */

static void
tile_values( GtkWidget* XP_UNUSED(widget), GtkAppGlobals* globals )
{
    if ( !!globals->cGlobals.game.server ) {
        XWStreamCtxt* stream = 
            mem_stream_make( MEMPOOL 
                             globals->cGlobals.params->vtMgr,
                             globals, 
                             CHANNEL_NONE, 
                             catOnClose );
        server_formatDictCounts( globals->cGlobals.game.server, stream, 5 );
        stream_putU8( stream, '\n' );
        stream_destroy( stream );
    }
    
} /* tile_values */

static void
game_history( GtkWidget* XP_UNUSED(widget), GtkAppGlobals* globals )
{
    catGameHistory( &globals->cGlobals );
} /* game_history */

static void
final_scores( GtkWidget* XP_UNUSED(widget), GtkAppGlobals* globals )
{
    XP_Bool gameOver = server_getGameIsOver( globals->cGlobals.game.server );

    if ( gameOver ) {
        printFinalScores( globals );
    } else {
        XP_Bool confirmed;
        confirmed = 
            gtkask( "Are you sure everybody wants to end the game now?",
                    2, "Yes", "No" ) == 0;

        if ( confirmed ) {
            server_endGame( globals->cGlobals.game.server );
            gameOver = TRUE;
        }
    }

    /* the end game listener will take care of printing the final scores */
} /* final_scores */

static void
new_game( GtkWidget* XP_UNUSED(widget), GtkAppGlobals* globals )
{
    gboolean confirmed;

    confirmed = newGameDialog( globals, XP_TRUE );
    if ( confirmed ) {
        CurGameInfo* gi = &globals->cGlobals.params->gi;
        XP_Bool isClient = gi->serverRole == SERVER_ISCLIENT;
        XP_U32 gameID = util_getCurSeconds( globals->cGlobals.params->util );

        XP_STATUSF( "grabbed gameID: %ld\n", gameID );
        game_reset( MEMPOOL &globals->cGlobals.game, gi,
                    globals->cGlobals.params->util,
                    gameID, &globals->cp, linux_send, globals );

        if ( isClient ) {
            XWStreamCtxt* stream =
                mem_stream_make( MEMPOOL 
                                 globals->cGlobals.params->vtMgr,
                                 globals, 
                                 CHANNEL_NONE, 
                                 sendOnClose );
            server_initClientConnection( globals->cGlobals.game.server, 
                                         stream );
        }

        (void)server_do( globals->cGlobals.game.server ); /* assign tiles, etc. */
        board_invalAll( globals->cGlobals.game.board );
        board_draw( globals->cGlobals.game.board );
    }

} /* new_game */

static void
game_info( GtkWidget* XP_UNUSED(widget), GtkAppGlobals* globals )
{
    /* Anything to do if OK is clicked?  Changed names etc. already saved.  Try
       server_do in case one's become a robot. */
    if ( newGameDialog( globals, XP_FALSE ) ) {
        if ( server_do( globals->cGlobals.game.server ) ) {
            board_draw( globals->cGlobals.game.board );
        }
    }
}

static void
load_game( GtkWidget* XP_UNUSED(widget), GtkAppGlobals* XP_UNUSED(globals) )
{
} /* load_game */

static void
save_game( GtkWidget* XP_UNUSED(widget), GtkAppGlobals* XP_UNUSED(globals) )
{
} /* save_game */

static void
load_dictionary( GtkWidget* XP_UNUSED(widget), 
                 GtkAppGlobals* XP_UNUSED(globals) )
{
} /* load_dictionary */

static void
handle_undo( GtkWidget* XP_UNUSED(widget), GtkAppGlobals* XP_UNUSED(globals) )
{
} /* handle_undo */

static void
handle_redo( GtkWidget* XP_UNUSED(widget), GtkAppGlobals* XP_UNUSED(globals) )
{
} /* handle_redo */

#ifdef FEATURE_TRAY_EDIT
static void
handle_trayEditToggle( GtkWidget* XP_UNUSED(widget), 
                       GtkAppGlobals* XP_UNUSED(globals), 
                       XP_Bool XP_UNUSED(on) )
{
} /* handle_trayEditToggle */

static void
handle_trayEditToggle_on( GtkWidget* widget, GtkAppGlobals* globals )
{
    handle_trayEditToggle( widget, globals, XP_TRUE );
}

static void
handle_trayEditToggle_off( GtkWidget* widget, GtkAppGlobals* globals )
{
    handle_trayEditToggle( widget, globals, XP_FALSE );
}
#endif

static void
handle_resend( GtkWidget* XP_UNUSED(widget), GtkAppGlobals* globals )
{
    CommsCtxt* comms = globals->cGlobals.game.comms;
    if ( comms != NULL ) {
        comms_resendAll( comms );
    }
} /* handle_resend */

#ifdef DEBUG
static void
handle_commstats( GtkWidget* XP_UNUSED(widget), GtkAppGlobals* globals )
{
    CommsCtxt* comms = globals->cGlobals.game.comms;

    if ( !!comms ) {
        XWStreamCtxt* stream = 
            mem_stream_make( MEMPOOL 
                             globals->cGlobals.params->vtMgr,
                             globals, 
                             CHANNEL_NONE, catOnClose );
        comms_getStats( comms, stream );
        stream_destroy( stream );
    }
} /* handle_commstats */
#endif

#ifdef MEM_DEBUG
static void
handle_memstats( GtkWidget* XP_UNUSED(widget), GtkAppGlobals* globals )
{
    XWStreamCtxt* stream = mem_stream_make( MEMPOOL 
					    globals->cGlobals.params->vtMgr,
					    globals, 
					    CHANNEL_NONE, catOnClose );
    mpool_stats( MEMPOOL stream );
    stream_destroy( stream );
    
} /* handle_memstats */
#endif

static GtkWidget*
createAddItem( GtkWidget* parent, gchar* label, 
               GtkSignalFunc handlerFunc, GtkAppGlobals* globals ) 
{
    GtkWidget* item = gtk_menu_item_new_with_label( label );

/*      g_print( "createAddItem called with label %s\n", label ); */

    if ( handlerFunc != NULL ) {
        g_signal_connect( GTK_OBJECT(item), "activate",
                          G_CALLBACK(handlerFunc), globals );
    }
    
    gtk_menu_append( GTK_MENU(parent), item );
    gtk_widget_show( item );

    return item;
} /* createAddItem */

static GtkWidget* 
makeMenus( GtkAppGlobals* globals, int XP_UNUSED(argc), 
           char** XP_UNUSED(argv) )
{
    GtkWidget* menubar = gtk_menu_bar_new();
    GtkWidget* fileMenu;

    fileMenu = makeAddSubmenu( menubar, "File" );
    (void)createAddItem( fileMenu, "Tile values", 
                         GTK_SIGNAL_FUNC(tile_values), globals );
    (void)createAddItem( fileMenu, "Game history", 
                         GTK_SIGNAL_FUNC(game_history), globals );

    (void)createAddItem( fileMenu, "Final scores", 
                         GTK_SIGNAL_FUNC(final_scores), globals );

    (void)createAddItem( fileMenu, "New game", 
                         GTK_SIGNAL_FUNC(new_game), globals );
    (void)createAddItem( fileMenu, "Game info", 
                         GTK_SIGNAL_FUNC(game_info), globals );

    (void)createAddItem( fileMenu, "Load game", 
                         GTK_SIGNAL_FUNC(load_game), globals );
    (void)createAddItem( fileMenu, "Save game", 
                         GTK_SIGNAL_FUNC(save_game), globals );

    (void)createAddItem( fileMenu, "Load dictionary", 
                         GTK_SIGNAL_FUNC(load_dictionary), globals );

    fileMenu = makeAddSubmenu( menubar, "Edit" );

    (void)createAddItem( fileMenu, "Undo", 
                         GTK_SIGNAL_FUNC(handle_undo), globals );
    (void)createAddItem( fileMenu, "Redo", 
                         GTK_SIGNAL_FUNC(handle_redo), globals );

#ifdef FEATURE_TRAY_EDIT
    (void)createAddItem( fileMenu, "Allow tray edit", 
                         GTK_SIGNAL_FUNC(handle_trayEditToggle_on), globals );
    (void)createAddItem( fileMenu, "Dis-allow tray edit", 
                         GTK_SIGNAL_FUNC(handle_trayEditToggle_off), globals );
#endif

    fileMenu = makeAddSubmenu( menubar, "Network" );

    (void)createAddItem( fileMenu, "Resend", 
                         GTK_SIGNAL_FUNC(handle_resend), globals );
#ifdef DEBUG
    (void)createAddItem( fileMenu, "Stats", 
                         GTK_SIGNAL_FUNC(handle_commstats), globals );
#endif
#ifdef MEM_DEBUG
    (void)createAddItem( fileMenu, "Mem stats", 
                         GTK_SIGNAL_FUNC(handle_memstats), globals );
#endif

    /*     (void)createAddItem( fileMenu, "Print board",  */
    /* 			 GTK_SIGNAL_FUNC(handle_print_board), globals ); */

    /*     listAllGames( menubar, argc, argv, globals ); */

    gtk_widget_show( menubar );

    return menubar;
} /* makeMenus */

static gboolean
handle_flip_button( GtkWidget* XP_UNUSED(widget), gpointer _globals )
{
    GtkAppGlobals* globals = (GtkAppGlobals*)_globals;
    if ( board_flip( globals->cGlobals.game.board ) ) {
        board_draw( globals->cGlobals.game.board );
    }
    return TRUE;
} /* handle_flip_button */

static gboolean
handle_value_button( GtkWidget* XP_UNUSED(widget), gpointer closure )
{
    GtkAppGlobals* globals = (GtkAppGlobals*)closure;
    if ( board_toggle_showValues( globals->cGlobals.game.board ) ) {
        board_draw( globals->cGlobals.game.board );
    }
    return TRUE;
} /* handle_value_button */

static void
handle_hint_button( GtkWidget* XP_UNUSED(widget), GtkAppGlobals* globals )
{
    XP_Bool redo;
    if ( board_requestHint( globals->cGlobals.game.board, 
#ifdef XWFEATURE_SEARCHLIMIT
                            XP_FALSE,
#endif
                            &redo ) ) {
        board_draw( globals->cGlobals.game.board );
    }
} /* handle_hint_button */

static void
handle_nhint_button( GtkWidget* XP_UNUSED(widget), GtkAppGlobals* globals )
{
    XP_Bool redo;

    board_resetEngine( globals->cGlobals.game.board );
    if ( board_requestHint( globals->cGlobals.game.board, 
#ifdef XWFEATURE_SEARCHLIMIT
                            XP_TRUE, 
#endif
                            &redo ) ) {
        board_draw( globals->cGlobals.game.board );
    }
} /* handle_hint_button */

static void
handle_colors_button( GtkWidget* XP_UNUSED(widget), 
                      GtkAppGlobals* XP_UNUSED(globals) )
{
/*     XP_Bool oldVal = board_getShowColors( globals->cGlobals.game.board ); */
/*     if ( board_setShowColors( globals->cGlobals.game.board, !oldVal ) ) { */
/* 	board_draw( globals->cGlobals.game.board );	 */
/*     } */
} /* handle_colors_button */

static void
handle_juggle_button( GtkWidget* XP_UNUSED(widget), GtkAppGlobals* globals )
{
    if ( board_juggleTray( globals->cGlobals.game.board ) ) {
        board_draw( globals->cGlobals.game.board );
    }
} /* handle_juggle_button */

static void
handle_undo_button( GtkWidget* XP_UNUSED(widget), GtkAppGlobals* globals )
{
    if ( server_handleUndo( globals->cGlobals.game.server ) ) {
        board_draw( globals->cGlobals.game.board );
    }
} /* handle_undo_button */

static void
handle_redo_button( GtkWidget* XP_UNUSED(widget), 
                    GtkAppGlobals* XP_UNUSED(globals) )
{
} /* handle_redo_button */

static void
handle_trade_button( GtkWidget* XP_UNUSED(widget), GtkAppGlobals* globals )
{
    if ( board_beginTrade( globals->cGlobals.game.board ) ) {
        board_draw( globals->cGlobals.game.board );
    }
} /* handle_juggle_button */

static void
handle_done_button( GtkWidget* XP_UNUSED(widget), GtkAppGlobals* globals )
{
    if ( board_commitTurn( globals->cGlobals.game.board ) ) {
        board_draw( globals->cGlobals.game.board );
    }
} /* handle_done_button */

static void
scroll_value_changed( GtkAdjustment *adj, GtkAppGlobals* globals )
{
    XP_U16 curYOffset, newValue;
    gfloat newValueF = adj->value;

    XP_ASSERT( newValueF >= 0.0
               && newValueF <= globals->cGlobals.params->nHidden );
    curYOffset = board_getYOffset( globals->cGlobals.game.board );
    newValue = (XP_U16)newValueF;

    if ( newValue != curYOffset ) {
        board_setYOffset( globals->cGlobals.game.board, newValue );
        board_draw( globals->cGlobals.game.board );
    }
} /* scroll_value_changed */

static void
handle_grid_button( GtkWidget* XP_UNUSED(widget), GtkAppGlobals* globals )
{
    XP_U16 scaleH, scaleV;
    XP_Bool gridOn = globals->gridOn;

    board_getScale( globals->cGlobals.game.board, &scaleH, &scaleV );

    if ( gridOn ) {
        --scaleH;
        --scaleV;
    } else {
        ++scaleH;
        ++scaleV;
    }

    board_setScale( globals->cGlobals.game.board, scaleH, scaleV );
    globals->gridOn = !gridOn;

    board_draw( globals->cGlobals.game.board );
} /* handle_grid_button */

static void
handle_hide_button( GtkWidget* XP_UNUSED(widget), GtkAppGlobals* globals )
{
    BoardCtxt* board;
    XP_Bool draw = XP_FALSE;

    if ( globals->cGlobals.params->nHidden > 0 ) {
        globals->adjustment->page_size = NUM_ROWS;
        globals->adjustment->value = 0.0;

        gtk_signal_emit_by_name( GTK_OBJECT(globals->adjustment), "changed" );
        gtk_adjustment_value_changed( GTK_ADJUSTMENT(globals->adjustment) );
    }

    board = globals->cGlobals.game.board;
    if ( TRAY_REVEALED == board_getTrayVisState( board ) ) {
        draw = board_hideTray( board );
    } else {
        draw = board_showTray( board );
    }
    if ( draw ) {
        board_draw( board );
    }
} /* handle_hide_button */

static void
handle_commit_button( GtkWidget* XP_UNUSED(widget), GtkAppGlobals* globals )
{
    if ( board_commitTurn( globals->cGlobals.game.board ) ) {
        board_draw( globals->cGlobals.game.board );
    }
} /* handle_commit_button */

static void
gtkUserError( GtkAppGlobals* XP_UNUSED(globals), char* format, ... )
{
    char buf[512];
    va_list ap;

    va_start( ap, format );

    vsprintf( buf, format, ap );

    (void)gtkask( buf, 1, "OK" );

    va_end(ap);
} /* gtkUserError */

static VTableMgr*
gtk_util_getVTManager(XW_UtilCtxt* uc)
{
    GtkAppGlobals* globals = (GtkAppGlobals*)uc->closure;
    return globals->cGlobals.params->vtMgr;
} /* linux_util_getVTManager */

static XP_S16
gtk_util_userPickTile( XW_UtilCtxt* uc, const PickInfo* pi,
                       XP_U16 playerNum,
                       const XP_UCHAR4* texts, XP_U16 nTiles )
{
    XP_S16 chosen;
    GtkAppGlobals* globals = (GtkAppGlobals*)uc->closure;
	XP_UCHAR* name = globals->cGlobals.params->gi.players[playerNum].name;

    chosen = gtkletterask( pi->why == PICK_FOR_BLANK, name, nTiles, texts );
    return chosen;
} /* gtk_util_userPickTile */

static XP_Bool
gtk_util_askPassword( XW_UtilCtxt* XP_UNUSED(uc), const XP_UCHAR* name, 
                      XP_UCHAR* buf, XP_U16* len )
{
    XP_Bool ok = gtkpasswdask( name, buf, len );
    return ok;
} /* gtk_util_askPassword */

static void
setCtrlsForTray( GtkAppGlobals* globals )
{
    XW_TrayVisState state = 
        board_getTrayVisState( globals->cGlobals.game.board );
    XP_S16 nHidden = globals->cGlobals.params->nHidden;

    if ( nHidden != 0 ) {
        XP_U16 pageSize = NUM_ROWS;

        if ( state == TRAY_HIDDEN ) { /* we recover what tray covers */
            nHidden -= TRAY_HT_ROWS;
        }
        if ( nHidden > 0 ) {
            pageSize -= nHidden;
        }
        globals->adjustment->page_size = pageSize;

        XP_LOGF( "%s: set pageSize = %d", __FUNCTION__,
                 pageSize );

        globals->adjustment->value = 
            board_getYOffset( globals->cGlobals.game.board );
        gtk_signal_emit_by_name( GTK_OBJECT(globals->adjustment), "changed" );
    }
} /* setCtrlsForTray */

static void
gtk_util_trayHiddenChange( XW_UtilCtxt* uc, XW_TrayVisState XP_UNUSED(state),
                           XP_U16 XP_UNUSED(nVisibleRows) )
{
    GtkAppGlobals* globals = (GtkAppGlobals*)uc->closure;
    setCtrlsForTray( globals );
} /* gtk_util_trayHiddenChange */

static void
gtk_util_yOffsetChange( XW_UtilCtxt* XP_UNUSED(uc), 
                        XP_U16 XP_UNUSED(oldOffset), 
                        XP_U16 XP_UNUSED(newOffset) )
{
} /* gtk_util_yOffsetChange */

static void
printFinalScores( GtkAppGlobals* globals )
{
    XWStreamCtxt* stream;

    stream = mem_stream_make( MEMPOOL 
                              globals->cGlobals.params->vtMgr,
                              globals, CHANNEL_NONE, catOnClose );
    server_writeFinalScores( globals->cGlobals.game.server, stream );
    stream_putU8( stream, '\n' );
    stream_destroy( stream );
} /* printFinalScores */

static void
gtk_util_notifyGameOver( XW_UtilCtxt* uc )
{
    GtkAppGlobals* globals = (GtkAppGlobals*)uc->closure;

    if ( globals->cGlobals.params->printHistory ) {
        catGameHistory( &globals->cGlobals );
    }

    printFinalScores( globals );

    if ( globals->cGlobals.params->quitAfter ) {
        quit( NULL, globals );
    } else if ( globals->cGlobals.params->undoWhenDone ) {
        server_handleUndo( globals->cGlobals.game.server );
    }

    board_draw( globals->cGlobals.game.board );
} /* gtk_util_notifyGameOver */

/* define this to prevent user events during debugging from stopping the engine */
/* #define DONT_ABORT_ENGINE */

static XP_Bool
gtk_util_hiliteCell( XW_UtilCtxt* uc, XP_U16 col, XP_U16 row )
{
    GtkAppGlobals* globals = (GtkAppGlobals*)uc->closure;
#ifndef DONT_ABORT_ENGINE
    gboolean pending;
#endif

    board_hiliteCellAt( globals->cGlobals.game.board, col, row );
    if ( globals->cGlobals.params->sleepOnAnchor ) {
    }

#ifdef DONT_ABORT_ENGINE
    return XP_TRUE;		/* keep going */
#else
    pending = gdk_events_pending();
    if ( pending ) {
        XP_DEBUGF( "gtk_util_hiliteCell=>%d", pending );
    }
    return !pending;
#endif
} /* gtk_util_hiliteCell */

static XP_Bool
gtk_util_engineProgressCallback( XW_UtilCtxt* XP_UNUSED(uc) )
{
#ifdef DONT_ABORT_ENGINE
    return XP_TRUE;		/* keep going */
#else
    gboolean pending = gdk_events_pending();

/*     XP_DEBUGF( "gdk_events_pending returned %d\n", pending ); */

    return !pending;
#endif
} /* gtk_util_engineProgressCallback */

static void
cancelTimer( GtkAppGlobals* globals, XWTimerReason why )
{
    guint src = globals->timerSources[why-1];
    if ( src != 0 ) {
        g_source_remove( src );
        globals->timerSources[why-1] = 0;
    }
} /* cancelTimer */

static gint
pentimer_idle_func( gpointer data )
{
    GtkAppGlobals* globals = (GtkAppGlobals*)data;
    struct timeval tv;
    XP_Bool callAgain = XP_TRUE;

    gettimeofday( &tv, NULL );

    if ( (tv.tv_usec - globals->penTv.tv_usec) >= globals->penTimerInterval) {
        linuxFireTimer( &globals->cGlobals, TIMER_PENDOWN );
        callAgain = XP_FALSE;
    } 

    return callAgain;
} /* timer_idle_func */

static gint
score_timer_func( gpointer data )
{
    GtkAppGlobals* globals = (GtkAppGlobals*)data;

    linuxFireTimer( &globals->cGlobals, TIMER_TIMERTICK );

    return XP_FALSE;
} /* score_timer_func */

#ifdef XWFEATURE_RELAY
static gint
heartbeat_timer_func( gpointer data )
{
    GtkAppGlobals* globals = (GtkAppGlobals*)data;

    if ( !globals->cGlobals.params->noHeartbeat ) {
        linuxFireTimer( &globals->cGlobals, TIMER_HEARTBEAT );
    }

    return (gint)0;
}
#endif

static void
gtk_util_setTimer( XW_UtilCtxt* uc, XWTimerReason why, 
                   XP_U16 XP_UNUSED_RELAY(when),
                   TimerProc proc, void* closure )
{
    GtkAppGlobals* globals = (GtkAppGlobals*)uc->closure;
    guint newSrc;

    cancelTimer( globals, why );

    if ( why == TIMER_PENDOWN ) {
        globals->penTimerInterval = 35 * 10000;

        (void)gettimeofday( &globals->penTv, NULL );
        newSrc = g_idle_add( pentimer_idle_func, globals );
    } else if ( why == TIMER_TIMERTICK ) {
        globals->scoreTimerInterval = 100 * 10000;

        (void)gettimeofday( &globals->scoreTv, NULL );

        newSrc = g_timeout_add( 1000, score_timer_func, globals );
#ifdef XWFEATURE_RELAY
    } else if ( why == TIMER_HEARTBEAT ) {
        newSrc = g_timeout_add( 1000 * when, heartbeat_timer_func, globals );
#endif
    } else {
        XP_ASSERT( 0 );
    }

    globals->cGlobals.timerProcs[why] = proc;
    globals->cGlobals.timerClosures[why] = closure;
    XP_ASSERT( newSrc != 0 );
    globals->timerSources[why-1] = newSrc;
} /* gtk_util_setTimer */

static gint
idle_func( gpointer data )
{
    GtkAppGlobals* globals = (GtkAppGlobals*)data;
/*     XP_DEBUGF( "idle_func called\n" ); */

    /* remove before calling server_do.  If server_do puts up a dialog that
       calls gtk_main, then this idle proc will also apply to that event loop
       and bad things can happen.  So kill the idle proc asap. */
    gtk_idle_remove( globals->idleID );

    if ( server_do( globals->cGlobals.game.server ) ) {
        XP_ASSERT( globals->cGlobals.game.board != NULL );
        board_draw( globals->cGlobals.game.board );
    }
    return 0; /* 0 will stop it from being called again */
} /* idle_func */

static void
gtk_util_requestTime( XW_UtilCtxt* uc ) 
{
    GtkAppGlobals* globals = (GtkAppGlobals*)uc->closure;
    globals->idleID = gtk_idle_add( idle_func, globals );
} /* gtk_util_requestTime */

static XP_Bool
gtk_util_warnIllegalWord( XW_UtilCtxt* uc, BadWordInfo* bwi, XP_U16 player,
			  XP_Bool turnLost )
{
    XP_Bool result;
    GtkAppGlobals* globals = (GtkAppGlobals*)uc->closure;
    char buf[300];

    if ( turnLost ) {
        char wordsBuf[256];
        XP_U16 i;
        XP_UCHAR* name = globals->cGlobals.params->gi.players[player].name;
        XP_ASSERT( !!name );

        for ( i = 0, wordsBuf[0] = '\0'; ; ) {
            char wordBuf[18];
            sprintf( wordBuf, "\"%s\"", bwi->words[i] );
            strcat( wordsBuf, wordBuf );
            if ( ++i == bwi->nWords ) {
                break;
            }
            strcat( wordsBuf, ", " );
        }

        sprintf( buf, "Player %d (%s) played illegal word[s] %s; loses turn.",
                 player+1, name, wordsBuf );

        if ( globals->cGlobals.params->skipWarnings ) {
            XP_LOGF( "%s", buf );
        }  else {
            gtkUserError( globals, buf );
        }
        result = XP_TRUE;
    } else {
        XP_ASSERT( bwi->nWords == 1 );
        sprintf( buf, "Word \"%s\" not in the current dictionary. "
                 "Use it anyway?", bwi->words[0] );
        result = 0 == gtkask( buf, 2, "Ok", "Cancel" );
    }

    return result;
} /* gtk_util_warnIllegalWord */

static XWStreamCtxt* 
gtk_util_makeStreamFromAddr(XW_UtilCtxt* uc, XP_PlayerAddr channelNo )
{
    GtkAppGlobals* globals = (GtkAppGlobals*)uc->closure;

    XWStreamCtxt* stream = mem_stream_make( MEMPOOL 
                                            globals->cGlobals.params->vtMgr,
                                            uc->closure, channelNo, 
                                            sendOnClose );
    return stream;
} /* gtk_util_makeStreamFromAddr */

#ifdef XWFEATURE_SEARCHLIMIT
static XP_Bool 
gtk_util_getTraySearchLimits( XW_UtilCtxt* XP_UNUSED(uc), 
                              XP_U16* XP_UNUSED(min), XP_U16* max )
{
    *max = askNTiles( MAX_TRAY_TILES, *max );
    return XP_TRUE;
}
#endif


static void
gtk_util_userError( XW_UtilCtxt* uc, UtilErrID id )
{
    GtkAppGlobals* globals = (GtkAppGlobals*)uc->closure;
    XP_Bool silent;
    XP_UCHAR* message = linux_getErrString( id, &silent );

    XP_LOGF( "%s(%d)", __FUNCTION__, id );

    if ( silent ) {
        XP_LOGF( message );
    } else {
        gtkUserError( globals, message );
    }
} /* gtk_util_userError */

static XP_Bool
gtk_util_userQuery( XW_UtilCtxt* XP_UNUSED(uc), UtilQueryID id, 
                    XWStreamCtxt* stream )
{
    XP_Bool result;
    char* question;
    char* answers[3];
    gint numAnswers = 0;
    XP_U16 okIndex = 1;
    XP_Bool freeMe = XP_FALSE;

    switch( id ) {
	
    case QUERY_COMMIT_TURN:
        question = strFromStream( stream );
        freeMe = XP_TRUE;
/*         len = stream_getSize( stream ); */
/*         question = malloc( len + 1 ); */
/*         stream_getBytes( stream, question, len ); */
/*         question[len] = '\0'; */
        answers[numAnswers++] = "Cancel";
        answers[numAnswers++] = "Ok";
        break;
    case QUERY_COMMIT_TRADE:
        question = "Are you sure you want to trade the selected tiles?";
        answers[numAnswers++] = "Cancel";
        answers[numAnswers++] = "Ok";
        break;
    case QUERY_ROBOT_MOVE:
    case QUERY_ROBOT_TRADE:
        question = strFromStream( stream );
        freeMe = XP_TRUE;
        answers[numAnswers++] = "Ok";
        okIndex = 0;
        break;

    default:
        XP_ASSERT( 0 );
        return XP_FALSE;
    }

    result = gtkask( question, numAnswers, 
                     answers[0], answers[1], answers[2] ) == okIndex;

    if ( freeMe > 0 ) {
        free( question );
    }

    return result;
} /* gtk_util_userQuery */

static XP_Bool
file_exists( const char* fileName ) 
{
    struct stat statBuf;

    int statResult = stat( fileName, &statBuf );
    return statResult == 0;
} /* file_exists */

static GtkWidget*
makeShowButtonFromBitmap( void* closure, const gchar* filename, 
                          const gchar* alt, GCallback func )
{
    GtkWidget* widget;
    GtkWidget* button;

    if ( file_exists( filename ) ) {
        widget = gtk_image_new_from_file (filename);
    } else {
       widget = gtk_label_new( alt );
    }
    gtk_widget_show( widget );

    button = gtk_button_new();
    gtk_container_add (GTK_CONTAINER (button), widget );
    gtk_widget_show (button);

    if ( func != NULL ) {
        g_signal_connect( GTK_OBJECT(button), "clicked", func, closure );
    }

    return button;
} /* makeShowButtonFromBitmap */

static GtkWidget* 
makeVerticalBar( GtkAppGlobals* globals, GtkWidget* XP_UNUSED(window) )
{
    GtkWidget* vbox;
    GtkWidget* button;

    vbox = gtk_vbutton_box_new();

    button = makeShowButtonFromBitmap( globals, "../flip.xpm", "f", 
                                       G_CALLBACK(handle_flip_button) );
    gtk_box_pack_start( GTK_BOX(vbox), button, FALSE, TRUE, 0 );

    button = makeShowButtonFromBitmap( globals, "../value.xpm", "v",
                                       G_CALLBACK(handle_value_button) );
    gtk_box_pack_start( GTK_BOX(vbox), button, FALSE, TRUE, 0 );

    button = makeShowButtonFromBitmap( globals, "../hint.xpm", "?",
                                       G_CALLBACK(handle_hint_button) );
    gtk_box_pack_start( GTK_BOX(vbox), button, FALSE, TRUE, 0 );

    button = makeShowButtonFromBitmap( globals, "../hintNum.xpm", "n",
                                       G_CALLBACK(handle_nhint_button) );
    gtk_box_pack_start( GTK_BOX(vbox), button, FALSE, TRUE, 0 );

    button = makeShowButtonFromBitmap( globals, "../colors.xpm", "c",
                                       G_CALLBACK(handle_colors_button) );
    gtk_box_pack_start( GTK_BOX(vbox), button, FALSE, TRUE, 0 );

    /* undo and redo buttons */
    button = makeShowButtonFromBitmap( globals, "../undo.xpm", "u",
                                       G_CALLBACK(handle_undo_button) );
    gtk_box_pack_start( GTK_BOX(vbox), button, FALSE, TRUE, 0 );
    button = makeShowButtonFromBitmap( globals, "../redo.xpm", "r",
                                       G_CALLBACK(handle_redo_button) );
    gtk_box_pack_start( GTK_BOX(vbox), button, FALSE, TRUE, 0 );

    /* the four buttons that on palm are beside the tray */
    button = makeShowButtonFromBitmap( globals, "../juggle.xpm", "j",
                                       G_CALLBACK(handle_juggle_button) );
    gtk_box_pack_start( GTK_BOX(vbox), button, FALSE, TRUE, 0 );

    button = makeShowButtonFromBitmap( globals, "../trade.xpm", "t",
                                       G_CALLBACK(handle_trade_button) );
    gtk_box_pack_start( GTK_BOX(vbox), button, FALSE, TRUE, 0 );
    button = makeShowButtonFromBitmap( globals, "../done.xpm", "d",
                                       G_CALLBACK(handle_done_button) );
    gtk_box_pack_start( GTK_BOX(vbox), button, FALSE, TRUE, 0 );

    gtk_widget_show( vbox );
    return vbox;
} /* makeVerticalBar */

static GtkWidget* 
makeButtons( GtkAppGlobals* globals, int XP_UNUSED(argc), 
             char** XP_UNUSED(argv) )
{
    short i;
    GtkWidget* hbox;
    GtkWidget* button;

    struct {
        char* name;
        GCallback func;
    } buttons[] = {
        /* 	{ "Flip", handle_flip_button }, */
        { "Grid", G_CALLBACK(handle_grid_button) },
        { "Hide", G_CALLBACK(handle_hide_button) },
        { "Commit", G_CALLBACK(handle_commit_button) },
    };
    
    hbox = gtk_hbox_new( FALSE, 0 );

    for ( i = 0; i < sizeof(buttons)/sizeof(*buttons); ++i ) {
        button = gtk_button_new_with_label( buttons[i].name );
        gtk_widget_show( button );
        g_signal_connect( GTK_OBJECT(button), "clicked",
                          G_CALLBACK(buttons[i].func), globals );

        gtk_box_pack_start( GTK_BOX(hbox), button, FALSE, TRUE, 0);    
    }

    gtk_widget_show( hbox );
    return hbox;
} /* makeButtons */

static void
setupGtkUtilCallbacks( GtkAppGlobals* globals, XW_UtilCtxt* util )
{
    util->vtable->m_util_userError = gtk_util_userError;
    util->vtable->m_util_userQuery = gtk_util_userQuery;
    util->vtable->m_util_getVTManager = gtk_util_getVTManager;
    util->vtable->m_util_userPickTile = gtk_util_userPickTile;
    util->vtable->m_util_askPassword = gtk_util_askPassword;
    util->vtable->m_util_trayHiddenChange = gtk_util_trayHiddenChange;
    util->vtable->m_util_yOffsetChange = gtk_util_yOffsetChange;
    util->vtable->m_util_notifyGameOver = gtk_util_notifyGameOver;
    util->vtable->m_util_hiliteCell = gtk_util_hiliteCell;
    util->vtable->m_util_engineProgressCallback = 
        gtk_util_engineProgressCallback;
    util->vtable->m_util_setTimer = gtk_util_setTimer;
    util->vtable->m_util_requestTime = gtk_util_requestTime;
    util->vtable->m_util_warnIllegalWord = gtk_util_warnIllegalWord;

    util->vtable->m_util_makeStreamFromAddr = gtk_util_makeStreamFromAddr;

#ifdef XWFEATURE_SEARCHLIMIT
    util->vtable->m_util_getTraySearchLimits = gtk_util_getTraySearchLimits;
#endif

    util->closure = globals;
} /* setupGtkUtilCallbacks */

static gboolean
newConnectionInput( GIOChannel *source,
                    GIOCondition condition,
                    gpointer data )
{
    gboolean keepSource;
    int sock = g_io_channel_unix_get_fd( source );
    GtkAppGlobals* globals = (GtkAppGlobals*)data;

    XP_LOGF( "%s:condition = 0x%x", __FUNCTION__, (int)condition );

/*     XP_ASSERT( sock == globals->cGlobals.socket ); */

    if ( (condition & (G_IO_HUP | G_IO_ERR)) != 0 ) {
        XP_LOGF( "dropping socket %d", sock );
        close( sock );
        globals->cGlobals.socket = -1;
#ifdef XWFEATURE_BLUETOOTH
        if ( COMMS_CONN_BT == globals->cGlobals.params->conType ) {
            linux_bt_socketclosed( &globals->cGlobals, sock );
        }
#endif
        keepSource = FALSE;           /* remove the event source */

    } else if ( (condition & G_IO_IN) != 0 ) {
        ssize_t nRead;
        unsigned char buf[512];

        if ( 0 ) {
#ifdef XWFEATURE_RELAY
        } else if ( globals->cGlobals.params->conType == COMMS_CONN_RELAY ) {
            nRead = linux_relay_receive( &globals->cGlobals, 
                                         buf, sizeof(buf) );
#endif
#ifdef XWFEATURE_BLUETOOTH
        } else if ( globals->cGlobals.params->conType == COMMS_CONN_BT ) {
            nRead = linux_bt_receive( sock, buf, sizeof(buf) );
#endif
        } else {
            XP_ASSERT( 0 );
        }

        if ( !globals->dropIncommingMsgs && nRead > 0 ) {
            XWStreamCtxt* inboundS;
            XP_Bool redraw = XP_FALSE;

            inboundS = stream_from_msgbuf( &globals->cGlobals, buf, nRead );
            if ( !!inboundS ) {
                if ( comms_checkIncomingStream( globals->cGlobals.game.comms, 
                                                inboundS, NULL ) ) {
                    redraw =
                        server_receiveMessage(globals->cGlobals.game.server,
                                              inboundS );
                }
                stream_destroy( inboundS );
            }

            /* if there's something to draw resulting from the message, we
               need to give the main loop time to reflect that on the screen
               before giving the server another shot.  So just call the idle
               proc. */
            if ( redraw ) {
                gtk_util_requestTime( globals->cGlobals.params->util );
            } else {
                redraw = server_do( globals->cGlobals.game.server );	    
            }
            if ( redraw ) {
                board_draw( globals->cGlobals.game.board );
            }
        } else {
            XP_LOGF( "errno from read: %d", errno );
        }
        keepSource = TRUE;
    }
    return keepSource;                /* FALSE means to remove event source */
} /* newConnectionInput */

/* Make gtk listen for events on the socket that clients will use to
 * connect to us.
 */
static void
gtkListenOnSocket( GtkAppGlobals* globals, int newSock )
{

    GIOChannel* channel = g_io_channel_unix_new( newSock );
    guint result = g_io_add_watch( channel,
                                   G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_PRI,
                                   newConnectionInput,
                                   globals );
    XP_LOGF( "g_io_add_watch(%d) => %d", newSock, result );
} /* gtkListenOnSocket */

static void
gtk_socket_changed( void* closure, int oldSock, int newSock )
{
    GtkAppGlobals* globals = (GtkAppGlobals*)closure;

    if ( oldSock != -1 ) {
        g_source_remove( oldSock );
        XP_LOGF( "Removed %d from gtk's list of listened-to sockets" );
    }
    if ( newSock != -1 ) {
        gtkListenOnSocket( globals, newSock );
    }
    globals->cGlobals.socket = newSock;

    /* A hack for the bluetooth case. */
    CommsCtxt* comms = globals->cGlobals.game.comms;
    if ( comms != NULL ) {
        comms_resendAll( comms );
    }
}

static gboolean
acceptorInput( GIOChannel* source, GIOCondition condition, gpointer data )
{
    gboolean keepSource;
    CommonGlobals* globals = (CommonGlobals*)data;
    LOG_FUNC();

    if ( (condition & G_IO_IN) != 0 ) {
        int listener = g_io_channel_unix_get_fd( source );
        keepSource = (*globals->acceptor)( listener, data );
    } else {
        keepSource = FALSE;
    }

    return keepSource;
}

static void
gtk_socket_acceptor( int listener, Acceptor func, CommonGlobals* globals )
{
    GIOChannel* channel;
    guint result;

    LOG_FUNC();

    XP_ASSERT( !globals->acceptor || (func == globals->acceptor) );
    globals->acceptor = func;

    channel = g_io_channel_unix_new( listener );
    result = g_io_add_watch( channel,
                             G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_PRI,
                             acceptorInput, globals );
    XP_LOGF( "%s: g_io_add_watch(%d) => %d", __FUNCTION__, listener, result );
}

static void
sendOnClose( XWStreamCtxt* stream, void* closure )
{
    XP_S16 result;
    GtkAppGlobals* globals = closure;

    XP_LOGF( "sendOnClose called" );
    result = comms_send( globals->cGlobals.game.comms, stream );
} /* sendOnClose */

static void 
drop_msg_toggle( GtkWidget* toggle, GtkAppGlobals* globals )
{
    globals->dropIncommingMsgs = gtk_toggle_button_get_active( 
        GTK_TOGGLE_BUTTON(toggle) );
} /* drop_msg_toggle */

int
gtkmain( LaunchParams* params, int argc, char *argv[] )
{
    short width, height;
    GtkWidget* window;
    GtkWidget* drawing_area;
    GtkWidget* menubar;
    GtkWidget* buttonbar;
    GtkWidget* vbox;
    GtkWidget* hbox;
    GtkAppGlobals globals;
    GtkWidget* dropCheck;

    memset( &globals, 0, sizeof(globals) );

    globals.cGlobals.params = params;
    globals.cGlobals.lastNTilesToUse = MAX_TRAY_TILES;
    globals.cGlobals.socket = -1;

    globals.cGlobals.socketChanged = gtk_socket_changed;
    globals.cGlobals.socketChangedClosure = &globals;
    globals.cGlobals.addAcceptor = gtk_socket_acceptor;

    globals.cp.showBoardArrow = XP_TRUE;
    globals.cp.showRobotScores = params->showRobotScores;

    setupGtkUtilCallbacks( &globals, params->util );

/*     globals.dictionary = params->dict; */
/*     globals.trayOverlaps = params->trayOverlaps; */
/*     globals.askNewGame = params->askNewGame; */
/*     globals.quitWhenDone = params->quitAfter; */
/*     globals.sleepOnAnchor = params->sleepOnAnchor; */
/*     globals.util = params->util; */
/*     globals.fileName = params->fileName; */

/*     globals.listenPort = params->listenPort; */

    /* Now set up the gtk stuff.  This is necessary before we make the
       draw ctxt */
    gtk_init( &argc, &argv );

    globals.window = window = gtk_window_new( GTK_WINDOW_TOPLEVEL );
    if ( !!params->fileName ) {
        gtk_window_set_title( GTK_WINDOW(window), params->fileName );
    }

    vbox = gtk_vbox_new (FALSE, 0);
    gtk_container_add( GTK_CONTAINER(window), vbox );
    gtk_widget_show( vbox );

    g_signal_connect( G_OBJECT (window), "destroy",
                      G_CALLBACK( quit ), &globals );

    menubar = makeMenus( &globals, argc, argv );
    gtk_box_pack_start( GTK_BOX(vbox), menubar, FALSE, TRUE, 0);

    dropCheck = gtk_check_button_new_with_label( "drop incoming messages" );
    g_signal_connect( GTK_OBJECT(dropCheck),
                      "toggled", G_CALLBACK(drop_msg_toggle), &globals );
    gtk_box_pack_start( GTK_BOX(vbox), dropCheck, FALSE, TRUE, 0);
    gtk_widget_show( dropCheck );

    buttonbar = makeButtons( &globals, argc, argv );
    gtk_box_pack_start( GTK_BOX(vbox), buttonbar, FALSE, TRUE, 0);

    drawing_area = gtk_drawing_area_new();
    globals.drawing_area = drawing_area;
    gtk_widget_show( drawing_area );

    width = HOR_SCORE_WIDTH + TIMER_WIDTH + TIMER_PAD;
    if ( globals.cGlobals.params->verticalScore ) {
        width += VERT_SCORE_WIDTH;
    }
    height = 196;
    if ( globals.cGlobals.params->nHidden == 0 ) {
        height += MIN_SCALE * TRAY_HT_ROWS;
    }

    gtk_widget_set_size_request( GTK_WIDGET(drawing_area), width, height );

    hbox = gtk_hbox_new( FALSE, 0 );
    gtk_box_pack_start( GTK_BOX (hbox), drawing_area, TRUE, TRUE, 0);

    if ( globals.cGlobals.params->nHidden != 0 ) {
        GtkWidget* vscrollbar;
        globals.adjustment = 
            (GtkAdjustment*)gtk_adjustment_new( 0, 0, NUM_ROWS, 1, 2, 
                                NUM_ROWS-globals.cGlobals.params->nHidden );
        vscrollbar = gtk_vscrollbar_new( globals.adjustment );
        g_signal_connect( GTK_OBJECT(globals.adjustment), "value_changed",
                          G_CALLBACK(scroll_value_changed), &globals );

        gtk_widget_show( vscrollbar );
        gtk_box_pack_start( GTK_BOX(hbox), vscrollbar, TRUE, TRUE, 0 );
    }

    gtk_box_pack_start( GTK_BOX (hbox), 
                        makeVerticalBar( &globals, window ), 
                        FALSE, TRUE, 0 );
    gtk_widget_show( hbox );

    gtk_box_pack_start( GTK_BOX(vbox), hbox/* drawing_area */, TRUE, TRUE, 0);

    g_signal_connect( GTK_OBJECT(drawing_area), "expose_event",
                      G_CALLBACK(expose_event), &globals );
    g_signal_connect( GTK_OBJECT(drawing_area),"configure_event",
                      G_CALLBACK(configure_event), &globals );
    g_signal_connect( GTK_OBJECT(drawing_area), "button_press_event",
                      G_CALLBACK(button_press_event), &globals );
    g_signal_connect( GTK_OBJECT(drawing_area), "motion_notify_event",
                      G_CALLBACK(motion_notify_event), &globals );
    g_signal_connect( GTK_OBJECT(drawing_area), "button_release_event",
                      G_CALLBACK(button_release_event), &globals );

#ifdef KEY_SUPPORT
# ifdef KEYBOARD_NAV
    g_signal_connect( GTK_OBJECT(window), "key_press_event",
                      G_CALLBACK(key_press_event), &globals );
# endif
    g_signal_connect( GTK_OBJECT(window), "key_release_event",
                      G_CALLBACK(key_release_event), &globals );
#endif

    gtk_widget_set_events( drawing_area, GDK_EXPOSURE_MASK
			 | GDK_LEAVE_NOTIFY_MASK
			 | GDK_BUTTON_PRESS_MASK
			 | GDK_POINTER_MOTION_MASK
			 | GDK_BUTTON_RELEASE_MASK
#ifdef KEY_SUPPORT
# ifdef KEYBOARD_NAV
			 | GDK_KEY_PRESS_MASK
# endif
			 | GDK_KEY_RELEASE_MASK
#endif
/*  			 | GDK_POINTER_MOTION_HINT_MASK */
			   );

    gtk_widget_show( window );

    gtk_main();

/*      MONCONTROL(1); */

    return 0;
} /* gtkmain */

#endif /* PLATFORM_GTK */
