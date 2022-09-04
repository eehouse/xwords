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
#include "dbgutil.h"
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
#include "mqttcon.h"

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
                          const CommsAddrRec* addrs );

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
        redraw = board_handlePenDown( globals->cGlobals.game.board, NULL_XWE,
                                      event->x, event->y, &handled );
        if ( redraw ) {
            board_draw( globals->cGlobals.game.board, NULL_XWE );
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
        handled = board_handlePenMove( globals->cGlobals.game.board,
                                       NULL_XWE, event->x, event->y );
        if ( handled ) {
            board_draw( globals->cGlobals.game.board, NULL_XWE );
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
                                    NULL_XWE, event->x, event->y );
        if ( redraw ) {
            board_draw( globals->cGlobals.game.board, NULL_XWE );
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
            board_handleKeyRepeat( globals->cGlobals.game.board, NULL_XWE,
                                   xpkey, &handled )
            : board_handleKeyDown( globals->cGlobals.game.board,
                                   NULL_XWE, xpkey, &handled );
        if ( draw ) {
            board_draw( globals->cGlobals.game.board, NULL_XWE );
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
        draw = board_handleKeyUp( globals->cGlobals.game.board, NULL_XWE,
                                  xpkey, &handled );
#ifdef KEYBOARD_NAV
        if ( movesCursor && !handled ) {
            BoardObjectType order[] = { OBJ_SCORE, OBJ_BOARD, OBJ_TRAY };
            draw = linShiftFocus( &globals->cGlobals, xpkey, order,
                                  NULL ) || draw;
        }
#endif
        if ( draw ) {
            board_draw( globals->cGlobals.game.board, NULL_XWE );
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

#ifdef XWFEATURE_RELAY
static void
relay_status_gtk( XWEnv XP_UNUSED(xwe), void* closure, CommsRelayState state )
{
    XP_LOGF( "%s got status: %s", __func__, CommsRelayState2Str(state) );
    GtkGameGlobals* globals = (GtkGameGlobals*)closure;
    CommonGlobals* cGlobals = &globals->cGlobals;
    if ( !!cGlobals->draw ) {
        cGlobals->state = state;
        globals->stateChar = 'A' + COMMS_RELAYSTATE_ALLCONNECTED - state;
        draw_gtk_status( (GtkDrawCtx*)(void*)cGlobals->draw, globals->stateChar );
    }
}

static void
relay_connd_gtk( XWEnv XP_UNUSED(xwe), void* closure, XP_UCHAR* const room,
                 XP_Bool XP_UNUSED(reconnect), XP_U16 devOrder, 
                 XP_Bool allHere, XP_U16 nMissing )
{
    GtkGameGlobals* globals = (GtkGameGlobals*)closure;
    globals->cGlobals.nMissing = nMissing;
    XP_Bool skip = XP_FALSE;
    char buf[256];

    if ( allHere ) {
        /* disable for now. Seeing this too often */
        skip = XP_TRUE;
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
relay_error_gtk( XWEnv XP_UNUSED(xwe), void* closure, XWREASON relayErr )
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
relay_sendNoConn_gtk( XWEnv XP_UNUSED(xwe), const XP_U8* msg, XP_U16 len,
                      const XP_UCHAR* XP_UNUSED(msgNo),
                      const XP_UCHAR* relayID, void* closure )
{
    GtkGameGlobals* globals = (GtkGameGlobals*)closure;
    XP_Bool success = XP_FALSE;
    CommonGlobals* cGlobals = &globals->cGlobals;
    LaunchParams* params = cGlobals->params;
    if ( params->useUdp && !cGlobals->draw ) {
        XP_U16 seed = comms_getChannelSeed( cGlobals->game.comms );
        XP_U32 clientToken = makeClientToken( cGlobals->rowid, seed );
        XP_S16 nSent = relaycon_sendnoconn( params, msg, len, relayID, 
                                            clientToken );
        success = nSent == len;
    }
    return success;
} /* relay_sendNoConn_gtk */
#endif

#ifdef RELAY_VIA_HTTP
static void
onJoined( void* closure, const XP_UCHAR* connname, XWHostID hid )
{
    GtkGameGlobals* globals = (GtkGameGlobals*)closure;
    XWGame* game = &globals->cGlobals.game;
    CommsCtxt* comms = game->comms;
    comms_gameJoined( comms, connname, hid );
    if ( hid > 1 ) {
        globals->cGlobals.gi->serverRole = SERVER_ISCLIENT;
        server_reset( game->server, game->comms );
        tryConnectToServer( &globals->cGlobals );
    }
}

static void
relay_requestJoin_gtk( void* closure, const XP_UCHAR* devID, const XP_UCHAR* room,
                       XP_U16 nPlayersHere, XP_U16 nPlayersTotal,
                       XP_U16 seed, XP_U16 lang )
{
    GtkGameGlobals* globals = (GtkGameGlobals*)closure;
    LaunchParams* params = globals->cGlobals.params;
    relaycon_join( params, devID, room, nPlayersHere, nPlayersTotal, seed, lang,
                   onJoined, globals );
}
#endif

#ifdef COMMS_XPORT_FLAGSPROC
static XP_U32
gtk_getFlags( XWEnv XP_UNUSED(xwe), void* closure )
{
    GtkGameGlobals* globals = (GtkGameGlobals*)closure;
# ifdef RELAY_VIA_HTTP
    XP_USE( globals );
    return COMMS_XPORT_FLAGS_HASNOCONN;
# else
    return (!!globals->cGlobals.draw) ? COMMS_XPORT_FLAGS_NONE
        : COMMS_XPORT_FLAGS_HASNOCONN;
# endif
}
#endif

static void
countChanged_gtk( XWEnv XP_UNUSED(xwe), void* closure, XP_U16 newCount )
{
    GtkGameGlobals* globals = (GtkGameGlobals*)closure;
    gchar buf[128];
    snprintf( buf, VSIZE(buf), "pending count: %d", newCount );
    gtk_label_set_text( GTK_LABEL(globals->countLabel), buf );
}

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
# ifdef RELAY_VIA_HTTP
    procs->requestJoin = relay_requestJoin_gtk;
# endif
#endif
    procs->countChanged = countChanged_gtk;
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
formatSizeKey( gchar* key, sqlite3_int64 rowid )
{
    sprintf( key, KEY_WIN_LOC ":%llx", rowid );
}

static void
resizeFromRowid( GtkGameGlobals* globals )
{
    CommonGlobals* cGlobals = &globals->cGlobals;
    const sqlite3_int64 rowid = cGlobals->rowid;
    if ( 0 != rowid ) {
        // XP_LOGFF( "(rowid=%lld)", rowid );
        gchar key[128];
        formatSizeKey( key, rowid );
        resizeFromSaved( globals->window, cGlobals->params->pDb, key );
        globals->winSizeSet = TRUE;
    }
}

static void
saveSizeRowid( GtkGameGlobals* globals )
{
    if ( globals->winSizeSet ) {
        sqlite3_int64 rowid = globals->cGlobals.rowid;

        gchar key[128];
        formatSizeKey( key, rowid );
        // XP_LOGFF( "key: %s", key );
        saveSize( &globals->lastConfigure, globals->cGlobals.params->pDb, key );
    }
}

static void
createOrLoadObjects( GtkGameGlobals* globals )
{
#ifndef XWFEATURE_STANDALONE_ONLY
#endif
    CommonGlobals* cGlobals = &globals->cGlobals;
    LaunchParams* params = cGlobals->params;

    cGlobals->draw = gtkDrawCtxtMake( globals->drawing_area,
                                      globals );

    TransportProcs procs;
    setTransportProcs( &procs, globals );

    if ( linuxOpenGame( cGlobals, &procs ) ) {
        if ( !params->fileName && !!params->dbName ) {
            XP_UCHAR buf[64];
            snprintf( buf, sizeof(buf), "%s / %lld", params->dbName,
                      cGlobals->rowid );
            gtk_window_set_title( GTK_WINDOW(globals->window), buf );
        }


        addDropChecks( globals );
        disenable_buttons( globals );
    }
} /* createOrLoadObjects */

/* Create a new backing pixmap of the appropriate size and set up contxt to
 * draw using that size.
 */
static gboolean
on_drawing_configure( GtkWidget* widget, GdkEventConfigure* XP_UNUSED(event),
                      GtkGameGlobals* globals )
{
    globals->gridOn = XP_TRUE;
    CommonGlobals* cGlobals = &globals->cGlobals;
    if ( cGlobals->draw == NULL ) {
        createOrLoadObjects( globals );
    }

    BoardCtxt* board = cGlobals->game.board;

    GtkAllocation alloc;
    gtk_widget_get_allocation( widget, &alloc );
    short bdWidth = alloc.width - (GTK_RIGHT_MARGIN + GTK_BOARD_LEFT_MARGIN);
    short bdHeight = alloc.height - (GTK_TOP_MARGIN + GTK_BOTTOM_MARGIN)
        - GTK_MIN_TRAY_SCALEV - GTK_BOTTOM_MARGIN;

    XP_ASSERT( !cGlobals->params->verticalScore ); /* not supported */

    BoardDims dims;
    board_figureLayout( board, NULL_XWE, cGlobals->gi,
                        GTK_BOARD_LEFT, GTK_HOR_SCORE_TOP, bdWidth, bdHeight,
                        110, 150, 200, bdWidth-25, 16, 16, XP_FALSE, &dims );
    board_applyLayout( board, NULL_XWE, &dims );

    setCtrlsForTray( globals );
    board_invalAll( board );

    XP_Bool inOut[2];
    board_zoom( board, NULL_XWE, 0, inOut );
    setZoomButtons( globals, inOut );

    return FALSE;
} /* on_drawing_configure */

static gboolean
on_window_configure( GtkWidget* XP_UNUSED(widget), GdkEventConfigure* event,
                     GtkGameGlobals* globals )
{
    globals->lastConfigure = *event;
    // saveSizeRowid( globals );

    return FALSE;
}

void
destroy_board_window( GtkWidget* XP_UNUSED(widget), GtkGameGlobals* globals )
{
    LOG_FUNC();
    if ( !!globals->cGlobals.game.comms ) {
        comms_stop( globals->cGlobals.game.comms
#ifdef XWFEATURE_RELAY
                    , NULL_XWE
#endif
                    );
    }
    linuxSaveGame( &globals->cGlobals );
    saveSizeRowid( globals );
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
        XWStreamCtxt* stream = mem_stream_make_raw( MPPARM(cGlobals->util->mpool)
                                                    cGlobals->params->vtMgr );
        if ( gdb_loadInviteAddrs( stream, cGlobals->params->pDb,
                                  cGlobals->rowid ) ) {
            CommsAddrRec addr = {0};
            addrFromStream( &addr, stream );
            comms_augmentHostAddr( cGlobals->game.comms, NULL_XWE, &addr );

            XP_U16 nRecs = stream_getU8( stream );
            XP_LOGF( "%s: got invite info: %d records", __func__, nRecs );
            for ( int ii = 0; ii < nRecs; ++ii ) {
                XP_UCHAR relayID[32];
                stringFromStreamHere( stream, relayID, sizeof(relayID) );
                XP_LOGF( "%s: loaded relayID %s", __func__, relayID );

                CommsAddrRec addr = {0};
                addrFromStream( &addr, stream );

                send_invites( cGlobals, 1, &addr );
            }
        }
        stream_destroy( stream, NULL_XWE );
    }

    resizeFromRowid( globals );
} /* on_board_window_shown */

static void
cleanup( GtkGameGlobals* globals )
{
    CommonGlobals* cGlobals = &globals->cGlobals;
    linuxSaveGame( cGlobals );
    if ( 0 < cGlobals->idleID ) {
        g_source_remove( cGlobals->idleID );
    }

    cancelTimers( cGlobals );

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
    game_dispose( &cGlobals->game, NULL_XWE );
    gi_disposePlayerInfo( MEMPOOL cGlobals->gi );

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
tile_values_impl( GtkGameGlobals* globals, bool full )
{
    CommonGlobals* cGlobals = &globals->cGlobals;
    if ( !!cGlobals->game.server ) {
        XWStreamCtxt* stream = 
            mem_stream_make( MEMPOOL 
                             cGlobals->params->vtMgr,
                             globals, 
                             CHANNEL_NONE, 
                             catOnClose );
        server_formatDictCounts( cGlobals->game.server, NULL_XWE,
                                 stream, 5, full );
        stream_putU8( stream, '\n' );
        stream_destroy( stream, NULL_XWE );
    }
    
} /* tile_values */

static void
tile_values( GtkWidget* XP_UNUSED(widget), GtkGameGlobals* globals )
{
    tile_values_impl( globals, false );
}

static void
tile_values_full( GtkWidget* XP_UNUSED(widget), GtkGameGlobals* globals )
{
    tile_values_impl( globals, true );
}

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
        stream_destroy( stream, NULL_XWE );
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
        server_endGame( globals->cGlobals.game.server, NULL_XWE );
        gameOver = TRUE;
    }

    /* the end game listener will take care of printing the final scores */
} /* final_scores */

static XP_Bool
new_game_impl( GtkGameGlobals* globals, XP_Bool fireConnDlg )
{
    XP_Bool success = XP_FALSE;
    CommonGlobals* cGlobals = &globals->cGlobals;

    CurGameInfo* gi = cGlobals->gi;
    CommsAddrRec addr;
    success = gtkNewGameDialog( globals, gi, &addr, XP_TRUE, fireConnDlg );
    if ( success ) {
#ifndef XWFEATURE_STANDALONE_ONLY
        XP_Bool isClient = gi->serverRole == SERVER_ISCLIENT;
        XP_ASSERT( !isClient ); /* Doesn't make sense! Send invitation. */
#endif
        TransportProcs procs = {
            .closure = globals,
            .send = LINUX_SEND,
#ifdef COMMS_HEARTBEAT
            .reset = linux_reset,
#endif
        };

        (void)game_reset( MEMPOOL &cGlobals->game, NULL_XWE, gi,
                          &cGlobals->selfAddr, NULL, cGlobals->util,
                          &cGlobals->cp, &procs );

        (void)server_do( cGlobals->game.server, NULL_XWE ); /* assign tiles, etc. */
        board_invalAll( cGlobals->game.board );
        board_draw( cGlobals->game.board, NULL_XWE );
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
    if ( gtkNewGameDialog( globals, gi, &addr, XP_FALSE, XP_FALSE ) ) {
        if ( server_do( globals->cGlobals.game.server, NULL_XWE ) ) {
            board_draw( globals->cGlobals.game.board, NULL_XWE );
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
			linux_dictionary_make( MPPARM(cGlobals->util->mpool) NULL_XWE,
                                   params, name, params->useMmap );
		game_changeDict( MPPARM(cGlobals->util->mpool) &cGlobals->game, NULL_XWE,
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
        board_draw( board, NULL_XWE );
    }
}

#ifndef XWFEATURE_STANDALONE_ONLY
static void
handle_resend( GtkWidget* XP_UNUSED(widget), GtkGameGlobals* globals )
{
    CommsCtxt* comms = globals->cGlobals.game.comms;
    if ( comms != NULL ) {
        comms_resendAll( comms, NULL_XWE, COMMS_CONN_NONE, XP_TRUE );
    }
} /* handle_resend */

#ifdef XWFEATURE_COMMSACK
static void
handle_ack( GtkWidget* XP_UNUSED(widget), GtkGameGlobals* globals )
{
    CommsCtxt* comms = globals->cGlobals.game.comms;
    if ( comms != NULL ) {
        comms_ackAny( comms, NULL_XWE );
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
        stream_destroy( stream, NULL_XWE );
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
    stream_destroy( stream, NULL_XWE );
    
} /* handle_memstats */

#endif

#ifdef XWFEATURE_ACTIVERECT
static gint
inval_board_ontimer( gpointer data )
{
    GtkGameGlobals* globals = (GtkGameGlobals*)data;
    BoardCtxt* board = globals->cGlobals.game.board;
    board_draw( board, NULL_XWE );
    return XP_FALSE;
} /* inval_board_ontimer */

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

GtkWidget*
createAddItem( GtkWidget* parent, gchar* label, 
               GCallback handlerFunc, gpointer closure )
{
    GtkWidget* item = gtk_menu_item_new_with_label( label );

    if ( handlerFunc != NULL ) {
        g_signal_connect( item, "activate", G_CALLBACK(handlerFunc),
                          closure );
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
    (void)createAddItem( fileMenu, "Tile values full",
                         (GCallback)tile_values_full, globals );
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
    if ( !globals->invite_button && 0 < nPending && !!globals->buttons_hbox ) {
        globals->invite_button = 
            addButton( globals->buttons_hbox, "Invite",
                       G_CALLBACK(handle_invite_button), globals );
    }

    GameStateInfo gsi;
    game_getState( &globals->cGlobals.game, NULL_XWE, &gsi );

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

    gtk_widget_set_sensitive( globals->pause_button, gsi.canPause );
    gtk_widget_set_sensitive( globals->unpause_button, gsi.canUnpause );
}

static gboolean
handle_flip_button( GtkWidget* XP_UNUSED(widget), gpointer _globals )
{
    GtkGameGlobals* globals = (GtkGameGlobals*)_globals;
    if ( board_flip( globals->cGlobals.game.board ) ) {
        board_draw( globals->cGlobals.game.board, NULL_XWE );
    }
    return TRUE;
} /* handle_flip_button */

static gboolean
handle_value_button( GtkWidget* XP_UNUSED(widget), gpointer closure )
{
    GtkGameGlobals* globals = (GtkGameGlobals*)closure;
    CommonGlobals* cGlobals = &globals->cGlobals;
    cGlobals->cp.tvType = (cGlobals->cp.tvType + 1) % TVT_N_ENTRIES;
    board_prefsChanged( cGlobals->game.board, &cGlobals->cp );
    board_draw( cGlobals->game.board, NULL_XWE );
    return TRUE;
} /* handle_value_button */

static void
handle_hint_button( GtkGameGlobals* globals, XP_Bool prev )
{
    XP_Bool redo;
    if ( board_requestHint( globals->cGlobals.game.board, NULL_XWE,
#ifdef XWFEATURE_SEARCHLIMIT
                            XP_FALSE,
#endif
                            prev, &redo ) ) {
        board_draw( globals->cGlobals.game.board, NULL_XWE );
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
    if ( board_requestHint( globals->cGlobals.game.board, NULL_XWE,
#ifdef XWFEATURE_SEARCHLIMIT
                            XP_TRUE, 
#endif
                            XP_FALSE, &redo ) ) {
        board_draw( globals->cGlobals.game.board, NULL_XWE );
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
    if ( board_juggleTray( globals->cGlobals.game.board, NULL_XWE ) ) {
        board_draw( globals->cGlobals.game.board, NULL_XWE );
    }
} /* handle_juggle_button */

static void
handle_undo_button( GtkWidget* XP_UNUSED(widget), GtkGameGlobals* globals )
{
    if ( server_handleUndo( globals->cGlobals.game.server, NULL_XWE, 0 ) ) {
        board_draw( globals->cGlobals.game.board, NULL_XWE );
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
    if ( board_redoReplacedTiles( board, NULL_XWE )
         || board_replaceTiles( board, NULL_XWE ) ) {
        board_draw( board, NULL_XWE );
    }
}

static void
handle_trade_button( GtkWidget* XP_UNUSED(widget), GtkGameGlobals* globals )
{
    if ( board_beginTrade( globals->cGlobals.game.board, NULL_XWE ) ) {
        board_draw( globals->cGlobals.game.board, NULL_XWE );
        disenable_buttons( globals );
    }
} /* handle_juggle_button */

static void
handle_done_button( GtkWidget* XP_UNUSED(widget), GtkGameGlobals* globals )
{
    if ( board_commitTurn( globals->cGlobals.game.board, NULL_XWE, XP_FALSE,
                           XP_FALSE, NULL ) ) {
        board_draw( globals->cGlobals.game.board, NULL_XWE );
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
    if ( board_zoom( globals->cGlobals.game.board, NULL_XWE, 1, inOut ) ) {
        board_draw( globals->cGlobals.game.board, NULL_XWE );
        setZoomButtons( globals, inOut );
    }
} /* handle_zoomin_button */

static void
handle_zoomout_button( GtkWidget* XP_UNUSED(widget), GtkGameGlobals* globals )
{
    XP_Bool inOut[2];
    if ( board_zoom( globals->cGlobals.game.board, NULL_XWE, -1, inOut ) ) {
        board_draw( globals->cGlobals.game.board, NULL_XWE );
        setZoomButtons( globals, inOut );
    }
} /* handle_zoomout_button */

#ifdef XWFEATURE_CHAT
static void
handle_chat_button( GtkWidget* XP_UNUSED(widget), GtkGameGlobals* globals )
{
    gchar* msg = gtkGetChatMessage( globals );
    if ( NULL != msg ) {
        board_sendChat( globals->cGlobals.game.board, NULL_XWE, msg );
        g_free( msg );
    }
}
#endif

static void
handle_pause_button( GtkWidget* XP_UNUSED(widget), GtkGameGlobals* globals )
{
    board_pause( globals->cGlobals.game.board, NULL_XWE, "whatever" );
    disenable_buttons( globals );
}

static void
handle_unpause_button( GtkWidget* XP_UNUSED(widget), GtkGameGlobals* globals )
{
    board_unpause( globals->cGlobals.game.board, NULL_XWE, "whatever" );
    disenable_buttons( globals );
}

static void
scroll_value_changed( GtkAdjustment *adj, GtkGameGlobals* globals )
{
    XP_U16 newValue;
    gfloat newValueF = gtk_adjustment_get_value( adj );

    /* XP_ASSERT( newValueF >= 0.0 */
    /*            && newValueF <= globals->cGlobals.params->nHidden ); */
    newValue = (XP_U16)newValueF;

    if ( board_setYOffset( globals->cGlobals.game.board, NULL_XWE, newValue ) ) {
        board_draw( globals->cGlobals.game.board, NULL_XWE );
    }
} /* scroll_value_changed */

static void
handle_grid_button( GtkWidget* XP_UNUSED(widget), GtkGameGlobals* globals )
{
    globals->gridOn = !globals->gridOn;

    board_invalAll( globals->cGlobals.game.board );
    board_draw( globals->cGlobals.game.board, NULL_XWE );
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
        draw = board_hideTray( board, NULL_XWE );
    } else {
        draw = board_showTray( board, NULL_XWE );
    }
    if ( draw ) {
        board_draw( board, NULL_XWE );
    }
} /* handle_hide_button */

static void
handle_commit_button( GtkWidget* XP_UNUSED(widget), GtkGameGlobals* globals )
{
    if ( board_commitTurn( globals->cGlobals.game.board, NULL_XWE,
                           XP_FALSE, XP_FALSE, NULL ) ) {
        board_draw( globals->cGlobals.game.board, NULL_XWE );
    }
} /* handle_commit_button */

static void
handle_invite_button( GtkWidget* XP_UNUSED(widget), GtkGameGlobals* globals )
{
    CommonGlobals* cGlobals = &globals->cGlobals;
    XP_U16 nMissing = server_getPendingRegs( globals->cGlobals.game.server );

    CommsAddrRec inviteAddr = {0};
    gint nPlayers = nMissing;
    XP_Bool confirmed = gtkInviteDlg( globals, &inviteAddr, &nPlayers );
    XP_LOGFF( "gtkInviteDlg() => %s", boolToStr(confirmed) );

    if ( confirmed ) {
        send_invites( cGlobals, nPlayers, &inviteAddr );
    }
} /* handle_invite_button */

static void
send_invites( CommonGlobals* cGlobals, XP_U16 nPlayers,
              const CommsAddrRec* destAddr )
{
    CommsAddrRec myAddr = {0};
    CommsCtxt* comms = cGlobals->game.comms;
    XP_ASSERT( comms );
    comms_getAddr( comms, &myAddr );

    gint forceChannel = 1;  /* 1 is what Android does. Limits to two-device games */

    NetLaunchInfo nli = {0};    /* include everything!!! */
    nli_init( &nli, cGlobals->gi, &myAddr, nPlayers, forceChannel );
    if ( addr_hasType( &myAddr, COMMS_CONN_RELAY ) ) {
        XP_UCHAR buf[32];
        snprintf( buf, sizeof(buf), "%X", makeRandomInt() );
        nli_setInviteID( &nli, buf ); /* PENDING: should not be relay only!!! */
    }
    // nli_setDevID( &nli, linux_getDevIDRelay( cGlobals->params ) );

    if ( addr_hasType( &myAddr, COMMS_CONN_MQTT ) ) {
        const MQTTDevID* devid = mqttc_getDevID( cGlobals->params );
        nli_setMQTTDevID( &nli, devid );
    }

#ifdef DEBUG
    {
        XWStreamCtxt* stream = mem_stream_make_raw( MPPARM(cGlobals->util->mpool)
                                                    cGlobals->params->vtMgr );
        nli_saveToStream( &nli, stream );
        NetLaunchInfo tmp;
        nli_makeFromStream( &tmp, stream );
        stream_destroy( stream, NULL_XWE );
        XP_ASSERT( 0 == memcmp( &nli, &tmp, sizeof(nli) ) );
    }
#endif

    if ( !!destAddr && '\0' != destAddr->u.sms.phone[0] && 0 < destAddr->u.sms.port ) {
        gchar gameName[64];
        snprintf( gameName, VSIZE(gameName), "Game %d", cGlobals->gi->gameID );

        linux_sms_invite( cGlobals->params, &nli,
                          destAddr->u.sms.phone, destAddr->u.sms.port );
    }
#ifdef XWFEATURE_RELAY
    if ( 0 != relayDevID || !!relayID ) {
        XP_ASSERT( 0 != relayDevID || (!!relayID && !!relayID[0]) );
        relaycon_invite( cGlobals->params, relayDevID, relayID, &nli );
    }
#endif

    if ( addr_hasType( destAddr, COMMS_CONN_MQTT ) ) {
        mqttc_invite( cGlobals->params, &nli, &destAddr->u.mqtt.devID );
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

static gint
ask_blank( gpointer data )
{
    GtkGameGlobals* globals = (GtkGameGlobals*)data;
    CommonGlobals* cGlobals = &globals->cGlobals;

    XP_UCHAR* name = globals->cGlobals.gi->players[cGlobals->selPlayer].name;
    XP_S16 result = gtkletterask( NULL, XP_FALSE, name, 1,
                                  cGlobals->nTiles, cGlobals->tiles, NULL );

    for ( int ii = 0; ii < cGlobals->nTiles; ++ii ) {
        g_free( (gpointer)cGlobals->tiles[ii] );
    }

    if ( result >= 0
         && board_setBlankValue( cGlobals->game.board, cGlobals->selPlayer,
                                 cGlobals->blankCol, cGlobals->blankRow,
                                 result ) ) {
        board_draw( cGlobals->game.board, NULL_XWE );
    }

    return 0;
}

static void
gtk_util_notifyPickTileBlank( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe),
                              XP_U16 playerNum, XP_U16 col,
                              XP_U16 row, const XP_UCHAR** texts, XP_U16 nTiles )
{
    GtkGameGlobals* globals = (GtkGameGlobals*)uc->closure;
    CommonGlobals* cGlobals = &globals->cGlobals;
    cGlobals->selPlayer = playerNum;
    cGlobals->blankCol = col;
    cGlobals->blankRow = row;
    cGlobals->nTiles = nTiles;
    for ( int ii = 0; ii < nTiles; ++ii ) {
        cGlobals->tiles[ii] = g_strdup( texts[ii] );
    }

    (void)g_idle_add( ask_blank, globals );
}

static gint
ask_tiles( gpointer data )
{
    GtkGameGlobals* globals = (GtkGameGlobals*)data;
    CommonGlobals* cGlobals = &globals->cGlobals;

    TrayTileSet newTiles = {0};
    XP_UCHAR* name = cGlobals->gi->players[cGlobals->selPlayer].name;
    for ( XP_Bool done = XP_FALSE; !done; ) {
        XP_S16 picked = gtkletterask( &newTiles, XP_TRUE, name,
                                      cGlobals->nToPick, cGlobals->nTiles,
                                      cGlobals->tiles, cGlobals->tileCounts );
        switch ( picked ) {
        case PICKER_PICKALL:
            done = XP_TRUE;
            break;
        case PICKER_BACKUP:
            if ( newTiles.nTiles > 0 ) {
                Tile backed = newTiles.tiles[--newTiles.nTiles];
                ++cGlobals->tileCounts[backed];
            }
            break;
        default:
            XP_ASSERT( picked >= 0 && picked < cGlobals->nTiles );
            --cGlobals->tileCounts[picked];
            newTiles.tiles[newTiles.nTiles++] = picked;
            done = newTiles.nTiles == cGlobals->nToPick;
            break;
        }
    }

    for ( int ii = 0; ii < cGlobals->nTiles; ++ii ) {
        g_free( (gpointer)cGlobals->tiles[ii] );
    }

    BoardCtxt* board = cGlobals->game.board;
    XP_Bool draw = XP_TRUE;
    if ( cGlobals->pickIsInitial ) {
        server_tilesPicked( cGlobals->game.server, NULL_XWE,
                            cGlobals->selPlayer, &newTiles );
    } else {
        draw = board_commitTurn( cGlobals->game.board, NULL_XWE,
                                 XP_TRUE, XP_TRUE, &newTiles );
    }

    if ( draw ) {
        board_draw( board, NULL_XWE );
    }

    return 0;
}

static void
gtk_util_informNeedPickTiles( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe),
                              XP_Bool isInitial, XP_U16 player, XP_U16 nToPick,
                              XP_U16 nFaces, const XP_UCHAR** faces,
                              const XP_U16* counts )
{
    GtkGameGlobals* globals = (GtkGameGlobals*)uc->closure;
    CommonGlobals* cGlobals = &globals->cGlobals;
    cGlobals->selPlayer = player;
    cGlobals->pickIsInitial = isInitial;

    cGlobals->nToPick = nToPick;
    cGlobals->nTiles = nFaces;
    for ( int ii = 0; ii < nFaces; ++ii ) {
        cGlobals->tiles[ii] = g_strdup( faces[ii] );
        cGlobals->tileCounts[ii] = counts[ii];
    }

    (void)g_idle_add( ask_tiles, globals );
} /* gtk_util_informNeedPickTiles */

static gint
ask_password( gpointer data )
{
    GtkGameGlobals* globals = (GtkGameGlobals*)data;
    CommonGlobals* cGlobals = &globals->cGlobals;
    XP_UCHAR buf[32];
    XP_U16 len = VSIZE(buf);
    if ( gtkpasswdask( cGlobals->askPassName, buf, &len ) ) {
        BoardCtxt* board = cGlobals->game.board;
        if ( board_passwordProvided( board, NULL_XWE, cGlobals->selPlayer, buf ) ) {
            board_draw( board, NULL_XWE );
        }
    }
    return 0;
}

static void
gtk_util_informNeedPassword( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe),
                             XP_U16 player, const XP_UCHAR* name )
{
    GtkGameGlobals* globals = (GtkGameGlobals*)uc->closure;
    CommonGlobals* cGlobals = &globals->cGlobals;
    cGlobals->askPassName = name;
    cGlobals->selPlayer = player;

    (void)g_idle_add( ask_password, globals );
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
gtk_util_trayHiddenChange( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe),
                           XW_TrayVisState XP_UNUSED(state),
                           XP_U16 XP_UNUSED(nVisibleRows) )
{
    GtkGameGlobals* globals = (GtkGameGlobals*)uc->closure;
    setCtrlsForTray( globals );
} /* gtk_util_trayHiddenChange */

static void
gtk_util_yOffsetChange( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe), XP_U16 maxOffset,
                        XP_U16 XP_UNUSED(oldOffset), 
                        XP_U16 newOffset )
{
    GtkGameGlobals* globals = (GtkGameGlobals*)uc->closure;
    /* adjustment is invalid when gtk's shutting down; ignore */
    if ( !!globals->adjustment && GTK_IS_ADJUSTMENT(globals->adjustment) ) {
        gint nRows = globals->cGlobals.gi->boardSize;
        gtk_adjustment_set_page_size(globals->adjustment, nRows - maxOffset);
        gtk_adjustment_set_value(globals->adjustment, newOffset);
        // gtk_adjustment_value_changed( globals->adjustment );
    }
} /* gtk_util_yOffsetChange */

#ifdef XWFEATURE_TURNCHANGENOTIFY
static void
gtk_util_turnChanged( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe), XP_S16 XP_UNUSED(newTurn) )
{
    GtkGameGlobals* globals = (GtkGameGlobals*)uc->closure;
    linuxSaveGame( &globals->cGlobals );
}
#endif

static void
gtkShowFinalScores( const GtkGameGlobals* globals, XP_Bool ignoreTimeout )
{
    XWStreamCtxt* stream;
    XP_UCHAR* text;
    const CommonGlobals* cGlobals = &globals->cGlobals;

    stream = mem_stream_make_raw( MPPARM(cGlobals->util->mpool)
                                  cGlobals->params->vtMgr );
    server_writeFinalScores( cGlobals->game.server, NULL_XWE, stream );

    text = strFromStream( stream );
    stream_destroy( stream, NULL_XWE );

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
gtk_util_notifyDupStatus( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe), XP_Bool XP_UNUSED(amHost),
                          const XP_UCHAR* msg )
{
    LOG_FUNC();
    GtkGameGlobals* globals = (GtkGameGlobals*)uc->closure;
    XP_UCHAR buf[256];
    XP_SNPRINTF( buf, VSIZE(buf), "notifyDupStatus(): msg: %s", msg );
    (void)gtkask( globals->window, buf, GTK_BUTTONS_OK, NULL );
}

static void
gtk_util_informMove( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe), XP_S16 XP_UNUSED(turn),
                     XWStreamCtxt* expl, XWStreamCtxt* words )
{
    GtkGameGlobals* globals = (GtkGameGlobals*)uc->closure;
    char* explStr = strFromStream( expl );
    gchar* msg = g_strdup_printf( "informMove():\nexpl: %s", explStr );
    if ( NULL != words ) {
        char* wordsStr = strFromStream( words );
        gchar* prev = msg;
        gchar* postfix = g_strdup_printf( "words: %s", wordsStr );
        free( wordsStr );
        msg = g_strconcat( msg, postfix, NULL );
        g_free( prev );
        g_free( postfix );
    }
    (void)gtkask( globals->window, msg, GTK_BUTTONS_OK, NULL );
    free( explStr );
    g_free( msg );
}

static void
gtk_util_informUndo( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe) )
{
    GtkGameGlobals* globals = (GtkGameGlobals*)uc->closure;
    (void)gtkask_timeout( globals->window, "Remote player undid a move",
                          GTK_BUTTONS_OK, NULL, 500 );
}

static void
gtk_util_notifyGameOver( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe), XP_S16 quitter )
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
        server_handleUndo( cGlobals->game.server, NULL_XWE, 0 );
        board_draw( cGlobals->game.board, NULL_XWE );
    } else if ( !cGlobals->params->skipGameOver ) {
        gtkShowFinalScores( globals, XP_TRUE );
    }
} /* gtk_util_notifyGameOver */

static void
gtk_util_informNetDict( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe),
                        const XP_UCHAR* XP_UNUSED(isoCode),
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

/* define this to prevent user events during debugging from stopping the engine */
/* #define DONT_ABORT_ENGINE */

#ifdef XWFEATURE_HILITECELL
static XP_Bool
gtk_util_hiliteCell( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe), XP_U16 col, XP_U16 row )
{
    GtkGameGlobals* globals = (GtkGameGlobals*)uc->closure;
#ifndef DONT_ABORT_ENGINE
    gboolean pending;
#endif

    board_hiliteCellAt( globals->cGlobals.game.board, NULL_XWE, col, row );
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
gtk_util_altKeyDown( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe) )
{
    GtkGameGlobals* globals = (GtkGameGlobals*)uc->closure;
    return globals->altKeyDown;
}

static XP_Bool
gtk_util_engineProgressCallback( XW_UtilCtxt* XP_UNUSED(uc), XWEnv XP_UNUSED(xwe) )
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
ask_bad_words( gpointer data )
{
    GtkGameGlobals* globals = (GtkGameGlobals*)data;
    CommonGlobals* cGlobals = &globals->cGlobals;

    if ( GTK_RESPONSE_YES == gtkask( globals->window, cGlobals->question,
                                     GTK_BUTTONS_YES_NO, NULL ) ) {
        board_commitTurn( cGlobals->game.board, NULL_XWE, XP_TRUE, XP_FALSE, NULL );
    }
    return 0;
}

static void
gtk_util_notifyIllegalWords( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe),
                             BadWordInfo* bwi, XP_U16 player,
                             XP_Bool turnLost )
{
    GtkGameGlobals* globals = (GtkGameGlobals*)uc->closure;
    CommonGlobals* cGlobals = &globals->cGlobals;
    char buf[300];
    gchar* strs = g_strjoinv( "\", \"", (gchar**)bwi->words );

    if ( turnLost ) {
        XP_UCHAR* name = cGlobals->gi->players[player].name;
        XP_ASSERT( !!name );

        sprintf( buf, "Player %d (%s) played illegal word[s] \"%s\"; loses turn.",
                 player+1, name, strs );

        if ( cGlobals->params->skipWarnings ) {
            XP_LOGF( "%s", buf );
        }  else {
            gtkUserError( globals, buf );
        }
    } else {
        sprintf( cGlobals->question, "Word[s] \"%s\" not in the current dictionary (%s). "
                 "Use anyway?", strs, bwi->dictName );

        (void)g_idle_add( ask_bad_words, globals );
    }
    g_free( strs );
} /* gtk_util_notifyIllegalWords */

static void
gtk_util_remSelected( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe) )
{
    GtkGameGlobals* globals = (GtkGameGlobals*)uc->closure;
    XWStreamCtxt* stream;
    XP_UCHAR* text;

    stream = mem_stream_make_raw( MEMPOOL
                                  globals->cGlobals.params->vtMgr );
    board_formatRemainingTiles( globals->cGlobals.game.board, NULL_XWE, stream );
    text = strFromStream( stream );
    stream_destroy( stream, NULL_XWE );

    (void)gtkask( globals->window, text, GTK_BUTTONS_OK, NULL );
    free( text );
}

static void
gtk_util_timerSelected( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe), XP_Bool inDuplicateMode,
                        XP_Bool canPause )
{
    if ( inDuplicateMode ) {
        GtkGameGlobals* globals = (GtkGameGlobals*)uc->closure;
        if ( canPause ) {
            handle_pause_button( NULL, globals );
        } else {
            handle_unpause_button( NULL, globals );
        }
    }
}

