/* -*-mode: C; fill-column: 78; c-basic-offset: 4; compile-command: "make MEMDEBUG=TRUE"; -*- */
/* 
 * Copyright 2001-2008 by Eric House (xwords@eehouse.org).  All rights
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


#include "gtkconnsdlg.h"
#include "gtkutils.h"

typedef struct _GtkConnsState {
    GtkAppGlobals* globals;
    CommsAddrRec* addr;
    DeviceRole role;

    GtkWidget* invite;
    GtkWidget* hostName;
    GtkWidget* port;
    GtkWidget* bthost;

    GtkWidget* notebook;
    CommsConnType pageTypes[COMMS_CONN_NTYPES];

    gboolean cancelled;
    gboolean readOnly;
} GtkConnsState;

static gint
conTypeToPageNum( const GtkConnsState* state, CommsConnType conType )
{
    gint pageNum = 0;           /* default */
    int ii;
    for ( ii = 0; ; ++ii ) {
        CommsConnType thisType = state->pageTypes[ii];
        if ( thisType == COMMS_CONN_NONE || thisType == conType ) {
            pageNum = ii;
            break;
        }
        XP_ASSERT( ii < VSIZE(state->pageTypes) );
    }
    return pageNum;
}

static CommsConnType
pageNoToConnType( const GtkConnsState* state, gint page )
{
    XP_ASSERT( page < VSIZE(state->pageTypes) );
    XP_ASSERT( state->pageTypes[page] != COMMS_CONN_NONE );
    return state->pageTypes[page];
}

static void
handle_ok( GtkWidget* XP_UNUSED(widget), gpointer closure )
{
    GtkConnsState* state = (GtkConnsState*)closure;
    if ( !state->readOnly ) {
        const gchar* txt;
        gint page = gtk_notebook_get_current_page
            ( GTK_NOTEBOOK(state->notebook) );
        CommsConnType conType = pageNoToConnType( state, page );

        switch ( conType ) {
        case COMMS_CONN_IP_DIRECT:
#ifdef XWFEATURE_RELAY
        case COMMS_CONN_RELAY:
            txt = gtk_entry_get_text( GTK_ENTRY(state->invite) );
            XP_STRNCPY( state->addr->u.ip_relay.invite, txt, 
                        sizeof(state->addr->u.ip_relay.invite) );
            txt = gtk_entry_get_text( GTK_ENTRY(state->hostName) );
            XP_STRNCPY( state->addr->u.ip_relay.hostName, txt,
                        sizeof(state->addr->u.ip_relay.hostName) );

            txt = gtk_entry_get_text( GTK_ENTRY(state->port) );
            state->addr->u.ip_relay.port = atoi( txt );
            break;
#endif
#ifdef XWFEATURE_BLUETOOTH
        case COMMS_CONN_BT:
            break;
#endif
        case COMMS_CONN_SMS:
            break;
        default:
            XP_ASSERT( 0 );     /* keep compiler happy */
            break;
        }

        state->addr->conType = conType;
    }
        
    state->cancelled = XP_FALSE;
    gtk_main_quit();
} /* handle_ok */

static void
handle_cancel( GtkWidget* XP_UNUSED(widget), void* closure )
{
    GtkConnsState* state = (GtkConnsState*)closure;
    state->cancelled = XP_TRUE;
    gtk_main_quit();
}

/*
 * Invite: _____
 * Relay:  _____
 * Port:   _____
 * Cancel OK
 */

