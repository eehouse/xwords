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

typedef struct _GtkPlayerDlgState {
    XW_DUtilCtxt* dutil;
    GtkWidget* dialog;
    gulong handlerID;
    GtkWidget* grid;
    int curRow;
} GtkPlayerDlgState;

typedef struct _GtkPlayerState {
    GtkPlayerDlgState* dlgState;
    const XP_UCHAR* name;
} GtkPlayerState;

static void
on_delete_button( GtkWidget* XP_UNUSED(widget), void* closure )
{
    GtkPlayerState* ps = (GtkPlayerState*)closure;
    GtkPlayerDlgState* dlgState = ps->dlgState;
    XP_LOGFF( "name: %s", ps->name );

    if ( KP_OK == kplr_deletePlayer( dlgState->dutil, NULL_XWE, ps->name ) ) {
        g_signal_handler_disconnect( dlgState->dialog, dlgState->handlerID );
        gtk_main_quit();
    }
}

static void
addForPlayer( GtkPlayerState* ps )
{
    GtkWidget* grid = ps->dlgState->grid;
    gtk_grid_set_row_spacing( GTK_GRID(grid), 10 );
    gtk_grid_set_column_spacing( GTK_GRID(grid), 10 );

    GtkPlayerDlgState* dlgState = ps->dlgState;

    CommsAddrRec addr;
    if ( kplr_getAddr( ps->dlgState->dutil, NULL_XWE, ps->name, &addr ) ) {
        int curRow = dlgState->curRow++;

        GtkWidget* vbox = gtk_box_new( GTK_ORIENTATION_VERTICAL, 0 );
        
        GtkWidget* label = gtk_label_new( ps->name );
        gtk_grid_attach( GTK_GRID(grid), label, 0, curRow, 1, 1 );

        CommsConnType typ;
        for ( XP_U32 state = 0; addr_iter( &addr, &typ, &state ); ) {
            GtkWidget* line = NULL;
            if ( COMMS_CONN_MQTT == typ ) {
                XP_UCHAR tmp[32];
                formatMQTTDevID( &addr.u.mqtt.devID, tmp, VSIZE(tmp) );
                XP_UCHAR buf[64];
                XP_SNPRINTF( buf, VSIZE(buf), "MQTT: %s", tmp );
                line = gtk_label_new( buf );
            } else if ( COMMS_CONN_SMS == typ ) {
                XP_UCHAR buf[64];
                XP_SNPRINTF( buf, VSIZE(buf), "SMS: %s", addr.u.sms.phone );
                line = gtk_label_new( buf );
            } else {
                const char* str = ConnType2Str( typ );
                line = gtk_label_new( str );
            }
            if ( !!line ) {
                gtk_box_pack_start( GTK_BOX(vbox), line, FALSE, TRUE, 0 );
            }
        }

        gtk_grid_attach( GTK_GRID(grid), vbox, 1, curRow, 1, 1 );

        GtkWidget* button = gtk_button_new_with_label( "Delete" );
        g_signal_connect( button, "clicked", G_CALLBACK(on_delete_button), ps );
        gtk_grid_attach( GTK_GRID(grid), button, 2, curRow, 1, 1 );
    }
}

static void
showDialog( XW_DUtilCtxt* dutil, GtkWindow* parent,
            const XP_UCHAR* players[], int nFound )
{
    GtkWidget* dialog = gtk_dialog_new();
    GtkWidget* grid = gtk_grid_new();
    GtkPlayerDlgState pds = { .dutil = dutil,
                              .grid = grid,
                              .dialog = dialog,
    };
    GtkPlayerState states[nFound];

    XP_ASSERT( 0 < nFound );
    for ( int ii = 0; ; ) {
        GtkPlayerState* ps = &states[ii];
        ps->name = players[ii];
        ps->dlgState = &pds;
        addForPlayer( ps );

        if ( ++ii == nFound ) {
            break;
        }

        GtkWidget* sep = gtk_separator_new( GTK_ORIENTATION_HORIZONTAL );
        gtk_grid_attach( GTK_GRID(grid), sep, 0, pds.curRow++, 3, 1 );
    }

    gtk_window_set_transient_for( GTK_WINDOW(dialog), GTK_WINDOW(parent));
    gtk_window_set_modal( GTK_WINDOW( dialog ), TRUE );
    pds.handlerID = g_signal_connect( G_OBJECT(dialog), "destroy", gtk_main_quit, NULL );
    gtk_container_add( GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),
                       grid );

    gtk_widget_show_all( dialog );
    gtk_main();
    gtk_widget_destroy( dialog );
}

void
gtkkp_show( GtkAppGlobals* apg, GtkWindow* parent )
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
        showDialog( dutil, parent, players, nFound );
    }
    LOG_RETURN_VOID();
}

#endif