static void
gtk_util_getMQTTIDsFor( XW_UtilCtxt* uc, XWEnv xwe, XP_U16 nRelayIDs,
                        const XP_UCHAR* relayIDs[] )
{
    XP_ASSERT(0);               /* implement me */
    XP_USE( uc );
    XP_USE( xwe );
    XP_USE( nRelayIDs );
    XP_USE( relayIDs );
}

#ifndef XWFEATURE_STANDALONE_ONLY
static XWStreamCtxt* 
gtk_util_makeStreamFromAddr(XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe), XP_PlayerAddr channelNo )
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
gtk_util_showChat( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe),
                   const XP_UCHAR* const msg, XP_S16 from,
                   XP_U32 tsSecs )
{
    GtkGameGlobals* globals = (GtkGameGlobals*)uc->closure;
    XP_UCHAR buf[1024];
    XP_UCHAR* name = "<unknown>";
    if ( 0 <= from ) {
        name = globals->cGlobals.gi->players[from].name;
    }

    GDateTime* dt = g_date_time_new_from_unix_utc( tsSecs );
    gchar* tsStr = g_date_time_format( dt, "%T" );
    XP_SNPRINTF( buf, VSIZE(buf), "Quoth %s at %s: \"%s\"", name, tsStr, msg );
    g_free( tsStr );
    g_date_time_unref (dt);

    (void)gtkask( globals->window, buf, GTK_BUTTONS_OK, NULL );
}
#endif
#endif

