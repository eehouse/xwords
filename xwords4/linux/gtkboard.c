/* -*- compile-command: "make MEMDEBUG=TRUE -j3"; -*- */
/* 
 * Copyright 2000-2015 by Eric House (xwords@eehouse.org).  All rights
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
#include <sys/stat.h>
#include <fcntl.h>

#include "main.h"
#include "linuxmain.h"
#include "linuxutl.h"
#include "linuxbt.h"
#include "linuxudp.h"
#include "linuxsms.h"
#include "gtkmain.h"

#include "draw.h"
#include "game.h"
#include "movestak.h"
#include "strutils.h"
#include "gtkask.h"
#include "gtkinvit.h"
#include "gtkaskm.h"
#include "gtkchat.h"
#include "gtknewgame.h"
#include "gtkletterask.h"
#include "gtkpasswdask.h"
#include "gtkntilesask.h"
#include "gtkaskdict.h"
#include "linuxdict.h"
/* #include "undo.h" */
#include "gtkdraw.h"
#include "memstream.h"
#include "filestream.h"
#include "gamesdb.h"
#include "relaycon.h"

/* static guint gtkSetupClientSocket( GtkGameGlobals* globals, int sock ); */
static void setCtrlsForTray( GtkGameGlobals* globals );
static void new_game( GtkWidget* widget, GtkGameGlobals* globals );
static XP_Bool new_game_impl( GtkGameGlobals* globals, XP_Bool fireConnDlg );
static void setZoomButtons( GtkGameGlobals* globals, XP_Bool* inOut );
static void disenable_buttons( GtkGameGlobals* globals );
static GtkWidget* addButton( GtkWidget* hbox, gchar* label, GCallback func, 
                             GtkGameGlobals* globals );
static void handle_invite_button( GtkWidget* widget, GtkGameGlobals* globals );
static void gtkShowFinalScores( const GtkGameGlobals* globals, 
                                XP_Bool ignoreTimeout );
static void send_invites( CommonGlobals* cGlobals, XP_U16 nPlayers,
                          XP_U32 devID, const XP_UCHAR* relayID, 
                          const XP_UCHAR* phone );


#define GTK_TRAY_HT_ROWS 3

#if 0
static XWStreamCtxt*
lookupClientStream( GtkGameGlobals* globals, int sock ) 
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
rememberClient( GtkGameGlobals* globals, guint key, int sock, 
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
gtkSetAltState( GtkGameGlobals* globals, guint state )
{
    globals->altKeyDown
        = (state & (GDK_MOD1_MASK|GDK_SHIFT_MASK|GDK_CONTROL_MASK)) != 0;
}

static gint
button_press_event( GtkWidget* XP_UNUSED(widget), GdkEventButton *event,
                    GtkGameGlobals* globals )
{
    XP_Bool redraw, handled;

    gtkSetAltState( globals, event->state );

    if ( !globals->mouseDown ) {
        globals->mouseDown = XP_TRUE;
        redraw = board_handlePenDown( globals->cGlobals.game.board, 
                                      event->x, event->y, &handled );
        if ( redraw ) {
            board_draw( globals->cGlobals.game.board );
            disenable_buttons( globals );
        }
    }
    return 1;
} /* button_press_event */

static gint
motion_notify_event( GtkWidget* XP_UNUSED(widget), GdkEventMotion *event,
                     GtkGameGlobals* globals )
{
    XP_Bool handled;

    gtkSetAltState( globals, event->state );

    if ( globals->mouseDown ) {
        handled = board_handlePenMove( globals->cGlobals.game.board, event->x, 
                                       event->y );
        if ( handled ) {
            board_draw( globals->cGlobals.game.board );
            disenable_buttons( globals );
        }
    } else {
        handled = XP_FALSE;
    }

    return handled;
} /* motion_notify_event */

static gint
button_release_event( GtkWidget* XP_UNUSED(widget), GdkEventMotion *event,
                      GtkGameGlobals* globals )
{
    XP_Bool redraw;

    gtkSetAltState( globals, event->state );

    if ( globals->mouseDown ) {
        redraw = board_handlePenUp( globals->cGlobals.game.board, 
                                    event->x, 
                                    event->y );
        if ( redraw ) {
            board_draw( globals->cGlobals.game.board );
            disenable_buttons( globals );
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
    case GDK_KEY_Return:
        xpkey = XP_RETURN_KEY;
        break;
    case GDK_KEY_space:
        xpkey = XP_RAISEFOCUS_KEY;
        break;

    case GDK_KEY_Left:
        xpkey = XP_CURSOR_KEY_LEFT;
        movesCursor = XP_TRUE;
        break;
    case GDK_KEY_Right:
        xpkey = XP_CURSOR_KEY_RIGHT;
        movesCursor = XP_TRUE;
        break;
    case GDK_KEY_Up:
        xpkey = XP_CURSOR_KEY_UP;
        movesCursor = XP_TRUE;
        break;
    case GDK_KEY_Down:
        xpkey = XP_CURSOR_KEY_DOWN;
        movesCursor = XP_TRUE;
        break;
#endif
    case GDK_KEY_BackSpace:
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
                 GtkGameGlobals* globals )
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
                   GtkGameGlobals* globals )
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
#ifdef KEYBOARD_NAV
    globals->keyDown = XP_FALSE;
#endif

    return handled? 1 : 0;        /* gtk will do something with the key if 0
                                     returned  */
} /* key_release_event */
#endif

#ifdef MEM_DEBUG
# define MEMPOOL globals->cGlobals.util->mpool,
#else
# define MEMPOOL
#endif

static void
relay_status_gtk( void* closure, CommsRelayState state )
{
    XP_LOGF( "%s got status: %s", __func__, CommsRelayState2Str(state) );
    GtkGameGlobals* globals = (GtkGameGlobals*)closure;
    if ( !!globals->draw ) {
        globals->cGlobals.state = state;
        globals->stateChar = 'A' + COMMS_RELAYSTATE_ALLCONNECTED - state;
        draw_gtk_status( globals->draw, globals->stateChar );
    }
}

static void
relay_connd_gtk( void* closure, XP_UCHAR* const room,
                 XP_Bool XP_UNUSED(reconnect), XP_U16 devOrder, 
                 XP_Bool allHere, XP_U16 nMissing )
{
    GtkGameGlobals* globals = (GtkGameGlobals*)closure;
    globals->cGlobals.nMissing = nMissing;
    XP_Bool skip = XP_FALSE;
    char buf[256];

    if ( allHere ) {
        snprintf( buf, sizeof(buf),
                  "All expected players have joined in %s.  Play!", room );
    } else {
        if ( nMissing > 0 ) {
            snprintf( buf, sizeof(buf), "%dth connected to relay; waiting "
                      "in %s for %d player[s].", devOrder, room, nMissing );
        } else {
            /* an allHere message should be coming immediately, so no
               notification now. */
            skip = XP_TRUE;
        }
    }

    if ( !skip ) {
        (void)gtkask_timeout( globals->window, buf, GTK_BUTTONS_OK, NULL, 500 );
    }

    disenable_buttons( globals );
}

static gint
invoke_new_game( gpointer data )
{
    GtkGameGlobals* globals = (GtkGameGlobals*)data;
    new_game_impl( globals, XP_FALSE );
    return 0;
}

static gint
invoke_new_game_conns( gpointer data )
{
    GtkGameGlobals* globals = (GtkGameGlobals*)data;
    new_game_impl( globals, XP_TRUE );
    return 0;
}

static void
relay_error_gtk( void* closure, XWREASON relayErr )
{
    LOG_FUNC();
    GtkGameGlobals* globals = (GtkGameGlobals*)closure;

    gint (*proc)( gpointer data ) = NULL;
    switch( relayErr ) {
    case XWRELAY_ERROR_NO_ROOM:
    case XWRELAY_ERROR_DUP_ROOM:
        proc = invoke_new_game_conns;
        break;
    case XWRELAY_ERROR_TOO_MANY:
    case XWRELAY_ERROR_BADPROTO:
        proc = invoke_new_game;
        break;
    case XWRELAY_ERROR_DELETED:
        (void)gtkask_timeout( globals->window,
                              "relay says another device deleted game.", 
                              GTK_BUTTONS_OK, NULL, 1000 );
        break;
    case XWRELAY_ERROR_DEADGAME:
        break;
    default:
        assert(0);
        break;
    }

    if ( !!proc ) {
        (void)g_idle_add( proc, globals );
    }
}

static XP_Bool 
relay_sendNoConn_gtk( const XP_U8* msg, XP_U16 len, 
                      const XP_UCHAR* XP_UNUSED(msgNo),
                      const XP_UCHAR* relayID, void* closure )
{
    GtkGameGlobals* globals = (GtkGameGlobals*)closure;
    XP_Bool success = XP_FALSE;
    LaunchParams* params = globals->cGlobals.params;
    if ( params->useUdp && !globals->draw ) {
        XP_U16 seed = comms_getChannelSeed( globals->cGlobals.game.comms );
        XP_U32 clientToken = makeClientToken( globals->cGlobals.selRow, seed );
        XP_S16 nSent = relaycon_sendnoconn( params, msg, len, relayID, 
                                            clientToken );
        success = nSent == len;
    }
    return success;
} /* relay_sendNoConn_gtk */

#ifdef COMMS_XPORT_FLAGSPROC
static XP_U32
gtk_getFlags( void* closure )
{
    GtkGameGlobals* globals = (GtkGameGlobals*)closure;
    return (!!globals->draw) ? COMMS_XPORT_FLAGS_NONE
        : COMMS_XPORT_FLAGS_HASNOCONN;
}
#endif

static void
setTransportProcs( TransportProcs* procs, GtkGameGlobals* globals ) 
{
    procs->closure = globals;
    procs->send = LINUX_SEND;
#ifdef COMMS_XPORT_FLAGSPROC
    procs->getFlags = gtk_getFlags;
#endif
#ifdef COMMS_HEARTBEAT
    procs->reset = linux_reset;
#endif
#ifdef XWFEATURE_RELAY
    procs->rstatus = relay_status_gtk;
    procs->rconnd = relay_connd_gtk;
    procs->rerror = relay_error_gtk;
    procs->sendNoConn = relay_sendNoConn_gtk;
#endif
}

#ifndef XWFEATURE_STANDALONE_ONLY
# ifdef DEBUG
static void 
drop_msg_toggle( GtkWidget* toggle, void* data )
{
    XP_Bool disabled = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(toggle) );
    long asInt = (long)data;
    XP_Bool send = 0 != (asInt & 1);
    asInt &= ~1;
    DropTypeData* datum = (DropTypeData*)asInt;
    comms_setAddrDisabled( datum->comms, datum->typ, send, disabled );
} /* drop_msg_toggle */