static GtkWidget*
makeRelayPage( GtkConnsState* state )
{
    GtkWidget* vbox = gtk_vbox_new( FALSE, 0 );
    const gchar* hint = NULL;

    if ( SERVER_ISSERVER == state->role ) {
        hint = "As host, you pick the room name for the game, and must "
            "connect first";
    } else {
        XP_ASSERT( SERVER_ISCLIENT == state->role );
        hint = "As guest, you get the room name from the host.   Be sure to "
            "let the host connect first to validate the name.";
    }

    gtk_box_pack_start( GTK_BOX(vbox), gtk_label_new( hint ), FALSE, TRUE, 0 );

    GtkWidget* hbox = makeLabeledField( "Room", &state->invite );
    gtk_entry_set_text( GTK_ENTRY(state->invite), 
                        state->addr->u.ip_relay.invite );
    gtk_box_pack_start( GTK_BOX(vbox), hbox, FALSE, TRUE, 0 );
    gtk_widget_set_sensitive( state->invite, !state->readOnly );

    hbox = makeLabeledField( "Relay address", &state->hostName );
    gtk_entry_set_text( GTK_ENTRY(state->hostName), 
                        state->addr->u.ip_relay.hostName );
    gtk_box_pack_start( GTK_BOX(vbox), hbox, FALSE, TRUE, 0 );
    gtk_widget_set_sensitive( state->hostName, !state->readOnly );

    hbox = makeLabeledField( "Relay port", &state->port );
    char buf[16];
    snprintf( buf, sizeof(buf), "%d", state->addr->u.ip_relay.port );
    gtk_entry_set_text( GTK_ENTRY(state->port), buf );
    gtk_box_pack_start( GTK_BOX(vbox), hbox, FALSE, TRUE, 0 );
    gtk_widget_set_sensitive( state->port, !state->readOnly );

    gtk_widget_show( vbox );
    
    return vbox;
} /* makeRelayPage */

static GtkWidget*
makeBTPage( GtkConnsState* state )
{
    GtkWidget* vbox = gtk_vbox_new( FALSE, 0 );

    GtkWidget* hbox = makeLabeledField( "Host device", &state->bthost );
    gtk_entry_set_text( GTK_ENTRY(state->bthost), state->addr->u.bt.hostName );
    gtk_box_pack_start( GTK_BOX(vbox), hbox, FALSE, TRUE, 0 );
    gtk_widget_set_sensitive( state->bthost, !state->readOnly );

    gtk_widget_show( vbox );

    return vbox;
} /* makeBTPage */

gboolean
gtkConnsDlg( GtkAppGlobals* globals, CommsAddrRec* addr, DeviceRole role,
             XP_Bool readOnly )
{
    GtkConnsState state;
    XP_MEMSET( &state, 0, sizeof(state) );
    gint loc;
    XP_U16 nTypes = 0;

    state.readOnly = readOnly;
    state.globals = globals;
    state.addr = addr;
    state.role = role;

    GtkWidget* dialog;
    GtkWidget* vbox;
    GtkWidget* hbox;

    state.notebook = gtk_notebook_new();

#ifdef XWFEATURE_RELAY
    state.pageTypes[nTypes++] = COMMS_CONN_RELAY;
    loc = gtk_notebook_append_page( GTK_NOTEBOOK(state.notebook), 
                                    makeRelayPage(&state),
                                    gtk_label_new( "Relay" ) );
#endif
#ifdef XWFEATURE_BLUETOOTH
    state.pageTypes[nTypes++] = COMMS_CONN_BT;
    loc = gtk_notebook_append_page( GTK_NOTEBOOK(state.notebook),
                                    makeBTPage(&state),
                                    gtk_label_new( "Bluetooth" ) );
#endif

    vbox = gtk_vbox_new( FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(vbox), state.notebook, FALSE, TRUE, 0 );
    state.pageTypes[nTypes++] = COMMS_CONN_NONE; /* mark end of list */

    gint pageNo = conTypeToPageNum( &state, addr->conType );
    gtk_notebook_set_current_page( GTK_NOTEBOOK(state.notebook), pageNo );

    gtk_widget_show( state.notebook );

    /* buttons at the bottom */
    hbox = gtk_hbox_new( FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(hbox), 
                        makeButton( "Ok", GTK_SIGNAL_FUNC(handle_ok) , &state ),
                        FALSE, TRUE, 0 );
    if ( !readOnly ) {
        gtk_box_pack_start( GTK_BOX(hbox), 
                            makeButton( "Cancel", 
                                        GTK_SIGNAL_FUNC(handle_cancel), 
                                        &state ),
                            FALSE, TRUE, 0 );
    }
    gtk_box_pack_start( GTK_BOX(vbox), hbox, FALSE, TRUE, 0 );

    gtk_widget_show( vbox );

    dialog = gtk_dialog_new();
    gtk_window_set_modal( GTK_WINDOW( dialog ), TRUE );
    gtk_container_add( GTK_CONTAINER( GTK_DIALOG(dialog)->action_area), vbox );

    gtk_widget_show_all( dialog );
    gtk_main();
    gtk_widget_destroy( dialog );

    return !state.cancelled;
} /* gtkConnsDlg */
#endif
