/* -*- compile-command: "make MEMDEBUG=TRUE -j5"; -*- */
/* 
 * Copyright 2001-2014 by Eric House (xwords@eehouse.org).  All rights
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

#include "gtkkpdlg.h"
#include "knownplyr.h"
#include "strutils.h"

typedef struct _GtkPlayerState {
    XW_DUtilCtxt* dutil;
    const XP_UCHAR* name;
} GtkPlayerState;

static void
on_delete_button( GtkWidget* XP_UNUSED(widget), void* closure )
{
    GtkPlayerState* ps = (GtkPlayerState*)closure;
    XP_LOGFF( "name: %s", ps->name );

    if ( KP_OK == kplr_deletePlayer( ps->dutil, NULL_XWE, ps->name ) ) {
        gtk_main_quit();
    }
}

static GtkWidget*
makeForPlayer( XW_DUtilCtxt* dutil, const gchar* name, void* closure )
{
    GtkWidget* vbox = NULL;

    CommsAddrRec addr;
    if ( kplr_getAddr( dutil, NULL_XWE, name, &addr ) ) {
        vbox = gtk_box_new( GTK_ORIENTATION_VERTICAL, 0 );
        
        GtkWidget* label = gtk_label_new( name );
        gtk_box_pack_start( GTK_BOX(vbox), label, FALSE, TRUE, 0 );

        if ( addr_hasType( &addr, COMMS_CONN_MQTT ) ) {
            XP_UCHAR buf[32];
            formatMQTTDevID( &addr.u.mqtt.devID, buf, VSIZE(buf) );
            label = gtk_label_new( buf );
            gtk_box_pack_start( GTK_BOX(vbox), label, FALSE, TRUE, 0 );
        }

        GtkWidget* button = gtk_button_new_with_label( "Delete" );
        g_signal_connect( button, "clicked", G_CALLBACK(on_delete_button), closure );
        gtk_box_pack_start( GTK_BOX(vbox), button, FALSE, TRUE, 0 );
    }
    
    return vbox;
}

static void
showDialog( XW_DUtilCtxt* dutil, const XP_UCHAR* players[], int nFound )
{
    GtkWidget* vbox = gtk_box_new( GTK_ORIENTATION_VERTICAL, 0 );

    GtkPlayerState states[nFound];

    for ( int ii = 0; ii < nFound; ++ii ) {
        GtkPlayerState* ps = &states[ii];
        ps->name = players[ii];
        ps->dutil = dutil;
        GtkWidget* one = makeForPlayer( dutil, players[ii], ps );
        if ( !!one ) {
            gtk_box_pack_start( GTK_BOX(vbox), one, FALSE, TRUE, 0 );
        }
    }

    GtkWidget* dialog = gtk_dialog_new();
    gtk_window_set_modal( GTK_WINDOW( dialog ), TRUE );
    gtk_container_add( GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),
                       vbox );

    gtk_widget_show_all( dialog );
    gtk_main();
    gtk_widget_destroy( dialog );
}

void
gtkkp_show( GtkAppGlobals* apg )
{
    XW_DUtilCtxt* dutil = apg->cag.params->dutil;

    XP_U16 nFound = 0;
    kplr_getNames( dutil, NULL_XWE, NULL, &nFound );
    const XP_UCHAR* players[nFound];
    kplr_getNames( dutil, NULL_XWE, players, &nFound );

    for ( int ii = 0; ii < nFound; ++ii ) {
        XP_LOGFF( "got one: %s", players[ii] );
    }

    if ( 0 < nFound ) {
        showDialog( dutil, players, nFound );
    }
}

#endif