#ifdef XWFEATURE_SEARCHLIMIT
static XP_Bool 
gtk_util_getTraySearchLimits( XW_UtilCtxt* XP_UNUSED(uc), XWEnv XP_UNUSED(xwe),
                              XP_U16* XP_UNUSED(min), XP_U16* max )
{
    *max = askNTiles( MAX_TRAY_TILES, *max );
    return XP_TRUE;
}
#endif

#ifndef XWFEATURE_MINIWIN
static void
gtk_util_bonusSquareHeld( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe), XWBonusType bonus )
{
    GtkGameGlobals* globals = (GtkGameGlobals*)uc->closure;
    gchar* msg = g_strdup_printf( "bonusSquareHeld(bonus=%d)", bonus );
    gtkask_timeout( globals->window, msg, GTK_BUTTONS_OK, NULL, 1000 );
    g_free( msg );
}

static void
gtk_util_playerScoreHeld( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe), XP_U16 player )
{
    LOG_FUNC();

    GtkGameGlobals* globals = (GtkGameGlobals*)uc->closure;

    LastMoveInfo lmi;
    if ( model_getPlayersLastScore( globals->cGlobals.game.model,
                                    NULL_XWE, player, &lmi ) ) {
        XP_UCHAR buf[128];
        formatLMI( &lmi, buf, VSIZE(buf) );
        (void)gtkask( globals->window, buf, GTK_BUTTONS_OK, NULL );
    }
}
#endif

