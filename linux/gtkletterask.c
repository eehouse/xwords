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
    XP_Bool* whichSet = (XP_Bool*)closure;
    *whichSet = 1;

    gtk_main_quit();
}

#define BUTTONS_PER_ROW 13

void
gtkletterask( DictionaryCtxt* dict, char* resultBuf )
{
    GtkWidget* dialog;
    GtkWidget* label;
    Tile tile, nonBlanks;
    XP_Bool results[32];	/* MAX NUM FACES */
    unsigned char buf[4];
    GtkWidget* vbox;
    GtkWidget* hbox = NULL;

    XP_U16 numFaces = dict_numTileFaces( dict );
    Tile blankFace = dict_getBlankTile( dict );

    XP_MEMSET( results, 0, sizeof(results) );

    XP_ASSERT( numFaces > 0 );

    vbox = gtk_vbox_new( FALSE, 0 );

    for ( nonBlanks = tile = 0; tile < numFaces; ++tile ) {
	GtkWidget* button;	
	if ( tile == blankFace ) {
	    continue;
	}

	if ( nonBlanks % BUTTONS_PER_ROW == 0 ) {
	    hbox = gtk_hbox_new( FALSE, 0 );
	}
	dict_tilesToString( dict, &nonBlanks, 1, buf );
	button = gtk_button_new_with_label( buf );

	gtk_box_pack_start( GTK_BOX(hbox), button, FALSE, TRUE, 0 );
	gtk_signal_connect( GTK_OBJECT(button), "clicked", button_event, 
			    &results[nonBlanks] );
	gtk_widget_show( button );

	if ( tile+1 == numFaces || (nonBlanks % BUTTONS_PER_ROW == 0) ) {
	    gtk_widget_show( hbox );
	    gtk_box_pack_start( GTK_BOX(vbox), hbox, FALSE, TRUE, 0 );
	}
	++nonBlanks;
    }
    gtk_widget_show( vbox );

    /* Create the widgets */
    dialog = gtk_dialog_new();
    gtk_window_set_modal( GTK_WINDOW( dialog ), TRUE );

    label = gtk_label_new( "Choose a letter for your blank." );

    gtk_container_add (GTK_CONTAINER (GTK_DIALOG(dialog)->vbox),
                       label);
    gtk_container_add( GTK_CONTAINER( GTK_DIALOG(dialog)->action_area), vbox);
    gtk_widget_show_all( dialog );

    gtk_main();

    gtk_widget_destroy( dialog );

    for ( nonBlanks = tile = 0; tile < numFaces; ++tile ) {
	if ( tile == blankFace ) {
	    continue;
	} else if ( results[nonBlanks] ) {
	    break;
	}
	++nonBlanks;
    }

    dict_tilesToString( dict, &tile, 1, resultBuf );
} /* gtkletterask */

#endif /* PLATFORM_GTK */