static void
addDropChecks( GtkGameGlobals* globals )
{
    CommsCtxt* comms = globals->cGlobals.game.comms;
    if ( !!comms ) {
        CommsAddrRec addr;
        comms_getAddr( comms, &addr );
        CommsConnType typ;
        for ( XP_U32 st = 0; addr_iter( &addr, &typ, &st ); ) {
            DropTypeData* datum = &globals->dropData[typ];
            datum->typ = typ;
            datum->comms = comms;

            GtkWidget* hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);

            gchar buf[32];
            snprintf( buf, sizeof(buf), "Drop %s messages", 
                      ConnType2Str( typ ) );
            GtkWidget* widget = gtk_label_new( buf );
            gtk_box_pack_start( GTK_BOX(hbox), widget, FALSE, TRUE, 0);
            gtk_widget_show( widget );

            widget = gtk_check_button_new_with_label( "Incoming" );
            if ( comms_getAddrDisabled( comms, typ, XP_FALSE ) ) {
                gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(widget), TRUE );
            }
            g_signal_connect( widget, "toggled", G_CALLBACK(drop_msg_toggle),
                              datum );
            gtk_box_pack_start( GTK_BOX(hbox), widget, FALSE, TRUE, 0);
            gtk_widget_show( widget );

            widget = gtk_check_button_new_with_label( "Outgoing" );
            if ( comms_getAddrDisabled( comms, typ, XP_TRUE ) ) {
                gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(widget), TRUE );
            }
            g_signal_connect( widget, "toggled", G_CALLBACK(drop_msg_toggle),
                              (void*)(((long)datum) | 1) );
            gtk_box_pack_start( GTK_BOX(hbox), widget, FALSE, TRUE, 0);
            gtk_widget_show( widget );

            gtk_widget_show( hbox );

            gtk_box_pack_start( GTK_BOX(globals->drop_checks_vbox), hbox, FALSE, TRUE, 0);
        }
        gtk_widget_show(globals->drop_checks_vbox);
    }
}
# else
#  define addDropChecks( globals )
# endif  /* DEBUG */
#endif

