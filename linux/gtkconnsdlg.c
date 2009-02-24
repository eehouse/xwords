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

    GtkWidget* cookie;
    GtkWidget* hostName;
    GtkWidget* port;

    gboolean cancelled;
    gboolean readOnly;
} GtkConnsState;

static void
handle_ok( GtkWidget* XP_UNUSED(widget), gpointer closure )
{
    GtkConnsState* state = (GtkConnsState*)closure;
    const gchar* txt;

    txt = gtk_entry_get_text( GTK_ENTRY(state->cookie) );
    XP_STRNCPY( state->addr->u.ip_relay.cookie, txt, 
                sizeof(state->addr->u.ip_relay.cookie) );
    txt = gtk_entry_get_text( GTK_ENTRY(state->hostName) );
    XP_STRNCPY( state->addr->u.ip_relay.hostName, txt,
                sizeof(state->addr->u.ip_relay.hostName) );

    txt = gtk_entry_get_text( GTK_ENTRY(state->port) );
    state->addr->u.ip_relay.port = atoi( txt );

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
 * Cookie: _____
 * Relay:  _____
 * Port:   _____
 * Cancel OK
 */

gboolean
gtkConnsDlg( GtkAppGlobals* globals, CommsAddrRec* addr, XP_Bool readOnly )
{
    LOG_FUNC();

    GtkConnsState state;
    XP_MEMSET( &state, 0, sizeof(state) );

    state.readOnly = readOnly;
    state.globals = globals;
    state.addr = addr;

    GtkWidget* dialog;
    GtkWidget* vbox;
    GtkWidget* hbox;

    vbox = gtk_vbox_new( FALSE, 0 );

    hbox = makeLabeledField( "Cookie", &state.cookie );
    gtk_entry_set_text( GTK_ENTRY(state.cookie), state.addr->u.ip_relay.cookie );
    gtk_box_pack_start( GTK_BOX(vbox), hbox, FALSE, TRUE, 0 );
    hbox = makeLabeledField( "Relay address", &state.hostName );
    gtk_entry_set_text( GTK_ENTRY(state.hostName), 
                        state.addr->u.ip_relay.hostName );
    gtk_box_pack_start( GTK_BOX(vbox), hbox, FALSE, TRUE, 0 );
    hbox = makeLabeledField( "Relay port", &state.port );
    char buf[16];
    snprintf( buf, sizeof(buf), "%d", state.addr->u.ip_relay.port );
    gtk_entry_set_text( GTK_ENTRY(state.port), buf );
    gtk_box_pack_start( GTK_BOX(vbox), hbox, FALSE, TRUE, 0 );
    
    /* buttons at the bottom */
    hbox = gtk_hbox_new( FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(hbox), 
                        makeButton( "Ok", GTK_SIGNAL_FUNC(handle_ok) , &state ),
                        FALSE, TRUE, 0 );
    gtk_box_pack_start( GTK_BOX(hbox), 
                        makeButton( "Cancel", GTK_SIGNAL_FUNC(handle_cancel), 
                                    &state ),
                        FALSE, TRUE, 0 );
    gtk_box_pack_start( GTK_BOX(vbox), hbox, FALSE, TRUE, 0 );


    gtk_widget_show( vbox );

    dialog = gtk_dialog_new();
    gtk_window_set_modal( GTK_WINDOW( dialog ), TRUE );
    gtk_container_add( GTK_CONTAINER( GTK_DIALOG(dialog)->action_area), vbox );


    gtk_widget_show_all( dialog );
    gtk_main();
    gtk_widget_destroy( dialog );

    return !state.cancelled;
}
#endif