#ifdef XWFEATURE_BOARDWORDS
static void
gtk_util_cellSquareHeld( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe), XWStreamCtxt* words )
{
    GtkGameGlobals* globals = (GtkGameGlobals*)uc->closure;
    const XP_U8* bytes = stream_getPtr( words );
    gchar* msg = g_strdup_printf( "words for lookup:\n%s",
                                  (XP_UCHAR*)bytes );
    gtktell( globals->window, msg );
    g_free( msg );
}
#endif

static void
gtk_util_informWordsBlocked( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe), XP_U16 nBadWords,
                             XWStreamCtxt* words, const XP_UCHAR* dict )
{
    XP_U16 len = stream_getSize( words );
    XP_UCHAR buf[len];
    stream_getBytes( words, buf, len );
    buf[len-1] = '\0';          /* overwrite \n */
    GtkGameGlobals* globals = (GtkGameGlobals*)uc->closure;
    gchar* msg = g_strdup_printf( "%d word[s] not found in %s:\n%s", nBadWords, dict, buf );
    gtkUserError( globals, msg );
    g_free( msg );
}

static void
gtk_util_userError( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe), UtilErrID id )
{
    GtkGameGlobals* globals = (GtkGameGlobals*)uc->closure;
    XP_Bool silent;
    const XP_UCHAR* message = linux_getErrString( id, &silent );

    XP_LOGF( "%s: %s", __func__, message );
    if ( !silent ) {
        gtkUserError( globals, message );
    }
} /* gtk_util_userError */

