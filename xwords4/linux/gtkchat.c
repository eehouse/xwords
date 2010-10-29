/* -*-mode: C; fill-column: 78; compile-command: "make MEMDEBUG=TRUE"; -*- */

/* 
 * Copyright 2000 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#include "gtkchat.h"

gchar*
gtkGetChatMessage( GtkAppGlobals* XP_UNUSED(globals) )
{
    gchar* result = NULL;
    GtkWidget* dialog = gtk_dialog_new_with_buttons( "message text", NULL, //GtkWindow *parent,
                                                     GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                     GTK_STOCK_OK,
                                                     GTK_RESPONSE_ACCEPT,
                                                     NULL );

    GtkWidget* entry = gtk_entry_new();

    gtk_container_add( GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG (dialog))),
                       entry );
    gtk_widget_show_all( dialog );
    gtk_dialog_run( GTK_DIALOG (dialog) );

    const char* text = gtk_entry_get_text( GTK_ENTRY(entry) );

    if ( 0 != text[0] ) {
        result = g_strdup( text );
    }
    gtk_widget_destroy (dialog); 

    LOG_RETURNF( "%s", result );
    return result;
}

#endif