static void
createOrLoadObjects( GtkGameGlobals* globals )
{
    XWStreamCtxt* stream = NULL;
    XP_Bool opened = XP_FALSE;

#ifndef XWFEATURE_STANDALONE_ONLY
#endif
    CommonGlobals* cGlobals = &globals->cGlobals;
    LaunchParams* params = cGlobals->params;

    globals->draw = (GtkDrawCtx*)gtkDrawCtxtMake( globals->drawing_area,
                                                  globals );

    TransportProcs procs;
    setTransportProcs( &procs, globals );

    if ( !!params->fileName && file_exists( params->fileName ) ) {
        stream = streamFromFile( cGlobals, params->fileName, globals );
#ifdef USE_SQLITE
    } else if ( !!params->dbFileName && file_exists( params->dbFileName ) ) {
        XP_UCHAR buf[32];
        XP_SNPRINTF( buf, sizeof(buf), "%d", params->dbFileID );
        mpool_setTag( MEMPOOL buf );
        stream = streamFromDB( cGlobals, globals );
#endif
    } else if ( !!cGlobals->pDb && 0 <= cGlobals->selRow ) {
        stream = mem_stream_make( MPPARM(cGlobals->util->mpool) 
                                  params->vtMgr, 
                                  cGlobals, CHANNEL_NONE, NULL );
        if ( !loadGame( stream, cGlobals->pDb, cGlobals->selRow ) ) {
            stream_destroy( stream );
            stream = NULL;
        }
    }

    if ( !!stream ) {
        if ( NULL == cGlobals->dict ) {
            cGlobals->dict = makeDictForStream( cGlobals, stream );
        }

        opened = game_makeFromStream( MEMPOOL stream, &cGlobals->game, 
                                      cGlobals->gi, cGlobals->dict, 
                                      &cGlobals->dicts, cGlobals->util, 
                                      (DrawCtx*)globals->draw, 
                                      &cGlobals->cp, &procs );
        XP_LOGF( "%s: loaded gi at %p", __func__, &cGlobals->gi );
        stream_destroy( stream );
    }

    if ( !opened ) {
        CommsAddrRec addr = cGlobals->addr;

        /* XP_MEMSET( &addr, 0, sizeof(addr) ); */
        /* addr.conType = cGlobals->addr.conType; */

#ifdef XWFEATURE_RELAY
        /* if ( addr.conType == COMMS_CONN_RELAY ) { */
        /*     XP_ASSERT( !!params->connInfo.relay.relayName ); */
        /*     globals->cGlobals.defaultServerName */
        /*         = params->connInfo.relay.relayName; */
        /* } */
#endif
        game_makeNewGame( MEMPOOL &cGlobals->game, cGlobals->gi,
                          cGlobals->util, (DrawCtx*)globals->draw,
                          &cGlobals->cp, &procs, params->gameSeed );

        // addr.conType = params->conType;
        CommsConnType typ;
        for ( XP_U32 st = 0; addr_iter( &addr, &typ, &st ); ) {
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
                XP_ASSERT( sizeof(addr.u.bt.btAddr) 
                           >= sizeof(params->connInfo.bt.hostAddr));
                XP_MEMCPY( &addr.u.bt.btAddr, &params->connInfo.bt.hostAddr,
                           sizeof(params->connInfo.bt.hostAddr) );
                break;
#endif
#ifdef XWFEATURE_IP_DIRECT
            case COMMS_CONN_IP_DIRECT:
                XP_STRNCPY( addr.u.ip.hostName_ip, params->connInfo.ip.hostName,
                            sizeof(addr.u.ip.hostName_ip) - 1 );
                addr.u.ip.port_ip = params->connInfo.ip.port;
                break;
#endif
#ifdef XWFEATURE_SMS
            case COMMS_CONN_SMS:
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

        /* Need to save in order to have a valid selRow for the first send */
        saveGame( cGlobals );

#ifndef XWFEATURE_STANDALONE_ONLY
        /* This may trigger network activity */
        if ( !!cGlobals->game.comms ) {
            comms_setAddr( cGlobals->game.comms, &addr );
        }
#endif
        model_setDictionary( cGlobals->game.model, cGlobals->dict );
        setSquareBonuses( cGlobals );
        model_setPlayerDicts( cGlobals->game.model, &cGlobals->dicts );

#ifdef XWFEATURE_SEARCHLIMIT
        cGlobals->gi->allowHintRect = params->allowHintRect;
#endif

        if ( params->needsNewGame ) {
            new_game_impl( globals, XP_FALSE );
#ifndef XWFEATURE_STANDALONE_ONLY
        } else {
            DeviceRole serverRole = cGlobals->gi->serverRole;
            if ( serverRole == SERVER_ISCLIENT ) {
                XWStreamCtxt* stream = 
                    mem_stream_make( MEMPOOL params->vtMgr, 
                                     cGlobals, CHANNEL_NONE,
                                     sendOnClose );
                (void)server_initClientConnection( cGlobals->game.server, 
                                                   stream );
            }
#endif
        }
    }

    if ( !params->fileName && !!params->dbName ) {
        XP_UCHAR buf[64];
        snprintf( buf, sizeof(buf), "%s / %lld", params->dbName,
                  cGlobals->selRow );
        gtk_window_set_title( GTK_WINDOW(globals->window), buf );
    }

#ifndef XWFEATURE_STANDALONE_ONLY
    if ( !!globals->cGlobals.game.comms ) {
        comms_start( globals->cGlobals.game.comms );
    }
#endif
    server_do( globals->cGlobals.game.server );
    saveGame( cGlobals );   /* again, to include address etc. */

    addDropChecks( globals );
    disenable_buttons( globals );
} /* createOrLoadObjects */

/* Create a new backing pixmap of the appropriate size and set up contxt to
 * draw using that size.
 */
static gboolean
configure_event( GtkWidget* widget, GdkEventConfigure* XP_UNUSED(event),
                 GtkGameGlobals* globals )
{
    globals->gridOn = XP_TRUE;
    if ( globals->draw == NULL ) {
        createOrLoadObjects( globals );
    }

    CommonGlobals* cGlobals = &globals->cGlobals;
    BoardCtxt* board = cGlobals->game.board;

    GtkAllocation alloc;
    gtk_widget_get_allocation( widget, &alloc );
    short bdWidth = alloc.width - (GTK_RIGHT_MARGIN + GTK_BOARD_LEFT_MARGIN);
    short bdHeight = alloc.height - (GTK_TOP_MARGIN + GTK_BOTTOM_MARGIN)
        - GTK_MIN_TRAY_SCALEV - GTK_BOTTOM_MARGIN;

#ifdef COMMON_LAYOUT
    XP_ASSERT( !cGlobals->params->verticalScore ); /* not supported */

    BoardDims dims;
    board_figureLayout( board, cGlobals->gi, 
                        GTK_BOARD_LEFT, GTK_HOR_SCORE_TOP, bdWidth, bdHeight,
#if 1
                        150, 200, 
#else
                        0, 0,
#endif
                        bdWidth-25, 16, 16, 
                        XP_FALSE, &dims );
    board_applyLayout( board, &dims );

#else
    short timerLeft, timerTop;
    gint hscale, vscale;
    gint trayTop;
    gint boardTop = 0;
    XP_U16 netStatWidth = 0;
    gint nCols;
    gint nRows;

    nCols = cGlobals->gi->boardSize;
    nRows = nCols;
    if ( cGlobals->params->verticalScore ) {
        bdWidth -= GTK_VERT_SCORE_WIDTH;
    }

    hscale = bdWidth / nCols;
    if ( 0 != cGlobals->params->nHidden ) {
        vscale = hscale;
    } else {
        vscale = (bdHeight / (nCols + GTK_TRAY_HT_ROWS)); /* makd tray height
                                                             3x cell height */
    }

    if ( !cGlobals->params->verticalScore ) {
        boardTop += GTK_HOR_SCORE_HEIGHT;
    }

    trayTop = boardTop + (vscale * nRows);
    /* move tray up if part of board's meant to be hidden */
    trayTop -= vscale * cGlobals->params->nHidden;
    board_setPos( board, GTK_BOARD_LEFT, boardTop,
                  hscale * nCols, vscale * nRows, hscale * 4, XP_FALSE );
    /* board_setScale( board, hscale, vscale ); */

    if ( !!cGlobals->game.comms ) {
        netStatWidth = GTK_NETSTAT_WIDTH;
    }

    timerTop = GTK_TIMER_TOP;
    if ( cGlobals->params->verticalScore ) {
        timerLeft = GTK_BOARD_LEFT + (hscale*nCols) + 1;
        board_setScoreboardLoc( board, 
                                timerLeft,
                                GTK_VERT_SCORE_TOP,
                                GTK_VERT_SCORE_WIDTH, 
                                vscale*nCols,
                                XP_FALSE );

    } else {
        timerLeft = GTK_BOARD_LEFT + (hscale*nCols)
            - GTK_TIMER_WIDTH - netStatWidth;
        board_setScoreboardLoc( board, 
                                GTK_BOARD_LEFT, GTK_HOR_SCORE_TOP,
                                timerLeft-GTK_BOARD_LEFT,
                                GTK_HOR_SCORE_HEIGHT, 
                                XP_TRUE );

    }

    /* Still pending: do this for the vertical score case */
    if ( cGlobals->game.comms ) {
        globals->netStatLeft = timerLeft + GTK_TIMER_WIDTH;
        globals->netStatTop = 0;
    }

    board_setTimerLoc( board, timerLeft, timerTop,
                       GTK_TIMER_WIDTH, GTK_HOR_SCORE_HEIGHT );

    board_setTrayLoc( board, GTK_TRAY_LEFT, trayTop, 
                      hscale * nCols, vscale * GTK_TRAY_HT_ROWS + 10, 
                      GTK_DIVIDER_WIDTH );

#endif

    setCtrlsForTray( globals );
    board_invalAll( board );

    XP_Bool inOut[2];
    board_zoom( board, 0, inOut );
    setZoomButtons( globals, inOut );

    return TRUE;
} /* configure_event */

void
destroy_board_window( GtkWidget* XP_UNUSED(widget), GtkGameGlobals* globals )
{
    LOG_FUNC();
    if ( !!globals->cGlobals.game.comms ) {
        comms_stop( globals->cGlobals.game.comms );
    }
    saveGame( &globals->cGlobals );
    windowDestroyed( globals );
}

static void
on_board_window_shown( GtkWidget* XP_UNUSED(widget), GtkGameGlobals* globals )
{
    LOG_FUNC();
    CommonGlobals* cGlobals = &globals->cGlobals;
    if ( server_getGameIsOver( cGlobals->game.server ) ) {
        gtkShowFinalScores( globals, XP_TRUE );
    }

    CommsCtxt* comms = cGlobals->game.comms;
    if ( !!comms /*&& COMMS_CONN_NONE == comms_getConTypes( comms )*/ ) {
        /* If it has pending invite info, send the invitation! */
        XWStreamCtxt* stream = mem_stream_make( MPPARM(cGlobals->util->mpool) 
                                                cGlobals->params->vtMgr, 
                                                cGlobals, CHANNEL_NONE, NULL );
        if ( loadInviteAddrs( stream, cGlobals->pDb, cGlobals->selRow ) ) {
            CommsAddrRec addr = {0};
            addrFromStream( &addr, stream );
            comms_setAddr( cGlobals->game.comms, &addr );

            XP_U16 nRecs = stream_getU8( stream );
            XP_LOGF( "%s: got invite info: %d records", __func__, nRecs );
            for ( int ii = 0; ii < nRecs; ++ii ) {
                XP_UCHAR relayID[32];
                stringFromStreamHere( stream, relayID, sizeof(relayID) );
                XP_LOGF( "%s: loaded relayID %s", __func__, relayID );

                CommsAddrRec addr = {0};
                addrFromStream( &addr, stream );

                send_invites( cGlobals, 1, 0, relayID, NULL );
            }
        }
        stream_destroy( stream );
    }
} /* on_board_window_shown */

static void
cleanup( GtkGameGlobals* globals )
{
    CommonGlobals* cGlobals = &globals->cGlobals;
    saveGame( cGlobals );
    g_source_remove( globals->idleID );

#ifdef XWFEATURE_BLUETOOTH
    linux_bt_close( cGlobals );
#endif
#ifdef XWFEATURE_SMS
    // linux_sms_close( cGlobals );
#endif
#ifdef XWFEATURE_IP_DIRECT
    linux_udp_close( cGlobals );
#endif
#ifdef XWFEATURE_RELAY
    linux_close_socket( cGlobals );
#endif
    game_dispose( &cGlobals->game );
    gi_disposePlayerInfo( MEMPOOL cGlobals->gi );
    dict_unref( cGlobals->dict );

    linux_util_vt_destroy( cGlobals->util );
    free( cGlobals->util );
} /* cleanup */

GtkWidget*
makeAddSubmenu( GtkWidget* menubar, gchar* label )
{
    GtkWidget* submenu;
    GtkWidget* item;

    item = gtk_menu_item_new_with_label( label );
    gtk_menu_shell_append( GTK_MENU_SHELL(menubar), item );
    
    submenu = gtk_menu_new();
    gtk_menu_item_set_submenu( GTK_MENU_ITEM(item), submenu );

    gtk_widget_show(item);

    return submenu;
} /* makeAddSubmenu */

static void
tile_values( GtkWidget* XP_UNUSED(widget), GtkGameGlobals* globals )
{
    CommonGlobals* cGlobals = &globals->cGlobals;
    if ( !!cGlobals->game.server ) {
        XWStreamCtxt* stream = 
            mem_stream_make( MEMPOOL 
                             cGlobals->params->vtMgr,
                             globals, 
                             CHANNEL_NONE, 
                             catOnClose );
        server_formatDictCounts( cGlobals->game.server, stream, 5 );
        stream_putU8( stream, '\n' );
        stream_destroy( stream );
    }
    
} /* tile_values */

static void
game_history( GtkWidget* XP_UNUSED(widget), GtkGameGlobals* globals )
{
    catGameHistory( &globals->cGlobals );
} /* game_history */

#ifdef TEXT_MODEL
static void
dump_board( GtkWidget* XP_UNUSED(widget), GtkGameGlobals* globals )
{
    if ( !!globals->cGlobals.game.model ) {
        XWStreamCtxt* stream = 
            mem_stream_make( MEMPOOL 
                             globals->cGlobals.params->vtMgr,
                             globals, 
                             CHANNEL_NONE, 
                             catOnClose );
        model_writeToTextStream( globals->cGlobals.game.model, stream );
        stream_destroy( stream );
    }
}
#endif

static void
final_scores( GtkWidget* XP_UNUSED(widget), GtkGameGlobals* globals )
{
    XP_Bool gameOver = server_getGameIsOver( globals->cGlobals.game.server );

    if ( gameOver ) {
        catFinalScores( &globals->cGlobals, -1 );
    } else if ( GTK_RESPONSE_YES == gtkask( globals->window,
                                            "Are you sure you want to resign?", 
                                            GTK_BUTTONS_YES_NO, NULL ) ) {
        globals->cGlobals.manualFinal = XP_TRUE;
        server_endGame( globals->cGlobals.game.server );
        gameOver = TRUE;
    }

    /* the end game listener will take care of printing the final scores */
} /* final_scores */

static XP_Bool
new_game_impl( GtkGameGlobals* globals, XP_Bool fireConnDlg )
{
    XP_Bool success = XP_FALSE;
    CommonGlobals* cGlobals = &globals->cGlobals;
    CommsAddrRec addr;

    if ( !!cGlobals->game.comms ) {
        comms_getAddr( cGlobals->game.comms, &addr );
    } else {
        comms_getInitialAddr( &addr, RELAY_NAME_DEFAULT, RELAY_PORT_DEFAULT );
    }

    CurGameInfo* gi = cGlobals->gi;
    success = newGameDialog( globals, gi, &addr, XP_TRUE, fireConnDlg );
    if ( success ) {
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

        if ( !game_reset( MEMPOOL &cGlobals->game, gi,
                          cGlobals->util,
                          &cGlobals->cp, &procs ) ) {
            /* if ( NULL == globals->draw ) { */
            /*     globals->draw = (GtkDrawCtx*)gtkDrawCtxtMake( globals->drawing_area, */
            /*                                                   globals ); */
            /* } */
            /* game_makeNewGame( MEMPOOL &globals->cGlobals.game, gi, */
            /*                   globals->cGlobals.params->util, */
            /*                   (DrawCtx*)globals->draw, */
            /*                   &globals->cGlobals.cp, &procs,  */
            /*                   globals->cGlobals.params->gameSeed ); */
            /* ModelCtxt* model = globals->cGlobals.game.model; */
            /* if ( NULL == model_getDictionary( model ) ) { */
            /*     DictionaryCtxt* dict = */
            /*         linux_dictionary_make( MEMPOOL globals->cGlobals.params, */
            /*                                gi->dictName, XP_TRUE ); */
            /*     model_setDictionary( model, dict ); */
            /* } */
        }

#ifndef XWFEATURE_STANDALONE_ONLY
        if ( !!cGlobals->game.comms ) {
            comms_setAddr( cGlobals->game.comms, &addr );
        } else if ( gi->serverRole != SERVER_STANDALONE ) {
            XP_ASSERT(0);
        }

        if ( isClient ) {
            XWStreamCtxt* stream =
                mem_stream_make( MEMPOOL cGlobals->params->vtMgr,
                                 cGlobals, CHANNEL_NONE, 
                                 sendOnClose );
            (void)server_initClientConnection( cGlobals->game.server, 
                                               stream );
        }
#endif
        (void)server_do( cGlobals->game.server ); /* assign tiles, etc. */
        board_invalAll( cGlobals->game.board );
        board_draw( cGlobals->game.board );
    }
    return success;
} /* new_game_impl */

static void
new_game( GtkWidget* XP_UNUSED(widget), GtkGameGlobals* globals )
{
    new_game_impl( globals, XP_FALSE );
} /* new_game */

static void
game_info( GtkWidget* XP_UNUSED(widget), GtkGameGlobals* globals )
{
    CommsAddrRec addr;
    comms_getAddr( globals->cGlobals.game.comms, &addr );

    /* Anything to do if OK is clicked?  Changed names etc. already saved.  Try
       server_do in case one's become a robot. */
    CurGameInfo* gi = globals->cGlobals.gi;
    if ( newGameDialog( globals, gi, &addr, XP_FALSE, XP_FALSE ) ) {
        if ( server_do( globals->cGlobals.game.server ) ) {
            board_draw( globals->cGlobals.game.board );
        }
    }
}

static void
load_game( GtkWidget* XP_UNUSED(widget), GtkGameGlobals* XP_UNUSED(globals) )
{
    XP_ASSERT(0);
} /* load_game */

static void
save_game( GtkWidget* XP_UNUSED(widget), GtkGameGlobals* XP_UNUSED(globals) )
{
    XP_ASSERT(0);
} /* save_game */

#ifdef XWFEATURE_CHANGEDICT
static void
change_dictionary( GtkWidget* XP_UNUSED(widget), GtkGameGlobals* globals )
{
    CommonGlobals* cGlobals = &globals->cGlobals;
    LaunchParams* params = cGlobals->params;
	GSList* dicts = listDicts( params );
	gchar buf[265];
	gchar* name = gtkaskdict( dicts, buf, VSIZE(buf) );
	if ( !!name ) {
		DictionaryCtxt* dict = 
			linux_dictionary_make( MPPARM(cGlobals->util->mpool) params, name,
								   params->useMmap );
		game_changeDict( MPPARM(cGlobals->util->mpool) &cGlobals->game, 
                         cGlobals->gi, dict );
	}
	g_slist_free( dicts );
} /* change_dictionary */
#endif

static void
handle_undo( GtkWidget* XP_UNUSED(widget), GtkGameGlobals* XP_UNUSED(globals) )
{
} /* handle_undo */

static void
handle_redo( GtkWidget* XP_UNUSED(widget), GtkGameGlobals* XP_UNUSED(globals) )
{
} /* handle_redo */

#ifdef FEATURE_TRAY_EDIT
static void
handle_trayEditToggle( GtkWidget* XP_UNUSED(widget), 
                       GtkGameGlobals* XP_UNUSED(globals), 
                       XP_Bool XP_UNUSED(on) )
{
} /* handle_trayEditToggle */

static void
handle_trayEditToggle_on( GtkWidget* widget, GtkGameGlobals* globals )
{
    handle_trayEditToggle( widget, globals, XP_TRUE );
}

static void
handle_trayEditToggle_off( GtkWidget* widget, GtkGameGlobals* globals )
{
    handle_trayEditToggle( widget, globals, XP_FALSE );
}
#endif

static void
handle_trade_cancel( GtkWidget* XP_UNUSED(widget), GtkGameGlobals* globals )
{
    BoardCtxt* board = globals->cGlobals.game.board;
    if ( board_endTrade( board ) ) {
        board_draw( board );
    }
}

#ifndef XWFEATURE_STANDALONE_ONLY
static void
handle_resend( GtkWidget* XP_UNUSED(widget), GtkGameGlobals* globals )
{
    CommsCtxt* comms = globals->cGlobals.game.comms;
    if ( comms != NULL ) {
        comms_resendAll( comms, COMMS_CONN_NONE, XP_TRUE );
    }
} /* handle_resend */

#ifdef XWFEATURE_COMMSACK
static void
handle_ack( GtkWidget* XP_UNUSED(widget), GtkGameGlobals* globals )
{
    CommsCtxt* comms = globals->cGlobals.game.comms;
    if ( comms != NULL ) {
        comms_ackAny( comms );
    }
}
#endif

#ifdef DEBUG
static void
handle_commstats( GtkWidget* XP_UNUSED(widget), GtkGameGlobals* globals )
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
handle_memstats( GtkWidget* XP_UNUSED(widget), GtkGameGlobals* globals )
{
    XWStreamCtxt* stream = mem_stream_make( MEMPOOL 
					    globals->cGlobals.params->vtMgr,
					    globals, 
					    CHANNEL_NONE, catOnClose );
    mpool_stats( MEMPOOL stream );
    stream_destroy( stream );
    
} /* handle_memstats */
#endif

#ifdef XWFEATURE_ACTIVERECT
static gint
inval_board_ontimer( gpointer data )
{
    GtkGameGlobals* globals = (GtkGameGlobals*)data;
    BoardCtxt* board = globals->cGlobals.game.board;
    board_draw( board );
    return XP_FALSE;
} /* pen_timer_func */

static void
frame_active( GtkWidget* XP_UNUSED(widget), GtkGameGlobals* globals )
{
    XP_Rect rect;
    BoardCtxt* board = globals->cGlobals.game.board;
    board_getActiveRect( board, &rect );
    frame_active_rect( globals->draw, &rect );
    board_invalRect( board, &rect );
    (void)g_timeout_add( 1000, inval_board_ontimer, globals );
}
#endif

static GtkWidget*
createAddItem( GtkWidget* parent, gchar* label, 
               GCallback handlerFunc, GtkGameGlobals* globals ) 
{
    GtkWidget* item = gtk_menu_item_new_with_label( label );

    if ( handlerFunc != NULL ) {
        g_signal_connect( item, "activate", G_CALLBACK(handlerFunc),
                          globals );
    }
    
    gtk_menu_shell_append( GTK_MENU_SHELL(parent), item );
    gtk_widget_show( item );

    return item;
} /* createAddItem */

static GtkWidget* 
makeMenus( GtkGameGlobals* globals )
{
    GtkWidget* menubar = gtk_menu_bar_new();
    GtkWidget* fileMenu;

    fileMenu = makeAddSubmenu( menubar, "File" );
    (void)createAddItem( fileMenu, "Tile values", 
                         (GCallback)tile_values, globals );
    (void)createAddItem( fileMenu, "Game history", 
                         (GCallback)game_history, globals );
#ifdef TEXT_MODEL
    (void)createAddItem( fileMenu, "Dump board", 
                         (GCallback)dump_board, globals );
#endif

    (void)createAddItem( fileMenu, "Final scores", 
                         (GCallback)final_scores, globals );

    (void)createAddItem( fileMenu, "New game", 
                         (GCallback)new_game, globals );
    (void)createAddItem( fileMenu, "Game info", 
                         (GCallback)game_info, globals );

    (void)createAddItem( fileMenu, "Load game", 
                         (GCallback)load_game, globals );
    (void)createAddItem( fileMenu, "Save game", 
                         (GCallback)save_game, globals );
#ifdef XWFEATURE_CHANGEDICT
    (void)createAddItem( fileMenu, "Change dictionary", 
                         (GCallback)change_dictionary, globals );
#endif
    (void)createAddItem( fileMenu, "Cancel trade", 
                         (GCallback)handle_trade_cancel, globals );

    fileMenu = makeAddSubmenu( menubar, "Edit" );

    (void)createAddItem( fileMenu, "Undo", 
                         (GCallback)handle_undo, globals );
    (void)createAddItem( fileMenu, "Redo", 
                         (GCallback)handle_redo, globals );

#ifdef FEATURE_TRAY_EDIT
    (void)createAddItem( fileMenu, "Allow tray edit", 
                         (GCallback)handle_trayEditToggle_on, globals );
    (void)createAddItem( fileMenu, "Dis-allow tray edit", 
                         (GCallback)handle_trayEditToggle_off, globals );
#endif
    fileMenu = makeAddSubmenu( menubar, "Network" );

#ifndef XWFEATURE_STANDALONE_ONLY
    (void)createAddItem( fileMenu, "Resend", 
                         (GCallback)handle_resend, globals );
#ifdef XWFEATURE_COMMSACK
    (void)createAddItem( fileMenu, "ack any", 
                         (GCallback)handle_ack, globals );
#endif
# ifdef DEBUG
    (void)createAddItem( fileMenu, "Stats", 
                         (GCallback)handle_commstats, globals );
# endif
#endif
#ifdef MEM_DEBUG
    (void)createAddItem( fileMenu, "Mem stats", 
                         (GCallback)handle_memstats, globals );
#endif

#ifdef XWFEATURE_ACTIVERECT
    fileMenu = makeAddSubmenu( menubar, "Test" );
    (void)createAddItem( fileMenu, "Frame active area", 
                         (GCallback)frame_active, globals );
#endif
    /*     (void)createAddItem( fileMenu, "Print board",  */
    /* 			 GTK_SIGNAL_FUNC(handle_print_board), globals ); */

    /*     listAllGames( menubar, argc, argv, globals ); */

    gtk_widget_show( menubar );

    return menubar;
} /* makeMenus */

static void
disenable_buttons( GtkGameGlobals* globals )
{
    XP_U16 nPending = server_getPendingRegs( globals->cGlobals.game.server );
    if ( !globals->invite_button && 0 < nPending ) {
        globals->invite_button = 
            addButton( globals->buttons_hbox, "Invite",
                       G_CALLBACK(handle_invite_button), globals );
    }

    GameStateInfo gsi;
    game_getState( &globals->cGlobals.game, &gsi );

    XP_Bool canFlip = 1 < board_visTileCount( globals->cGlobals.game.board );
    gtk_widget_set_sensitive( globals->flip_button, canFlip );

    XP_Bool canToggle = board_canTogglePending( globals->cGlobals.game.board );
    gtk_widget_set_sensitive( globals->toggle_undo_button, canToggle );

    gtk_widget_set_sensitive( globals->prevhint_button, gsi.canHint );
    gtk_widget_set_sensitive( globals->nexthint_button, gsi.canHint );

    if ( !!globals->invite_button ) {
        gtk_widget_set_sensitive( globals->invite_button, 0 < nPending );
    }
    gtk_widget_set_sensitive( globals->commit_button, gsi.curTurnSelected );

#ifdef XWFEATURE_CHAT
    gtk_widget_set_sensitive( globals->chat_button, gsi.canChat );
#endif
}

static gboolean
handle_flip_button( GtkWidget* XP_UNUSED(widget), gpointer _globals )
{
    GtkGameGlobals* globals = (GtkGameGlobals*)_globals;
    if ( board_flip( globals->cGlobals.game.board ) ) {
        board_draw( globals->cGlobals.game.board );
    }
    return TRUE;
} /* handle_flip_button */

static gboolean
handle_value_button( GtkWidget* XP_UNUSED(widget), gpointer closure )
{
    GtkGameGlobals* globals = (GtkGameGlobals*)closure;
    if ( board_toggle_showValues( globals->cGlobals.game.board ) ) {
        board_draw( globals->cGlobals.game.board );
    }
    return TRUE;
} /* handle_value_button */

static void
handle_hint_button( GtkGameGlobals* globals, XP_Bool prev )
{
    XP_Bool redo;
    if ( board_requestHint( globals->cGlobals.game.board, 
#ifdef XWFEATURE_SEARCHLIMIT
                            XP_FALSE,
#endif
                            prev, &redo ) ) {
        board_draw( globals->cGlobals.game.board );
        disenable_buttons( globals );
    }
} /* handle_hint_button */

static void
handle_prevhint_button( GtkWidget* XP_UNUSED(widget), GtkGameGlobals* globals )
{
    handle_hint_button( globals, XP_TRUE );
} /* handle_prevhint_button */

static void
handle_nexthint_button( GtkWidget* XP_UNUSED(widget), GtkGameGlobals* globals )
{
    handle_hint_button( globals, XP_FALSE );
} /* handle_nexthint_button */

static void
handle_nhint_button( GtkWidget* XP_UNUSED(widget), GtkGameGlobals* globals )
{
    XP_Bool redo;

    board_resetEngine( globals->cGlobals.game.board );
    if ( board_requestHint( globals->cGlobals.game.board, 
#ifdef XWFEATURE_SEARCHLIMIT
                            XP_TRUE, 
#endif
                            XP_FALSE, &redo ) ) {
        board_draw( globals->cGlobals.game.board );
    }
} /* handle_nhint_button */

static void
handle_colors_button( GtkWidget* XP_UNUSED(widget), 
                      GtkGameGlobals* XP_UNUSED(globals) )
{
/*     XP_Bool oldVal = board_getShowColors( globals->cGlobals.game.board ); */
/*     if ( board_setShowColors( globals->cGlobals.game.board, !oldVal ) ) { */
/* 	board_draw( globals->cGlobals.game.board );	 */
/*     } */
} /* handle_colors_button */

static void
handle_juggle_button( GtkWidget* XP_UNUSED(widget), GtkGameGlobals* globals )
{
    if ( board_juggleTray( globals->cGlobals.game.board ) ) {
        board_draw( globals->cGlobals.game.board );
    }
} /* handle_juggle_button */

static void
handle_undo_button( GtkWidget* XP_UNUSED(widget), GtkGameGlobals* globals )
{
    if ( server_handleUndo( globals->cGlobals.game.server, 0 ) ) {
        board_draw( globals->cGlobals.game.board );
    }
} /* handle_undo_button */

static void
handle_redo_button( GtkWidget* XP_UNUSED(widget), 
                    GtkGameGlobals* XP_UNUSED(globals) )
{
} /* handle_redo_button */

static void
handle_toggle_undo( GtkWidget* XP_UNUSED(widget), 
                    GtkGameGlobals* globals )
{
    BoardCtxt* board = globals->cGlobals.game.board;
    if ( board_redoReplacedTiles( board ) || board_replaceTiles( board ) ) {
        board_draw( board );
    }
}

static void
handle_trade_button( GtkWidget* XP_UNUSED(widget), GtkGameGlobals* globals )
{
    if ( board_beginTrade( globals->cGlobals.game.board ) ) {
        board_draw( globals->cGlobals.game.board );
        disenable_buttons( globals );
    }
} /* handle_juggle_button */

static void
handle_done_button( GtkWidget* XP_UNUSED(widget), GtkGameGlobals* globals )
{
    if ( board_commitTurn( globals->cGlobals.game.board ) ) {
        board_draw( globals->cGlobals.game.board );
        disenable_buttons( globals );
    }
} /* handle_done_button */

static void
setZoomButtons( GtkGameGlobals* globals, XP_Bool* inOut )
{
    gtk_widget_set_sensitive( globals->zoomin_button, inOut[0] );
    gtk_widget_set_sensitive( globals->zoomout_button, inOut[1] );
}

static void
handle_zoomin_button( GtkWidget* XP_UNUSED(widget), GtkGameGlobals* globals )
{
    XP_Bool inOut[2];
    if ( board_zoom( globals->cGlobals.game.board, 1, inOut ) ) {
        board_draw( globals->cGlobals.game.board );
        setZoomButtons( globals, inOut );
    }
} /* handle_zoomin_button */

static void
handle_zoomout_button( GtkWidget* XP_UNUSED(widget), GtkGameGlobals* globals )
{
    XP_Bool inOut[2];
    if ( board_zoom( globals->cGlobals.game.board, -1, inOut ) ) {
        board_draw( globals->cGlobals.game.board );
        setZoomButtons( globals, inOut );
    }
} /* handle_zoomout_button */

#ifdef XWFEATURE_CHAT
static void
handle_chat_button( GtkWidget* XP_UNUSED(widget), GtkGameGlobals* globals )
{
    gchar* msg = gtkGetChatMessage( globals );
    if ( NULL != msg ) {
        board_sendChat( globals->cGlobals.game.board, msg );
        g_free( msg );
    }
}
#endif

static void
scroll_value_changed( GtkAdjustment *adj, GtkGameGlobals* globals )
{
    XP_U16 newValue;
    gfloat newValueF = gtk_adjustment_get_value( adj );

    /* XP_ASSERT( newValueF >= 0.0 */
    /*            && newValueF <= globals->cGlobals.params->nHidden ); */
    newValue = (XP_U16)newValueF;

    if ( board_setYOffset( globals->cGlobals.game.board, newValue ) ) {
        board_draw( globals->cGlobals.game.board );
    }
} /* scroll_value_changed */

static void
handle_grid_button( GtkWidget* XP_UNUSED(widget), GtkGameGlobals* globals )
{
    globals->gridOn = !globals->gridOn;

    board_invalAll( globals->cGlobals.game.board );
    board_draw( globals->cGlobals.game.board );
} /* handle_grid_button */

static void
handle_hide_button( GtkWidget* XP_UNUSED(widget), GtkGameGlobals* globals )
{
    BoardCtxt* board;
    XP_Bool draw = XP_FALSE;

    if ( globals->cGlobals.params->nHidden > 0 ) {
        gint nRows = globals->cGlobals.gi->boardSize;
        gtk_adjustment_set_page_size( globals->adjustment, nRows );
        gtk_adjustment_set_value( globals->adjustment, 0.0 );

        g_signal_emit_by_name( globals->adjustment, "changed" );
        // gtk_adjustment_value_changed( GTK_ADJUSTMENT(globals->adjustment) );
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
handle_commit_button( GtkWidget* XP_UNUSED(widget), GtkGameGlobals* globals )
{
    if ( board_commitTurn( globals->cGlobals.game.board ) ) {
        board_draw( globals->cGlobals.game.board );
    }
} /* handle_commit_button */

static void
handle_invite_button( GtkWidget* XP_UNUSED(widget), GtkGameGlobals* globals )
{
    CommonGlobals* cGlobals = &globals->cGlobals;
    /* const CurGameInfo* gi = cGlobals->gi; */

    /* gchar* countStr; */
    /* gchar* phone = NULL; */
    /* gchar* portstr = NULL; */
    /* gchar* forceChannelStr; */
    /* AskMInfo infos[] = { */
    /*     { "Number of players", &countStr }, */
    /*     { "Remote phone#", &phone }, */
    /*     { "Remote port", &portstr }, */
    /*     { "Force channel", &forceChannelStr }, */
    /* }; */

    XP_U16 nMissing = server_getPendingRegs( globals->cGlobals.game.server );
    /* gchar buf[64]; */
    /* sprintf( buf, "%d", nMissing ); */
    /* countStr = buf; */
    /* gchar forceChannelBuf[64]; */
    /* sprintf( forceChannelBuf, "%d", 1 ); */
    /* forceChannelStr = forceChannelBuf; */

    CommsAddrRec inviteAddr = {0};
    gint nPlayers = nMissing;
    XP_U32 devID;
    XP_Bool confirmed = gtkInviteDlg( globals, &inviteAddr, &nPlayers, &devID );
    XP_LOGF( "%s: inviteDlg => %d", __func__, confirmed );

    if ( confirmed ) {
        send_invites( cGlobals, nPlayers, devID, NULL, NULL );
    }
} /* handle_invite_button */

static void
send_invites( CommonGlobals* cGlobals, XP_U16 nPlayers,
              XP_U32 devID, const XP_UCHAR* relayID, 
              const XP_UCHAR* phone )
{
    CommsAddrRec addr = {0};
    CommsCtxt* comms = cGlobals->game.comms;
    XP_ASSERT( comms );
    comms_getAddr( comms, &addr );

    gint forceChannel = 0;  /* PENDING */

    NetLaunchInfo nli = {0};
    nli_init( &nli, cGlobals->gi, &addr, nPlayers, forceChannel );
    nli_setDevID( &nli, linux_getDevIDRelay( cGlobals->params ) );

#ifdef DEBUG
    {
        XWStreamCtxt* stream = mem_stream_make( MPPARM(cGlobals->util->mpool)
                                                cGlobals->params->vtMgr,
                                                NULL, CHANNEL_NONE, NULL );
        nli_saveToStream( &nli, stream );
        NetLaunchInfo tmp;
        nli_makeFromStream( &tmp, stream );
        stream_destroy( stream );
        XP_ASSERT( 0 == memcmp( &nli, &tmp, sizeof(nli) ) );
    }
#endif

    if ( !!phone ) {
        XP_ASSERT( 0 );         /* not implemented */
        /* linux_sms_invite( cGlobals->params, gi, &addr, gameName, */
        /*                   nPlayers, forceChannel,  */
        /*                   inviteAddr.u.sms.phone, inviteAddr.u.sms.port ); */
    }
    if ( 0 != devID || !!relayID ) {
        XP_ASSERT( 0 != devID || (!!relayID && !!relayID[0]) );
        relaycon_invite( cGlobals->params, devID, relayID, &nli );
    }

    /* while ( gtkaskm( "Invite how many and how?", infos, VSIZE(infos) ) ) {  */
    /*     int nPlayers = atoi( countStr ); */
    /*     if ( 0 >= nPlayers || nPlayers > nMissing ) { */
    /*         gchar buf[128]; */
    /*         sprintf( buf, "Please invite between 1 and %d players (inclusive).",  */
    /*                  nMissing ); */
    /*         gtktell( globals->window, buf ); */
    /*         break; */
    /*     } */

    /*     int port = atoi( portstr ); */
    /*     if ( 0 == port ) { */
    /*         gtktell( globals->window, "Port must be a number and not 0." ); */
    /*         break; */
    /*     } */
    /*     int forceChannel = atoi( forceChannelStr ); */
    /*     if ( 1 > forceChannel || 4 <= forceChannel ) { */
    /*         gtktell( globals->window, "Channel must be between 1 and the number of client devices." ); */
    /*         break; */
    /*     } */

    /*     gchar gameName[64]; */
    /*     snprintf( gameName, VSIZE(gameName), "Game %d", gi->gameID ); */

    /*     CommsAddrRec addr; */
    /*     CommsCtxt* comms = globals->cGlobals.game.comms; */
    /*     XP_ASSERT( comms ); */
    /*     comms_getAddr( comms, &addr ); */

    /*     linux_sms_invite( globals->cGlobals.params, gi, &addr, gameName, */
    /*                       nPlayers, forceChannel, phone, port ); */
    /*     break; */
    /* } */
    /* for ( int ii = 0; ii < VSIZE(infos); ++ii ) { */
    /*     g_free( *infos[ii].result ); */
    /* } */
}

static void
gtkUserError( GtkGameGlobals* globals, const char* format, ... )
{
    char buf[512];
    va_list ap;

    va_start( ap, format );

    vsprintf( buf, format, ap );

    (void)gtkask( globals->window, buf, GTK_BUTTONS_OK, NULL );

    va_end(ap);
} /* gtkUserError */

static VTableMgr*
gtk_util_getVTManager(XW_UtilCtxt* uc)
{
    GtkGameGlobals* globals = (GtkGameGlobals*)uc->closure;
    return globals->cGlobals.params->vtMgr;
} /* linux_util_getVTManager */

static XP_S16
gtk_util_userPickTileBlank( XW_UtilCtxt* uc, XP_U16 playerNum, 
                            const XP_UCHAR** texts, XP_U16 nTiles )
{
    XP_S16 chosen;
    GtkGameGlobals* globals = (GtkGameGlobals*)uc->closure;
	XP_UCHAR* name = globals->cGlobals.gi->players[playerNum].name;

    chosen = gtkletterask( NULL, XP_FALSE, name, nTiles, texts );
    return chosen;
}

static XP_S16
gtk_util_userPickTileTray( XW_UtilCtxt* uc, const PickInfo* pi,
                           XP_U16 playerNum, const XP_UCHAR** texts, 
                           XP_U16 nTiles )
{
    XP_S16 chosen;
    GtkGameGlobals* globals = (GtkGameGlobals*)uc->closure;
	XP_UCHAR* name = globals->cGlobals.gi->players[playerNum].name;

    chosen = gtkletterask( pi, XP_TRUE, name, nTiles, texts );
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
setCtrlsForTray( GtkGameGlobals* XP_UNUSED(globals) )
{
#if 0
    XW_TrayVisState state = 
        board_getTrayVisState( globals->cGlobals.game.board );
    XP_S16 nHidden = globals->cGlobals.params->nHidden;

    if ( nHidden != 0 ) {
        XP_U16 pageSize = nRows;

        if ( state == TRAY_HIDDEN ) { /* we recover what tray covers */
            nHidden -= GTK_TRAY_HT_ROWS;
        }
        if ( nHidden > 0 ) {
            pageSize -= nHidden;
        }
        globals->adjustment->page_size = pageSize;

        globals->adjustment->value = 
            board_getYOffset( globals->cGlobals.game.board );
        gtk_signal_emit_by_name( globals->adjustment, "changed" );
    }
#endif
} /* setCtrlsForTray */

static void
gtk_util_trayHiddenChange( XW_UtilCtxt* uc, XW_TrayVisState XP_UNUSED(state),
                           XP_U16 XP_UNUSED(nVisibleRows) )
{
    GtkGameGlobals* globals = (GtkGameGlobals*)uc->closure;
    setCtrlsForTray( globals );
} /* gtk_util_trayHiddenChange */

static void
gtk_util_yOffsetChange( XW_UtilCtxt* uc, XP_U16 maxOffset, 
                        XP_U16 XP_UNUSED(oldOffset), 
                        XP_U16 newOffset )
{
    GtkGameGlobals* globals = (GtkGameGlobals*)uc->closure;
    if ( !!globals->adjustment ) {
        gint nRows = globals->cGlobals.gi->boardSize;
        gtk_adjustment_set_page_size(globals->adjustment, nRows - maxOffset);
        gtk_adjustment_set_value(globals->adjustment, newOffset);
        // gtk_adjustment_value_changed( globals->adjustment );
    }
} /* gtk_util_yOffsetChange */

static void
gtkShowFinalScores( const GtkGameGlobals* globals, XP_Bool ignoreTimeout )
{
    XWStreamCtxt* stream;
    XP_UCHAR* text;
    const CommonGlobals* cGlobals = &globals->cGlobals;

    stream = mem_stream_make( MPPARM(cGlobals->util->mpool)
                              cGlobals->params->vtMgr,
                              NULL, CHANNEL_NONE, NULL );
    server_writeFinalScores( cGlobals->game.server, stream );

    text = strFromStream( stream );
    stream_destroy( stream );

    XP_U16 timeout = (ignoreTimeout || cGlobals->manualFinal)
        ? 0 : cGlobals->params->askTimeout;
    const AskPair buttons[] = {
      { "OK", 1 },
      { "Rematch", 2 },
      { NULL, 0 }
    };

    gint chosen = gtkask_timeout( globals->window, text, GTK_BUTTONS_NONE, 
                                  buttons, timeout );
    free( text );
    if ( 2 == chosen ) {
        make_rematch( globals->apg, cGlobals );
    }
} /* gtkShowFinalScores */

static void
gtk_util_informMove( XW_UtilCtxt* uc, XP_S16 XP_UNUSED(turn), 
                     XWStreamCtxt* expl, XWStreamCtxt* words )
{
    GtkGameGlobals* globals = (GtkGameGlobals*)uc->closure;
    char* question = strFromStream( !!words? words : expl );
    (void)gtkask( globals->window, question, GTK_BUTTONS_OK, NULL );
    free( question );
}

static void
gtk_util_informUndo( XW_UtilCtxt* uc )
{
    GtkGameGlobals* globals = (GtkGameGlobals*)uc->closure;
    (void)gtkask_timeout( globals->window, "Remote player undid a move",
                          GTK_BUTTONS_OK, NULL, 500 );
}

static void
gtk_util_notifyGameOver( XW_UtilCtxt* uc, XP_S16 quitter )
{
    GtkGameGlobals* globals = (GtkGameGlobals*)uc->closure;
    CommonGlobals* cGlobals = &globals->cGlobals;

    if ( cGlobals->params->printHistory ) {
        catGameHistory( cGlobals );
    }

    catFinalScores( cGlobals, quitter );

    if ( cGlobals->params->quitAfter >= 0 ) {
        sleep( cGlobals->params->quitAfter );
        destroy_board_window( NULL, globals );
    } else if ( cGlobals->params->undoWhenDone ) {
        server_handleUndo( cGlobals->game.server, 0 );
        board_draw( cGlobals->game.board );
    } else if ( !cGlobals->params->skipGameOver ) {
        gtkShowFinalScores( globals, XP_TRUE );
    }
} /* gtk_util_notifyGameOver */

static void
gtk_util_informNetDict( XW_UtilCtxt* uc, XP_LangCode XP_UNUSED(lang),
                        const XP_UCHAR* oldName,
                        const XP_UCHAR* newName, const XP_UCHAR* newSum,
                        XWPhoniesChoice phoniesAction )
{
    if ( 0 != strcmp( oldName, newName ) ) {
        GtkGameGlobals* globals = (GtkGameGlobals*)uc->closure;
        gchar buf[512];
        int offset = snprintf( buf, VSIZE(buf),
                               "dict changing from %s to %s (sum=%s).", 
                               oldName, newName, newSum );
        if ( PHONIES_DISALLOW == phoniesAction ) {
            snprintf( &buf[offset], VSIZE(buf)-offset, "%s",
                      "\nPHONIES_DISALLOW is set so this may "
                      "lead to some surprises." );
        }
        (void)gtkask( globals->window, buf, GTK_BUTTONS_OK, NULL );
    }
}

static gint
changeRoles( gpointer data )
{
    linuxChangeRoles( (CommonGlobals*)data );
    return 0;
}

static void
gtk_util_setIsServer( XW_UtilCtxt* uc, XP_Bool isServer )
{
    CommonGlobals* cGlobals = (CommonGlobals*)uc->closure;
    linuxSetIsServer( cGlobals, isServer );

    (void)g_idle_add( changeRoles, cGlobals );
}

/* define this to prevent user events during debugging from stopping the engine */
/* #define DONT_ABORT_ENGINE */

#ifdef XWFEATURE_HILITECELL
static XP_Bool
gtk_util_hiliteCell( XW_UtilCtxt* uc, XP_U16 col, XP_U16 row )
{
    GtkGameGlobals* globals = (GtkGameGlobals*)uc->closure;
#ifndef DONT_ABORT_ENGINE
    gboolean pending;
#endif

    board_hiliteCellAt( globals->cGlobals.game.board, col, row );
    if ( globals->cGlobals.params->sleepOnAnchor ) {
        usleep( 10000 );
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
#endif

static XP_Bool
gtk_util_altKeyDown( XW_UtilCtxt* uc )
{
    GtkGameGlobals* globals = (GtkGameGlobals*)uc->closure;
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
cancelTimer( GtkGameGlobals* globals, XWTimerReason why )
{
    guint src = globals->timerSources[why-1];
    if ( src != 0 ) {
        g_source_remove( src );
        globals->timerSources[why-1] = 0;
    }
} /* cancelTimer */

static gint
pen_timer_func( gpointer data )
{
    GtkGameGlobals* globals = (GtkGameGlobals*)data;

    if ( linuxFireTimer( &globals->cGlobals, TIMER_PENDOWN ) ) {
        board_draw( globals->cGlobals.game.board );
    }

    return XP_FALSE;
} /* pen_timer_func */

static gint
score_timer_func( gpointer data )
{
    GtkGameGlobals* globals = (GtkGameGlobals*)data;

    if ( linuxFireTimer( &globals->cGlobals, TIMER_TIMERTICK ) ) {
        board_draw( globals->cGlobals.game.board );
    }

    return XP_FALSE;
} /* score_timer_func */

#ifndef XWFEATURE_STANDALONE_ONLY
static gint
comms_timer_func( gpointer data )
{
    GtkGameGlobals* globals = (GtkGameGlobals*)data;

    if ( linuxFireTimer( &globals->cGlobals, TIMER_COMMS ) ) {
        board_draw( globals->cGlobals.game.board );
    }

    return (gint)0;
}
#endif

#ifdef XWFEATURE_SLOW_ROBOT
static gint
slowrob_timer_func( gpointer data )
{
    GtkGameGlobals* globals = (GtkGameGlobals*)data;

    if ( linuxFireTimer( &globals->cGlobals, TIMER_SLOWROBOT ) ) {
        board_draw( globals->cGlobals.game.board );
    }

    return (gint)0;
}
#endif

static void
gtk_util_setTimer( XW_UtilCtxt* uc, XWTimerReason why, 
                   XP_U16 XP_UNUSED_STANDALONE(when),
                   XWTimerProc proc, void* closure )
{
    GtkGameGlobals* globals = (GtkGameGlobals*)uc->closure;
    guint newSrc;

    cancelTimer( globals, why );

    if ( why == TIMER_PENDOWN ) {
        if ( 0 != globals->timerSources[why-1] ) {
            g_source_remove( globals->timerSources[why-1] );
        }
        newSrc = g_timeout_add( 1000, pen_timer_func, globals );
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
    GtkGameGlobals* globals = (GtkGameGlobals*)uc->closure;
    globals->cGlobals.timerInfo[why].proc = NULL;
}

static gint
idle_func( gpointer data )
{
    GtkGameGlobals* globals = (GtkGameGlobals*)data;
/*     XP_DEBUGF( "idle_func called\n" ); */

    /* remove before calling server_do.  If server_do puts up a dialog that
       calls gtk_main, then this idle proc will also apply to that event loop
       and bad things can happen.  So kill the idle proc asap. */
    g_source_remove( globals->idleID );

    ServerCtxt* server = globals->cGlobals.game.server;
    if ( !!server && server_do( server ) ) {
        if ( !!globals->cGlobals.game.board ) {
            board_draw( globals->cGlobals.game.board );
        }
    }
    return 0; /* 0 will stop it from being called again */
} /* idle_func */

static void
gtk_util_requestTime( XW_UtilCtxt* uc ) 
{
    GtkGameGlobals* globals = (GtkGameGlobals*)uc->closure;
    globals->idleID = g_idle_add( idle_func, globals );
} /* gtk_util_requestTime */

static XP_Bool
gtk_util_warnIllegalWord( XW_UtilCtxt* uc, BadWordInfo* bwi, XP_U16 player,
			  XP_Bool turnLost )
{
    XP_Bool result;
    GtkGameGlobals* globals = (GtkGameGlobals*)uc->closure;
    char buf[300];

    if ( turnLost ) {
        char wordsBuf[256];
        XP_U16 i;
        XP_UCHAR* name = globals->cGlobals.gi->players[player].name;
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
        sprintf( buf, "Word \"%s\" not in the current dictionary (%s). "
                 "Use it anyway?", bwi->words[0], bwi->dictName );
        result = GTK_RESPONSE_YES == gtkask( globals->window, buf, 
                                             GTK_BUTTONS_YES_NO, NULL );
    }

    return result;
} /* gtk_util_warnIllegalWord */

static void
gtk_util_remSelected( XW_UtilCtxt* uc )
{
    GtkGameGlobals* globals = (GtkGameGlobals*)uc->closure;
    XWStreamCtxt* stream;
    XP_UCHAR* text;

    stream = mem_stream_make( MEMPOOL 
                              globals->cGlobals.params->vtMgr,
                              globals, CHANNEL_NONE, NULL );
    board_formatRemainingTiles( globals->cGlobals.game.board, stream );
    text = strFromStream( stream );
    stream_destroy( stream );

    (void)gtkask( globals->window, text, GTK_BUTTONS_OK, NULL );
    free( text );
}

#ifndef XWFEATURE_STANDALONE_ONLY
static XWStreamCtxt* 
gtk_util_makeStreamFromAddr(XW_UtilCtxt* uc, XP_PlayerAddr channelNo )
{
    GtkGameGlobals* globals = (GtkGameGlobals*)uc->closure;

    XWStreamCtxt* stream = mem_stream_make( MEMPOOL 
                                            globals->cGlobals.params->vtMgr,
                                            &globals->cGlobals, channelNo, 
                                            sendOnClose );
    return stream;
} /* gtk_util_makeStreamFromAddr */

#ifdef XWFEATURE_CHAT
static void
gtk_util_showChat( XW_UtilCtxt* uc, const XP_UCHAR* const msg, XP_S16 from )
{
    GtkGameGlobals* globals = (GtkGameGlobals*)uc->closure;
    XP_UCHAR buf[1024];
    XP_UCHAR* name = "<unknown>";
    if ( 0 <= from ) {
        name = globals->cGlobals.gi->players[from].name;
    }
    XP_SNPRINTF( buf, VSIZE(buf), "quoth %s: %s", name, msg );
    (void)gtkask( globals->window, buf, GTK_BUTTONS_OK, NULL );
}
#endif
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

#ifndef XWFEATURE_MINIWIN
static void
gtk_util_bonusSquareHeld( XW_UtilCtxt* uc, XWBonusType bonus )
{
    LOG_FUNC();
    XP_USE( uc );
    XP_USE( bonus );
}

static void
gtk_util_playerScoreHeld( XW_UtilCtxt* uc, XP_U16 player )
{
    LOG_FUNC();

    GtkGameGlobals* globals = (GtkGameGlobals*)uc->closure;

    LastMoveInfo lmi;
    if ( model_getPlayersLastScore( globals->cGlobals.game.model,
                                    player, &lmi ) ) {
        XP_UCHAR buf[128];
        formatLMI( &lmi, buf, VSIZE(buf) );
        (void)gtkask( globals->window, buf, GTK_BUTTONS_OK, NULL );
    }
}
#endif

#ifdef XWFEATURE_BOARDWORDS
static void
gtk_util_cellSquareHeld( XW_UtilCtxt* uc, XWStreamCtxt* words )
{
    XP_USE( uc );
    catOnClose( words, NULL );
    fprintf( stderr, "\n" );
}
#endif

static void
gtk_util_userError( XW_UtilCtxt* uc, UtilErrID id )
{
    GtkGameGlobals* globals = (GtkGameGlobals*)uc->closure;
    XP_Bool silent;
    const XP_UCHAR* message = linux_getErrString( id, &silent );

    XP_LOGF( "%s: %s", __func__, message );
    if ( !silent ) {
        gtkUserError( globals, message );
    }
} /* gtk_util_userError */

static XP_Bool
gtk_util_userQuery( XW_UtilCtxt* uc, UtilQueryID id, 
                    XWStreamCtxt* stream )
{
    GtkGameGlobals* globals = (GtkGameGlobals*)uc->closure;
    XP_Bool result;
    char* question;
    XP_Bool freeMe = XP_FALSE;
    GtkButtonsType buttons = GTK_BUTTONS_YES_NO;

    switch( id ) {
	
    case QUERY_COMMIT_TURN:
        question = strFromStream( stream );
        freeMe = XP_TRUE;
        break;
    case QUERY_ROBOT_TRADE:
        question = strFromStream( stream );
        freeMe = XP_TRUE;
        buttons = GTK_BUTTONS_OK;
        break;

    default:
        XP_ASSERT( 0 );
        return XP_FALSE;
    }
    gint chosen = gtkask( globals->window, question, buttons, NULL );
    result = GTK_RESPONSE_OK == chosen || chosen == GTK_RESPONSE_YES;

    if ( freeMe ) {
        free( question );
    }

    return result;
} /* gtk_util_userQuery */

static XP_Bool
gtk_util_confirmTrade( XW_UtilCtxt* uc, 
                       const XP_UCHAR** tiles, XP_U16 nTiles )
{
    GtkGameGlobals* globals = (GtkGameGlobals*)uc->closure;
    char question[256];
    formatConfirmTrade( tiles, nTiles, question, sizeof(question) );
    return GTK_RESPONSE_YES == gtkask( globals->window, question, 
                                       GTK_BUTTONS_YES_NO, NULL );
}

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
        g_signal_connect( button, "clicked", func, closure );
    }

    return button;
} /* makeShowButtonFromBitmap */

static GtkWidget* 
makeVerticalBar( GtkGameGlobals* globals, GtkWidget* XP_UNUSED(window) )
{
    GtkWidget* vbox;
    GtkWidget* button;

    vbox = gtk_button_box_new( GTK_ORIENTATION_VERTICAL );

    button = makeShowButtonFromBitmap( globals, "../flip.xpm", "f", 
                                       G_CALLBACK(handle_flip_button) );
    gtk_box_pack_start( GTK_BOX(vbox), button, FALSE, TRUE, 0 );
    globals->flip_button = button;

    button = makeShowButtonFromBitmap( globals, "../value.xpm", "v",
                                       G_CALLBACK(handle_value_button) );
    gtk_box_pack_start( GTK_BOX(vbox), button, FALSE, TRUE, 0 );

    button = makeShowButtonFromBitmap( globals, "../hint.xpm", "?-",
                                       G_CALLBACK(handle_prevhint_button) );
    globals->prevhint_button = button;
    gtk_box_pack_start( GTK_BOX(vbox), button, FALSE, TRUE, 0 );
    button = makeShowButtonFromBitmap( globals, "../hint.xpm", "?+",
                                       G_CALLBACK(handle_nexthint_button) );
    globals->nexthint_button = button;
    gtk_box_pack_start( GTK_BOX(vbox), button, FALSE, TRUE, 0 );

    button = makeShowButtonFromBitmap( globals, "../hintNum.xpm", "n",
                                       G_CALLBACK(handle_nhint_button) );
    gtk_box_pack_start( GTK_BOX(vbox), button, FALSE, TRUE, 0 );

    button = makeShowButtonFromBitmap( globals, "../colors.xpm", "c",
                                       G_CALLBACK(handle_colors_button) );
    gtk_box_pack_start( GTK_BOX(vbox), button, FALSE, TRUE, 0 );

    /* undo and redo buttons */
    button = makeShowButtonFromBitmap( globals, "../undo.xpm", "U",
                                       G_CALLBACK(handle_undo_button) );
    gtk_box_pack_start( GTK_BOX(vbox), button, FALSE, TRUE, 0 );
    button = makeShowButtonFromBitmap( globals, "../redo.xpm", "R",
                                       G_CALLBACK(handle_redo_button) );
    gtk_box_pack_start( GTK_BOX(vbox), button, FALSE, TRUE, 0 );

    button = makeShowButtonFromBitmap( globals, "", "u/r",
                                       G_CALLBACK(handle_toggle_undo) );
    globals->toggle_undo_button = button;
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
    button = makeShowButtonFromBitmap( globals, "../done.xpm", "+",
                                       G_CALLBACK(handle_zoomin_button) );
    globals->zoomin_button = button;
    gtk_box_pack_start( GTK_BOX(vbox), button, FALSE, TRUE, 0 );
    button = makeShowButtonFromBitmap( globals, "../done.xpm", "-",
                                       G_CALLBACK(handle_zoomout_button) );
    globals->zoomout_button = button;
    gtk_box_pack_start( GTK_BOX(vbox), button, FALSE, TRUE, 0 );
#ifdef XWFEATURE_CHAT
    button = makeShowButtonFromBitmap( globals, "", "chat",
                                       G_CALLBACK(handle_chat_button) );
    globals->chat_button = button;
    gtk_box_pack_start( GTK_BOX(vbox), button, FALSE, TRUE, 0 );
#endif

    gtk_widget_show( vbox );
    return vbox;
} /* makeVerticalBar */

static GtkWidget*
addButton( GtkWidget* hbox, gchar* label, GCallback func, GtkGameGlobals* globals )

{
    GtkWidget* button = gtk_button_new_with_label( label );
    gtk_widget_show( button );
    g_signal_connect( button, "clicked", G_CALLBACK(func), globals );
    gtk_box_pack_start( GTK_BOX(hbox), button, FALSE, TRUE, 0);
    return button;
 }

static GtkWidget* 
makeButtons( GtkGameGlobals* globals )
{
    GtkWidget* hbox = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 0 );
    globals->buttons_hbox = hbox;

    (void)addButton( hbox, "Grid", G_CALLBACK(handle_grid_button), globals );
    (void)addButton( hbox, "Hide", G_CALLBACK(handle_hide_button), globals );
    globals->commit_button =
        addButton( hbox, "Commit", G_CALLBACK(handle_commit_button), globals );

    gtk_widget_show( hbox );
    return hbox;
} /* makeButtons */

static void
setupGtkUtilCallbacks( GtkGameGlobals* globals, XW_UtilCtxt* util )
{
    util->vtable->m_util_userError = gtk_util_userError;
    util->vtable->m_util_userQuery = gtk_util_userQuery;
    util->vtable->m_util_confirmTrade = gtk_util_confirmTrade;
    util->vtable->m_util_getVTManager = gtk_util_getVTManager;
    util->vtable->m_util_userPickTileBlank = gtk_util_userPickTileBlank;
    util->vtable->m_util_userPickTileTray = gtk_util_userPickTileTray;
    util->vtable->m_util_askPassword = gtk_util_askPassword;
    util->vtable->m_util_trayHiddenChange = gtk_util_trayHiddenChange;
    util->vtable->m_util_yOffsetChange = gtk_util_yOffsetChange;
    util->vtable->m_util_informMove = gtk_util_informMove;
    util->vtable->m_util_informUndo = gtk_util_informUndo;
    util->vtable->m_util_notifyGameOver = gtk_util_notifyGameOver;
    util->vtable->m_util_informNetDict = gtk_util_informNetDict;
    util->vtable->m_util_setIsServer = gtk_util_setIsServer;
#ifdef XWFEATURE_HILITECELL
    util->vtable->m_util_hiliteCell = gtk_util_hiliteCell;
#endif
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
#ifdef XWFEATURE_CHAT
    util->vtable->m_util_showChat = gtk_util_showChat;
#endif
#ifdef XWFEATURE_SEARCHLIMIT
    util->vtable->m_util_getTraySearchLimits = gtk_util_getTraySearchLimits;
#endif

#ifndef XWFEATURE_MINIWIN
    util->vtable->m_util_bonusSquareHeld = gtk_util_bonusSquareHeld;
    util->vtable->m_util_playerScoreHeld = gtk_util_playerScoreHeld;
#endif
#ifdef XWFEATURE_BOARDWORDS
    util->vtable->m_util_cellSquareHeld = gtk_util_cellSquareHeld;
#endif

    util->closure = globals;
} /* setupGtkUtilCallbacks */

#ifndef XWFEATURE_STANDALONE_ONLY
typedef struct _SockInfo {
    GIOChannel* channel;
    guint watch;
    int socket;
} SockInfo;

static void
gtk_socket_added( void* closure, int newSock, GIOFunc proc )
{
    GtkGameGlobals* globals = (GtkGameGlobals*)closure;

    if ( newSock != -1 ) {
        XP_ASSERT( !!proc );
        GIOChannel* channel = g_io_channel_unix_new( newSock );
        g_io_channel_set_close_on_unref( channel, TRUE );
#ifdef DEBUG
        guint result = 
#endif
            g_io_add_watch( channel, G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_PRI,
                            proc, globals );
        XP_LOGF( "g_io_add_watch(%d) => %d", newSock, result );
    }
    /* A hack for the bluetooth case. */
    CommsCtxt* comms = globals->cGlobals.game.comms;
    
    CommsAddrRec addr;
    comms_getAddr( comms, &addr );
    if ( (comms != NULL) && (addr_hasType( &addr, COMMS_CONN_BT) ) ) {
        comms_resendAll( comms, COMMS_CONN_NONE, XP_FALSE );
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
        XP_FREE( globals->util->mpool, info );
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
        info = XP_MALLOC( globals->util->mpool, sizeof(*info) );
        info->channel = channel;
        info->watch = watch;
        info->socket = listener;
        *storage = info;
    }
} /* gtk_socket_acceptor */
#endif  /* #ifndef XWFEATURE_STANDALONE_ONLY */

/* int */
/* board_main( LaunchParams* params ) */
/* { */
/*     GtkGameGlobals globals; */
/*     initGlobals( &globals, params ); */

/*     if ( !!params->pipe && !!params->fileName ) { */
/*         read_pipe_then_close( &globals.cGlobals, NULL ); */
/*     } else { */
/*         gtk_widget_show( globals.window ); */

/*         gtk_main(); */
/*     } */
/*     /\*      MONCONTROL(1); *\/ */

/*     cleanup( &globals ); */

/*     return 0; */
/* } */

static void
initGlobalsNoDraw( GtkGameGlobals* globals, LaunchParams* params, 
                   CurGameInfo* gi )
{
    memset( globals, 0, sizeof(*globals) );

    globals->cGlobals.gi = &globals->gi;
    if ( !gi ) {
        gi = &params->pgi;
    }
    gi_copy( MPPARM(params->mpool) globals->cGlobals.gi, gi );

    globals->cGlobals.params = params;
    globals->cGlobals.lastNTilesToUse = MAX_TRAY_TILES;
#ifndef XWFEATURE_STANDALONE_ONLY
# ifdef XWFEATURE_RELAY
    globals->cGlobals.relaySocket = -1;
# endif

    globals->cGlobals.socketAdded = gtk_socket_added;
    globals->cGlobals.socketAddedClosure = globals;
    globals->cGlobals.onSave = onGameSaved;
    globals->cGlobals.onSaveClosure = globals;
    globals->cGlobals.addAcceptor = gtk_socket_acceptor;
#endif

    globals->cGlobals.cp.showBoardArrow = XP_TRUE;
    globals->cGlobals.cp.hideTileValues = params->hideValues;
    globals->cGlobals.cp.skipCommitConfirm = params->skipCommitConfirm;
    globals->cGlobals.cp.sortNewTiles = params->sortNewTiles;
    globals->cGlobals.cp.showColors = params->showColors;
    globals->cGlobals.cp.allowPeek = params->allowPeek;
    globals->cGlobals.cp.showRobotScores = params->showRobotScores;
#ifdef XWFEATURE_SLOW_ROBOT
    globals->cGlobals.cp.robotThinkMin = params->robotThinkMin;
    globals->cGlobals.cp.robotThinkMax = params->robotThinkMax;
    globals->cGlobals.cp.robotTradePct = params->robotTradePct;
#endif
#ifdef XWFEATURE_CROSSHAIRS
    globals->cGlobals.cp.hideCrosshairs = params->hideCrosshairs;
#endif

    setupUtil( &globals->cGlobals );
    setupGtkUtilCallbacks( globals, globals->cGlobals.util );
}

/* This gets called all the time, e.g. when the mouse moves across
   drawing-area boundaries. So invalidating is crazy expensive. But this is a
   test app....*/

static gboolean
on_draw_event( GtkWidget *widget, cairo_t* cr, gpointer user_data )
{
    // XP_LOGF( "%s(widget=%p)", __func__, widget );
    GtkGameGlobals* globals = (GtkGameGlobals*)user_data;
    board_invalAll( globals->cGlobals.game.board );
    board_draw( globals->cGlobals.game.board );
    draw_gtk_status( globals->draw, globals->stateChar );

    XP_USE(widget);
    XP_USE(cr);
    return FALSE;
}

void
initGlobals( GtkGameGlobals* globals, LaunchParams* params, CurGameInfo* gi )
{
    CommonGlobals* cGlobals = &globals->cGlobals;
    short width, height;
    GtkWidget* window;
    GtkWidget* drawing_area;
    GtkWidget* menubar;
    GtkWidget* vbox;
    GtkWidget* hbox;

    initGlobalsNoDraw( globals, params, gi );
    if ( !!gi ) {
        XP_ASSERT( !cGlobals->dict );
        cGlobals->dict = linux_dictionary_make( MEMPOOL params,
                                                gi->dictName, XP_TRUE );
        gi->dictLang = dict_getLangCode( cGlobals->dict );
    }

    globals->window = window = gtk_window_new( GTK_WINDOW_TOPLEVEL );
    if ( !!params->fileName ) {
        gtk_window_set_title( GTK_WINDOW(window), params->fileName );
    }

    vbox = gtk_box_new( GTK_ORIENTATION_VERTICAL, 0 );
    gtk_container_add( GTK_CONTAINER(window), vbox );
    gtk_widget_show( vbox );

    g_signal_connect( window, "destroy", G_CALLBACK(destroy_board_window),
                      globals );
    XP_ASSERT( !!globals );
    g_signal_connect( window, "show", G_CALLBACK( on_board_window_shown ),
                      globals );

    menubar = makeMenus( globals );
    gtk_box_pack_start( GTK_BOX(vbox), menubar, FALSE, TRUE, 0);

#if ! defined XWFEATURE_STANDALONE_ONLY && defined DEBUG
    globals->drop_checks_vbox = gtk_box_new( GTK_ORIENTATION_VERTICAL, 0 );
    gtk_box_pack_start( GTK_BOX(vbox), globals->drop_checks_vbox, 
                        FALSE, TRUE, 0 );
#endif

    gtk_box_pack_start( GTK_BOX(vbox), makeButtons( globals ), FALSE, TRUE, 0);

    drawing_area = gtk_drawing_area_new();
    g_signal_connect(G_OBJECT(drawing_area), "draw", G_CALLBACK(on_draw_event), globals);

    globals->drawing_area = drawing_area;
    gtk_widget_show( drawing_area );

    width = GTK_HOR_SCORE_WIDTH + GTK_TIMER_WIDTH + GTK_TIMER_PAD;
    if ( params->verticalScore ) {
        width += GTK_VERT_SCORE_WIDTH;
    }
    height = 196;
    if ( params->nHidden == 0 ) {
        height += GTK_MIN_SCALE * GTK_TRAY_HT_ROWS;
    }

    gtk_widget_set_size_request( GTK_WIDGET(drawing_area), width, height );

    hbox = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 0 );
    gtk_box_pack_start( GTK_BOX(hbox), drawing_area, TRUE, TRUE, 0);

    /* install scrollbar even if not needed -- since zooming can make it
       needed later */
    GtkWidget* vscrollbar;
    gint nRows = cGlobals->gi->boardSize;
    globals->adjustment = (GtkAdjustment*)
        gtk_adjustment_new( 0, 0, nRows, 1, 2, 
                            nRows - params->nHidden );
    vscrollbar = gtk_scrollbar_new( GTK_ORIENTATION_VERTICAL, globals->adjustment );
    g_signal_connect( globals->adjustment, "value_changed",
                      G_CALLBACK(scroll_value_changed), globals );
    gtk_widget_show( vscrollbar );
    gtk_box_pack_start( GTK_BOX(hbox), vscrollbar, TRUE, TRUE, 0 );

    gtk_box_pack_start( GTK_BOX (hbox), 
                        makeVerticalBar( globals, window ), 
                        FALSE, TRUE, 0 );
    gtk_widget_show( hbox );

    gtk_box_pack_start( GTK_BOX(vbox), hbox/* drawing_area */, TRUE, TRUE, 0);

    g_signal_connect( drawing_area,"configure_event",
                      G_CALLBACK(configure_event), globals );
    g_signal_connect( drawing_area, "button_press_event",
                      G_CALLBACK(button_press_event), globals );
    g_signal_connect( drawing_area, "motion_notify_event",
                      G_CALLBACK(motion_notify_event), globals );
    g_signal_connect( drawing_area, "button_release_event",
                      G_CALLBACK(button_release_event), globals );

    setOneSecondTimer( cGlobals );

#ifdef KEY_SUPPORT
# ifdef KEYBOARD_NAV
    g_signal_connect( window, "key_press_event",
                      G_CALLBACK(key_press_event), globals );
# endif
    g_signal_connect( window, "key_release_event",
                      G_CALLBACK(key_release_event), globals );
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
} /* initGlobals */

void
freeGlobals( GtkGameGlobals* globals )
{
    cleanup( globals );
}

XP_Bool
loadGameNoDraw( GtkGameGlobals* globals, LaunchParams* params, 
                sqlite3_int64 rowid )
{
    LOG_FUNC();
    sqlite3* pDb = params->pDb;
    initGlobalsNoDraw( globals, params, NULL );

    TransportProcs procs;
    setTransportProcs( &procs, globals );

    CommonGlobals* cGlobals = &globals->cGlobals;
    cGlobals->selRow = rowid;
    cGlobals->pDb = pDb;
    XWStreamCtxt* stream = mem_stream_make( MPPARM(cGlobals->util->mpool) 
                                            params->vtMgr, cGlobals, 
                                            CHANNEL_NONE, NULL );
    XP_Bool loaded = loadGame( stream, cGlobals->pDb, rowid );
    if ( loaded ) {
        if ( NULL == cGlobals->dict ) {
            cGlobals->dict = makeDictForStream( cGlobals, stream );
        }
        loaded = game_makeFromStream( MEMPOOL stream, &cGlobals->game, 
                                      cGlobals->gi, cGlobals->dict,
                                      &cGlobals->dicts, cGlobals->util, 
                                      (DrawCtx*)NULL, &cGlobals->cp, &procs );
        if ( loaded ) {
            XP_LOGF( "%s: game loaded", __func__ );
#ifndef XWFEATURE_STANDALONE_ONLY
            if ( !!globals->cGlobals.game.comms ) {
                comms_resendAll( globals->cGlobals.game.comms, COMMS_CONN_NONE,
                                 XP_FALSE );
            }
#endif
        }
    }
    stream_destroy( stream );
    return loaded;
}

XP_Bool
makeNewGame( GtkGameGlobals* globals )
{
    CommonGlobals* cGlobals = &globals->cGlobals;
    if ( !!cGlobals->game.comms ) {
        comms_getAddr( cGlobals->game.comms, &cGlobals->addr );
    } else {
        LaunchParams* params = globals->cGlobals.params;
        const XP_UCHAR* relayName = params->connInfo.relay.relayName;
        if ( !relayName ) {
            relayName = RELAY_NAME_DEFAULT;
        }
        XP_U16 relayPort = params->connInfo.relay.defaultSendPort;
        if ( 0 == relayPort ) {
            relayPort = RELAY_PORT_DEFAULT;
        }
        comms_getInitialAddr( &cGlobals->addr, relayName, relayPort );
    }

    CurGameInfo* gi = cGlobals->gi;
    XP_Bool success = newGameDialog( globals, gi, &cGlobals->addr, 
                                     XP_TRUE, XP_FALSE );
    if ( success && !!gi->dictName && !cGlobals->dict ) {
        cGlobals->dict =
            linux_dictionary_make( MEMPOOL cGlobals->params,
                                   gi->dictName, XP_TRUE );
        gi->dictLang = dict_getLangCode( cGlobals->dict );
    }
    LOG_RETURNF( "%d", success );
    return success;
}

#endif /* PLATFORM_GTK */
