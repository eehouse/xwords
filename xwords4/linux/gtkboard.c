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
#include "device.h"
#include "gamemgr.h"
#include "gtkask.h"
#include "gtkinvit.h"
#include "gtkaskm.h"
#include "gtkchat.h"
#include "gtknewgame.h"
#include "gtkletterask.h"
#include "gtkpasswdask.h"
#include "gtkntilesask.h"
#include "gtkaskdict.h"
#include "gtkaskbad.h"
#include "gtkaskgo.h"
#include "linuxdict.h"
/* #include "undo.h" */
#include "gtkdraw.h"
#include "memstream.h"
#include "gamesdb.h"
#include "mqttcon.h"

/* static guint gtkSetupClientSocket( GtkGameGlobals* globals, int sock ); */
static void setCtrlsForTray( CommonGlobals* cGlobals );
static void setZoomButtons( GtkGameGlobals* globals, XP_Bool* inOut );
static void disenable_buttons( GtkGameGlobals* globals );
static GtkWidget* addButton( GtkWidget* hbox, gchar* label, GCallback func, 
                             void* closure );
static void handle_invite_button( GtkWidget* widget, GtkGameGlobals* globals );
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
    XP_Bool handled;

    gtkSetAltState( globals, event->state );

    if ( !globals->mouseDown ) {
        globals->mouseDown = XP_TRUE;
        GameRef gr = globals->cGlobals.gr;
        // BoardCtxt* board = gr_getGame(globals->cGlobals.gameRef)->board;
        XW_DUtilCtxt* dutil = globals->cGlobals.params->dutil;
        gr_handlePenDown( dutil, gr, NULL_XWE, event->x, event->y, &handled );
        /* if ( redraw ) { */
        /*     gr_draw( dutil, gr, NULL_XWE ); */
        disenable_buttons( globals );
        /* } */
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
        GameRef gr =  globals->cGlobals.gr;
        XW_DUtilCtxt* dutil = globals->cGlobals.params->dutil;
        gr_handlePenMove( dutil, gr, NULL_XWE, event->x, event->y );
        /* if ( handled ) { */
        /*     gr_draw( dutil, gr, NULL_XWE ); */
            disenable_buttons( globals );
        /* } */
    } else {
        handled = XP_FALSE;
    }

    return handled;
} /* motion_notify_event */