static gint
ask_move( gpointer data )
{
    GtkGameGlobals* globals = (GtkGameGlobals*)data;
    CommonGlobals* cGlobals = &globals->cGlobals;
    GtkButtonsType buttons = GTK_BUTTONS_YES_NO;
    gint chosen = gtkask( globals->window, cGlobals->question, buttons, NULL );
    if ( GTK_RESPONSE_OK == chosen || chosen == GTK_RESPONSE_YES ) {
        BoardCtxt* board = cGlobals->game.board;
        if ( board_commitTurn( board, NULL_XWE, XP_TRUE, XP_TRUE, NULL ) ) {
            board_draw( board, NULL_XWE );
        }
    }
    return 0;
}

static void
gtk_util_notifyMove( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe), XWStreamCtxt* stream )
{
    GtkGameGlobals* globals = (GtkGameGlobals*)uc->closure;
    CommonGlobals* cGlobals = &globals->cGlobals;
    /* char* question; */
    /* XP_Bool freeMe = XP_FALSE; */

    XP_U16 len = stream_getSize( stream );
    XP_ASSERT( len <= VSIZE(cGlobals->question) );
    stream_getBytes( stream, cGlobals->question, len );
    cGlobals->question[len] = '\0';
    (void)g_idle_add( ask_move, globals );
} /* gtk_util_userQuery */

