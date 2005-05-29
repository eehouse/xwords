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
button_event( GtkWidget* widget, gpointer closure )
{
    XP_Bool* whichSet = (XP_Bool*)closure;
    *whichSet = 1;

    gtk_main_quit();
} /* button_event */

#ifdef FEATURE_TRAY_EDIT
static void
abort_button_event( GtkWidget* widget, gpointer closure )
{
    gtk_main_quit();
} /* abort_button_event */
#endif

#define BUTTONS_PER_ROW 13

XP_S16
gtkletterask( XP_Bool forBlank, XP_UCHAR* name, 
              XP_U16 nTiles, const XP_UCHAR4* texts )
{
    GtkWidget* dialog;
    GtkWidget* label;
    XP_Bool results[MAX_UNIQUE_TILES];
    GtkWidget* vbox;
    GtkWidget* hbox = NULL;
    char* txt;
    XP_S16 i;
    GtkWidget* button;	
    XP_UCHAR buf[64];

    XP_MEMSET( results, 0, sizeof(results) );

    vbox = gtk_vbox_new( FALSE, 0 );

    for ( i = 0; i < nTiles; ++i ) {

        if ( i % BUTTONS_PER_ROW == 0 ) {
            hbox = gtk_hbox_new( FALSE, 0 );
        }
        button = gtk_button_new_with_label( texts[i] );

        gtk_box_pack_start( GTK_BOX(hbox), button, FALSE, TRUE, 0 );
        g_signal_connect( GTK_OBJECT(button), "clicked", 
                          G_CALLBACK(button_event), &results[i] );
        gtk_widget_show( button );

        if ( i+1 == nTiles || (i % BUTTONS_PER_ROW == 0) ) {
            gtk_widget_show( hbox );
            gtk_box_pack_start( GTK_BOX(vbox), hbox, FALSE, TRUE, 0 );
        }
    }

#ifdef FEATURE_TRAY_EDIT
    button = gtk_button_new_with_label( "Just pick em!" );
    hbox = gtk_hbox_new( FALSE, 0 );
    g_signal_connect( GTK_OBJECT(button), "clicked", 
                      G_CALLBACK(abort_button_event), NULL );
    gtk_box_pack_start( GTK_BOX(hbox), button, FALSE, TRUE, 0 );
    gtk_widget_show( button );
    gtk_widget_show( hbox );
    gtk_box_pack_start( GTK_BOX(vbox), hbox, FALSE, TRUE, 0 );
#endif

    gtk_widget_show( vbox );

    /* Create the widgets */
    dialog = gtk_dialog_new();
    gtk_window_set_modal( GTK_WINDOW( dialog ), TRUE );

    if ( forBlank ) {
        txt = "Choose a letter for your blank.";
    } else {
        char* fmt = "Choose a tile for %s.";
        XP_SNPRINTF( buf, sizeof(buf), fmt, name );
        txt = buf;
    }
    label = gtk_label_new( txt );

    gtk_container_add (GTK_CONTAINER (GTK_DIALOG(dialog)->vbox),
                       label);
    gtk_container_add( GTK_CONTAINER( GTK_DIALOG(dialog)->action_area), vbox);
    gtk_widget_show_all( dialog );

    gtk_main();

    gtk_widget_destroy( dialog );

    for ( i = 0; i < nTiles; ++i ) {
        if ( results[i] ) {
            break;
        }
    }
    if ( i == nTiles ) {
        i = -1;
    }

    return i;
} /* gtkletterask */

#endif /* PLATFORM_GTK */
