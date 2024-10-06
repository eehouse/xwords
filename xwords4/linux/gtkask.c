
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

void
gtktell( GtkWidget* parent, const gchar *message )
{
    (void)gtkask( parent, message, GTK_BUTTONS_OK, NULL );
}

gint
gtkask( GtkWidget* parent, const gchar *message, GtkButtonsType buttons,
        const AskPair* buttxts )

{
    return gtkask_timeout( parent, message, buttons, buttxts, 0 );
}

gint
gtkask_timeout( GtkWidget* parent, const gchar* message,
                GtkButtonsType buttons, const AskPair* buttxts,
                uint32_t timeoutMS )
{
    guint src = 0;
    GtkWidget* dlg = gtk_message_dialog_new( (GtkWindow*)parent, 
                                             GTK_MESSAGE_QUESTION,
                                             GTK_DIALOG_MODAL,
                                             buttons, "%s", message );

    if ( timeoutMS > 0 ) {
        XP_LOGF( "%s(\"%s\")", __func__, message ); /* log since times out... */
        src = g_timeout_add( timeoutMS, timer_func, dlg );
    }

    while ( !!buttxts && !!buttxts->txt ) {
        (void)gtk_dialog_add_button( GTK_DIALOG(dlg), buttxts->txt, buttxts->result );
        ++buttxts;
    }

    gint response = gtk_dialog_run( GTK_DIALOG(dlg) );
    gtk_widget_destroy( dlg );

    if ( 0 != src ) {
        g_source_remove( src );
    }

    LOG_RETURNF( "%d", response );
    return response;
} /* gtkask_timeout */

bool
gtkask_radios( GtkWidget* parent, const gchar *message,
               const AskPair* buttxts, int* chosen )
{
    gint askResponse = gtkask_timeout( parent, message, GTK_BUTTONS_CANCEL, buttxts, 0 );
    bool result = askResponse != GTK_RESPONSE_CANCEL;
    if ( result ) {
        for ( int ii = 0; ; ++ii ) {
            if ( !buttxts[ii].txt ) {
                XP_ASSERT(0);
                break;
            } else if ( askResponse == buttxts[ii].result ) {
                *chosen = ii;
                break;
            }
        }
    }
    return result;
}

#endif