static gint
button_release_event( GtkWidget* XP_UNUSED(widget), GdkEventMotion *event,
                      GtkGameGlobals* globals )
{
    gtkSetAltState( globals, event->state );

    if ( globals->mouseDown ) {
        GameRef gr = globals->cGlobals.gr;
        XW_DUtilCtxt* dutil = globals->cGlobals.params->dutil;
        gr_handlePenUp( dutil, gr, NULL_XWE, event->x, event->y );
        /* if ( redraw ) { */
            gr_draw( dutil, gr, NULL_XWE );
        /*     disenable_buttons( globals ); */
        /* } */
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
        XP_LOGFF( "... it's a DEL" );
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
        GameRef gr = globals->cGlobals.gr;
        XW_DUtilCtxt* dutil = globals->cGlobals.params->dutil;
        if ( globals->keyDown ) {
            gr_handleKeyRepeat( dutil, gr, NULL_XWE, xpkey, &handled );
        } else {
            gr_handleKeyDown( dutil, gr, NULL_XWE, xpkey, &handled );
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
        GameRef gr = globals->cGlobals.gr;
        XW_DUtilCtxt* dutil = globals->cGlobals.params->dutil;
        gr_handleKeyUp( dutil, gr, NULL_XWE, xpkey, &handled );
#ifdef KEYBOARD_NAV
        if ( movesCursor && !handled ) {
            BoardObjectType order[] = { OBJ_SCORE, OBJ_BOARD, OBJ_TRAY };
            linShiftFocus( &globals->cGlobals, xpkey, order, NULL );
        }
#endif
    }

/*     XP_ASSERT( globals->keyDown ); */
#ifdef KEYBOARD_NAV
    globals->keyDown = XP_FALSE;
#endif

    return handled? 1 : 0;        /* gtk will do something with the key if 0
                                     returned  */
} /* key_release_event */
#endif

#ifdef XWFEATURE_RELAY
static void
relay_status_gtk( XWEnv XP_UNUSED(xwe), void* closure, CommsRelayState state )
{
    XP_LOGFF( "got status: %s", CommsRelayState2Str(state) );
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
        globals->cGlobals.gi->deviceRole = ROLE_ISGUEST;
        server_reset( game->server, game->comms );
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

/* static void */
/* countChanged_gtk( XWEnv XP_UNUSED(xwe), void* closure, XP_U16 newCount, */
/*                   XP_Bool quashed ) */
/* { */
/*     GtkGameGlobals* globals = (GtkGameGlobals*)closure; */
/*     gchar buf[128]; */
/*     snprintf( buf, VSIZE(buf), "pending count: %d%s" */
/*               "\nGameID: %08X", newCount, */
/*               quashed?"q":"", globals->cGlobals.gi->gameID); */
/*     gtk_label_set_text( GTK_LABEL(globals->countLabel), buf ); */
/* } */

/* static void */
/* setTransportProcs( TransportProcs* procs, GtkGameGlobals* globals )  */
/* { */
/*     XP_ASSERT( !procs->closure ); */
/*     procs->closure = globals; */
/*     procs->sendMsgs = linux_send; */
/* #ifdef XWFEATURE_COMMS_INVITE */
/*     procs->sendInvt = linux_send_invt; */
/* #endif */
/* #ifdef COMMS_XPORT_FLAGSPROC */
/*     procs->getFlags = gtk_getFlags; */
/* #endif */
/* #ifdef COMMS_HEARTBEAT */
/*     procs->reset = linux_reset; */
/* #endif */
/* #ifdef XWFEATURE_RELAY */
/*     procs->rstatus = relay_status_gtk; */
/*     procs->rconnd = relay_connd_gtk; */
/*     procs->rerror = relay_error_gtk; */
/*     procs->sendNoConn = relay_sendNoConn_gtk; */
/* # ifdef RELAY_VIA_HTTP */
/*     procs->requestJoin = relay_requestJoin_gtk; */
/* # endif */
/* #endif */
/*     procs->countChanged = countChanged_gtk; */
/* } */

#ifdef DEBUG
static void 
drop_msg_toggle( GtkWidget* toggle, void* data )
{
    XP_Bool disabled = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(toggle) );
    long asInt = (long)data;
    XP_Bool send = 0 != (asInt & 1);
    asInt &= ~1;
    DropTypeData* datum = (DropTypeData*)asInt;
    gr_setAddrDisabled( datum->dutil, datum->gr, NULL_XWE, datum->typ, send, disabled );
} /* drop_msg_toggle */

static void
addDropChecks( GtkGameGlobals* globals )
{
    GameRef gr = globals->cGlobals.gr;
    XW_DUtilCtxt* dutil = globals->cGlobals.params->dutil;
    if ( gr_haveComms(dutil, gr, NULL_XWE) ) {
        CommsAddrRec selfAddr;
        gr_getSelfAddr( dutil, gr, NULL_XWE, &selfAddr );
        CommsConnType typ;
        for ( XP_U32 st = 0; addr_iter( &selfAddr, &typ, &st ); ) {
            DropTypeData* datum = &globals->dropData[typ];
            datum->dutil = dutil;
            datum->typ = typ;
            datum->gr = gr;

            GtkWidget* hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);

            gchar buf[32];
            snprintf( buf, sizeof(buf), "Drop %s messages", 
                      ConnType2Str( typ ) );
            GtkWidget* widget = gtk_label_new( buf );
            gtk_box_pack_start( GTK_BOX(hbox), widget, FALSE, TRUE, 0);
            gtk_widget_show( widget );

            widget = gtk_check_button_new_with_label( "Incoming" );
            if ( gr_getAddrDisabled( dutil, gr, NULL_XWE, typ, XP_FALSE ) ) {
                gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(widget), TRUE );
            }
            g_signal_connect( widget, "toggled", G_CALLBACK(drop_msg_toggle),
                              datum );
            gtk_box_pack_start( GTK_BOX(hbox), widget, FALSE, TRUE, 0);
            gtk_widget_show( widget );

            widget = gtk_check_button_new_with_label( "Outgoing" );
            if ( gr_getAddrDisabled( dutil, gr, NULL_XWE, typ, XP_TRUE ) ) {
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
#else
# define addDropChecks( globals )
#endif  /* DEBUG */

static void
createOrLoadObjects( GtkGameGlobals* globals )
{
    CommonGlobals* cGlobals = &globals->cGlobals;
    assertMainThread( cGlobals );
    LaunchParams* params = cGlobals->params;

    cGlobals->draw = gtkDrawCtxtMake( globals->drawing_area,
                                      globals, DT_SCREEN );
    XW_DUtilCtxt* dutil = params->dutil;
    const CurGameInfo* gi = gr_getGI( dutil, cGlobals->gr, NULL_XWE );
    XP_ASSERT( !cGlobals->util );
    cGlobals->util = linux_util_make( dutil, gi, cGlobals->gr );
    setupLinuxUtilCallbacks( cGlobals->util, XP_FALSE );

    gr_setDraw( params->dutil, cGlobals->gr, NULL_XWE, cGlobals->draw,
                cGlobals->util );

    if ( linuxOpenGame( cGlobals ) ) {
        if ( !params->fileName && !!params->dbName ) {
            XP_UCHAR buf[64];
            snprintf( buf, sizeof(buf), "%s / %lX", params->dbName,
                      cGlobals->gr );
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

    // BoardCtxt* board = gr_getGame(cGlobals->gr)->board;

    GtkAllocation alloc;
    gtk_widget_get_allocation( widget, &alloc );
    short bdWidth = alloc.width - (GTK_RIGHT_MARGIN + GTK_BOARD_LEFT_MARGIN);
    short bdHeight = alloc.height - (GTK_TOP_MARGIN + GTK_BOTTOM_MARGIN)
        - GTK_MIN_TRAY_SCALEV - GTK_BOTTOM_MARGIN;

    XP_ASSERT( !cGlobals->params->verticalScore ); /* not supported */

    BoardDims dims;
    GameRef gr = cGlobals->gr;
    XW_DUtilCtxt* dutil = cGlobals->params->dutil;
    gr_figureLayout( dutil, gr, NULL_XWE, 
                     GTK_BOARD_LEFT, GTK_HOR_SCORE_TOP, bdWidth, bdHeight,
                     110, 150, 200, bdWidth-25, 16, 16, XP_FALSE, &dims );
    gr_applyLayout( dutil, gr, NULL_XWE, &dims );

    setCtrlsForTray( cGlobals );
    gr_invalAll( dutil, gr, NULL_XWE );

    XP_Bool inOut[2];
    gr_zoom( dutil, gr, NULL_XWE, 0, inOut );
    setZoomButtons( globals, inOut );

    return FALSE;
} /* on_drawing_configure */

static gboolean
on_window_configure( GtkWidget* XP_UNUSED(widget), GdkEventConfigure* event,
                     GtkGameGlobals* bGlobals )
{
    bGlobals->lastConfigure = *event;

    return FALSE;
}

void
destroy_board_window( GtkWidget* XP_UNUSED(widget), GtkGameGlobals* globals )
{
    LOG_FUNC();
    /* GameRef gr = globals->cGlobals.gr; */
    /* XW_DUtilCtxt* dutil = globals->cGlobals.params->dutil; */
    /* if ( gr_haveComms( dutil, gr, NULL_XWE) ) { */
    /*     gr_stop( dutil, gr, NULL_XWE ); */
    /* } */
    /* linuxSaveGame( &globals->cGlobals ); */
    windowDestroyed( globals );
}

static void
on_board_window_shown( GtkWidget* XP_UNUSED(widget), GtkGameGlobals* globals )
{
    LOG_FUNC();
    CommonGlobals* cGlobals = &globals->cGlobals;
    XW_DUtilCtxt* dutil = cGlobals->params->dutil;
    if ( gr_getGameIsOver( dutil, cGlobals->gr, NULL_XWE ) ) {
        gtkShowFinalScores( globals, XP_TRUE );
    }
} /* on_board_window_shown */

static void
cleanup( GtkGameGlobals* globals )
{
    LOG_FUNC();
    CommonGlobals* cGlobals = &globals->cGlobals;
    XP_ASSERT( 0 == cGlobals->refCount );
    // linuxSaveGame( cGlobals );
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
    // gi_disposePlayerInfo( globals->params->mpool cGlobals->gi );

    draw_unref( cGlobals->draw, NULL_XWE );
    util_unref( cGlobals->util, NULL_XWE );
    gr_setDraw( cGlobals->params->dutil, cGlobals->gr, NULL_XWE,
                NULL, NULL );
    cGlobals->draw = NULL;
    cGlobals->util = NULL;

    XP_LOGFF( "nuking %p", globals );
    g_free( globals );
    LOG_RETURN_VOID();
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
    XWStreamCtxt* stream = 
        mem_stream_make( MPPARM(cGlobals->params->mpool)
                         cGlobals->params->vtMgr,
                         CHANNEL_NONE );
    XW_DUtilCtxt* dutil = cGlobals->params->dutil;
    gr_formatDictCounts( dutil, cGlobals->gr, NULL_XWE, stream, 5, full );
    stream_putU8( stream, '\n' );
    catAndClose( stream );
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
    catGameHistory( globals->cGlobals.params, globals->cGlobals.gr );
} /* game_history */

#ifdef TEXT_MODEL
static void
dump_board( GtkWidget* XP_UNUSED(widget), GtkGameGlobals* globals )
{
    // ModelCtxt* model =  gr_getGame(globals->cGlobals.gr)->model;
    CommonGlobals* cGlobals = &globals->cGlobals;
    XWStreamCtxt* stream = 
        mem_stream_make( MPPARM(cGlobals->params->mpool)
                         cGlobals->params->vtMgr,
                         CHANNEL_NONE );
    XW_DUtilCtxt* dutil = cGlobals->params->dutil;
    gr_writeToTextStream( dutil, cGlobals->gr, NULL_XWE, stream );
    catAndClose( stream );
}
#endif

static void
final_scores( GtkWidget* XP_UNUSED(widget), GtkGameGlobals* globals )
{
    CommonGlobals* cGlobals = &globals->cGlobals;
    // ServerCtxt* server = gr_getGame(cGlobals->gr)->server;
    XW_DUtilCtxt* dutil = cGlobals->params->dutil;
    XP_Bool gameOver = gr_getGameIsOver( dutil, cGlobals->gr, NULL_XWE );

    if ( gameOver ) {
        catFinalScores( cGlobals->params, cGlobals->gr, -1 );
    } else if ( GTK_RESPONSE_YES == gtkask( globals->window,
                                            "Are you sure you want to resign?", 
                                            GTK_BUTTONS_YES_NO, NULL ) ) {
        globals->cGlobals.manualFinal = XP_TRUE;
        gr_endGame( dutil, cGlobals->gr, NULL_XWE );
        gameOver = TRUE;
    }

    /* the end game listener will take care of printing the final scores */
} /* final_scores */

static void
game_info( GtkWidget* XP_UNUSED(widget), GtkGameGlobals* globals )
{
#ifdef XWFEATURE_DEVICE_STORES
    XP_ASSERT(0);
    XP_USE(globals);
#else
    CommsAddrRec selfAddr;
    // XWGame* game = gr_getGame(globals->cGlobals.gr );
    GameRef gr = globals->cGlobals.gr;
    gr_getSelfAddr( dutil, gr, &selfAddr );

    /* Anything to do if OK is clicked?  Changed names etc. already saved.  Try
       server_do in case one's become a robot. */
    CurGameInfo* gi = globals->cGlobals.gi;
    if ( gtkNewGameDialog( globals, gi, &selfAddr, XP_FALSE, XP_FALSE ) ) {
        if ( gr_do( dutil, gr, NULL_XWE ) ) {
            gr_draw( dutil, gr, NULL_XWE );
        }
    }
#endif
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
	GSList* dicts = ldm_listDicts( params->ldm );
	gchar buf[265];
	gchar* name = gtkaskdict( dicts, buf, VSIZE(buf) );
	if ( !!name ) {
		DictionaryCtxt* dict = 
			linux_dictionary_make( MPPARM(cGlobals->params->mpool)
                                   params, name, params->useMmap );
		gr_changeDict( params->dutil, cGlobals->gr, NULL_XWE,
                       dict );
        dict_unref( dict, NULL_XWE );
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
    // BoardCtxt* board = globals->cGlobals.game.board;
    GameRef gr = globals->cGlobals.gr;
    XW_DUtilCtxt* dutil = globals->cGlobals.params->dutil;
    gr_endTrade( dutil, gr, NULL_XWE );
}

static void
handle_resend( GtkWidget* XP_UNUSED(widget), GtkGameGlobals* globals )
{
    GameRef gr = globals->cGlobals.gr;
    XW_DUtilCtxt* dutil = globals->cGlobals.params->dutil;
    if ( gr_haveComms(dutil, gr, NULL_XWE) ) {
        gr_resendAll( dutil, gr, NULL_XWE, COMMS_CONN_NONE, XP_TRUE );
    }
} /* handle_resend */

#ifdef XWFEATURE_COMMSACK
static void
handle_ack( GtkWidget* XP_UNUSED(widget), GtkGameGlobals* globals )
{
    GameRef gr = globals->cGlobals.gr;
    XW_DUtilCtxt* dutil = globals->cGlobals.params->dutil;
    gr_ackAny( dutil, gr, NULL_XWE );
}
#endif

#ifdef DEBUG
static void
handle_commstats( GtkWidget* XP_UNUSED(widget), GtkGameGlobals* globals )
{
    // CommsCtxt* comms = globals->cGlobals.game.comms;
    CommonGlobals* cGlobals = &globals->cGlobals;
    GameRef gr = cGlobals->gr;
    XW_DUtilCtxt* dutil = cGlobals->params->dutil;
    if ( gr_haveComms(dutil, gr, NULL_XWE) ) {
        XWStreamCtxt* stream = 
            mem_stream_make( MPPARM(cGlobals->params->mpool)
                             cGlobals->params->vtMgr,
                             CHANNEL_NONE );
        gr_getStats( dutil, gr, NULL_XWE, stream );
        catAndClose( stream );
    }
} /* handle_commstats */
#endif

#ifdef MEM_DEBUG
static void
handle_memstats( GtkWidget* XP_UNUSED(widget), GtkGameGlobals* globals )
{
    CommonGlobals* cGlobals = &globals->cGlobals;
    XWStreamCtxt* stream = mem_stream_make( MPPARM(cGlobals->params->mpool)
                                            cGlobals->params->vtMgr, 
                                            CHANNEL_NONE );
    mpool_stats( cGlobals->params->mpool, stream );
    catAndClose( stream );
} /* handle_memstats */

#endif

#ifdef XWFEATURE_ACTIVERECT
static gint
inval_board_ontimer( gpointer data )
{
    GtkGameGlobals* globals = (GtkGameGlobals*)data;
    BoardCtxt* board = globals->cGlobals.game.board;
    gr_draw( dutil, board, NULL_XWE );
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
    // XWGame* game = &globals->cGlobals.game;
    GameRef gr = globals->cGlobals.gr;
    XW_DUtilCtxt* dutil = globals->cGlobals.params->dutil;
    XP_U16 nPending = gr_getPendingRegs( dutil, gr, NULL_XWE );
    if ( !globals->invite_button
         && 0 < nPending
         && !gr_isFromRematch( dutil, gr, NULL_XWE )
         && !!globals->buttons_hbox ) {
        globals->invite_button = 
            addButton( globals->buttons_hbox, "Invite",
                       G_CALLBACK(handle_invite_button), globals );
    }

    GameStateInfo gsi;
    gr_getState( dutil, gr, NULL_XWE, &gsi );

    XP_Bool canFlip = 1 < gr_visTileCount( dutil, gr, NULL_XWE );
    gtk_widget_set_sensitive( globals->flip_button, canFlip );

    XP_Bool canToggle = gr_canTogglePending( dutil, gr, NULL_XWE );
    gtk_widget_set_sensitive( globals->toggle_undo_button, canToggle );

    gtk_widget_set_sensitive( globals->prevhint_button, gsi.canHint );
    gtk_widget_set_sensitive( globals->nexthint_button, gsi.canHint );

    if ( !!globals->invite_button ) {
        gtk_widget_set_sensitive( globals->invite_button, 0 < nPending );
    }
    gtk_widget_set_sensitive( globals->commit_button, gsi.curTurnSelected );

    gtk_widget_set_sensitive( globals->chat_button, gsi.canChat );

    gtk_widget_set_sensitive( globals->pause_button, gsi.canPause );
    gtk_widget_set_sensitive( globals->unpause_button, gsi.canUnpause );
}

static gboolean
handle_flip_button( GtkWidget* XP_UNUSED(widget), gpointer _globals )
{
    GtkGameGlobals* globals = (GtkGameGlobals*)_globals;
    XW_DUtilCtxt* dutil = globals->cGlobals.params->dutil;
    gr_flip( dutil, globals->cGlobals.gr, NULL_XWE );
    return TRUE;
} /* handle_flip_button */

static gboolean
handle_value_button( GtkWidget* XP_UNUSED(widget), gpointer closure )
{
    GtkGameGlobals* globals = (GtkGameGlobals*)closure;
    CommonGlobals* cGlobals = &globals->cGlobals;
    XW_DUtilCtxt* dutil = cGlobals->params->dutil;
    cGlobals->cp.tvType = (cGlobals->cp.tvType + 1) % TVT_N_ENTRIES;
    gr_prefsChanged( dutil, cGlobals->gr, NULL_XWE, &cGlobals->cp );
    gr_draw( dutil, cGlobals->gr, NULL_XWE );
    return TRUE;
} /* handle_value_button */

static void
handle_hint_button( GtkGameGlobals* globals, XP_Bool prev )
{
    XP_Bool redo;
    XW_DUtilCtxt* dutil = globals->cGlobals.params->dutil;
    if ( gr_requestHint( dutil, globals->cGlobals.gr, NULL_XWE,
#ifdef XWFEATURE_SEARCHLIMIT
                            XP_FALSE,
#endif
                            prev, &redo ) ) {
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
    CommonGlobals* cGlobals = &globals->cGlobals;
    XW_DUtilCtxt* dutil = cGlobals->params->dutil;
    GameRef gr = cGlobals->gr;
    gr_resetEngine( dutil, gr, NULL_XWE );
    gr_requestHint( dutil, gr, NULL_XWE,
#ifdef XWFEATURE_SEARCHLIMIT
                    XP_TRUE,
#endif
                    XP_FALSE, &redo );
} /* handle_nhint_button */

static void
handle_colors_button( GtkWidget* XP_UNUSED(widget), 
                      GtkGameGlobals* XP_UNUSED(globals) )
{
/*     XP_Bool oldVal = board_getShowColors( globals->cGlobals.game.board ); */
/*     if ( board_setShowColors( globals->cGlobals.game.board, !oldVal ) ) { */
/* 	gr_draw( globals->cGlobals.game.board );	 */
/*     } */
} /* handle_colors_button */

static void
handle_juggle_button( GtkWidget* XP_UNUSED(widget), GtkGameGlobals* globals )
{
    XW_DUtilCtxt* dutil = globals->cGlobals.params->dutil;
    gr_juggleTray( dutil, globals->cGlobals.gr, NULL_XWE );
} /* handle_juggle_button */

static void
handle_undo_button( GtkWidget* XP_UNUSED(widget), GtkGameGlobals* globals )
{
    XW_DUtilCtxt* dutil = globals->cGlobals.params->dutil;
    if ( gr_handleUndo( dutil, globals->cGlobals.gr, NULL_XWE, 0 ) ) {
        gr_draw( dutil, globals->cGlobals.gr, NULL_XWE );
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
    GameRef gr = globals->cGlobals.gr;
    XW_DUtilCtxt* dutil = globals->cGlobals.params->dutil;
    gr_replaceTiles( dutil, gr, NULL_XWE );
}

static void
handle_trade_button( GtkWidget* XP_UNUSED(widget), GtkGameGlobals* globals )
{
    XW_DUtilCtxt* dutil = globals->cGlobals.params->dutil;
    gr_beginTrade( dutil, globals->cGlobals.gr, NULL_XWE );
    disenable_buttons( globals );
} /* handle_juggle_button */

static void
handle_done_button( GtkWidget* XP_UNUSED(widget), GtkGameGlobals* globals )
{
    XW_DUtilCtxt* dutil = globals->cGlobals.params->dutil;
    gr_commitTurn( dutil, globals->cGlobals.gr, NULL_XWE,
                   NULL, XP_FALSE, NULL );
    /*     gr_draw( dutil, globals->cGlobals.gr, NULL_XWE ); */
    /*     disenable_buttons( globals ); */
    /* } */
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
    XW_DUtilCtxt* dutil = globals->cGlobals.params->dutil;
    gr_zoom( dutil, globals->cGlobals.gr, NULL_XWE, 1, inOut );
    setZoomButtons( globals, inOut );
} /* handle_zoomin_button */

static void
handle_zoomout_button( GtkWidget* XP_UNUSED(widget), GtkGameGlobals* globals )
{
    XP_Bool inOut[2];
    XW_DUtilCtxt* dutil = globals->cGlobals.params->dutil;
    gr_zoom( dutil, globals->cGlobals.gr, NULL_XWE, -1, inOut );
    setZoomButtons( globals, inOut );
} /* handle_zoomout_button */

static void
handle_chat_button( GtkWidget* XP_UNUSED(widget), GtkGameGlobals* globals )
{
    launchChat( globals );
}

static void
handle_pause_button( GtkWidget* XP_UNUSED(widget), GtkGameGlobals* globals )
{
    XW_DUtilCtxt* dutil = globals->cGlobals.params->dutil;
    gr_pause( dutil, globals->cGlobals.gr, NULL_XWE, "whatever" );
    disenable_buttons( globals );
}

static void
handle_unpause_button( GtkWidget* XP_UNUSED(widget), GtkGameGlobals* globals )
{
    XW_DUtilCtxt* dutil = globals->cGlobals.params->dutil;
    gr_unpause( dutil, globals->cGlobals.gr, NULL_XWE, "whatever" );
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
    XW_DUtilCtxt* dutil = globals->cGlobals.params->dutil;
    if ( gr_setYOffset( dutil, globals->cGlobals.gr, NULL_XWE, newValue ) ) {
        gr_draw( dutil, globals->cGlobals.gr, NULL_XWE );
    }
} /* scroll_value_changed */

static void
handle_grid_button( GtkWidget* XP_UNUSED(widget), GtkGameGlobals* globals )
{
    globals->gridOn = !globals->gridOn;
    XW_DUtilCtxt* dutil = globals->cGlobals.params->dutil;
    gr_invalAll( dutil, globals->cGlobals.gr, NULL_XWE );
    gr_draw( dutil, globals->cGlobals.gr, NULL_XWE );
} /* handle_grid_button */

static void
handle_hide_button( GtkWidget* XP_UNUSED(widget), GtkGameGlobals* globals )
{
    // BoardCtxt* board;
    XP_Bool draw = XP_FALSE;

    if ( globals->cGlobals.params->nHidden > 0 ) {
        gint nRows = globals->cGlobals.gi->boardSize;
        gtk_adjustment_set_page_size( globals->adjustment, nRows );
        gtk_adjustment_set_value( globals->adjustment, 0.0 );

        g_signal_emit_by_name( globals->adjustment, "changed" );
        // gtk_adjustment_value_changed( GTK_ADJUSTMENT(globals->adjustment) );
    }

    // board = globals->cGlobals.game.board;
    GameRef gr = globals->cGlobals.gr;
    XW_DUtilCtxt* dutil = globals->cGlobals.params->dutil;
    if ( TRAY_REVEALED == gr_getTrayVisState( dutil, gr, NULL_XWE ) ) {
        draw = gr_hideTray( dutil, gr, NULL_XWE );
    } else {
        draw = gr_showTray( dutil, gr, NULL_XWE );
    }
    if ( draw ) {
        gr_draw( dutil, gr, NULL_XWE );
    }
} /* handle_hide_button */

static void
handle_commit_button( GtkWidget* XP_UNUSED(widget), GtkGameGlobals* globals )
{
    XW_DUtilCtxt* dutil = globals->cGlobals.params->dutil;
    gr_commitTurn( dutil, globals->cGlobals.gr, NULL_XWE,
                   NULL, XP_FALSE, NULL );
    /*     gr_draw( dutil, globals->cGlobals.gr, NULL_XWE ); */
    /* } */
} /* handle_commit_button */

static void
handle_invite_button( GtkWidget* XP_UNUSED(widget), GtkGameGlobals* globals )
{
    CommonGlobals* cGlobals = &globals->cGlobals;
    XW_DUtilCtxt* dutil = cGlobals->params->dutil;
    XP_U16 nMissing = gr_getPendingRegs( dutil, globals->cGlobals.gr, NULL_XWE );

    CommsAddrRec inviteAddr = {};
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
    // CommsCtxt* comms = cGlobals->game.comms;
    CommsAddrRec myAddr = {};
    GameRef gr = cGlobals->gr;
    XW_DUtilCtxt* dutil = cGlobals->params->dutil;
    gr_getSelfAddr( dutil, gr, NULL_XWE, &myAddr );

    XP_U16 channel;
    if ( gr_getOpenChannel( dutil, gr, NULL_XWE, &channel ) ) {
        gint forceChannel = channel;

        NetLaunchInfo nli = {};    /* include everything!!! */
        nli_init( &nli, cGlobals->gi, &myAddr, nPlayers, forceChannel );
#ifdef XWFEATURE_RELAY
        if ( addr_hasType( &myAddr, COMMS_CONN_RELAY ) ) {
            XP_UCHAR buf[32];
            snprintf( buf, sizeof(buf), "%X", makeRandomInt() );
            nli_setInviteID( &nli, buf ); /* PENDING: should not be relay only!!! */
        }
#endif
        // nli_setDevID( &nli, linux_getDevIDRelay( cGlobals->params ) );

        if ( addr_hasType( &myAddr, COMMS_CONN_MQTT ) ) {
            const MQTTDevID* devid = mqttc_getDevID( cGlobals->params );
            nli_setMQTTDevID( &nli, devid );
        }
        if ( addr_hasType( &myAddr, COMMS_CONN_SMS ) ) {
            nli_setPhone( &nli, myAddr.u.sms.phone );
        }

#ifdef DEBUG
        {
            XWStreamCtxt* stream = mem_stream_make_raw( MPPARM(cGlobals->params->mpool)
                                                        cGlobals->params->vtMgr );
            nli_saveToStream( &nli, stream );
            NetLaunchInfo tmp;
            nli_makeFromStream( &tmp, stream );
            stream_destroy( stream );
            XP_ASSERT( 0 == memcmp( &nli, &tmp, sizeof(nli) ) );
        }
#endif

#ifdef XWFEATURE_COMMS_INVITE
        gr_invite( dutil, gr, NULL_XWE, &nli, destAddr, XP_TRUE );
#else
        if ( !!destAddr && '\0' != destAddr->u.sms.phone[0] && 0 < destAddr->u.sms.port ) {
            linux_sms_invite( cGlobals->params, &nli,
                              destAddr->u.sms.phone, destAddr->u.sms.port );
        }
# ifdef XWFEATURE_RELAY
        if ( 0 != relayDevID || !!relayID ) {
            XP_ASSERT( 0 != relayDevID || (!!relayID && !!relayID[0]) );
            relaycon_invite( cGlobals->params, relayDevID, relayID, &nli );
        }
# endif

        if ( addr_hasType( destAddr, COMMS_CONN_MQTT ) ) {
            mqttc_invite( cGlobals->params, 0, &nli, &destAddr->u.mqtt.devID );
        }
#endif
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
    XP_Bool skipUserErrs = globals->cGlobals.params->skipUserErrs;
    if ( !skipUserErrs ) {
        char buf[512];
        va_list ap;
        va_start( ap, format );
        vsnprintf( buf, sizeof(buf), format, ap );

        (void)gtkask( globals->window, buf, GTK_BUTTONS_OK, NULL );

        va_end(ap);
    }
} /* gtkUserError */

static gint
ask_blank( gpointer data )
{
    CommonGlobals* cGlobals = (CommonGlobals*)data;
    const XP_UCHAR* name = cGlobals->gi->players[cGlobals->selPlayer].name;
    XP_S16 result = gtkletterask( NULL, XP_FALSE, name, 1,
                                  cGlobals->nTiles, cGlobals->tiles, NULL );

    for ( int ii = 0; ii < cGlobals->nTiles; ++ii ) {
        g_free( (gpointer)cGlobals->tiles[ii] );
    }

    XW_DUtilCtxt* dutil = cGlobals->params->dutil;
    if ( result >= 0 ) {
        gr_setBlankValue( dutil, cGlobals->gr, NULL_XWE,
                          cGlobals->selPlayer, cGlobals->blankCol,
                          cGlobals->blankRow, result );
    }

    return 0;
}

static void
gtk_util_notifyPickTileBlank( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe),
                              XP_U16 playerNum, XP_U16 col,
                              XP_U16 row, const XP_UCHAR** texts, XP_U16 nTiles )
{
    CommonGlobals* cGlobals = globalsForUtil( uc, XP_FALSE );
    cGlobals->selPlayer = playerNum;
    cGlobals->blankCol = col;
    cGlobals->blankRow = row;
    cGlobals->nTiles = nTiles;
    for ( int ii = 0; ii < nTiles; ++ii ) {
        cGlobals->tiles[ii] = g_strdup( texts[ii] );
    }

    (void)g_idle_add( ask_blank, cGlobals );
}

static gint
ask_tiles( gpointer data )
{
    CommonGlobals* cGlobals = (CommonGlobals*)data;

    TrayTileSet newTiles = {};
    const XP_UCHAR* name = cGlobals->gi->players[cGlobals->selPlayer].name;
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

    GameRef gr = cGlobals->gr;
    XP_Bool draw = XP_TRUE;
    XW_DUtilCtxt* dutil = cGlobals->params->dutil;
    if ( cGlobals->pickIsInitial ) {
        gr_tilesPicked( dutil, gr, NULL_XWE, cGlobals->selPlayer, &newTiles );
    } else {
        PhoniesConf pc = { .confirmed = XP_TRUE };
        gr_commitTurn( dutil, gr, NULL_XWE, &pc, XP_TRUE, &newTiles );
        draw = XP_FALSE;
    }

    if ( draw ) {
        gr_draw( dutil, gr, NULL_XWE );
    }

    return 0;
} /* ask_tiles */

static void
gtk_util_informNeedPickTiles( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe),
                              XP_Bool isInitial, XP_U16 player, XP_U16 nToPick,
                              XP_U16 nFaces, const XP_UCHAR** faces,
                              const XP_U16* counts )
{
    CommonGlobals* cGlobals = globalsForUtil( uc, XP_FALSE );
    cGlobals->selPlayer = player;
    cGlobals->pickIsInitial = isInitial;

    cGlobals->nToPick = nToPick;
    cGlobals->nTiles = nFaces;
    for ( int ii = 0; ii < nFaces; ++ii ) {
        cGlobals->tiles[ii] = g_strdup( faces[ii] );
        cGlobals->tileCounts[ii] = counts[ii];
    }

    (void)g_idle_add( ask_tiles, cGlobals );
} /* gtk_util_informNeedPickTiles */

static gint
ask_password( gpointer data )
{
    CommonGlobals* cGlobals = (CommonGlobals*)data;
    XP_UCHAR buf[32];
    XP_U16 len = VSIZE(buf);
    if ( gtkpasswdask( cGlobals->askPassName, buf, &len ) ) {
        XW_DUtilCtxt* dutil = cGlobals->params->dutil;
        if ( gr_passwordProvided( dutil, cGlobals->gr, NULL_XWE,
                                  cGlobals->selPlayer, buf ) ) {
            gr_draw( dutil, cGlobals->gr, NULL_XWE );
        }
    }
    return 0;
}

static void
gtk_util_informNeedPassword( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe),
                             XP_U16 player, const XP_UCHAR* name )
{
    CommonGlobals* cGlobals = globalsForUtil( uc, XP_FALSE );
    cGlobals->askPassName = name;
    cGlobals->selPlayer = player;

    (void)g_idle_add( ask_password, cGlobals );
} /* gtk_util_askPassword */

static void
setCtrlsForTray( CommonGlobals* XP_UNUSED(cGlobals) )
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
    CommonGlobals* cGlobals = globalsForUtil( uc, XP_FALSE );
    setCtrlsForTray( cGlobals );
} /* gtk_util_trayHiddenChange */

static void
gtk_util_yOffsetChange( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe),
                        XP_U16 maxOffset,
                        XP_U16 XP_UNUSED(oldOffset), 
                        XP_U16 newOffset )
{
    CommonGlobals* cGlobals = globalsForUtil( uc, XP_FALSE );
    if ( !!cGlobals ) {
        GtkGameGlobals* globals = (GtkGameGlobals*)cGlobals;

        /* adjustment is invalid when gtk's shutting down; ignore */
        if ( !!globals->adjustment
             && GTK_IS_ADJUSTMENT(globals->adjustment) ) {
            gint nRows = globals->cGlobals.gi->boardSize;
            gtk_adjustment_set_page_size(globals->adjustment,
                                         nRows - maxOffset);
            gtk_adjustment_set_value(globals->adjustment, newOffset);
            // gtk_adjustment_value_changed( globals->adjustment );
        }
    }
} /* gtk_util_yOffsetChange */

static void
gtk_util_dictGone( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe),
                   const XP_UCHAR* XP_UNUSED_DBG(dictName) )
{
    XP_LOGFF( "(dictName: %s)", dictName );
    CommonGlobals* cGlobals = globalsForUtil( uc, XP_FALSE );
    if ( !!cGlobals ) {
        // GtkGameGlobals* globals = (GtkGameGlobals*)cGlobals;
        XP_LOGFF( "I want to close this game; how???" );
    }
}

#ifdef XWFEATURE_TURNCHANGENOTIFY
static void
gtk_util_turnChanged( XW_UtilCtxt* XP_UNUSED(uc), XWEnv XP_UNUSED(xwe),
                      XP_S16 XP_UNUSED(newTurn) )
{
    /* CommonGlobals* cGlobals = globalsForUtil( uc, XP_FALSE ); */
    /* if ( !!cGlobals ) { */
    /*     linuxSaveGame( cGlobals ); */
    /* } */
}
#endif

void
gtkShowFinalScores( GtkGameGlobals* globals, XP_Bool ignoreTimeout )
{
    XWStreamCtxt* stream;
    XP_UCHAR* text;
    const CommonGlobals* cGlobals = &globals->cGlobals;
    LaunchParams* params = cGlobals->params;

    stream = mem_stream_make_raw( MPPARM(params->mpool)
                                  params->vtMgr );
    XW_DUtilCtxt* dutil = params->dutil;
    gr_writeFinalScores( dutil, cGlobals->gr, NULL_XWE, stream );

    text = strFromStream( stream );
    stream_destroy( stream );

    XP_U16 timeout = (ignoreTimeout || cGlobals->manualFinal)
        ? 0 : params->askTimeout;
    XP_Bool archive, delete;
    if ( gtkAskGameOver( globals->window, text, timeout, &archive, &delete ) ) {
        make_rematch( (GtkAppGlobals*)params->cag, cGlobals->gr, archive, delete );
    } else if ( delete ) {
        gchar* message = "Are you sure you want to delete this game?";
        if ( gtkask_confirm( globals->window, message ) ) {
            gmgr_deleteGame( dutil, NULL_XWE, cGlobals->gr );
            gtktell( globals->window, "Not sure deleting an open game works..." );
            // destroy_board_window( NULL, globals );
        }
    }
    free( text );
} /* gtkShowFinalScores */

static void
gtk_util_notifyDupStatus( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe), XP_Bool XP_UNUSED(amHost),
                          const XP_UCHAR* msg )
{
    LOG_FUNC();
    CommonGlobals* cGlobals = globalsForUtil( uc, XP_FALSE );
    GtkGameGlobals* globals = (GtkGameGlobals*)cGlobals;

    XP_UCHAR buf[256];
    XP_SNPRINTF( buf, VSIZE(buf), "notifyDupStatus(): msg: %s", msg );
    (void)gtkask( globals->window, buf, GTK_BUTTONS_OK, NULL );
}

static void
gtk_util_informUndo( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe) )
{
    CommonGlobals* cGlobals = globalsForUtil( uc, XP_FALSE );
    GtkGameGlobals* globals = (GtkGameGlobals*)cGlobals;

    (void)gtkask_timeout( globals->window, "Remote player undid a move",
                          GTK_BUTTONS_OK, NULL, 500 );
}

static void
gtk_util_informNetDict( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe),
                        const XP_UCHAR* XP_UNUSED(isoCode),
                        const XP_UCHAR* oldName,
                        const XP_UCHAR* newName, const XP_UCHAR* newSum,
                        XWPhoniesChoice phoniesAction )
{
    if ( 0 != strcmp( oldName, newName ) ) {
        CommonGlobals* cGlobals = globalsForUtil( uc, XP_FALSE );
        GtkGameGlobals* globals = (GtkGameGlobals*)cGlobals;

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
gtk_util_hiliteCell( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe),
                     XP_U16 col, XP_U16 row )
{
    CommonGlobals* cGlobals = globalsForUtil( uc, XP_FALSE );
    if ( !!cGlobals ) {
        // GtkGameGlobals* globals = (GtkGameGlobals*)cGlobals;
        LaunchParams* params = cGlobals->params;
#ifndef DONT_ABORT_ENGINE
        gboolean pending;
#endif
        XW_DUtilCtxt* dutil = params->dutil;
        gr_hiliteCellAt( dutil, cGlobals->gr, NULL_XWE, col, row );
        if ( params->sleepOnAnchor ) {
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
    } else {
        return XP_FALSE;
    }
} /* gtk_util_hiliteCell */
#endif

static XP_Bool
gtk_util_altKeyDown( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe) )
{
    CommonGlobals* cGlobals = globalsForUtil( uc, XP_FALSE );
    GtkGameGlobals* globals = (GtkGameGlobals*)cGlobals;
    return globals->altKeyDown;
}

#ifdef XWFEATURE_STOP_ENGINE
static XP_Bool
gtk_util_stopEngineProgress( XW_UtilCtxt* XP_UNUSED(uc), XWEnv XP_UNUSED(xwe) )
{
#ifdef DONT_ABORT_ENGINE
    return XP_FALSE;		/* don't abort; keep going */
#else
    gboolean pending = gdk_events_pending();

/*     XP_DEBUGF( "gdk_events_pending returned %d\n", pending ); */

    return !pending;
#endif
} /* gtk_util_stopEngineProgress */
#endif

typedef struct _BadWordsData {
    GtkGameGlobals* globals;
    XP_U32 bwKey;
    GStrv words;
    gchar* dictName;
} BadWordsData;

static gint
ask_bad_words( gpointer data )
{
    BadWordsData* bwd = (BadWordsData*)data;
    CommonGlobals* cGlobals = &bwd->globals->cGlobals;

    bool skipNext = false;
    if ( gtkAskBad( bwd->globals, bwd->words, bwd->dictName, &skipNext ) ) {
        PhoniesConf pc = {
            .confirmed = XP_TRUE,
            .key = skipNext ? bwd->bwKey : 0,
        };
        XW_DUtilCtxt* dutil = cGlobals->params->dutil;
        gr_commitTurn( dutil, cGlobals->gr, NULL_XWE, &pc, XP_FALSE, NULL );
    }

    g_free( bwd->dictName );
    g_strfreev( bwd->words );
    XP_FREE( cGlobals->params->mpool, bwd );
    return 0;
}

static void
gtk_util_notifyIllegalWords( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe),
                             const BadWordInfo* bwi,
                             const XP_UCHAR* dictName,
                             XP_U16 player, XP_Bool turnLost,
                             XP_U32 bwKey )
{
    CommonGlobals* cGlobals = globalsForUtil( uc, XP_FALSE );
    GtkGameGlobals* globals = (GtkGameGlobals*)cGlobals;

    if ( turnLost ) {
        XP_ASSERT( 0 == bwKey );
        char buf[300];
        gchar* strs = g_strjoinv( "\", \"", (gchar**)bwi->words );

        const XP_UCHAR* name = cGlobals->gi->players[player].name;
        XP_ASSERT( !!name[0] );

        sprintf( buf, "Player %d (%s) played illegal word[s] \"%s\"; loses turn.",
                 player+1, name, strs );

        if ( cGlobals->params->skipWarnings ) {
            XP_LOGFF( "%s", buf );
        }  else {
            gtkUserError( globals, buf );
        }
        g_free( strs );
    } else {
        BadWordsData* bwd = XP_MALLOC( cGlobals->params->mpool, sizeof(*bwd) );
        bwd->globals = globals;
        bwd->dictName = g_strdup( dictName );
        bwd->bwKey = bwKey;

        GStrvBuilder* builder = g_strv_builder_new();
        for ( const char* const* word = bwi->words; !!*word; ++word ) {
            g_strv_builder_add( builder, *word );
        }
        bwd->words = g_strv_builder_end( builder );
        g_strv_builder_unref( builder );

        (void)g_idle_add( ask_bad_words, bwd );
    }
} /* gtk_util_notifyIllegalWords */

