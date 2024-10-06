/* 
 * Copyright 2001-2008 by Eric House (xwords@eehouse.org).  All rights
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

#include "gtkutils.h"

GtkWidget*
makeButton( const char* text, GCallback func, gpointer data )
{
    GtkWidget* button = gtk_button_new_with_label( text );
    g_signal_connect( button, "clicked", func, data );
    gtk_widget_show( button );

    return button;
} /* makeButton */


GtkWidget*
makeLabeledField( const char* labelText, GtkWidget** field, 
                  const gchar* initialVal )
{
    char buf[64];
    snprintf( buf, sizeof(buf), "%s:", labelText );
    GtkWidget* label = gtk_label_new( buf );

    *field = gtk_entry_new();
    if ( !!initialVal ) {
        gtk_entry_set_text( GTK_ENTRY(*field), initialVal );
    }

    GtkWidget* hbox = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 0 );
    gtk_box_pack_start( GTK_BOX(hbox), label, FALSE, TRUE, 0 );
    gtk_box_pack_start( GTK_BOX(hbox), *field, FALSE, TRUE, 0 );
    return hbox;
}

#endif
