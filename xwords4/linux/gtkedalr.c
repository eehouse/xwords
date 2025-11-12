/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
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

#include "gtkedalr.h"

XP_Bool
gtkEditAlert( GtkWidget* parent, const gchar* expl,
              XP_UCHAR name[], XP_U16 bufLen )
{
    GtkWidget* dialog =
        gtk_dialog_new_with_buttons( NULL, GTK_WINDOW(parent),
                                     GTK_DIALOG_MODAL
                                     | GTK_DIALOG_DESTROY_WITH_PARENT,
                                     "Ok", GTK_RESPONSE_ACCEPT,
                                     "Cancel", GTK_RESPONSE_REJECT,
                                     NULL );
    GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

    GtkWidget* label = gtk_label_new( expl );
    gtk_container_add( GTK_CONTAINER(content), label );

    GtkWidget* entry = gtk_entry_new();
    gtk_entry_set_text( GTK_ENTRY(entry), name );
    gtk_container_add( GTK_CONTAINER(content), entry );

    gtk_widget_show_all( dialog );
    gint response = gtk_dialog_run( GTK_DIALOG(dialog) );

    XP_Bool accept = GTK_RESPONSE_ACCEPT == response;
    if ( accept ) {
        const gchar* txt = gtk_entry_get_text( GTK_ENTRY(entry) );
        XP_SNPRINTF( name, bufLen, "%s", txt );
    }

    gtk_widget_destroy( dialog );
    return accept;
}

#endif