static gint
ask_trade( gpointer data )
{
    GtkGameGlobals* globals = (GtkGameGlobals*)data;
    CommonGlobals* cGlobals = &globals->cGlobals;

    if ( GTK_RESPONSE_YES == gtkask( globals->window,
                                     cGlobals->question, 
                                     GTK_BUTTONS_YES_NO, NULL ) ) {
        BoardCtxt* board = cGlobals->game.board;
        if ( board_commitTurn( board, NULL_XWE, XP_TRUE, XP_TRUE, NULL ) ) {
            board_draw( board, NULL_XWE );
        }
    }
    return 0;
}

static void
gtk_util_notifyTrade( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe),
                      const XP_UCHAR** tiles, XP_U16 nTiles )
{
    GtkGameGlobals* globals = (GtkGameGlobals*)uc->closure;
    formatConfirmTrade( &globals->cGlobals, tiles, nTiles );

    (void)g_idle_add( ask_trade, globals );
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
addVBarButton( GtkGameGlobals* globals, const gchar* icon, const gchar* title,
               GCallback func, GtkWidget* vbox )
{
    GtkWidget* button = makeShowButtonFromBitmap( globals, icon, title,
                                                  G_CALLBACK(func) );
    gtk_box_pack_start( GTK_BOX(vbox), button, FALSE, TRUE, 0 );
    return button;
}

static GtkWidget* 
makeVerticalBar( GtkGameGlobals* globals, GtkWidget* XP_UNUSED(window) )
{
    GtkWidget* vbox = gtk_button_box_new( GTK_ORIENTATION_VERTICAL );

    globals->flip_button = addVBarButton( globals, "../flip.xpm", "f",
                                          G_CALLBACK(handle_flip_button),
                                          vbox );

    addVBarButton( globals, "../value.xpm", "v",
                   G_CALLBACK(handle_value_button), vbox );
    globals->prevhint_button
        = addVBarButton( globals, "../hint.xpm", "?-", G_CALLBACK(handle_prevhint_button), vbox );
    globals->nexthint_button
        = addVBarButton( globals, "../hint.xpm", "?+", G_CALLBACK(handle_nexthint_button), vbox );
    addVBarButton( globals, "../hintNum.xpm", "n", G_CALLBACK(handle_nhint_button), vbox );

    addVBarButton( globals, "../colors.xpm", "c", G_CALLBACK(handle_colors_button), vbox );

    /* undo and redo buttons */
    addVBarButton( globals, "../undo.xpm", "U", G_CALLBACK(handle_undo_button), vbox );

    addVBarButton( globals, "../redo.xpm", "R", G_CALLBACK(handle_redo_button), vbox );

    globals->toggle_undo_button
        = addVBarButton( globals, "", "u/r", G_CALLBACK(handle_toggle_undo), vbox );

    /* the four buttons that on palm are beside the tray */
    addVBarButton( globals, "../juggle.xpm", "j", G_CALLBACK(handle_juggle_button), vbox );

    addVBarButton( globals, "../trade.xpm", "t", G_CALLBACK(handle_trade_button), vbox );

    addVBarButton( globals, "../done.xpm", "d", G_CALLBACK(handle_done_button), vbox );

    globals->zoomin_button
        = addVBarButton( globals, "../done.xpm", "+", G_CALLBACK(handle_zoomin_button), vbox );

    globals->zoomout_button
        = addVBarButton( globals, "../done.xpm", "-", G_CALLBACK(handle_zoomout_button), vbox );

#ifdef XWFEATURE_CHAT
    globals->chat_button = addVBarButton( globals, "", "Chat",
                                          G_CALLBACK(handle_chat_button), vbox );
#endif

    globals->pause_button = addVBarButton( globals, "", "Pause",
                                           G_CALLBACK(handle_pause_button), vbox );
    globals->unpause_button = addVBarButton( globals, "", "Unpause",
                                             G_CALLBACK(handle_unpause_button), vbox );

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
    util->closure = globals;

#define SET_PROC(NAM) util->vtable->m_util_##NAM = gtk_util_##NAM

    SET_PROC(userError);
    SET_PROC(notifyMove);
    SET_PROC(notifyTrade);
    SET_PROC(notifyPickTileBlank);
    SET_PROC(informNeedPickTiles);
    SET_PROC(informNeedPassword);
    SET_PROC(trayHiddenChange);
    SET_PROC(yOffsetChange);
#ifdef XWFEATURE_TURNCHANGENOTIFY
    SET_PROC(turnChanged);
#endif
    SET_PROC(notifyDupStatus);
    SET_PROC(informMove);
    SET_PROC(informUndo);
    SET_PROC(notifyGameOver);
    SET_PROC(informNetDict);
#ifdef XWFEATURE_HILITECELL
    SET_PROC(hiliteCell);
#endif
    SET_PROC(altKeyDown);
    SET_PROC(engineProgressCallback);
    SET_PROC(notifyIllegalWords);
    SET_PROC(remSelected);
    SET_PROC(getMQTTIDsFor);
    SET_PROC(timerSelected);
#ifndef XWFEATURE_STANDALONE_ONLY
    SET_PROC(makeStreamFromAddr);
#endif
#ifdef XWFEATURE_CHAT
    SET_PROC(showChat);
#endif
#ifdef XWFEATURE_SEARCHLIMIT
    SET_PROC(getTraySearchLimits);
#endif

#ifndef XWFEATURE_MINIWIN
    SET_PROC(bonusSquareHeld);
    SET_PROC(playerScoreHeld);
#endif
#ifdef XWFEATURE_BOARDWORDS
    SET_PROC(cellSquareHeld);
#endif
    SET_PROC(informWordsBlocked);
#undef SET_PROC

    assertTableFull( util->vtable, sizeof(*util->vtable), "gtk util" );
} /* setupGtkUtilCallbacks */

#ifndef XWFEATURE_STANDALONE_ONLY
typedef struct _SockInfo {
    GIOChannel* channel;
    guint watch;
    int socket;
} SockInfo;

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

    CommonGlobals* cGlobals = &globals->cGlobals;
    cGlobals->gi = &cGlobals->_gi;
    if ( !gi ) {
        gi = &params->pgi;
    }
    gi_copy( MPPARM(params->mpool) cGlobals->gi, gi );

    cGlobals->params = params;
    cGlobals->lastNTilesToUse = MAX_TRAY_TILES;
#ifndef XWFEATURE_STANDALONE_ONLY
# ifdef XWFEATURE_RELAY
    cGlobals->relaySocket = -1;
# endif

    cGlobals->socketAddedClosure = globals;
    cGlobals->onSave = gtkOnGameSaved;
    cGlobals->onSaveClosure = globals;
    cGlobals->addAcceptor = gtk_socket_acceptor;
#endif

    cGlobals->cp.showBoardArrow = XP_TRUE;
    cGlobals->cp.hideTileValues = params->hideValues;
    cGlobals->cp.skipMQTTAdd = params->skipMQTTAdd;
    cGlobals->cp.skipCommitConfirm = params->skipCommitConfirm;
    cGlobals->cp.sortNewTiles = params->sortNewTiles;
    cGlobals->cp.showColors = params->showColors;
    cGlobals->cp.allowPeek = params->allowPeek;
    cGlobals->cp.showRobotScores = params->showRobotScores;
#ifdef XWFEATURE_SLOW_ROBOT
    cGlobals->cp.robotThinkMin = params->robotThinkMin;
    cGlobals->cp.robotThinkMax = params->robotThinkMax;
    cGlobals->cp.robotTradePct = params->robotTradePct;
#endif
#ifdef XWFEATURE_ROBOTPHONIES
    cGlobals->cp.makePhonyPct = params->makePhonyPct;
#endif
#ifdef XWFEATURE_CROSSHAIRS
    cGlobals->cp.hideCrosshairs = params->hideCrosshairs;
#endif

    setupUtil( cGlobals );
    setupGtkUtilCallbacks( globals, cGlobals->util );

    makeSelfAddress( &cGlobals->selfAddr, params );
}