static void
gtk_util_remSelected( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe) )
{
    CommonGlobals* cGlobals = globalsForUtil( uc, XP_FALSE );
    GtkGameGlobals* globals = (GtkGameGlobals*)cGlobals;
    XWStreamCtxt* stream;
    XP_UCHAR* text;

    stream = mem_stream_make_raw( MPPARM(cGlobals->params->mpool)
                                  cGlobals->params->vtMgr );
    
    XW_DUtilCtxt* dutil = cGlobals->params->dutil;
    gr_formatRemainingTiles( dutil, cGlobals->gr, NULL_XWE, stream );
    text = strFromStream( stream );
    stream_destroy( stream );

    (void)gtkask( globals->window, text, GTK_BUTTONS_OK, NULL );
    free( text );
}

static void
gtk_util_timerSelected( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe), XP_Bool inDuplicateMode,
                        XP_Bool canPause )
{
    if ( inDuplicateMode ) {
        CommonGlobals* cGlobals = globalsForUtil( uc, XP_FALSE );
        GtkGameGlobals* globals = (GtkGameGlobals*)cGlobals;
        if ( canPause ) {
            handle_pause_button( NULL, globals );
        } else {
            handle_unpause_button( NULL, globals );
        }
    }
}

static XP_Bool
gtk_util_showChat( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe),
                   const XP_UCHAR* const XP_UNUSED(msg),
                   XP_S16 XP_UNUSED(from), XP_U32 XP_UNUSED(tsSecs) )
{
    CommonGlobals* cGlobals = globalsForUtil( uc, XP_FALSE );
    XP_Bool shown = !!cGlobals;
    if ( shown ) {
        launchChat( (GtkGameGlobals*)cGlobals );
    }
    return shown;
}

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
    CommonGlobals* cGlobals = globalsForUtil( uc, XP_FALSE );
    GtkGameGlobals* globals = (GtkGameGlobals*)cGlobals;

    gchar* msg = g_strdup_printf( "bonusSquareHeld(bonus=%d)", bonus );
    gtkask_timeout( globals->window, msg, GTK_BUTTONS_OK, NULL, 1000 );
    g_free( msg );
}

