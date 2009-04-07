/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
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
button_event( GtkWidget* XP_UNUSED(widget), gpointer closure )
{
    XP_Bool* whichSet = (XP_Bool*)closure;
    *whichSet = 1;

    gtk_main_quit();
} /* button_event */

#ifdef FEATURE_TRAY_EDIT
static void
abort_button_event( GtkWidget* XP_UNUSED(widget), gpointer XP_UNUSED(closure) )
{
    gtk_main_quit();
} /* abort_button_event */
#endif

#define BUTTONS_PER_ROW 13

XP_S16
gtkletterask( const PickInfo* pi, const XP_UCHAR* name, 
              XP_U16 nTiles, const XP_UCHAR** texts )
{
    GtkWidget* dialog;
    GtkWidget* label;
    XP_Bool results[MAX_UNIQUE_TILES];
    GtkWidget* vbox;
    GtkWidget* hbox = NULL;
    char* txt;
    XP_S16 ii;
    GtkWidget* button;	
    XP_UCHAR buf[64];

    XP_MEMSET( results, 0, sizeof(results) );

    vbox = gtk_vbox_new( FALSE, 0 );

    for ( ii = 0; ii < nTiles; ++ii ) {

        if ( ii % BUTTONS_PER_ROW == 0 ) {
            hbox = gtk_hbox_new( FALSE, 0 );
        }
        button = gtk_button_new_with_label( texts[ii] );

        gtk_box_pack_start( GTK_BOX(hbox), button, FALSE, TRUE, 0 );
        g_signal_connect( GTK_OBJECT(button), "clicked", 
                          G_CALLBACK(button_event), &results[ii] );
        gtk_widget_show( button );

        if ( ii+1 == nTiles || (ii % BUTTONS_PER_ROW == 0) ) {
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

    XP_Bool forBlank = PICK_FOR_BLANK == pi->why;
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

    if ( !forBlank ) {
        char curTilesBuf[64];
        int len = snprintf( curTilesBuf, sizeof(curTilesBuf), "%s", 
                            "Tiles so far: " );
        for ( ii = 0; ii < pi->nCurTiles; ++ii ) {
            len += snprintf( &curTilesBuf[len], sizeof(curTilesBuf) - len, "%s ", 
                             pi->curTiles[ii] );
        }

        GtkWidget* curTilesLabel = gtk_label_new( curTilesBuf );
        gtk_container_add( GTK_CONTAINER (GTK_DIALOG(dialog)->vbox),
                           curTilesLabel );
    }

    gtk_container_add( GTK_CONTAINER( GTK_DIALOG(dialog)->action_area), vbox);
    gtk_widget_show_all( dialog );

    gtk_main();

    gtk_widget_destroy( dialog );

    for ( ii = 0; ii < nTiles; ++ii ) {
        if ( results[ii] ) {
            break;
        }
    }
    if ( ii == nTiles ) {
        ii = -1;
    }

    return ii;
} /* gtkletterask */

#endif /* PLATFORM_GTK */
