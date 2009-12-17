/* -*-mode: C; fill-column: 78; c-basic-offset: 4;  compile-command: "make MEMDEBUG=TRUE"; -*- */
/* 
 * Copyright 2000-2009 by Eric House (xwords@eehouse.org).  All rights reserved.
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
#include <ctype.h>
#include <gdk/gdkkeysyms.h>
#include <errno.h>
#include <signal.h>

#ifndef CLIENT_ONLY
/*  # include <prc.h> */
#endif
#include <sys/types.h>
#include <unistd.h>

#include "main.h"
#include "linuxmain.h"
#include "linuxutl.h"
#include "linuxbt.h"
#include "linuxudp.h"
#include "linuxsms.h"
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
#ifndef XWFEATURE_STANDALONE_ONLY
static void sendOnClose( XWStreamCtxt* stream, void* closure );
#endif
static void setCtrlsForTray( GtkAppGlobals* globals );
static void printFinalScores( GtkAppGlobals* globals );
static void new_game( GtkWidget* widget, GtkAppGlobals* globals );
static void new_game_impl( GtkAppGlobals* globals, XP_Bool fireConnDlg );


#define GTK_TRAY_HT_ROWS 3

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

static void
gtkSetAltState( GtkAppGlobals* globals, guint state )
{
    globals->altKeyDown
        = (state & (GDK_MOD1_MASK|GDK_SHIFT_MASK|GDK_CONTROL_MASK)) != 0;
}

static gint
button_press_event( GtkWidget* XP_UNUSED(widget), GdkEventButton *event,
                    GtkAppGlobals* globals )
{
    XP_Bool redraw, handled;

    gtkSetAltState( globals, event->state );

    if ( !globals->mouseDown ) {
        globals->mouseDown = XP_TRUE;
        redraw = board_handlePenDown( globals->cGlobals.game.board, 
                                      event->x, event->y, &handled );
        if ( redraw ) {
            board_draw( globals->cGlobals.game.board );
        }
    }
    return 1;
} /* button_press_event */

