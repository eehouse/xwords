/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 2000 by Eric House (fixin@peak.org).  All rights reserved.
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

#include <stdarg.h>

#include "gtkask.h"

static void
button_event( GtkWidget* widget, void* closure )
{
    gboolean* whichSet = (gboolean*)closure;
    *whichSet = 1;

    gtk_main_quit();
}

gint
gtkask( GtkAppGlobals* globals, gchar *message, gint numButtons,
	char* button1, ... )
{
    GtkWidget* dialog;
    GtkWidget* label;
    GtkWidget* button;
    short i;
    gboolean* results = g_malloc( numButtons * sizeof(results[0]) );
    char** butList = &button1;

    /* Create the widgets */
    dialog = gtk_dialog_new();
    gtk_window_set_modal( GTK_WINDOW( dialog ), TRUE );

    label = gtk_label_new( message );

    for ( i = 0; i < numButtons; ++i ) {
	button = gtk_button_new_with_label( *butList );

	results[i] = 0;
	gtk_signal_connect( GTK_OBJECT( button ), "clicked",
			    GTK_SIGNAL_FUNC(button_event), &results[i] );
	
	gtk_container_add( GTK_CONTAINER( GTK_DIALOG(dialog)->action_area),
			   button );

	++butList;
    }
    
    /* Add the label, and show everything we've added to the dialog. */
    gtk_container_add (GTK_CONTAINER (GTK_DIALOG(dialog)->vbox),
                       label);
    gtk_widget_show_all (dialog);

    /* returns when button handler calls gtk_main_quit */
    gtk_main();

    gtk_widget_destroy( dialog );

    for ( i = 0; i < numButtons; ++i ) {
	if ( results[i] ) {
	    break;
	}
    }
    g_free( results );
    return i;
 } /* gtkask */

#endif