static void
gtk_util_playerScoreHeld( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe), XP_U16 player )
{
    LOG_FUNC();
    CommonGlobals* cGlobals = globalsForUtil( uc, XP_FALSE );
    GtkGameGlobals* globals = (GtkGameGlobals*)cGlobals;

    LastMoveInfo lmi;
    XW_DUtilCtxt* dutil = cGlobals->params->dutil;
    if ( gr_getPlayersLastScore( dutil, cGlobals->gr,
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
    CommonGlobals* cGlobals = globalsForUtil( uc, XP_FALSE );
    GtkGameGlobals* globals = (GtkGameGlobals*)cGlobals;
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
    CommonGlobals* cGlobals = globalsForUtil( uc, XP_FALSE );
    GtkGameGlobals* globals = (GtkGameGlobals*)cGlobals;

    gchar* msg = g_strdup_printf( "%d word[s] not found in %s:\n%s", nBadWords, dict, buf );
    gtkUserError( globals, msg );
    g_free( msg );
}

static void
gtk_util_userError( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe), UtilErrID id )
{
    CommonGlobals* cGlobals = globalsForUtil( uc, XP_FALSE );
    GtkGameGlobals* globals = (GtkGameGlobals*)cGlobals;

    XP_Bool silent;
    const XP_UCHAR* message = linux_getErrString( id, &silent );

    XP_LOGFF( "%s", message );
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
        GameRef gr = cGlobals->gr;
        PhoniesConf pc = { .confirmed = XP_TRUE };
        XW_DUtilCtxt* dutil = cGlobals->params->dutil;
        gr_commitTurn( dutil, gr, NULL_XWE, &pc, XP_TRUE, NULL );
        /*     gr_draw( dutil, gr, NULL_XWE ); */
        /* } */
    }
    return 0;
}

