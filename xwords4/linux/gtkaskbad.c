/* -*- compile-command: "make MEMDEBUG=TRUE -j5"; -*- */
/* 
 * Copyright 2001-2024 by Eric House (xwords@eehouse.org).  All rights
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

#include "gtkaskbad.h"
#include "gtkutils.h"
#include "dbgutil.h"

typedef struct _AskBadState {
    GtkWidget* check;
    GStrv words;
    bool skipNext;
    const char* dictName;
} AskBadState;

/* static void */
/* handle_response( GtkWidget* item, AskBadState* state ) */
/* { */
/*     LOG_FUNC(); */
/* } */

static void
handle_check_toggled( GtkWidget* item, AskBadState* state )
{
    XP_ASSERT( item == state->check );
    state->skipNext = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(item));
    XP_LOGFF( "checked: %s", boolToStr(state->skipNext) );
}

/* static void */
/* handle_ok( GtkWidget* XP_UNUSED(item), AskBadState* state ) */
/* { */
/*     state->confirmed = true; */
/*     gtk_main_quit(); */
/* } */

/* static void */
/* handle_cancel( GtkWidget* XP_UNUSED(item), AskBadState* state ) */
/* { */
/*     state->confirmed = false; */
/*     gtk_main_quit(); */
/* } */

static GtkWidget*
buildDialog( AskBadState* state )
{
    GtkWidget* dialog = gtk_dialog_new_with_buttons( NULL, NULL, //GtkWindow *parent,
                                                     GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                     "Ok", GTK_RESPONSE_ACCEPT,
                                                     "Cancel", GTK_RESPONSE_REJECT,
                                                     NULL );

    /* GtkWidget* bc = gtk_dialog_get_action_area( GTK_DIALOG(dialog) ); */
    /* g_object_set_property( G_OBJECT(bc), "halign", GTK_ALIGN_CENTER ); */

    GtkWidget* vbox = gtk_dialog_get_content_area( GTK_DIALOG(dialog) );

    gchar* words = g_strjoinv( "\n", state->words );
    
    gchar* msg = g_strdup_printf("The word (or words) below are not in the wordlist %s. "
                                 "\n\n%s\n\n"
                                 "Would you like to accept them anyway?\n",
                                 state->dictName, words );

    GtkWidget* label = gtk_label_new ( msg );
    g_free( words );
    g_free( msg );
    gtk_widget_show( label );
    gtk_box_pack_start( GTK_BOX(vbox), label, FALSE, TRUE, 0 );

    state->check = gtk_check_button_new_with_label( "Always accept" );
    g_signal_connect( state->check, "toggled",
                      (GCallback)handle_check_toggled, state );
    gtk_widget_show( state->check );
    gtk_box_pack_start( GTK_BOX(vbox), state->check, FALSE, TRUE, 0 );

    gtk_widget_show( vbox );

    return dialog;
}

/* return true if not cancelled */
bool
gtkAskBad( GtkGameGlobals* globals, GStrv words, const char* dictName,
           bool* skipNext )
{
    XP_USE( globals );

    AskBadState state = {
        .words = words,
        .dictName = dictName,
    };
    GtkWidget* dialog = buildDialog( &state );
    gtk_window_set_modal( GTK_WINDOW( dialog ), TRUE );

    gtk_widget_show_all( dialog );
    gint response = gtk_dialog_run( GTK_DIALOG(dialog) );
    gtk_widget_destroy( dialog );
    
    *skipNext = state.skipNext;
    return GTK_RESPONSE_ACCEPT == response;
}

#endif
