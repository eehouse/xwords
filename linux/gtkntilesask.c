/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 2003 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#include "gtkntilesask.h"

static void
button_event( GtkWidget* XP_UNUSED(widget), void* closure )
{
    XP_Bool* result = (XP_Bool*)closure;
    *result = XP_TRUE;
    gtk_main_quit();
} /* button_event */

XP_U16
askNTiles( XP_U16 max, XP_U16 deflt )
{
    XP_U16 result = 0;
    XP_Bool results[MAX_TRAY_TILES];
    GtkWidget* dialog;
    GtkWidget* label;
    GtkWidget* hbox = NULL;
    XP_U16 i;
    GtkWidget* button;	
    XP_UCHAR defbuf[48];

    XP_MEMSET( results, 0, sizeof(results) );
    
    dialog = gtk_dialog_new();
    gtk_window_set_modal( GTK_WINDOW( dialog ), TRUE );

    sprintf( defbuf, "Limit hint to how many tiles (deflt=%d)?", deflt );
    label = gtk_label_new( defbuf );
    gtk_container_add (GTK_CONTAINER (GTK_DIALOG(dialog)->vbox),
                       label);

    hbox = gtk_hbox_new( FALSE, 0 );
    for ( i = 0; i < max; ++i ) {
        XP_UCHAR buf[3];

        sprintf( buf, "%d", i+1 );
        button = gtk_button_new_with_label( buf );

        gtk_box_pack_start( GTK_BOX(hbox), button, FALSE, TRUE, 0 );
        g_signal_connect( GTK_OBJECT(button), "clicked", 
                          G_CALLBACK(button_event), 
                          &results[i] );
        gtk_widget_show( button );
    }
    gtk_widget_show( hbox );
    gtk_box_pack_start( GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox, 
                        FALSE, TRUE, 0 );

    sprintf( defbuf, "Default (%d)", deflt );
    button = gtk_button_new_with_label( defbuf );
    gtk_box_pack_start( GTK_BOX(GTK_DIALOG(dialog)->vbox), button, FALSE, 
                        TRUE, 0 );
    g_signal_connect( GTK_OBJECT(button), "clicked", 
                      G_CALLBACK(button_event), 
                      &results[deflt-1] );
    gtk_widget_show( button );

    gtk_widget_show_all( dialog );

    gtk_main();

    gtk_widget_destroy( dialog );

    for ( i = 0; i < MAX_TRAY_TILES; ++i ) {
        if ( results[i] ) {
            result = i + 1;
            break;
        }
    }

    return result;
} /* askNTiles */

#endif /* PLATFORM_GTK */
