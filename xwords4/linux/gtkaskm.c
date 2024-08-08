/* -*-mode: C; fill-column: 78; compile-command: "make MEMDEBUG=TRUE"; -*- */

/* 
 * Copyright 2013 by Eric House (xwords@eehouse.org).  All rights reserved.
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
n *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#ifdef PLATFORM_GTK

#include <stdarg.h>

#include "gtkaskm.h"
#include "gtkutils.h"

typedef struct _AskMState {
    GtkWidget* okButton;
    GtkWidget* cancelButton;
    gboolean cancelled;
} AskMState;

static void
button_clicked( GtkWidget* widget, gpointer closure )
{
    AskMState* state = (AskMState*)closure;
    state->cancelled = widget == state->cancelButton;
    XP_LOGF( "%s: cancelled = %d", __func__, state->cancelled );
    gtk_main_quit();
}

XP_Bool
gtkaskm( const gchar* message, AskMInfo* infos, int nInfos )
{
    AskMState state = {};
    GtkWidget* dialog = gtk_dialog_new();
    GtkWidget* fields[nInfos];
    gtk_window_set_modal( GTK_WINDOW( dialog ), TRUE );

    gtk_window_set_title( GTK_WINDOW(dialog), message );

    int ii;
    GtkWidget* vbox = gtk_box_new( GTK_ORIENTATION_VERTICAL, 0 );//gtk_vbox_new
    for ( ii = 0; ii < nInfos; ++ii ) {
        AskMInfo* info = &infos[ii];
        GtkWidget* row = makeLabeledField( info->label, &fields[ii], *info->result );
        gtk_box_pack_start( GTK_BOX(vbox), row, FALSE, TRUE, 0 );
        gtk_widget_show( row );
    }

    GtkWidget* hbox = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 0 );
    state.okButton = gtk_button_new_with_label( "Ok" );
    g_signal_connect( state.okButton, "clicked", 
                      G_CALLBACK(button_clicked), &state );
    gtk_box_pack_start( GTK_BOX(hbox), state.okButton, FALSE, TRUE, 0 );
    state.cancelButton = gtk_button_new_with_label( "Cancel" );
    g_signal_connect( state.cancelButton, "clicked", 
                      G_CALLBACK(button_clicked), &state );
    gtk_box_pack_start( GTK_BOX(hbox), state.cancelButton, FALSE, TRUE, 0 );
    gtk_box_pack_start( GTK_BOX(vbox), hbox, FALSE, TRUE, 0 );

    /* gtk_container_add(GTK_CONTAINER(gtk_dialog_get_action_area(GTK_DIALOG(dialog))), */
    /*                   vbox); */
    XP_LOGF( "%s(): not adding vbox!!!", __func__ );
    XP_ASSERT(0);
    gtk_widget_show_all( dialog );

    gtk_main();

    for ( ii = 0; ii < nInfos; ++ii ) {
        AskMInfo* info = &infos[ii];
        if ( !state.cancelled ) {
            XP_LOGF( "%s: got text %s", __func__, 
                     gtk_entry_get_text( GTK_ENTRY(fields[ii]) ) );
        } else {
            *info->result = NULL;
        }
    }

    gtk_widget_destroy( dialog );

    return !state.cancelled;
    // return response == GTK_RESPONSE_OK || response == GTK_RESPONSE_YES;
} /* gtkask */

#endif