static void
gtk_util_countChanged( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe),
                       XP_U16 newCount, XP_Bool quashed )
{
    CommonGlobals* cGlobals = globalsForUtil( uc, XP_FALSE );
    if ( !!cGlobals ) {
        GtkGameGlobals* globals = (GtkGameGlobals*)cGlobals;

        gchar buf[128];
        snprintf( buf, VSIZE(buf), "pending count: %d%s", newCount,
                  quashed?"q":"");
        gtk_label_set_text( GTK_LABEL(globals->countLabel), buf );
    }
}

static void
gtk_util_notifyMove( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe), XWStreamCtxt* stream )
{
    CommonGlobals* cGlobals = globalsForUtil( uc, XP_FALSE );
    GtkGameGlobals* globals = (GtkGameGlobals*)cGlobals;
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
        PhoniesConf pc = { .confirmed = XP_TRUE };
        GameRef gr = cGlobals->gr;
        XW_DUtilCtxt* dutil = cGlobals->params->dutil;
        gr_commitTurn( dutil, gr, NULL_XWE, &pc, XP_TRUE, NULL );
    }
    return 0;
}

static void
gtk_util_notifyTrade( XW_UtilCtxt* uc, XWEnv XP_UNUSED(xwe),
                      const XP_UCHAR** tiles, XP_U16 nTiles )
{
    CommonGlobals* cGlobals = globalsForUtil( uc, XP_FALSE );
    GtkGameGlobals* globals = (GtkGameGlobals*)cGlobals;

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

    globals->chat_button = addVBarButton( globals, "", "Chat",
                                          G_CALLBACK(handle_chat_button), vbox );

    globals->pause_button = addVBarButton( globals, "", "Pause",
                                           G_CALLBACK(handle_pause_button), vbox );
    globals->unpause_button = addVBarButton( globals, "", "Unpause",
                                             G_CALLBACK(handle_unpause_button), vbox );

    gtk_widget_show( vbox );
    return vbox;
} /* makeVerticalBar */

