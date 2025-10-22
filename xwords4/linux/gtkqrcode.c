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

#include <gtk/gtk.h>
#include <qrencode.h>

#include "main.h"
#include "gtkboard.h"
#include "gtkmain.h"
#include "gtkutils.h"

#include "gtkqrcode.h"
#include "strutils.h"

/* QR code drawing using libqrencode */
static void
draw_qr_code(cairo_t* cr, const gchar* data, gint canvas_size)
{
    QRcode* qrcode = QRcode_encodeString(data, 0, QR_ECLEVEL_L, QR_MODE_8, 1);
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
    gchar* data = (gchar*)user_data;
    GtkAllocation allocation;
    gtk_widget_get_allocation(widget, &allocation);
    
    gint size = MIN(allocation.width, allocation.height);
    draw_qr_code(cr, data, size);
    
    return TRUE;
}

void
gtkShowQRCode(GtkWidget* parent, const gchar* data, const gchar* title)
{
    LOG_FUNC();
    XP_LOGFF("(title: %s)", title );
    GtkWidget* dialog = gtk_dialog_new_with_buttons(
        title ? title : "QR Code",
        GTK_WINDOW(parent),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "Close", GTK_RESPONSE_CLOSE,
        NULL
    );

    GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    
    /* Create main container */
    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(content), vbox);
    
    /* Add explanation label */
    GtkWidget* label = gtk_label_new("Game Information QR Code");
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
    
    /* Create drawing area for QR code */
    GtkWidget* drawing_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(drawing_area, 300, 300);
    
    /* Make a copy of data for the callback */
    gchar* data_copy = g_strdup(data);
    g_signal_connect(drawing_area, "draw", G_CALLBACK(qr_draw_callback), data_copy);
    
    /* Add drawing area to a frame for better appearance */
    GtkWidget* frame = gtk_frame_new(NULL);
    gtk_container_add(GTK_CONTAINER(frame), drawing_area);
    gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 0);
    
    /* Add data text for reference */
    GtkWidget* data_label = gtk_label_new("Data:");
    gtk_box_pack_start(GTK_BOX(vbox), data_label, FALSE, FALSE, 0);
    
    GtkWidget* data_text = gtk_label_new(data);
    gtk_label_set_selectable(GTK_LABEL(data_text), TRUE);
    gtk_label_set_line_wrap(GTK_LABEL(data_text), TRUE);
    gtk_box_pack_start(GTK_BOX(vbox), data_text, FALSE, FALSE, 0);
    
    /* Show all widgets */
    gtk_widget_show_all(dialog);
    
    /* Run dialog */
    gtk_dialog_run(GTK_DIALOG(dialog));
    
    /* Cleanup */
    g_free(data_copy);
    gtk_widget_destroy(dialog);
}

#endif /* PLATFORM_GTK */
