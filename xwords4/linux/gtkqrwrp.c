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
 *
 * This was written with some help from github copilot -- actual help, not its
 * usual fumbling :-)
 */

#ifdef PLATFORM_GTK

#include <qrencode.h>
#include "gtkqrwrp.h"

struct QRThingState {
    XP_UCHAR* str;
};

static void
draw_qr_code(cairo_t* cr, QRThingState* state, gint canvas_size)
{
    QRcode* qrcode = QRcode_encodeString(state->str, 0, QR_ECLEVEL_L, QR_MODE_8, 1);
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
}

static gboolean
qr_draw_callback(GtkWidget* widget, cairo_t* cr, gpointer user_data)
{
    QRThingState* state = (QRThingState*)user_data;

    GtkAllocation allocation;
    gtk_widget_get_allocation(widget, &allocation);
    
    gint size = MIN(allocation.width, allocation.height);
    draw_qr_code(cr, state, size);
    
    return TRUE;
}

static void
copy_button_clicked( GtkWidget* XP_UNUSED(widget), gpointer closure )
{
    QRThingState* state = (QRThingState*)closure;
    GtkClipboard* clipboard = gtk_clipboard_get( GDK_SELECTION_CLIPBOARD );
    gtk_clipboard_set_text( clipboard, state->str, strlen(state->str) );
}

QRThingState*
mkQRThing( GtkWidget** widgetp, const XP_UCHAR* str, XP_U16 len )
{
    QRThingState* state = g_malloc0( sizeof(*state) );
    state->str = g_malloc0(len+1);
    memcpy( state->str, str, len );

    GtkWidget* vbox = gtk_box_new( GTK_ORIENTATION_VERTICAL, 0 );

    GtkWidget* area = gtk_drawing_area_new();
    gtk_widget_set_size_request(area, 300, 300);
    g_signal_connect(area, "draw", G_CALLBACK(qr_draw_callback), state);

    GtkWidget* frame = gtk_frame_new(NULL);
    gtk_container_add(GTK_CONTAINER(frame), area);

    gtk_container_add(GTK_CONTAINER(vbox), frame);

    gchar* label = g_strdup_printf("Copy %s", state->str);
    GtkWidget* button = gtk_button_new_with_label( label );
    g_signal_connect( button, "clicked",
                      G_CALLBACK(copy_button_clicked), state );

    g_free( label );
    gtk_container_add(GTK_CONTAINER(vbox), button);

    *widgetp = vbox;
    return state;
}

void
freeQRThing( QRThingState** thingp )
{
    if ( !!*thingp ) {
        g_free( (*thingp)->str );
        g_free( *thingp );
        *thingp = NULL;
    }
}
#endif /* PLATFORM_GTK */