static gint
motion_notify_event( GtkWidget* XP_UNUSED(widget), GdkEventMotion *event,
                     GtkAppGlobals* globals )
{
    XP_Bool handled;

    gtkSetAltState( globals, event->state );

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

    gtkSetAltState( globals, event->state );

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
#ifdef NUMBER_KEY_AS_INDEX
        } else if ( isdigit( keyval ) ) {
            xpkey = keyval;
            break;
#endif
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

    gtkSetAltState( globals, event->state );

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

    gtkSetAltState( globals, event->state );

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

static void
relay_status_gtk( void* closure, CommsRelayState state )
{
    XP_LOGF( "%s got status: %s", __func__, CommsRelayState2Str(state) );
    GtkAppGlobals* globals = (GtkAppGlobals*)closure;
    globals->cGlobals.state = state;
    globals->stateChar = 'A' + COMMS_RELAYSTATE_ALLCONNECTED - state;
    draw_gtk_status( globals->draw, globals->stateChar );
}

static void
relay_connd_gtk( void* closure, XP_Bool allHere, XP_U16 nMissing )
{
    XP_Bool skip = XP_FALSE;
    char buf[256];

    if ( allHere ) {
        snprintf( buf, sizeof(buf), "All expected players have joined.  Play!" );
    } else {
        GtkAppGlobals* globals = (GtkAppGlobals*)closure;
        DeviceRole role = globals->cGlobals.params->serverRole;
        if ( role == SERVER_ISSERVER ) {
            snprintf( buf, sizeof(buf), "Connected to relay as host; waiting "
                      "for %d player[s].", nMissing );
        } else if ( nMissing > 0 ) {
            snprintf( buf, sizeof(buf), "Connected to relay as guest.  Still "
                      "waiting for %d player[s].", nMissing );
        } else {
            /* an allHere message should be coming immediately, so no
               notification now. */
            skip = XP_TRUE;
        }
    }

    if ( !skip ) {
        (void)gtkask( buf, GTK_BUTTONS_OK );
    }
}

static gint
invoke_new_game( gpointer data )
{
    GtkAppGlobals* globals = (GtkAppGlobals*)data;
    new_game_impl( globals, XP_FALSE );
    return 0;
}

static gint
invoke_new_game_conns( gpointer data )
{
    GtkAppGlobals* globals = (GtkAppGlobals*)data;
    new_game_impl( globals, XP_TRUE );
    return 0;
}

static void
relay_error_gtk( void* closure, XWREASON relayErr )
{
    LOG_FUNC();
    GtkAppGlobals* globals = (GtkAppGlobals*)closure;

    gint (*proc)( gpointer data );
    switch( relayErr ) {
    case XWRELAY_ERROR_NO_ROOM:
    case XWRELAY_ERROR_DUP_ROOM:
        proc = invoke_new_game_conns;
        break;
    case XWRELAY_ERROR_TOO_MANY:
        proc = invoke_new_game;
        break;
    default:
        assert(0);
        break;
    }

    (void)g_idle_add( proc, globals );
}

static void
createOrLoadObjects( GtkAppGlobals* globals )
{
    XWStreamCtxt* stream = NULL;
    XP_Bool opened = XP_FALSE;

#ifndef XWFEATURE_STANDALONE_ONLY
    DeviceRole serverRole = globals->cGlobals.params->serverRole;
    XP_Bool isServer = serverRole != SERVER_ISCLIENT;
#endif
    LaunchParams* params = globals->cGlobals.params;

    globals->draw = (GtkDrawCtx*)gtkDrawCtxtMake( globals->drawing_area,
                                                  globals );

    TransportProcs procs = {
        .closure = globals,
        .send = LINUX_SEND,
#ifdef COMMS_HEARTBEAT
        .reset = linux_reset,
#endif
#ifdef XWFEATURE_RELAY
        .rstatus = relay_status_gtk,
        .rconnd = relay_connd_gtk,
        .rerror = relay_error_gtk,
#endif
    };

    if ( !!params->fileName && file_exists( params->fileName ) ) {

        stream = streamFromFile( &globals->cGlobals, params->fileName, globals );

        opened = game_makeFromStream( MEMPOOL stream, &globals->cGlobals.game, 
                                      &globals->cGlobals.params->gi, 
                                      params->dict, params->util, 
                                      (DrawCtx*)globals->draw, 
                                      &globals->cp, &procs );
        
        stream_destroy( stream );
    }

    if ( !opened ) {
        CommsAddrRec addr;

        XP_MEMSET( &addr, 0, sizeof(addr) );
        addr.conType = params->conType;

#ifdef XWFEATURE_RELAY
        if ( addr.conType == COMMS_CONN_RELAY ) {
            XP_ASSERT( !!params->connInfo.relay.relayName );
            globals->cGlobals.defaultServerName
                = params->connInfo.relay.relayName;
        }
#endif

        params->gi.gameID = util_getCurSeconds(globals->cGlobals.params->util);
        XP_STATUSF( "grabbed gameID: %d\n", params->gi.gameID );

        game_makeNewGame( MEMPOOL &globals->cGlobals.game, &params->gi,
                          params->util, (DrawCtx*)globals->draw,
                          params->gi.gameID, &globals->cp, &procs );

        addr.conType = params->conType;
        if ( 0 ) {
#ifdef XWFEATURE_RELAY
        } else if ( addr.conType == COMMS_CONN_RELAY ) {
            addr.u.ip_relay.ipAddr = 0;
            addr.u.ip_relay.port = params->connInfo.relay.defaultSendPort;
            XP_STRNCPY( addr.u.ip_relay.hostName, params->connInfo.relay.relayName,
                        sizeof(addr.u.ip_relay.hostName) - 1 );
            XP_STRNCPY( addr.u.ip_relay.invite, params->connInfo.relay.invite,
                        sizeof(addr.u.ip_relay.invite) - 1 );
#endif
#ifdef XWFEATURE_BLUETOOTH
        } else if ( addr.conType == COMMS_CONN_BT ) {
            XP_ASSERT( sizeof(addr.u.bt.btAddr) 
                       >= sizeof(params->connInfo.bt.hostAddr));
            XP_MEMCPY( &addr.u.bt.btAddr, &params->connInfo.bt.hostAddr,
                       sizeof(params->connInfo.bt.hostAddr) );
#endif
#ifdef XWFEATURE_IP_DIRECT
        } else if ( addr.conType == COMMS_CONN_IP_DIRECT ) {
            XP_STRNCPY( addr.u.ip.hostName_ip, params->connInfo.ip.hostName,
                        sizeof(addr.u.ip.hostName_ip) - 1 );
            addr.u.ip.port_ip = params->connInfo.ip.port;
#endif
#ifdef XWFEATURE_SMS
        } else if ( addr.conType == COMMS_CONN_SMS ) {
            XP_STRNCPY( addr.u.sms.phone, params->connInfo.sms.serverPhone,
                        sizeof(addr.u.sms.phone) - 1 );
            addr.u.sms.port = params->connInfo.sms.port;
#endif
        }

#ifndef XWFEATURE_STANDALONE_ONLY
        /* This may trigger network activity */
        if ( !!globals->cGlobals.game.comms ) {
            comms_setAddr( globals->cGlobals.game.comms, &addr );
        }
#endif
        model_setDictionary( globals->cGlobals.game.model, params->dict );

        /*         params->gi.phoniesAction = PHONIES_DISALLOW; */
#ifdef XWFEATURE_SEARCHLIMIT
        params->gi.allowHintRect = params->allowHintRect;
#endif

        if ( params->needsNewGame ) {
            new_game_impl( globals, XP_FALSE );
#ifndef XWFEATURE_STANDALONE_ONLY
        } else if ( !isServer ) {
            XWStreamCtxt* stream = 
                mem_stream_make( MEMPOOL params->vtMgr, globals, CHANNEL_NONE,
                                 sendOnClose );
            server_initClientConnection( globals->cGlobals.game.server, 
                                         stream );
#endif
        }
    }

#ifndef XWFEATURE_STANDALONE_ONLY
    if ( !!globals->cGlobals.game.comms ) {
        comms_start( globals->cGlobals.game.comms );
    }
#endif
    server_do( globals->cGlobals.game.server );

} /* createOrLoadObjects */

/* Create a new backing pixmap of the appropriate size and set up contxt to
 * draw using that size.
 */
static gboolean
configure_event( GtkWidget* widget, GdkEventConfigure* XP_UNUSED(event),
                 GtkAppGlobals* globals )
{
    short bdWidth, bdHeight, leftMargin, topMargin;
    short timerLeft, timerTop;
    gint hscale, vscale;
    gint trayTop;
    gint boardTop = 0;
    XP_U16 netStatWidth = 0;

    if ( globals->draw == NULL ) {
        createOrLoadObjects( globals );
    }

    bdWidth = widget->allocation.width - (GTK_RIGHT_MARGIN
                                          + GTK_BOARD_LEFT_MARGIN);
    if ( globals->cGlobals.params->verticalScore ) {
        bdWidth -= GTK_VERT_SCORE_WIDTH;
    }
    bdHeight = widget->allocation.height - (GTK_TOP_MARGIN + GTK_BOTTOM_MARGIN)
        - GTK_MIN_TRAY_SCALEV - GTK_BOTTOM_MARGIN;

    hscale = bdWidth / GTK_NUM_COLS;
    vscale = (bdHeight / (GTK_NUM_ROWS + GTK_TRAY_HT_ROWS)); /* makd tray
                                                                height 3x cell
                                                                height */
    leftMargin = (bdWidth - (hscale*GTK_NUM_COLS)) / 2;
    topMargin = (bdHeight - (vscale*(GTK_NUM_ROWS*2))) / 2;

    if ( !globals->cGlobals.params->verticalScore ) {
        boardTop += GTK_HOR_SCORE_HEIGHT;
    }

    trayTop = boardTop + (vscale * GTK_NUM_ROWS);
    /* move tray up if part of board's meant to be hidden */
    trayTop -= vscale * globals->cGlobals.params->nHidden;
    board_setPos( globals->cGlobals.game.board, GTK_BOARD_LEFT, boardTop,
                  XP_FALSE );
    board_setScale( globals->cGlobals.game.board, hscale, vscale );
    board_setShowColors( globals->cGlobals.game.board, XP_TRUE );
    globals->gridOn = XP_TRUE;

    if ( !!globals->cGlobals.game.comms ) {
        netStatWidth = GTK_NETSTAT_WIDTH;
    }

    timerTop = GTK_TIMER_TOP;
    if ( globals->cGlobals.params->verticalScore ) {
        timerLeft = GTK_BOARD_LEFT + (hscale*GTK_NUM_COLS) + 1;
        board_setScoreboardLoc( globals->cGlobals.game.board, 
                                timerLeft,
                                GTK_VERT_SCORE_TOP,
                                GTK_VERT_SCORE_WIDTH, 
                                vscale*GTK_NUM_COLS,
                                XP_FALSE );

    } else {
        timerLeft = GTK_BOARD_LEFT + (hscale*GTK_NUM_COLS)
            - GTK_TIMER_WIDTH - netStatWidth;
        board_setScoreboardLoc( globals->cGlobals.game.board, 
                                GTK_BOARD_LEFT, GTK_HOR_SCORE_TOP,
                                timerLeft-GTK_BOARD_LEFT,
                                GTK_HOR_SCORE_HEIGHT, 
                                XP_TRUE );

    }

    /* Still pending: do this for the vertical score case */
    if ( globals->cGlobals.game.comms ) {
        globals->netStatLeft = timerLeft + GTK_TIMER_WIDTH;
        globals->netStatTop = 0;
    }

    board_setTimerLoc( globals->cGlobals.game.board, timerLeft, timerTop,
                       GTK_TIMER_WIDTH, GTK_HOR_SCORE_HEIGHT );

    board_setTrayLoc( globals->cGlobals.game.board, GTK_TRAY_LEFT, trayTop, 
                      hscale * GTK_NUM_COLS, vscale * GTK_TRAY_HT_ROWS + 10, 
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
    draw_gtk_status( globals->draw, globals->stateChar );
    
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
#ifdef XWFEATURE_SMS
    linux_sms_close( &globals->cGlobals );
#endif
#ifdef XWFEATURE_IP_DIRECT
    linux_udp_close( &globals->cGlobals );
#endif
#ifdef XWFEATURE_RELAY
    linux_close_socket( &globals->cGlobals );
#endif

    gtk_main_quit();
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
        if ( gtkask( "Are you sure everybody wants to end the game now?", 
                     GTK_BUTTONS_YES_NO ) ) {
            server_endGame( globals->cGlobals.game.server );
            gameOver = TRUE;
        }
    }

    /* the end game listener will take care of printing the final scores */
} /* final_scores */

static void
new_game_impl( GtkAppGlobals* globals, XP_Bool fireConnDlg )
{
    CommsAddrRec addr;

    if ( !!globals->cGlobals.game.comms ) {
        comms_getAddr( globals->cGlobals.game.comms, &addr );
    } else {
        comms_getInitialAddr( &addr );
    }

    if ( newGameDialog( globals, &addr, XP_TRUE, fireConnDlg ) ) {
        CurGameInfo* gi = &globals->cGlobals.params->gi;
#ifndef XWFEATURE_STANDALONE_ONLY
        XP_Bool isClient = gi->serverRole == SERVER_ISCLIENT;
#endif
        TransportProcs procs = {
            .closure = globals,
            .send = LINUX_SEND,
#ifdef COMMS_HEARTBEAT
            .reset = linux_reset,
#endif
        };

        game_reset( MEMPOOL &globals->cGlobals.game, gi,
                    globals->cGlobals.params->util,
                    0, &globals->cp, &procs );

#ifndef XWFEATURE_STANDALONE_ONLY
        if ( !!globals->cGlobals.game.comms ) {
            comms_setAddr( globals->cGlobals.game.comms, &addr );
        } else if ( gi->serverRole != SERVER_STANDALONE ) {
            XP_ASSERT(0);
        }

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
#endif
        (void)server_do( globals->cGlobals.game.server ); /* assign tiles, etc. */
        board_invalAll( globals->cGlobals.game.board );
        board_draw( globals->cGlobals.game.board );
    }

} /* new_game_impl */

static void
new_game( GtkWidget* XP_UNUSED(widget), GtkAppGlobals* globals )
{
    new_game_impl( globals, XP_FALSE );
} /* new_game */

static void
game_info( GtkWidget* XP_UNUSED(widget), GtkAppGlobals* globals )
{
    CommsAddrRec addr;
    comms_getAddr( globals->cGlobals.game.comms, &addr );

    /* Anything to do if OK is clicked?  Changed names etc. already saved.  Try
       server_do in case one's become a robot. */
    if ( newGameDialog( globals, &addr, XP_FALSE, XP_FALSE ) ) {
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

#ifndef XWFEATURE_STANDALONE_ONLY
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

#ifndef XWFEATURE_STANDALONE_ONLY
    (void)createAddItem( fileMenu, "Resend", 
                         GTK_SIGNAL_FUNC(handle_resend), globals );
# ifdef DEBUG
    (void)createAddItem( fileMenu, "Stats", 
                         GTK_SIGNAL_FUNC(handle_commstats), globals );
# endif
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
} /* handle_nhint_button */

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
    XP_U16 newValue;
    gfloat newValueF = adj->value;

    XP_ASSERT( newValueF >= 0.0
               && newValueF <= globals->cGlobals.params->nHidden );
    newValue = (XP_U16)newValueF;

    if ( board_setYOffset( globals->cGlobals.game.board, newValue ) ) {
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
        globals->adjustment->page_size = GTK_NUM_ROWS;
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
gtkUserError( GtkAppGlobals* XP_UNUSED(globals), const char* format, ... )
{
    char buf[512];
    va_list ap;

    va_start( ap, format );

    vsprintf( buf, format, ap );

    (void)gtkask( buf, GTK_BUTTONS_OK );

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
                       XP_U16 playerNum, const XP_UCHAR** texts, 
                       XP_U16 nTiles )
{
    XP_S16 chosen;
    GtkAppGlobals* globals = (GtkAppGlobals*)uc->closure;
	XP_UCHAR* name = globals->cGlobals.params->gi.players[playerNum].name;

    chosen = gtkletterask( pi, name, nTiles, texts );
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
        XP_U16 pageSize = GTK_NUM_ROWS;

        if ( state == TRAY_HIDDEN ) { /* we recover what tray covers */
            nHidden -= GTK_TRAY_HT_ROWS;
        }
        if ( nHidden > 0 ) {
            pageSize -= nHidden;
        }
        globals->adjustment->page_size = pageSize;

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
gtk_util_yOffsetChange( XW_UtilCtxt* uc, XP_U16 XP_UNUSED(oldOffset), 
                        XP_U16 newOffset )
{
    GtkAppGlobals* globals = (GtkAppGlobals*)uc->closure;
    XP_ASSERT( globals->cGlobals.params->nHidden > 0 );
    globals->adjustment->value = newOffset;
    gtk_adjustment_value_changed( GTK_ADJUSTMENT(globals->adjustment) );
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

    if ( globals->cGlobals.params->quitAfter >= 0 ) {
        sleep( globals->cGlobals.params->quitAfter );
        quit( NULL, globals );
    } else if ( globals->cGlobals.params->undoWhenDone ) {
        server_handleUndo( globals->cGlobals.game.server );
        board_draw( globals->cGlobals.game.board );
    }
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
gtk_util_altKeyDown( XW_UtilCtxt* uc )
{
    GtkAppGlobals* globals = (GtkAppGlobals*)uc->closure;
    return globals->altKeyDown;
}

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
} /* pentimer_idle_func */

static gint
score_timer_func( gpointer data )
{
    GtkAppGlobals* globals = (GtkAppGlobals*)data;

    linuxFireTimer( &globals->cGlobals, TIMER_TIMERTICK );

    return XP_FALSE;
} /* score_timer_func */

#ifndef XWFEATURE_STANDALONE_ONLY
static gint
comms_timer_func( gpointer data )
{
    GtkAppGlobals* globals = (GtkAppGlobals*)data;

    linuxFireTimer( &globals->cGlobals, TIMER_COMMS );

    return (gint)0;
}
#endif

#ifdef XWFEATURE_SLOW_ROBOT
static gint
slowrob_timer_func( gpointer data )
{
    GtkAppGlobals* globals = (GtkAppGlobals*)data;

    linuxFireTimer( &globals->cGlobals, TIMER_SLOWROBOT );

    return (gint)0;
}
#endif

static void
gtk_util_setTimer( XW_UtilCtxt* uc, XWTimerReason why, 
                   XP_U16 XP_UNUSED_STANDALONE(when),
                   XWTimerProc proc, void* closure )
{
    GtkAppGlobals* globals = (GtkAppGlobals*)uc->closure;
    guint newSrc;

    cancelTimer( globals, why );

    if ( why == TIMER_PENDOWN ) {
        /* half a second */
        globals->penTimerInterval = 50 * 10000;

        (void)gettimeofday( &globals->penTv, NULL );
        newSrc = g_idle_add( pentimer_idle_func, globals );
    } else if ( why == TIMER_TIMERTICK ) {
        /* one second */
        globals->scoreTimerInterval = 100 * 10000;

        (void)gettimeofday( &globals->scoreTv, NULL );

        newSrc = g_timeout_add( 1000, score_timer_func, globals );
#ifndef XWFEATURE_STANDALONE_ONLY
    } else if ( why == TIMER_COMMS ) {
        newSrc = g_timeout_add( 1000 * when, comms_timer_func, globals );
#endif
#ifdef XWFEATURE_SLOW_ROBOT
    } else if ( why == TIMER_SLOWROBOT ) {
        newSrc = g_timeout_add( 1000 * when, slowrob_timer_func, globals );
#endif
    } else {
        XP_ASSERT( 0 );
    }

    globals->cGlobals.timerInfo[why].proc = proc;
    globals->cGlobals.timerInfo[why].closure = closure;
    XP_ASSERT( newSrc != 0 );
    globals->timerSources[why-1] = newSrc;
} /* gtk_util_setTimer */

static void
gtk_util_clearTimer( XW_UtilCtxt* uc, XWTimerReason why )
{
    GtkAppGlobals* globals = (GtkAppGlobals*)uc->closure;
    globals->cGlobals.timerInfo[why].proc = NULL;
}

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
        if ( !!globals->cGlobals.game.board ) {
            board_draw( globals->cGlobals.game.board );
        }
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
        result = gtkask( buf, GTK_BUTTONS_YES_NO );
    }

    return result;
} /* gtk_util_warnIllegalWord */

static void
gtk_util_remSelected( XW_UtilCtxt* uc )
{
    GtkAppGlobals* globals = (GtkAppGlobals*)uc->closure;
    XWStreamCtxt* stream;
    XP_UCHAR* text;

    stream = mem_stream_make( MEMPOOL 
                              globals->cGlobals.params->vtMgr,
                              globals, CHANNEL_NONE, NULL );
    board_formatRemainingTiles( globals->cGlobals.game.board, stream );
    text = strFromStream( stream );
    stream_destroy( stream );

    (void)gtkask( text, GTK_BUTTONS_OK );
    free( text );
}

#ifndef XWFEATURE_STANDALONE_ONLY
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
#endif

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
    const XP_UCHAR* message = linux_getErrString( id, &silent );

    XP_LOGF( "%s(%d)", __func__, id );

    if ( silent ) {
        XP_LOGF( "%s", message );
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
    XP_Bool freeMe = XP_FALSE;
    GtkButtonsType buttons = GTK_BUTTONS_YES_NO;

    switch( id ) {
	
    case QUERY_COMMIT_TURN:
        question = strFromStream( stream );
        freeMe = XP_TRUE;
        break;
    case QUERY_COMMIT_TRADE:
        question = "Are you sure you want to trade the selected tiles?";
        break;
    case QUERY_ROBOT_MOVE:
    case QUERY_ROBOT_TRADE:
        question = strFromStream( stream );
        freeMe = XP_TRUE;
        buttons = GTK_BUTTONS_OK;
        break;

    default:
        XP_ASSERT( 0 );
        return XP_FALSE;
    }

    result = gtkask( question, buttons );

    if ( freeMe > 0 ) {
        free( question );
    }

    return result;
} /* gtk_util_userQuery */

static GtkWidget*
makeShowButtonFromBitmap( void* closure, const gchar* filename, 
                          const gchar* alt, GCallback func )
{
    GtkWidget* widget;
    GtkWidget* button;

    if ( file_exists( filename ) ) {
        widget = gtk_image_new_from_file( filename );
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
    util->vtable->m_util_altKeyDown = gtk_util_altKeyDown;
    util->vtable->m_util_engineProgressCallback = 
        gtk_util_engineProgressCallback;
    util->vtable->m_util_setTimer = gtk_util_setTimer;
    util->vtable->m_util_clearTimer = gtk_util_clearTimer;
    util->vtable->m_util_requestTime = gtk_util_requestTime;
    util->vtable->m_util_warnIllegalWord = gtk_util_warnIllegalWord;
    util->vtable->m_util_remSelected = gtk_util_remSelected;
#ifndef XWFEATURE_STANDALONE_ONLY
    util->vtable->m_util_makeStreamFromAddr = gtk_util_makeStreamFromAddr;
#endif
#ifdef XWFEATURE_SEARCHLIMIT
    util->vtable->m_util_getTraySearchLimits = gtk_util_getTraySearchLimits;
#endif

    util->closure = globals;
} /* setupGtkUtilCallbacks */

#ifndef XWFEATURE_STANDALONE_ONLY
static gboolean
newConnectionInput( GIOChannel *source,
                    GIOCondition condition,
                    gpointer data )
{
    gboolean keepSource;
    int sock = g_io_channel_unix_get_fd( source );
    GtkAppGlobals* globals = (GtkAppGlobals*)data;

    XP_LOGF( "%s(%p):condition = 0x%x", __func__, source, (int)condition );

/*     XP_ASSERT( sock == globals->cGlobals.socket ); */

    if ( (condition & (G_IO_HUP | G_IO_ERR)) != 0 ) {
        XP_LOGF( "dropping socket %d", sock );
        close( sock );
#ifdef XWFEATURE_RELAY
        globals->cGlobals.socket = -1;
#endif
        if ( 0 ) {
#ifdef XWFEATURE_BLUETOOTH
        } else if ( COMMS_CONN_BT == globals->cGlobals.params->conType ) {
            linux_bt_socketclosed( &globals->cGlobals, sock );
#endif
#ifdef XWFEATURE_IP_DIRECT
        } else if ( COMMS_CONN_IP_DIRECT == globals->cGlobals.params->conType ) {
            linux_udp_socketclosed( &globals->cGlobals, sock );
#endif
        }
        keepSource = FALSE;           /* remove the event source */

    } else if ( (condition & G_IO_IN) != 0 ) {
        ssize_t nRead;
        unsigned char buf[512];
        CommsAddrRec* addrp = NULL;
#if defined XWFEATURE_IP_DIRECT || defined XWFEATURE_SMS
        CommsAddrRec addr;
#endif

        switch ( comms_getConType( globals->cGlobals.game.comms ) ) {
#ifdef XWFEATURE_RELAY
        case COMMS_CONN_RELAY:
            XP_ASSERT( globals->cGlobals.socket == sock );
            nRead = linux_relay_receive( &globals->cGlobals, buf, sizeof(buf) );
            break;
#endif
#ifdef XWFEATURE_BLUETOOTH
        case COMMS_CONN_BT:
            nRead = linux_bt_receive( sock, buf, sizeof(buf) );
            break;
#endif
#ifdef XWFEATURE_SMS
        case COMMS_CONN_SMS:
            addrp = &addr;
            nRead = linux_sms_receive( &globals->cGlobals, sock, 
                                       buf, sizeof(buf), addrp );
            break;
#endif
#ifdef XWFEATURE_IP_DIRECT
        case COMMS_CONN_IP_DIRECT:
            addrp = &addr;
            nRead = linux_udp_receive( sock, buf, sizeof(buf), addrp, &globals->cGlobals );
            break;
#endif
        default:
            XP_ASSERT( 0 );
        }

        if ( !globals->dropIncommingMsgs && nRead > 0 ) {
            XWStreamCtxt* inboundS;
            XP_Bool redraw = XP_FALSE;

            inboundS = stream_from_msgbuf( &globals->cGlobals, buf, nRead );
            if ( !!inboundS ) {
                XP_LOGF( "%s: got %d bytes", __func__, nRead );
                if ( comms_checkIncomingStream( globals->cGlobals.game.comms, 
                                                inboundS, addrp ) ) {
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
            XP_LOGF( "errno from read: %d/%s", errno, strerror(errno) );
        }
        keepSource = TRUE;
    }
    return keepSource;                /* FALSE means to remove event source */
} /* newConnectionInput */

typedef struct SockInfo {
    GIOChannel* channel;
    guint watch;
    int socket;
} SockInfo;

static void
gtk_socket_changed( void* closure, int oldSock, int newSock, void** storage )
{
    GtkAppGlobals* globals = (GtkAppGlobals*)closure;
    SockInfo* info = (SockInfo*)*storage;
    XP_LOGF( "%s(old:%d; new:%d)", __func__, oldSock, newSock );

    if ( oldSock != -1 ) {
        XP_ASSERT( info != NULL );
        g_source_remove( info->watch );
        g_io_channel_unref( info->channel );
        XP_FREE( globals->cGlobals.params->util->mpool, info );
        *storage = NULL;
        XP_LOGF( "Removed socket %d from gtk's list of listened-to sockets", 
                 oldSock );
    }
    if ( newSock != -1 ) {
        info = (SockInfo*)XP_MALLOC( globals->cGlobals.params->util->mpool,
                                     sizeof(*info) );
        GIOChannel* channel = g_io_channel_unix_new( newSock );
        g_io_channel_set_close_on_unref( channel, TRUE );
        guint result = g_io_add_watch( channel,
                                       G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_PRI,
                                       newConnectionInput,
                                       globals );
        info->channel = channel;
        info->watch = result;
        *storage = info;
        XP_LOGF( "g_io_add_watch(%d) => %d", newSock, result );
    }
#ifdef XWFEATURE_RELAY
    globals->cGlobals.socket = newSock;
#endif
    /* A hack for the bluetooth case. */
    CommsCtxt* comms = globals->cGlobals.game.comms;
    if ( (comms != NULL) && (comms_getConType(comms) == COMMS_CONN_BT) ) {
        comms_resendAll( comms );
    }
    LOG_RETURN_VOID();
} /* gtk_socket_changed */

static gboolean
acceptorInput( GIOChannel* source, GIOCondition condition, gpointer data )
{
    gboolean keepSource;
    CommonGlobals* globals = (CommonGlobals*)data;
    LOG_FUNC();

    if ( (condition & G_IO_IN) != 0 ) {
        int listener = g_io_channel_unix_get_fd( source );
        XP_LOGF( "%s: input on socket %d", __func__, listener );
        keepSource = (*globals->acceptor)( listener, data );
    } else {
        keepSource = FALSE;
    }

    return keepSource;
} /* acceptorInput */

static void
gtk_socket_acceptor( int listener, Acceptor func, CommonGlobals* globals,
                     void** storage )
{
    SockInfo* info = (SockInfo*)*storage;
    GIOChannel* channel;
    guint watch;

    LOG_FUNC();

    if ( listener == -1 ) {
        XP_ASSERT( !!globals->acceptor );
        globals->acceptor = NULL;
        XP_ASSERT( !!info );
#ifdef DEBUG
        int oldSock = info->socket;
#endif
        g_source_remove( info->watch );
        g_io_channel_unref( info->channel );
        XP_FREE( globals->params->util->mpool, info );
        *storage = NULL;
        XP_LOGF( "Removed listener %d from gtk's list of listened-to sockets", oldSock );
    } else {
        XP_ASSERT( !globals->acceptor || (func == globals->acceptor) );
        globals->acceptor = func;

        channel = g_io_channel_unix_new( listener );
        g_io_channel_set_close_on_unref( channel, TRUE );
        watch = g_io_add_watch( channel,
                                G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_PRI,
                                acceptorInput, globals );
        g_io_channel_unref( channel ); /* only main loop holds it now */
        XP_LOGF( "%s: g_io_add_watch(%d) => %d", __func__, listener, watch );

        XP_ASSERT( NULL == info );
        info = XP_MALLOC( globals->params->util->mpool, sizeof(*info) );
        info->channel = channel;
        info->watch = watch;
        info->socket = listener;
        *storage = info;
    }
} /* gtk_socket_acceptor */

static void
sendOnClose( XWStreamCtxt* stream, void* closure )
{
    GtkAppGlobals* globals = closure;

    XP_LOGF( "sendOnClose called" );
    (void)comms_send( globals->cGlobals.game.comms, stream );
} /* sendOnClose */

static void 
drop_msg_toggle( GtkWidget* toggle, GtkAppGlobals* globals )
{
    globals->dropIncommingMsgs = gtk_toggle_button_get_active( 
        GTK_TOGGLE_BUTTON(toggle) );
} /* drop_msg_toggle */
#endif

static GtkAppGlobals* g_globals_for_signal;
static void
handle_sigintterm( int XP_UNUSED(sig) )
{
    quit( NULL, g_globals_for_signal );
}

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
#ifndef XWFEATURE_STANDALONE_ONLY
    GtkWidget* dropCheck;
#endif

    g_globals_for_signal = &globals;
    struct sigaction act = { .sa_handler = handle_sigintterm };
    sigaction( SIGINT, &act, NULL );
    sigaction( SIGTERM, &act, NULL );

    memset( &globals, 0, sizeof(globals) );

    globals.cGlobals.params = params;
    globals.cGlobals.lastNTilesToUse = MAX_TRAY_TILES;
#ifndef XWFEATURE_STANDALONE_ONLY
# ifdef XWFEATURE_RELAY
    globals.cGlobals.socket = -1;
# endif

    globals.cGlobals.socketChanged = gtk_socket_changed;
    globals.cGlobals.socketChangedClosure = &globals;
    globals.cGlobals.addAcceptor = gtk_socket_acceptor;
#endif

    globals.cp.showBoardArrow = XP_TRUE;
    globals.cp.hideTileValues = params->hideValues;
    globals.cp.skipCommitConfirm = params->skipCommitConfirm;
    globals.cp.showRobotScores = params->showRobotScores;
#ifdef XWFEATURE_SLOW_ROBOT
    globals.cp.robotThinkMin = params->robotThinkMin;
    globals.cp.robotThinkMax = params->robotThinkMax;
#endif

    setupGtkUtilCallbacks( &globals, params->util );

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

#ifndef XWFEATURE_STANDALONE_ONLY
    dropCheck = gtk_check_button_new_with_label( "drop incoming messages" );
    g_signal_connect( GTK_OBJECT(dropCheck),
                      "toggled", G_CALLBACK(drop_msg_toggle), &globals );
    gtk_box_pack_start( GTK_BOX(vbox), dropCheck, FALSE, TRUE, 0);
    gtk_widget_show( dropCheck );
#endif

    buttonbar = makeButtons( &globals, argc, argv );
    gtk_box_pack_start( GTK_BOX(vbox), buttonbar, FALSE, TRUE, 0);

    drawing_area = gtk_drawing_area_new();
    globals.drawing_area = drawing_area;
    gtk_widget_show( drawing_area );

    width = GTK_HOR_SCORE_WIDTH + GTK_TIMER_WIDTH + GTK_TIMER_PAD;
    if ( globals.cGlobals.params->verticalScore ) {
        width += GTK_VERT_SCORE_WIDTH;
    }
    height = 196;
    if ( globals.cGlobals.params->nHidden == 0 ) {
        height += GTK_MIN_SCALE * GTK_TRAY_HT_ROWS;
    }

    gtk_widget_set_size_request( GTK_WIDGET(drawing_area), width, height );

    hbox = gtk_hbox_new( FALSE, 0 );
    gtk_box_pack_start( GTK_BOX (hbox), drawing_area, TRUE, TRUE, 0);

    if ( globals.cGlobals.params->nHidden != 0 ) {
        GtkWidget* vscrollbar;
        globals.adjustment = 
            (GtkAdjustment*)gtk_adjustment_new( 0, 0, GTK_NUM_ROWS, 1, 2, 
                   GTK_NUM_ROWS-globals.cGlobals.params->nHidden );
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
