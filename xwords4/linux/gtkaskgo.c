/* 
 * Copyright 2025 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#include "gtkaskgo.h"
#include "comtypes.h"

typedef struct _GOState {
    GtkWidget* lastSel;
} GOState;

static gint
timer_func( gpointer data )
{
    GtkWidget* dlg = (GtkWidget*)data;
    gtk_widget_destroy( dlg );
    return 0;
}

static void
toggled( GtkToggleButton* togglebutton, gpointer user_data )
{
    GOState* state = (GOState*)user_data;
    state->lastSel = GTK_WIDGET(togglebutton);
    XP_LOGFF( "togglebutton now %p", state->lastSel );
}

XP_Bool
gtkAskGameOver( GtkWidget* parent, const XP_UCHAR* text,
                XP_U16 timeoutMS, XP_Bool* archiveP, XP_Bool* deleteP )
{
    guint src = 0;
    const char* groups[] = { "Leave in place", "Archive", "Delete", };
    GtkWidget* radios[VSIZE(groups)];
    GOState state = {};

    GtkWidget* dialog =
        gtk_dialog_new_with_buttons( NULL, GTK_WINDOW(parent),
                                     GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                     "Rematch", GTK_RESPONSE_ACCEPT,
                                     "Ok", GTK_RESPONSE_REJECT,
                                     NULL );
    GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

    GtkWidget* label = gtk_label_new( text );
    gtk_box_pack_start( GTK_BOX(content), GTK_WIDGET(label), FALSE, TRUE, 0 );

    GtkWidget* prev = NULL;
    for ( int ii = 0; ii < VSIZE(groups); ++ii ) {
        const gchar* txt = groups[ii];
        GtkWidget* radio = radios[ii] = 
            gtk_radio_button_new_with_label_from_widget( GTK_RADIO_BUTTON(prev),
                                                         txt );
        g_signal_connect( radio, "toggled", G_CALLBACK(toggled), &state );
        gtk_box_pack_start( GTK_BOX(content), GTK_WIDGET(radio), FALSE,
                            TRUE, 0 );
        prev = radio;
    }
    /* set the first radio */
    state.lastSel = radios[0];
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(state.lastSel), XP_TRUE );

    if ( timeoutMS > 0 ) {
        src = g_timeout_add( timeoutMS, timer_func, dialog );
    }

    gtk_widget_show_all( dialog );
    gint response = gtk_dialog_run( GTK_DIALOG(dialog) );
    *archiveP = state.lastSel == radios[1];
    *deleteP = state.lastSel == radios[2];

    if ( 0 != src ) {
        g_source_remove( src );
    }
    gtk_widget_destroy( dialog );
    
    return GTK_RESPONSE_ACCEPT == response;
}

#endif
