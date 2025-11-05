/* -*- compile-command: "make MEMDEBUG=TRUE -j3"; -*- */
/* 
 * Copyright 2025 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#include "gtknetst.h"
#include "gtkqrwrp.h"
#include "knownplyr.h" 

typedef struct _NetStateState {
    XW_DUtilCtxt* dutil;
    GameRef gr;
    GtkGameGlobals* globals;
    CommsAddrRec addrs[4];
    QRThingState* things[4];
    XP_U16 nRecs;
} NetStateState;

static GtkWidget*
makeForAddr( NetStateState* nss, int index )
{
    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);

    const CommsAddrRec* addr = &nss->addrs[index];
    const XP_UCHAR* name = kplr_nameForAddress( nss->dutil, NULL_XWE, addr );
    if ( !name ) {
        name = "<unknown>";
    }

    GtkWidget* label = gtk_label_new(name);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

    XWStreamCtxt* stream = gr_getPendingPacketsFor( nss->dutil, nss->gr,
                                                    NULL_XWE, addr, NULL, NULL );
    if ( !!stream ) {
        const XP_UCHAR* ptr = (XP_UCHAR*)strm_getPtr( stream );
        XP_U16 len = strm_getSize( stream );

        GtkWidget* widget;
        nss->things[index] = mkQRThing( &widget, ptr, len );
        gtk_box_pack_start(GTK_BOX(vbox), widget, TRUE, TRUE, 0);

        strm_destroy( stream );
    }

    return vbox;
}

void
gtkNetStateDlg( GtkGameGlobals* globals )
{
    CommonGlobals* cGlobals = &globals->cGlobals;
    LaunchParams* params = cGlobals->params;
    NetStateState nss = {
        .dutil = params->dutil,
        .gr = cGlobals->gr,
        .globals = globals,
        .nRecs = VSIZE(nss.addrs),
    };
    
    gr_getAddrs( params->dutil, cGlobals->gr, NULL_XWE, nss.addrs,
                 &nss.nRecs );
    XP_LOGFF( "got %d addrs:", nss.nRecs );
    for ( int ii = 0; ii < nss.nRecs; ++ii ) {
        logAddr( params->dutil, &nss.addrs[ii], __func__ );
    }

    GtkWidget* parent = getWindow(globals);
    GtkWidget* dialog = gtk_dialog_new_with_buttons(
        "Net state",
        GTK_WINDOW(parent),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "Close", GTK_RESPONSE_CLOSE,
        NULL
    );

    GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(content), vbox);

    for ( int ii = 0; ii < nss.nRecs; ++ii ) {
        GtkWidget* box = makeForAddr( &nss, ii );
        gtk_box_pack_start(GTK_BOX(vbox), box, FALSE, FALSE, 0);
    }
    
    gtk_widget_show_all(dialog);
    
    gtk_dialog_run(GTK_DIALOG(dialog));
    
    gtk_widget_destroy(dialog);
}
#endif /* PLATFORM_GTK */