static GtkWidget*
addButton( GtkWidget* hbox, gchar* label, GCallback func, void* closure )

{
    GtkWidget* button = gtk_button_new_with_label( label );
    gtk_widget_show( button );
    g_signal_connect( button, "clicked", G_CALLBACK(func), closure );
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

void
setupGtkUtilCallbacks( XW_UtilCtxt* util )
{
    // util->closure = globals;
#define SET_PROC(NAM) util->vtable->m_util_##NAM = gtk_util_##NAM
    SET_PROC(userError);
    SET_PROC(countChanged);
    SET_PROC(notifyMove);
    SET_PROC(notifyTrade);
    SET_PROC(notifyPickTileBlank);
    SET_PROC(informNeedPickTiles);
    SET_PROC(informNeedPassword);
    SET_PROC(trayHiddenChange);
    SET_PROC(yOffsetChange);
    SET_PROC(dictGone);
#ifdef XWFEATURE_TURNCHANGENOTIFY
    SET_PROC(turnChanged);
#endif
    SET_PROC(notifyDupStatus);
    SET_PROC(informUndo);
    SET_PROC(informNetDict);
#ifdef XWFEATURE_HILITECELL
    SET_PROC(hiliteCell);
#endif
    SET_PROC(altKeyDown);
#ifdef XWFEATURE_STOP_ENGINE
    SET_PROC(stopEngineProgress);
#endif
    SET_PROC(notifyIllegalWords);
    SET_PROC(remSelected);
    SET_PROC(timerSelected);
    SET_PROC(showChat);
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

static void
freeGTKBoardGlobals( CommonGlobals* cGlobals )
{
    LOG_FUNC();
    XP_ASSERT( 0 == cGlobals->refCount );
    freeGlobals( (GtkGameGlobals*)cGlobals );
}

CommonGlobals*
allocGTKBoardGlobals()
{
    GtkGameGlobals* globals = g_malloc0( sizeof(*globals) );
    CommonGlobals* cGlobals = &globals->cGlobals;
    XP_LOGFF( "allocated cGlobals: %p", cGlobals );
    cg_init( cGlobals, freeGTKBoardGlobals );
    return cGlobals;
}

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
        XP_LOGFF( "input on socket %d", listener );
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
        XP_FREE( globals->params->mpool, info );
        *storage = NULL;
        XP_LOGFF( "Removed listener %d from gtk's list of listened-to sockets", oldSock );
    } else {
        XP_ASSERT( !globals->acceptor || (func == globals->acceptor) );
        globals->acceptor = func;

        channel = g_io_channel_unix_new( listener );
        g_io_channel_set_close_on_unref( channel, TRUE );
        watch = g_io_add_watch( channel,
                                G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_PRI,
                                acceptorInput, globals );
        g_io_channel_unref( channel ); /* only main loop holds it now */
        XP_LOGFF( "g_io_add_watch(%d) => %d", listener, watch );

        XP_ASSERT( NULL == info );
        info = XP_MALLOC( globals->params->mpool, sizeof(*info) );
        info->channel = channel;
        info->watch = watch;
        info->socket = listener;
        *storage = info;
    }
} /* gtk_socket_acceptor */

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
                   GameRef gr )
{
    CommonGlobals* cGlobals = &globals->cGlobals;
    XP_ASSERT( !cGlobals->params || cGlobals->params == params );
    cGlobals->params = params;

    // #ifndef XWFEATURE_DEVICE_STORES
    XP_ASSERT( !cGlobals->gr || cGlobals->gr == gr );
    cGlobals->gr = gr;
    cGlobals->gi = gr_getGI(params->dutil, gr, NULL_XWE );
    XP_ASSERT( !!cGlobals->gi );

    cGlobals->params = params;
    cGlobals->lastNTilesToUse = MAX_TRAY_TILES;
#ifdef XWFEATURE_RELAY
    cGlobals->relaySocket = -1;
#endif

    cGlobals->socketAddedClosure = globals;
    cGlobals->onSave = gtkOnGameSaved;
    cGlobals->onSaveClosure = globals;
    cGlobals->addAcceptor = gtk_socket_acceptor;

    cpFromLP( &cGlobals->cp, params );

    makeSelfAddress( &cGlobals->selfAddr, params );
}

