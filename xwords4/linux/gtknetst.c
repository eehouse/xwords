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

#include <qrencode.h>

#include "gtknetst.h"
#include "knownplyr.h" 

typedef struct _Datum {
    XP_U8* data;
    XP_U16 len;
} Datum;

typedef struct _NetStateState {
    XW_DUtilCtxt* dutil;
    GameRef gr;
    GtkGameGlobals* globals;
    CommsAddrRec addrs[4];
    GtkWidget* areas[4];
    XP_U16 nRecs;
    Datum data[4];
} NetStateState;

/* QR code drawing using libqrencode */
static void
draw_qr_code(cairo_t* cr, Datum* datum, gint canvas_size)
{
    gchar* str64 = g_base64_encode( datum->data, datum->len );
    
    QRcode* qrcode = QRcode_encodeString(str64, 0, QR_ECLEVEL_L, QR_MODE_8, 1);
    if (qrcode == NULL) {
        /* Draw error pattern if QR generation fails */
        cairo_set_source_rgb(cr, 1.0, 0.0, 0.0);
        cairo_rectangle(cr, 0, 0, canvas_size, canvas_size);
        cairo_fill(cr);
        return;
    }
    
    gint qr_size = qrcode->width;
    gint cell_size = canvas_size / qr_size;
    gint offset = (canvas_size - (cell_size * qr_size)) / 2;
    
    /* Draw white background */
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_rectangle(cr, 0, 0, canvas_size, canvas_size);
    cairo_fill(cr);
    
    /* Draw QR code modules */
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    
    for (gint y = 0; y < qr_size; y++) {
        for (gint x = 0; x < qr_size; x++) {
            if (qrcode->data[y * qr_size + x] & 1) {
                cairo_rectangle(cr, 
                              offset + x * cell_size, 
                              offset + y * cell_size, 
                              cell_size, cell_size);
                cairo_fill(cr);
            }
        }
    }
    
    QRcode_free(qrcode);
    g_free( str64 );
}

static gboolean
qr_draw_callback(GtkWidget* widget, cairo_t* cr, gpointer user_data)
{
    NetStateState* nss = (NetStateState*)user_data;
    int index = -1;
    for ( int ii = 0; ii < nss->nRecs; ++ii ) {
        if (nss->areas[ii] == widget) {
            index = ii;
            break;
        }
    }
    XP_ASSERT( index >= 0 );

    GtkAllocation allocation;
    gtk_widget_get_allocation(widget, &allocation);
    
    gint size = MIN(allocation.width, allocation.height);
    draw_qr_code(cr, &nss->data[index], size);
    
    return TRUE;
}

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
                                                    NULL_XWE, addr );
    if ( !!stream ) {
        XP_U16 size = stream_getSize(stream);
        nss->data[index].data = g_malloc0( size );
        nss->data[index].len = size;
        stream_getBytes( stream, nss->data[index].data, size );
        
        GtkWidget* area = gtk_drawing_area_new();
        nss->areas[index] = area;
        gtk_widget_set_size_request(area, 300, 300);
        g_signal_connect(area, "draw", G_CALLBACK(qr_draw_callback), nss);

        GtkWidget* frame = gtk_frame_new(NULL);
        gtk_container_add(GTK_CONTAINER(frame), area);
        gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 0);


        stream_destroy( stream );
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

    for ( int ii = 0; ii < nss.nRecs; ++ii ) {
        XP_U8* data = nss.data[ii].data;
        if ( !!data ) {
            g_free(data);
        }
    }
}

#endif /* PLATFORM_GTK */
