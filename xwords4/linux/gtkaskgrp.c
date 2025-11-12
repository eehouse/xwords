/* 
 * Copyright 2025 by Eric House (xwords@eehouse.org).  All rights
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

#include "gtkaskgrp.h"
// #include "gtkboard.h"

typedef struct _State {
    XP_U16 nGroups;
    // GtkWidget* radios[];
    GtkToggleButton* lastSel;
} State;

static void
toggled( GtkToggleButton* togglebutton, gpointer user_data )
{
    State* state = (State*)user_data;
    state->lastSel = togglebutton;
    XP_LOGFF( "togglebutton now %p", state->lastSel );
}

/* return true if not cancelled */
bool
gtkAskGroup( GtkWidget* parent, XP_UCHAR* groupNames[],
             XP_U16 nGroups, XP_U16* selIndex )
{
    State state = {.nGroups = nGroups,};
    GtkWidget* radios[nGroups];
    // state.radios = radios;

    GtkWidget* dialog =
        gtk_dialog_new_with_buttons( NULL, GTK_WINDOW(parent),
                                     GTK_DIALOG_MODAL
                                     | GTK_DIALOG_DESTROY_WITH_PARENT,
                                     "Ok", GTK_RESPONSE_ACCEPT,
                                     "Cancel", GTK_RESPONSE_REJECT,
                                     NULL );
    GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

    GtkWidget* prev = NULL;
    for ( int ii = 0; ii < nGroups; ++ii ) {
        const gchar* txt = groupNames[ii];
        GtkWidget* radio = radios[ii] = 
            gtk_radio_button_new_with_label_from_widget( GTK_RADIO_BUTTON(prev),
                                                         txt );
        g_signal_connect( radio, "toggled", G_CALLBACK(toggled), &state );
        gtk_box_pack_start( GTK_BOX(content), GTK_WIDGET(radio), FALSE,
                            TRUE, 0 );
        prev = radio;
    }
    /* set the first one */
    
    state.lastSel = GTK_TOGGLE_BUTTON(radios[0]);
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(state.lastSel), true );

    gtk_widget_show_all( dialog );
    gint response = gtk_dialog_run( GTK_DIALOG(dialog) );

    XP_Bool accept = GTK_RESPONSE_ACCEPT == response;
    if ( accept ) {
        for ( int ii = 0; ii < nGroups; ++ii ) {
            if ( radios[ii] == GTK_WIDGET(state.lastSel) ) {
                *selIndex = ii;
                XP_LOGFF( "chose %d", ii );
                break;
            }
        }
    }

    gtk_widget_destroy( dialog );
    return accept;
}

#endif