static gint
draw_idle( gpointer data )
{
    CommonGlobals* cGlobals = (CommonGlobals*)data;
    gr_draw( cGlobals->params->dutil, cGlobals->gr, NULL_XWE );
    return 0;
}

/* This gets called all the time, e.g. when the mouse moves across
   drawing-area boundaries. So invalidating is crazy expensive. But this is a
   test app....*/

static gboolean
on_draw_event( GtkWidget* widget, cairo_t* cr, gpointer user_data )
{
    XP_USE(widget);
    XP_LOGFF( "(cairo=%p)", cr );

    GtkGameGlobals* globals = (GtkGameGlobals*)user_data;
    CommonGlobals* cGlobals = &globals->cGlobals;
    GtkDrawCtx* draw = (GtkDrawCtx*)cGlobals->draw;
    if ( gtk_draw_does_offscreen(draw) ) {
        cairo_surface_t* surface = gtk_draw_get_surface( draw );
        if ( !!surface ) {
            cairo_set_source_surface( cr, surface, 0, 0 );
            cairo_paint( cr );
            cairo_show_page( cr );
        } else {
            g_idle_add( draw_idle, cGlobals );
        }
    } else {
        XW_DUtilCtxt* dutil = cGlobals->params->dutil;
        gr_invalAll( dutil, cGlobals->gr, NULL_XWE );
        gr_draw( dutil, cGlobals->gr, NULL_XWE );
        draw_gtk_status( (GtkDrawCtx*)(void*)cGlobals->draw, globals->stateChar );
    }

    return FALSE;
}

