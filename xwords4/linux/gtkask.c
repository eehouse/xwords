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

#include <stdarg.h>

#include "gtkask.h"

static gint
timer_func( gpointer data )
{
    GtkWidget* dlg = (GtkWidget*)data;
    gtk_widget_destroy( dlg );
    return 0;
}

XP_Bool
gtkask( const gchar *message, GtkButtonsType buttons )
{
    return gtkask_timeout( message, buttons, 0 );
}

XP_Bool
gtkask_timeout( const gchar *message, GtkButtonsType buttons, XP_U16 timeout )
{
    guint src = 0;
    GtkWidget* dlg = gtk_message_dialog_new( NULL, /* parent */
                                             GTK_MESSAGE_QUESTION,
                                             GTK_DIALOG_MODAL,
                                             buttons, "%s", message );

    if ( timeout > 0 ) {
        src = g_timeout_add( 1000 * timeout, timer_func, dlg );
    }

    gint response = gtk_dialog_run( GTK_DIALOG(dlg) );
    gtk_widget_destroy( dlg );

    if ( 0 != src ) {
        g_source_remove( src );
    }

    return response == GTK_RESPONSE_OK || response == GTK_RESPONSE_YES;
} /* gtkask */

#endif
