/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 2000 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#include "gtkpasswdask.h"
#include <gtk/gtk.h>

static void
button_event( GtkWidget* XP_UNUSED(widget), void* closure )
{
    XP_Bool* okptr = (XP_Bool*)closure;
    *okptr = XP_TRUE;
    gtk_main_quit();
} /* ok_button_event */

XP_Bool
gtkpasswdask( const char* name, char* outbuf, XP_U16* buflen )
{
    XP_Bool ok = XP_FALSE;
    XP_Bool ignore;
    char buf[64];
    short i;

    GtkWidget* entry;
    GtkWidget* vbox;
    GtkWidget* hbox;
    GtkWidget* dialog;
    GtkWidget* label;

    char* labels[] = { "Ok", "Cancel" };
    XP_Bool* boolps[] = { &ok, &ignore };

    dialog = gtk_dialog_new();
    gtk_window_set_modal( GTK_WINDOW( dialog ), TRUE );

    snprintf( buf, sizeof(buf), "Password for player \"%s\"", name );
    label = gtk_label_new( buf );

    gtk_container_add( GTK_CONTAINER (GTK_DIALOG(dialog)->vbox),
                       label );

    /* we need a text field and two buttons as well */
    vbox = gtk_vbox_new(FALSE, 0);

    entry = gtk_entry_new();
    gtk_widget_show( entry );
    gtk_box_pack_start( GTK_BOX(vbox), entry, FALSE, TRUE, 0 );
    
    hbox = gtk_hbox_new(FALSE, 0);

    for ( i = 0; i < 2; ++i ) {
        GtkWidget* button = gtk_button_new_with_label( labels[i] );
        g_signal_connect( GTK_OBJECT(button), "clicked", 
                          G_CALLBACK(button_event),
                          boolps[i] );
        gtk_box_pack_start( GTK_BOX(hbox), button, FALSE, TRUE, 0 );
        gtk_widget_show( button );
    }

    gtk_box_pack_start( GTK_BOX(vbox), hbox, FALSE, TRUE, 0 );

    gtk_container_add( GTK_CONTAINER( GTK_DIALOG(dialog)->action_area), vbox);

    gtk_widget_show_all( dialog );

    gtk_main();

    if ( ok ) {
        const char* text = gtk_entry_get_text( GTK_ENTRY(entry) );
        strncpy( outbuf, text, *buflen );
        *buflen = strlen(outbuf);
    }

    gtk_widget_destroy( dialog );

    return ok;
} /* gtkpasswdask */

#endif /* PLATFORM_GTK */