void
initBoardGlobalsGtk( GtkGameGlobals* globals, LaunchParams* params, GameRef gr )
{
    short width, height;
    GtkWidget* window;
    GtkWidget* drawing_area;
    GtkWidget* menubar;
    GtkWidget* vbox;
    GtkWidget* hbox;

    initGlobalsNoDraw( globals, params, gr );

    globals->window = window = gtk_window_new( GTK_WINDOW_TOPLEVEL );
    CommonAppGlobals* cag = globals->cGlobals.params->cag;
    GtkWidget* parent = ((GtkAppGlobals*)cag)->window;
    gtk_window_set_transient_for( GTK_WINDOW(globals->window),
                                  GTK_WINDOW(parent) );

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

#ifdef DEBUG
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
    CommonGlobals* cGlobals = &globals->cGlobals;
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
    LOG_FUNC();
    cleanup( globals );
    LOG_RETURN_VOID();
}

XP_Bool
loadGameNoDraw( GtkGameGlobals* globals, LaunchParams* params, 
                sqlite3_int64 rowid )
{
    XP_ASSERT(0);
    /* this function probably goes away. It exists to do what should be
       needed once game management is handled in common/ code */
    XP_ASSERT(0);
    XP_LOGFF( "(rowid: %llX)", rowid );
    sqlite3* pDb = params->pDb;
    initGlobalsNoDraw( globals, params, 0 );

    CommonGlobals* cGlobals = &globals->cGlobals;
    // cGlobals->rowid = rowid;
    XWStreamCtxt* stream = mem_stream_make_raw( MPPARM(cGlobals->params->mpool)
                                                params->vtMgr );
    XP_Bool loaded = gdb_loadGame( stream, pDb, NULL, rowid );
    if ( loaded ) {
        XW_DUtilCtxt* dutil = params->dutil;
        cGlobals->gr = dvc_makeFromStream( dutil,
                                                NULL_XWE, stream, cGlobals->gi,
                                                NULL, (DrawCtx*)NULL,
                                                &cGlobals->cp );
        loaded = !!cGlobals->gr;
        if ( loaded ) {
            XP_LOGFF( "game loaded" );
            gr_resendAll( dutil, globals->cGlobals.gr, NULL_XWE, COMMS_CONN_NONE,
                          XP_FALSE );
        } else {
            // game_dispose( &cGlobals->game, NULL_XWE );
        }
    }
    stream_destroy( stream );
    LOG_RETURNF( "%s", boolToStr(loaded) );
    return loaded;
}
#endif /* PLATFORM_GTK */
