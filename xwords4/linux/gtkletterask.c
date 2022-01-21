/* -*- compile-command: "make -j3 MEMDEBUG=TRUE"; -*- */
/* 
 * Copyright 2000-2009 by Eric House (xwords@eehouse.org).  All rights
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

#include <stdarg.h>

#include "gtkask.h"

static void
set_bool_and_quit( GtkWidget* widget, gpointer closure )
{
    XP_Bool* whichSet = (XP_Bool*)closure;
    *whichSet = XP_TRUE;

    GtkWidget* dialog = gtk_widget_get_toplevel( widget );
    gtk_dialog_response( GTK_DIALOG(dialog), 1000 );
} /* button_event */

#ifdef FEATURE_TRAY_EDIT
static void
abort_button_event( GtkWidget* widget, gpointer XP_UNUSED(closure) )
{
    GtkWidget* dialog = gtk_widget_get_toplevel( widget );
    gtk_dialog_response( GTK_DIALOG(dialog), 1000 );
} /* abort_button_event */
#endif

#define BUTTONS_PER_ROW 13

XP_S16
gtkletterask( const TrayTileSet* curPick, XP_Bool forTray, const XP_UCHAR* name,
              XP_U16 nToPick, XP_U16 nTiles, const XP_UCHAR** texts,
              const XP_U16* counts )
{
    GtkWidget* dialog;
    GtkWidget* label;
    XP_Bool results[MAX_UNIQUE_TILES];
    GtkWidget* vbox;
    GtkWidget* hbox = NULL;
    char* txt;
    GtkWidget* button;	
    XP_UCHAR buf[64];
    XP_Bool backedUp = XP_FALSE;

    XP_MEMSET( results, XP_FALSE, sizeof(results) );

    vbox = gtk_box_new( GTK_ORIENTATION_VERTICAL, 0 );

    for ( int ii = 0; ii < nTiles; ++ii ) {

        if ( ii % BUTTONS_PER_ROW == 0 ) {
            hbox = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 0 );
        }
        button = gtk_button_new_with_label( texts[ii] );

        gtk_box_pack_start( GTK_BOX(hbox), button, FALSE, TRUE, 0 );
        g_signal_connect( button, "clicked", 
                          G_CALLBACK(set_bool_and_quit), &results[ii] );

        /* disable if we don't have any tiles! */
        gtk_widget_set_sensitive( button, !counts || counts[ii] > 0 );

        if ( ii+1 == nTiles || (ii % BUTTONS_PER_ROW == 0) ) {
            gtk_box_pack_start( GTK_BOX(vbox), hbox, FALSE, TRUE, 0 );
        }
    }

#ifdef FEATURE_TRAY_EDIT
    hbox = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 0 );

    button = gtk_button_new_with_label( "Just pick em!" );
    g_signal_connect( button, "clicked", 
                      G_CALLBACK(abort_button_event), NULL );
    gtk_box_pack_start( GTK_BOX(hbox), button, FALSE, TRUE, 0 );

    button = gtk_button_new_with_label( "Back up" );
    g_signal_connect( button, "clicked", 
                      G_CALLBACK(set_bool_and_quit), &backedUp );
    gtk_box_pack_start( GTK_BOX(hbox), button, FALSE, TRUE, 0 );

    gtk_box_pack_start( GTK_BOX(vbox), hbox, FALSE, TRUE, 0 );
#endif

    /* Create the widgets */
    dialog = gtk_dialog_new();
    gtk_window_set_modal( GTK_WINDOW( dialog ), TRUE );

    if ( forTray ) {
        char* fmt = "Choose %d tiles for %s.";
        XP_SNPRINTF( buf, sizeof(buf), fmt, nToPick, name );
        txt = buf;
    } else {
        txt = "Choose a letter for your blank.";
    }
    label = gtk_label_new( txt );
    gtk_container_add (GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),
                       label);

    if ( forTray ) {
        char curTilesBuf[64];
        int len = snprintf( curTilesBuf, sizeof(curTilesBuf), "%s", 
                            "Tiles so far: " );
        for ( int ii = 0; ii < curPick->nTiles; ++ii ) {
            Tile tile = curPick->tiles[ii];
            len += snprintf( &curTilesBuf[len], sizeof(curTilesBuf) - len, "%s ", 
                             texts[tile] );
        }

        GtkWidget* curTilesLabel = gtk_label_new( curTilesBuf );
        gtk_container_add( GTK_CONTAINER (gtk_dialog_get_content_area(GTK_DIALOG(dialog))),
                           curTilesLabel );
    }

    // gtk_container_add( GTK_CONTAINER( gtk_dialog_get_action_area(GTK_DIALOG(dialog))), vbox);
    gtk_dialog_add_action_widget( GTK_DIALOG(dialog), vbox, 0 );
    gtk_widget_show_all( dialog );

    // gint dlgResult =
    (void)gtk_dialog_run( GTK_DIALOG( dialog ) );

    gtk_widget_destroy( dialog );

    XP_S16 result;
    if ( backedUp ) {
        result = PICKER_BACKUP;
    } else {
        result = PICKER_PICKALL;
        for ( int ii = 0; ii < nTiles; ++ii ) {
            if ( results[ii] ) {
                result = ii;
                break;
            }
        }
    }

    return result;
} /* gtkletterask */

#endif /* PLATFORM_GTK */
