/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 2000-2003 by Eric House (fixin@peak.org).  All rights reserved.
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
#ifndef CLIENT_ONLY
/*  # include <prc.h> */
#endif
#include <sys/types.h>
#include <unistd.h>

#include "main.h"
#include "linuxmain.h"
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
static XP_Bool file_exists( char* fileName );
static void gtkListenOnSocket( GtkAppGlobals* globals, int newSock );
static void setCtrlsForTray( GtkAppGlobals* globals );
static void printFinalScores( GtkAppGlobals* globals );


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
button_press_event( GtkWidget *widget, GdkEventButton *event,
                    GtkAppGlobals* globals )
{
    XP_Bool redraw, handled;

    globals->mouseDown = XP_TRUE;

    redraw = board_handlePenDown( globals->cGlobals.game.board, 
                                  event->x, event->y, event->time, &handled );
    if ( redraw ) {
        board_draw( globals->cGlobals.game.board );
    }
    return 1;
} /* button_press_event */

static gint
motion_notify_event( GtkWidget *widget, GdkEventMotion *event,
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
button_release_event( GtkWidget *widget, GdkEventMotion *event,
                      GtkAppGlobals* globals )
{
    XP_Bool redraw;

    if ( globals->mouseDown ) {
        redraw = board_handlePenUp( globals->cGlobals.game.board, 
                                    event->x, 
                                    event->y, event->time );
        if ( redraw ) {
            board_draw( globals->cGlobals.game.board );
        }
        globals->mouseDown = XP_FALSE;
    }
    return 1;
} /* button_release_event */

static gint
key_release_event( GtkWidget *widget, GdkEventKey* event,
		 GtkAppGlobals* globals )
{
    XP_Key xpkey = XP_KEY_NONE;
    guint keyval = event->keyval;

    XP_LOGF( "got key 0x%x", keyval );

    switch( keyval ) {    

    case GDK_Left:
        xpkey = XP_CURSOR_KEY_LEFT;
        break;
    case GDK_Right:
        xpkey = XP_CURSOR_KEY_RIGHT;
        break;
    case GDK_Up:
        xpkey = XP_CURSOR_KEY_UP;
        break;
    case GDK_Down:
        xpkey = XP_CURSOR_KEY_DOWN;
        break;
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
        return FALSE;
    }

    if ( xpkey != XP_KEY_NONE ) {
        if ( board_handleKey( globals->cGlobals.game.board, xpkey ) ) {
            board_draw( globals->cGlobals.game.board );
        }
    }

    return 0;
} /* key_release_event */

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
createOrLoadObjects( GtkAppGlobals* globals, GtkWidget *widget )
{
    XWStreamCtxt* stream = NULL;

    Connectedness serverRole = globals->cGlobals.params->serverRole;
    XP_Bool isServer = serverRole != SERVER_ISCLIENT;
    LaunchParams* params = globals->cGlobals.params;

    globals->draw = (GtkDrawCtx*)gtkDrawCtxtMake( widget, globals );

    if ( !!params->fileName && file_exists( params->fileName ) ) {

	stream = streamFromFile( globals, params->fileName );

	game_makeFromStream( MEMPOOL stream, &globals->cGlobals.game, 
                         &globals->cGlobals.params->gi, 
                         params->dict, params->util, 
                         (DrawCtx*)globals->draw, 
                         &globals->cp,
                         linux_tcp_send, globals );

	stream_destroy( stream );

    } else {			/* not reading from a saved file */
        XP_U16 gameID = (XP_U16)util_getCurSeconds( globals->cGlobals.params->util );
        CommsAddrRec addr;

        XP_ASSERT( !!params->relayName );
        globals->cGlobals.defaultServerName = params->relayName;

        params->gi.gameID = util_getCurSeconds(globals->cGlobals.params->util);
        XP_STATUSF( "grabbed gameID: %ld\n", params->gi.gameID );

        game_makeNewGame( MEMPOOL &globals->cGlobals.game, &params->gi,
                          params->util, (DrawCtx*)globals->draw,
                          gameID, &globals->cp, linux_tcp_send, globals );

        addr.conType = COMMS_CONN_IP;
        addr.u.ip.ipAddr = 0;       /* ??? */
        addr.u.ip.port = params->defaultSendPort;
        comms_setAddr( globals->cGlobals.game.comms, 
                       &addr, params->defaultListenPort );

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
static gint
configure_event( GtkWidget *widget, GdkEventConfigure *event,
		 GtkAppGlobals* globals )
{
    short width, height, leftMargin, topMargin;
    short timerLeft, timerTop;
    gboolean firstTime;
    gint hscale, vscale;
    gint trayTop;
    gint boardTop = 0;

    firstTime = (globals->draw == NULL);

    if ( firstTime ) {
        int listenSocket = linux_init_socket( &globals->cGlobals );
        gtkListenOnSocket( globals, listenSocket );

        createOrLoadObjects( globals, widget );
    }

    width = widget->allocation.width - (RIGHT_MARGIN + BOARD_LEFT_MARGIN);
    height = widget->allocation.height - (TOP_MARGIN + BOTTOM_MARGIN)
        - MIN_TRAY_SCALEV - BOTTOM_MARGIN;

    hscale = width / NUM_COLS;
    vscale = (height / (NUM_ROWS + 2)); /* makd tray height 2x cell height */

    leftMargin = (widget->allocation.width - (hscale*NUM_COLS)) / 2;
    topMargin = (widget->allocation.height - (vscale*(NUM_ROWS*2))) / 2;


    if ( !globals->cGlobals.params->verticalScore ) {
        boardTop += HOR_SCORE_HEIGHT;
    }

    trayTop = boardTop + (vscale * NUM_ROWS) + BOTTOM_MARGIN + 1;
    if ( globals->cGlobals.params->trayOverlaps ) {
        trayTop -= vscale * 2;
    }
    board_setPos( globals->cGlobals.game.board, BOARD_LEFT, boardTop,
                  XP_FALSE );
    board_setScale( globals->cGlobals.game.board, hscale, vscale );
    board_setShowColors( globals->cGlobals.game.board, XP_TRUE );
    globals->gridOn = XP_TRUE;

    if ( globals->cGlobals.params->verticalScore ) {
        board_setScoreboardLoc( globals->cGlobals.game.board, 
                                BOARD_LEFT + (MIN_SCALE*NUM_COLS) + 1,
                                VERT_SCORE_TOP,
                                VERT_SCORE_WIDTH, 
                                VERT_SCORE_HEIGHT,
                                XP_FALSE );

        timerLeft = BOARD_LEFT + (MIN_SCALE*NUM_COLS) + 1;
        timerTop = TIMER_TOP;

    } else {
        board_setScoreboardLoc( globals->cGlobals.game.board, 
                                BOARD_LEFT, HOR_SCORE_TOP,
                                HOR_SCORE_WIDTH, HOR_SCORE_HEIGHT, 
                                XP_TRUE );

        timerLeft = BOARD_LEFT + (MIN_SCALE*NUM_COLS) + 1;
        timerTop = TIMER_TOP;
    }

    board_setTimerLoc( globals->cGlobals.game.board, timerLeft, timerTop,
                       TIMER_WIDTH, HOR_SCORE_HEIGHT );

    board_setTrayLoc( globals->cGlobals.game.board, TRAY_LEFT, trayTop, 
                      hscale * 2, vscale * 2,
                      GTK_DIVIDER_WIDTH );

    setCtrlsForTray( globals );
    
    board_invalAll( globals->cGlobals.game.board );
    board_draw( globals->cGlobals.game.board );

    return TRUE;
} /* configure_event */

/* Redraw the screen from the backing pixmap */
static gint
expose_event( GtkWidget      *widget,
	      GdkEventExpose *event,
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
    board_invalRect( globals->cGlobals.game.board, (XP_Rect*)&event->area );

/*     board_invalAll( globals->cGlobals.game.board ); */
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
quit( void* dunno, GtkAppGlobals* globals )
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
tile_values( GtkWidget* widget, GtkAppGlobals* globals )
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
game_history( GtkWidget* widget, GtkAppGlobals* globals )
{
    catGameHistory( &globals->cGlobals );
} /* game_history */

static void
final_scores( GtkWidget* widget, GtkAppGlobals* globals )
{
    XP_Bool gameOver = server_getGameIsOver( globals->cGlobals.game.server );

    if ( gameOver ) {
        printFinalScores( globals );
    } else {
        XP_Bool confirmed;
        confirmed = 
            gtkask( globals,
                    "Are you sure everybody wants to end the game now?",
                    2, "Yes", "No" ) == 0;

        if ( confirmed ) {
            server_endGame( globals->cGlobals.game.server );
            gameOver = TRUE;
        }
    }

    /* the end game listener will take care of printing the final scores */
} /* final_scores */

static void
new_game( GtkWidget* widget, GtkAppGlobals* globals )
{
    gboolean confirmed;

    confirmed = newGameDialog( globals );
    if ( confirmed ) {
        CurGameInfo* gi = &globals->cGlobals.params->gi;
        XP_Bool isClient = gi->serverRole == SERVER_ISCLIENT;
        XP_U32 gameID = util_getCurSeconds( globals->cGlobals.params->util );

        XP_STATUSF( "grabbed gameID: %ld\n", gameID );
        game_reset( MEMPOOL &globals->cGlobals.game, gi, gameID, &globals->cp,
                    linux_tcp_send, globals );

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
load_game( GtkWidget* widget, GtkAppGlobals* globals )
{
} /* load_game */

static void
save_game( GtkWidget* widget, GtkAppGlobals* globals )
{
} /* save_game */

static void
load_dictionary( GtkWidget* widget, GtkAppGlobals* globals )
{
} /* load_dictionary */

static void
handle_undo( GtkWidget* widget, GtkAppGlobals* globals )
{
} /* handle_undo */

static void
handle_redo( GtkWidget* widget, GtkAppGlobals* globals )
{
} /* handle_redo */

#ifdef FEATURE_TRAY_EDIT
static void
handle_trayEditToggle( GtkWidget* widget, GtkAppGlobals* globals, XP_Bool on )
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
handle_resend( GtkWidget* widget, GtkAppGlobals* globals )
{
    CommsCtxt* comms = globals->cGlobals.game.comms;
    comms_resendAll( comms );
} /* handle_resend */

#ifdef DEBUG
static void
handle_commstats( GtkWidget* widget, GtkAppGlobals* globals )
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
handle_memstats( GtkWidget* widget, GtkAppGlobals* globals )
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
	gtk_signal_connect( GTK_OBJECT(item), "activate",
			    GTK_SIGNAL_FUNC(handlerFunc), globals );
    }
    
    gtk_menu_append( GTK_MENU(parent), item );
    gtk_widget_show( item );

    return item;
} /* createAddItem */

static GtkWidget* 
makeMenus( GtkAppGlobals* globals, int argc, char** argv )
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

static void
handle_flip_button( GtkWidget* widget, GtkAppGlobals* globals )
{
    if ( board_flip( globals->cGlobals.game.board ) ) {
        board_draw( globals->cGlobals.game.board );
    }
} /* handle_flip_button */

static void
handle_value_button( GtkWidget* widget, GtkAppGlobals* globals )
{
    if ( board_toggle_showValues( globals->cGlobals.game.board ) ) {
        board_draw( globals->cGlobals.game.board );
    }
} /* handle_value_button */

static void
handle_hint_button( GtkWidget* widget, GtkAppGlobals* globals )
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
handle_nhint_button( GtkWidget* widget, GtkAppGlobals* globals )
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
handle_colors_button( GtkWidget* widget, GtkAppGlobals* globals )
{
/*     XP_Bool oldVal = board_getShowColors( globals->cGlobals.game.board ); */
/*     if ( board_setShowColors( globals->cGlobals.game.board, !oldVal ) ) { */
/* 	board_draw( globals->cGlobals.game.board );	 */
/*     } */
} /* handle_colors_button */

static void
handle_juggle_button( GtkWidget* widget, GtkAppGlobals* globals )
{
    if ( board_juggleTray( globals->cGlobals.game.board ) ) {
        board_draw( globals->cGlobals.game.board );
    }
} /* handle_juggle_button */

static void
handle_undo_button( GtkWidget* widget, GtkAppGlobals* globals )
{
    if ( server_handleUndo( globals->cGlobals.game.server ) ) {
        board_draw( globals->cGlobals.game.board );
    }
} /* handle_undo_button */

static void
handle_redo_button( GtkWidget* widget, GtkAppGlobals* globals )
{
} /* handle_redo_button */

static void
handle_trade_button( GtkWidget* widget, GtkAppGlobals* globals )
{
    if ( board_beginTrade( globals->cGlobals.game.board ) ) {
        board_draw( globals->cGlobals.game.board );
    }
} /* handle_juggle_button */

static void
scroll_value_changed( GtkAdjustment *adj, GtkAppGlobals* globals )
{
    XP_U16 curYOffset, newValue;
    gfloat newValueF = adj->value;

    XP_ASSERT( newValueF >= 0.0 && newValueF <= 2.0 );
    curYOffset = board_getYOffset( globals->cGlobals.game.board );
    newValue = (XP_U16)newValueF;

    if ( newValue != curYOffset ) {
        board_setYOffset( globals->cGlobals.game.board, newValue );
        board_draw( globals->cGlobals.game.board );
    }
} /* scroll_value_changed */

static void
handle_grid_button( GtkWidget* widget, GtkAppGlobals* globals )
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
handle_hide_button( GtkWidget* widget, GtkAppGlobals* globals )
{
    if ( globals->cGlobals.params->trayOverlaps ) {
	globals->adjustment->page_size = MAX_ROWS;
	globals->adjustment->value = 0.0;

	gtk_signal_emit_by_name( GTK_OBJECT(globals->adjustment), "changed" );
	gtk_adjustment_value_changed( GTK_ADJUSTMENT(globals->adjustment) );
    }
/*     board_setTrayVisible( globals->board, XP_FALSE, XP_TRUE ); */
    if ( board_hideTray( globals->cGlobals.game.board ) ) {
        board_draw( globals->cGlobals.game.board );
    }
} /* handle_hide_button */

static void
handle_commit_button( GtkWidget* widget, GtkAppGlobals* globals )
{
    if ( board_commitTurn( globals->cGlobals.game.board ) ) {
        board_draw( globals->cGlobals.game.board );
    }
} /* handle_commit_button */

static void
gtkUserError( GtkAppGlobals* globals, char* format, ... )
{
    char buf[512];
    va_list ap;

    va_start( ap, format );

    vsprintf( buf, format, ap );

    (void)gtkask( globals, buf, 1, "OK" );

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
gtk_util_askPassword( XW_UtilCtxt* uc, const XP_UCHAR* name, 
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
    if ( globals->cGlobals.params->trayOverlaps ) {
        globals->adjustment->page_size = state==TRAY_HIDDEN? 15 : 13;
        if ( state != TRAY_HIDDEN ) { /* do we need to adjust scrollbar? */
            globals->adjustment->value = 
                board_getYOffset( globals->cGlobals.game.board );
        }
        gtk_signal_emit_by_name( GTK_OBJECT(globals->adjustment), "changed" );
    }
} /* setCtrlsForTray */

static void
gtk_util_trayHiddenChange( XW_UtilCtxt* uc, XW_TrayVisState state )
{
    GtkAppGlobals* globals = (GtkAppGlobals*)uc->closure;
    setCtrlsForTray( globals );
} /* gtk_util_trayHiddenChange */

static void
gtk_util_yOffsetChange( XW_UtilCtxt* uc, XP_U16 oldOffset, XP_U16 newOffset )
{
    GtkAppGlobals* globals = (GtkAppGlobals*)uc->closure;
    board_invalAll( globals->cGlobals.game.board );
/*     board_draw( globals->board ); */
} /* gtk_util_trayHiddenChange */

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
gtk_util_engineProgressCallback( XW_UtilCtxt* uc )
{
#ifdef DONT_ABORT_ENGINE
    return XP_TRUE;		/* keep going */
#else
    gboolean pending = gdk_events_pending();

/*     XP_DEBUGF( "gdk_events_pending returned %d\n", pending ); */

    return !pending;
#endif
} /* gtk_util_engineProgressCallback */

static gint
pentimer_idle_func( gpointer data )
{
    GtkAppGlobals* globals = (GtkAppGlobals*)data;
    struct timeval tv;
    XP_Bool callAgain = XP_TRUE;
    
    gettimeofday( &tv, NULL );

    if ( (tv.tv_usec - globals->penTv.tv_usec) >= globals->penTimerInterval) {
        board_timerFired( globals->cGlobals.game.board, TIMER_PENDOWN );
        callAgain = XP_FALSE;
    } 

    return callAgain;
} /* timer_idle_func */

static gint
score_timer_func( gpointer data )
{
    GtkAppGlobals* globals = (GtkAppGlobals*)data;
    XP_Bool callAgain = XP_TRUE;

    board_timerFired( globals->cGlobals.game.board, TIMER_TIMERTICK );
    callAgain = XP_FALSE;

    return callAgain;
} /* score_timer_func */

static void
gtk_util_setTimer( XW_UtilCtxt* uc, XWTimerReason why )
{
    GtkAppGlobals* globals = (GtkAppGlobals*)uc->closure;

    if ( why == TIMER_PENDOWN ) {
        globals->penTimerInterval = 35 * 10000;
        (void)gettimeofday( &globals->penTv, NULL );
        (void)gtk_idle_add( pentimer_idle_func, globals );    
    } else if ( why == TIMER_TIMERTICK ) {
        globals->scoreTimerInterval = 100 * 10000;
        (void)gettimeofday( &globals->scoreTv, NULL );

        (void)gtk_timeout_add( 1000, score_timer_func, globals );
    }

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
	result = 0 == gtkask( globals, buf, 2, "Ok", "Cancel" );
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

static void
gtk_util_listenPortChange( XW_UtilCtxt* uc, XP_U16 newPort )
{
#ifdef DEBUG
    GtkAppGlobals* globals = (GtkAppGlobals*)uc->closure;
#endif
    XP_LOGF( "listenPortChange  called: not sure what to do" );

    /* if this isn't true, need to tear down and rebind socket */
    XP_ASSERT( newPort == globals->cGlobals.params->defaultListenPort );
} /* gtk_util_listenPortChange */

#ifdef XWFEATURE_SEARCHLIMIT
static XP_Bool 
gtk_util_getTraySearchLimits( XW_UtilCtxt* uc, XP_U16* min, XP_U16* max )
{
    GtkAppGlobals* globals = (GtkAppGlobals*)uc->closure;
    *max = askNTiles( globals, MAX_TRAY_TILES, *max );
    return XP_TRUE;
}
#endif


static void
gtk_util_userError( XW_UtilCtxt* uc, UtilErrID id )
{
    GtkAppGlobals* globals = (GtkAppGlobals*)uc->closure;
    XP_UCHAR* message = linux_getErrString( id );

    gtkUserError( globals, message );
} /* gtk_util_userError */

static XP_Bool
gtk_util_userQuery( XW_UtilCtxt* uc, UtilQueryID id, XWStreamCtxt* stream )
{
    XP_Bool result;
    GtkAppGlobals* globals = (GtkAppGlobals*)uc->closure;
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

    result = gtkask( globals, question, numAnswers, 
                     answers[0], answers[1], answers[2] ) == okIndex;

    if ( freeMe > 0 ) {
        free( question );
    }

    return result;
} /* gtk_util_userQuery */

static XP_Bool
file_exists( char* fileName ) 
{
    struct stat statBuf;

    int statResult = stat( fileName, &statBuf );
    return statResult == 0;
} /* file_exists */

static GtkWidget*
makeShowButtonFromBitmap( GtkAppGlobals* globals, GtkWidget* parent,
			  char* fileName, char* alt, GtkSignalFunc func )
{
    GtkWidget* button;
    GtkWidget* pixmapWid;
    GdkPixmap* pixmap;
    GdkBitmap *mask;
    GtkStyle *style;

    if ( file_exists( fileName ) ) {
	button = gtk_button_new();

	style = gtk_widget_get_style(parent);

	pixmap = gdk_pixmap_create_from_xpm( parent->window, &mask,
					     &style->bg[GTK_STATE_NORMAL],
					     fileName );
	pixmapWid = gtk_pixmap_new( pixmap, mask );
	gtk_container_add( GTK_CONTAINER(button), pixmapWid );

	gtk_widget_show( pixmapWid );
    } else {
	button = gtk_button_new_with_label( alt );
    }
    gtk_widget_show( button );

    if ( func != NULL ) {
	gtk_signal_connect( GTK_OBJECT(button), "clicked", func, globals );
    }

    return button;
} /* makeShowButtonFromBitmap */

static GtkWidget* 
makeVerticalBar( GtkAppGlobals* globals, GtkWidget* window )
{
    GtkWidget* vbox;
    GtkWidget* button;
    GtkWidget* vscrollbar;

    vbox = gtk_vbox_new( FALSE, 0 );

    button = makeShowButtonFromBitmap( globals, window, "../flip.xpm", "f",
				       handle_flip_button );
    gtk_box_pack_start( GTK_BOX(vbox), button, FALSE, TRUE, 0 );

    button = makeShowButtonFromBitmap( globals, window, "../value.xpm", "v",
				       handle_value_button );
    gtk_box_pack_start( GTK_BOX(vbox), button, FALSE, TRUE, 0 );

    button = makeShowButtonFromBitmap( globals, window, "../hint.xpm", "?",
				       handle_hint_button );
    gtk_box_pack_start( GTK_BOX(vbox), button, FALSE, TRUE, 0 );

    button = makeShowButtonFromBitmap( globals, window, "../hintNum.xpm", "n",
                                       handle_nhint_button );
    gtk_box_pack_start( GTK_BOX(vbox), button, FALSE, TRUE, 0 );

    button = makeShowButtonFromBitmap( globals, window, "../colors.xpm", "c",
				       handle_colors_button );
    gtk_box_pack_start( GTK_BOX(vbox), button, FALSE, TRUE, 0 );

    if ( globals->cGlobals.params->trayOverlaps ) {
        globals->adjustment = (GtkAdjustment*)gtk_adjustment_new( 0, 0, 15, 
                                                                  1, 2, 13 );
        vscrollbar = gtk_vscrollbar_new( globals->adjustment );
        gtk_signal_connect( GTK_OBJECT(globals->adjustment), "value_changed",
                            GTK_SIGNAL_FUNC(scroll_value_changed), globals );

        gtk_widget_show( vscrollbar );
        gtk_box_pack_start( GTK_BOX(vbox), vscrollbar, TRUE, TRUE, 0 );
    }

    /* undo and redo buttons */
    button = makeShowButtonFromBitmap( globals, window, "../undo.xpm", "u",
				       handle_undo_button );
    gtk_box_pack_start( GTK_BOX(vbox), button, FALSE, TRUE, 0 );
    button = makeShowButtonFromBitmap( globals, window, "../redo.xpm", "r",
				       handle_redo_button );
    gtk_box_pack_start( GTK_BOX(vbox), button, FALSE, TRUE, 0 );

    /* the four buttons that on palm are beside the tray */
    button = makeShowButtonFromBitmap( globals, window, "../juggle.xpm", "j",
				       handle_juggle_button );
    gtk_box_pack_start( GTK_BOX(vbox), button, FALSE, TRUE, 0 );

    button = makeShowButtonFromBitmap( globals, window, "../trade.xpm", "t",
				       handle_trade_button );
    gtk_box_pack_start( GTK_BOX(vbox), button, FALSE, TRUE, 0 );

    button = makeShowButtonFromBitmap( globals, window, "../hide.xpm", "h",
				       handle_hide_button );
    gtk_box_pack_start( GTK_BOX(vbox), button, FALSE, TRUE, 0 );

    button = makeShowButtonFromBitmap( globals, window, "../hide.xpm", "d",
				       handle_commit_button );
    gtk_box_pack_start( GTK_BOX(vbox), button, FALSE, TRUE, 0 );



    gtk_widget_show( vbox );
    return vbox;
} /* makeVerticalBar */

static GtkWidget* 
makeButtons( GtkAppGlobals* globals, int argc, char** argv )
{
    short i;
    GtkWidget* hbox;
    GtkWidget* button;

    struct {
        char* name;
        GtkSignalFunc func;
    } buttons[] = {
        /* 	{ "Flip", handle_flip_button }, */
        { "Grid", handle_grid_button },
        { "Hide", handle_hide_button },
        { "Commit", handle_commit_button },
    };
    
    hbox = gtk_hbox_new( FALSE, 0 );

    for ( i = 0; i < sizeof(buttons)/sizeof(*buttons); ++i ) {
        button = gtk_button_new_with_label( buttons[i].name );
        gtk_widget_show( button );
        gtk_signal_connect( GTK_OBJECT(button), "clicked",
                            GTK_SIGNAL_FUNC(buttons[i].func), globals );

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

#ifdef BEYOND_IR
    util->vtable->m_util_listenPortChange = gtk_util_listenPortChange;
#endif

    util->closure = globals;
} /* setupGtkUtilCallbacks */


static gboolean
newConnectionInput( GIOChannel *source,
                    GIOCondition condition,
                    gpointer data )
{
    int sock = g_io_channel_unix_get_fd( source );
    GtkAppGlobals* globals = (GtkAppGlobals*)data;

    XP_ASSERT( sock == globals->cGlobals.socket );

    if ( (condition & G_IO_HUP) != 0 ) {

        globals->cGlobals.socket = -1;
        return FALSE;           /* remove the event source */

    } else if ( (condition & G_IO_IN) != 0 ) {
        ssize_t nRead;
        unsigned short packetSize;
        unsigned short tmp;
        unsigned char buf[512];

        XP_LOGF( "activity on socket %d", sock );
        nRead = recv( sock, &tmp, sizeof(tmp), 0 );
        assert( nRead == 2 );

        packetSize = ntohs( tmp );
        nRead = recv( sock, buf, packetSize, 0 );

        if ( !globals->dropIncommingMsgs && nRead > 0 ) {
            XWStreamCtxt* inboundS;
            XP_Bool redraw = XP_FALSE;

            inboundS = stream_from_msgbuf( &globals->cGlobals, buf, nRead );
            if ( !!inboundS ) {
                if ( comms_checkIncommingStream( globals->cGlobals.game.comms, 
                                                 inboundS, NULL ) ) {
                    redraw = server_receiveMessage(globals->cGlobals.game.server
                                                   , inboundS );
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
    }
    return TRUE;                /* FALSE means to remove event source */
} /* newConnectionInput */

/* Make gtk listen for events on the socket that clients will use to
 * connect to us.
 */
static void
gtkListenOnSocket( GtkAppGlobals* globals, int newSock )
{
    GIOChannel* channel = g_io_channel_unix_new( newSock );
    guint result = g_io_add_watch( channel,
                                   G_IO_IN | G_IO_HUP,
                                   newConnectionInput,
                                   globals );
    XP_LOGF( "g_io_add_watch => %d", result );
} /* gtkListenOnSocket */

static void
sendOnClose( XWStreamCtxt* stream, void* closure )
{
    XP_S16 result;
    GtkAppGlobals* globals = closure;

    XP_LOGF( "sendOnClose called" );
    result = comms_send( globals->cGlobals.game.comms, COMMS_CONN_IP, stream );
} /* sendOnClose */

static void 
drop_msg_toggle( GtkWidget* toggle, GtkAppGlobals* globals )
{
    globals->dropIncommingMsgs = gtk_toggle_button_get_active( 
        GTK_TOGGLE_BUTTON(toggle) );
} /* drop_msg_toggle */

int
gtkmain( XP_Bool isServer, LaunchParams* params, int argc, char *argv[] )
{
    short width, height;
    GtkWidget* window;
    GtkWidget* drawing_area;
    GtkWidget* menubar;
    GtkWidget* buttonbar;
    GtkWidget* vbox;
    GtkWidget* hbox;
    GtkWidget* vertBar;
    GtkAppGlobals globals;
    GtkWidget* dropCheck;

    memset( &globals, 0, sizeof(globals) );

    globals.cGlobals.params = params;
    globals.cGlobals.lastNTilesToUse = MAX_TRAY_TILES;
    globals.cGlobals.socket = -1;

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

    gtk_signal_connect( GTK_OBJECT(window), "destroy",
			GTK_SIGNAL_FUNC (quit), &globals );

    menubar = makeMenus( &globals, argc, argv );
    gtk_box_pack_start( GTK_BOX(vbox), menubar, FALSE, TRUE, 0);

    dropCheck = gtk_check_button_new_with_label( "drop incomming messages" );
    gtk_signal_connect(GTK_OBJECT(dropCheck),
		       "toggled", GTK_SIGNAL_FUNC(drop_msg_toggle), &globals );
    gtk_box_pack_start( GTK_BOX(vbox), dropCheck, FALSE, TRUE, 0);
    gtk_widget_show( dropCheck );

    buttonbar = makeButtons( &globals, argc, argv );
    gtk_box_pack_start( GTK_BOX(vbox), buttonbar, FALSE, TRUE, 0);

    vertBar = makeVerticalBar( &globals, window );

    drawing_area = gtk_drawing_area_new();

#if 0
    width = (MAX_COLS * MIN_SCALE) + LEFT_MARGIN + RIGHT_MARGIN;
    height = (MAX_ROWS * MIN_SCALE) + TOP_MARGIN + BOTTOM_MARGIN
	+ MIN_TRAY_SCALE + BOTTOM_MARGIN;
#else
    width = 180;
    height = 196;
    if ( !globals.cGlobals.params->trayOverlaps ) {
        height += MIN_SCALE * 2;
    }
#endif
    gtk_drawing_area_size( GTK_DRAWING_AREA (drawing_area), 
                           width, height );

    hbox = gtk_hbox_new( FALSE, 0 );
    gtk_box_pack_start (GTK_BOX (hbox), drawing_area, TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (hbox), vertBar, TRUE, TRUE, 0);
    gtk_widget_show( hbox );
    gtk_widget_show( drawing_area );

    gtk_box_pack_start (GTK_BOX (vbox), hbox/* drawing_area */, TRUE, TRUE, 0);

    gtk_signal_connect( GTK_OBJECT(drawing_area), "expose_event",
			(GtkSignalFunc) expose_event, &globals );
    gtk_signal_connect( GTK_OBJECT(drawing_area),"configure_event",
			(GtkSignalFunc) configure_event, &globals );
    gtk_signal_connect( GTK_OBJECT(drawing_area), "button_press_event",
			(GtkSignalFunc)button_press_event, &globals );
    gtk_signal_connect( GTK_OBJECT(drawing_area), "motion_notify_event",
			(GtkSignalFunc)motion_notify_event, &globals );
    gtk_signal_connect( GTK_OBJECT(drawing_area), "button_release_event",
			(GtkSignalFunc)button_release_event, &globals );

    gtk_signal_connect( GTK_OBJECT(window), "key_release_event",
                        GTK_SIGNAL_FUNC(key_release_event), &globals );
    
    gtk_widget_set_events( drawing_area, GDK_EXPOSURE_MASK
			 | GDK_LEAVE_NOTIFY_MASK
			 | GDK_BUTTON_PRESS_MASK
			 | GDK_POINTER_MOTION_MASK
			 | GDK_BUTTON_RELEASE_MASK
			 | GDK_KEY_PRESS_MASK
			 | GDK_KEY_RELEASE_MASK
/*  			 | GDK_POINTER_MOTION_HINT_MASK */
			   );

    gtk_widget_show( window );

    gtk_main();

/*      MONCONTROL(1); */

    return 0;
} /* gtkmain */

#endif /* PLATFORM_GTK */