/* This gets called all the time, e.g. when the mouse moves across
   drawing-area boundaries. So invalidating is crazy expensive. But this is a
   test app....*/

static gboolean
on_draw_event( GtkWidget* widget, cairo_t* cr, gpointer user_data )
{
    // XP_LOGF( "%s(widget=%p)", __func__, widget );

    /* GdkRectangle rect; */
    /* if ( gdk_cairo_get_clip_rectangle( cr, &rect) ) { */
        /* XP_LOGF( "%s(): clip: x:%d,y:%d,w:%d,h:%d", __func__, */
        /*          rect.x, rect.y, rect.width, rect.height ); */
    /* } */

    GtkGameGlobals* globals = (GtkGameGlobals*)user_data;
    CommonGlobals* cGlobals = &globals->cGlobals;
    board_invalAll( cGlobals->game.board );
    board_draw( cGlobals->game.board, NULL_XWE );
    draw_gtk_status( (GtkDrawCtx*)(void*)cGlobals->draw, globals->stateChar );

    XP_USE(widget);
    XP_USE(cr);
    return FALSE;
}

void
initBoardGlobalsGtk( GtkGameGlobals* globals, LaunchParams* params,
                     CurGameInfo* gi )
{
    CommonGlobals* cGlobals = &globals->cGlobals;
    short width, height;
    GtkWidget* window;
    GtkWidget* drawing_area;
    GtkWidget* menubar;
    GtkWidget* vbox;
    GtkWidget* hbox;

    initGlobalsNoDraw( globals, params, gi );

    globals->window = window = gtk_window_new( GTK_WINDOW_TOPLEVEL );
    if ( !!params->fileName ) {
        gtk_window_set_title( GTK_WINDOW(window), params->fileName );
    }

    g_signal_connect( window, "configure-event",
                      G_CALLBACK(on_window_configure), globals );

    vbox = gtk_box_new( GTK_ORIENTATION_VERTICAL, 0 );
    gtk_container_add( GTK_CONTAINER(window), vbox );
    gtk_widget_show( vbox );

#ifdef DEBUG
        gulong id =
#endif
            g_signal_connect( window, "destroy", G_CALLBACK(destroy_board_window),
                              globals );
    XP_ASSERT( id > 0 );
    XP_ASSERT( !!globals );
#ifdef DEBUG
    id =
#endif
        g_signal_connect( window, "show", G_CALLBACK( on_board_window_shown ),
                          globals );
    XP_ASSERT( id > 0 );

    menubar = makeMenus( globals );
    gtk_box_pack_start( GTK_BOX(vbox), menubar, FALSE, TRUE, 0);

#if ! defined XWFEATURE_STANDALONE_ONLY && defined DEBUG
    globals->drop_checks_vbox = gtk_box_new( GTK_ORIENTATION_VERTICAL, 0 );
    gtk_box_pack_start( GTK_BOX(vbox), globals->drop_checks_vbox, 
                        FALSE, TRUE, 0 );
#endif

    gtk_box_pack_start( GTK_BOX(vbox), makeButtons( globals ), FALSE, TRUE, 0);

    drawing_area = gtk_drawing_area_new();
    gtk_widget_add_events( drawing_area, GDK_ALL_EVENTS_MASK );
#ifdef DEBUG
    id =
#endif
        g_signal_connect(G_OBJECT(drawing_area), "draw",
                         G_CALLBACK(on_draw_event), globals);
    XP_ASSERT( id > 0 );

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
#ifdef DEBUG
    id =
#endif
        g_signal_connect( globals->adjustment, "value_changed",
                          G_CALLBACK(scroll_value_changed), globals );
    XP_ASSERT( id > 0 );
    gtk_widget_show( vscrollbar );
    gtk_box_pack_start( GTK_BOX(hbox), vscrollbar, FALSE, FALSE, 0 );

    gtk_box_pack_start( GTK_BOX (hbox), 
                        makeVerticalBar( globals, window ), 
                        FALSE, TRUE, 0 );
    gtk_widget_show( hbox );

    gtk_box_pack_start( GTK_BOX(vbox), hbox/* drawing_area */, TRUE, TRUE, 0);

    GtkWidget* label = globals->countLabel = gtk_label_new( "" );
    gtk_box_pack_start( GTK_BOX(vbox), label, TRUE, TRUE, 0);
    gtk_widget_show( label );
#ifdef DEBUG
    id =
#endif
        g_signal_connect( drawing_area, "configure-event",
                          G_CALLBACK(on_drawing_configure), globals );
    XP_ASSERT( id > 0 );
#ifdef DEBUG
    id =
#endif
        g_signal_connect( drawing_area, "button_press_event",
                          G_CALLBACK(button_press_event), globals );
    XP_ASSERT( id > 0 );
#ifdef DEBUG
    id =
#endif
        g_signal_connect( drawing_area, "motion_notify_event",
                          G_CALLBACK(motion_notify_event), globals );
    XP_ASSERT( id > 0 );
#ifdef DEBUG
    id =
#endif
        g_signal_connect( drawing_area, "button_release_event",
                          G_CALLBACK(button_release_event), globals );
    XP_ASSERT( id > 0 );

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
    cGlobals->rowid = rowid;
    XWStreamCtxt* stream = mem_stream_make_raw( MPPARM(cGlobals->util->mpool)
                                                params->vtMgr );
    XP_Bool loaded = gdb_loadGame( stream, pDb, rowid );
    if ( loaded ) {
        loaded = game_makeFromStream( MEMPOOL NULL_XWE, stream, &cGlobals->game,
                                      cGlobals->gi,
                                      cGlobals->util, (DrawCtx*)NULL, &cGlobals->cp, &procs );
        if ( loaded ) {
            XP_LOGF( "%s: game loaded", __func__ );
#ifndef XWFEATURE_STANDALONE_ONLY
            if ( !!globals->cGlobals.game.comms ) {
                comms_resendAll( globals->cGlobals.game.comms, NULL_XWE, COMMS_CONN_NONE,
                                 XP_FALSE );
            }
#endif
        } else {
            game_dispose( &cGlobals->game, NULL_XWE );
        }
    }
    stream_destroy( stream, NULL_XWE );
    return loaded;
}

XP_Bool
makeNewGame( GtkGameGlobals* globals )
{
    CommonGlobals* cGlobals = &globals->cGlobals;
    XP_Bool success = gtkNewGameDialog( globals, cGlobals->gi,
                                        &cGlobals->selfAddr,
                                        XP_TRUE, XP_FALSE );
    LOG_RETURNF( "%s", boolToStr(success) );
    return success;
}

#endif /* PLATFORM_GTK */
