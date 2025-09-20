
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
#include "gtkutils.h"
#include "gtkask.h"

typedef struct _ChatState {
    XW_DUtilCtxt* dutil;
    GameRef gr;
    GtkWidget* dialog;
    GtkWidget* chatVbox;
    GtkWidget* entry;
} ChatState;

static void
addChats( ChatState* cs )
{
    GtkWidget* chatVbox = cs->chatVbox;

    /* Remove everything -- though now there's only ever one child, a grid. */
    GList* children = gtk_container_get_children(GTK_CONTAINER(chatVbox));
    for( GList* iter = children; iter != NULL; iter = g_list_next(iter)) {
        gtk_container_remove( GTK_CONTAINER(chatVbox), GTK_WIDGET(iter->data) );
    }
    g_list_free(children);

    GtkWidget* grid = gtk_grid_new();
    gtk_box_pack_start( GTK_BOX(chatVbox), grid, FALSE, TRUE, 0 );
    gtk_grid_set_column_spacing( GTK_GRID(grid), 3 );

    XP_U16 nChats = gr_getChatCount( cs->dutil, cs->gr, NULL_XWE );
    for ( XP_U16 ii = 0; ii < nChats; ++ii ) {
        XP_UCHAR msg[1024];
        XP_U16 len = VSIZE(msg);
        XP_S16 from;
        gr_getNthChat( cs->dutil, cs->gr, NULL_XWE, ii, msg,
                       &len, &from, NULL, XP_TRUE );

        XP_UCHAR name[64];
        XP_U16 nameLen = VSIZE(name);
        gr_getPlayerName( cs->dutil, cs->gr, NULL_XWE, from, name, &nameLen );

        gtk_grid_attach( GTK_GRID(grid), gtk_label_new( name ), 0, ii, 1, 1 );
        gtk_grid_attach( GTK_GRID(grid), gtk_label_new( msg ), 1, ii, 1, 1 );
    }
    gtk_widget_show_all( chatVbox );
}

static void
onSend( GtkWidget* XP_UNUSED(widget), void* closure )
{
    ChatState* cs = (ChatState*)closure;
    GtkWidget* entry = cs->entry;
    const char* text = gtk_entry_get_text( GTK_ENTRY(entry) );
    if ( text[0] ) {
        gr_sendChat( cs->dutil, cs->gr, NULL_XWE, text );
        gtk_entry_set_text( GTK_ENTRY(entry), "" );
        addChats( cs );
    }
}

static void
onDone( GtkWidget* XP_UNUSED(widget), void* XP_UNUSED(closure) )
{
    gtk_main_quit();
}

static void
onDelete( GtkWidget* XP_UNUSED(widget), void* closure )
{
    ChatState* cs = (ChatState*)closure;
    GtkButtonsType buttons = GTK_BUTTONS_YES_NO;
    gint chosen = gtkask( cs->dialog,
                          "Are you sure you want to delete all chat messages?"
                          "\n\n(There is no effect on other devices in this game.)",
                          buttons, NULL );
    if ( GTK_RESPONSE_OK == chosen || chosen == GTK_RESPONSE_YES ) {
        gr_deleteChats( cs->dutil, cs->gr, NULL_XWE );
        addChats( cs );
    }
}

static void
launchChatOnce( GtkGameGlobals* globals )
{
    XP_ASSERT( !globals->chatOpenState );

    ChatState* state = g_malloc0( sizeof(*state) );
    globals->chatOpenState = state;
    state->dutil = globals->cGlobals.params->dutil;
    state->gr = globals->cGlobals.gr;

    state->dialog = gtk_dialog_new();
    gtk_window_set_modal( GTK_WINDOW(state->dialog), TRUE );
    /* This will center me on the parent (board) window */
    gtk_window_set_transient_for( GTK_WINDOW(state->dialog),
                                  GTK_WINDOW(globals->window) );

    GtkWidget* vbox = gtk_box_new( GTK_ORIENTATION_VERTICAL, 0 );
    gtk_dialog_add_action_widget( GTK_DIALOG(state->dialog), vbox, 0 );

    /* Container for chat history */
    state->chatVbox = gtk_box_new( GTK_ORIENTATION_VERTICAL, 0 );
    gtk_box_pack_start( GTK_BOX(vbox), state->chatVbox, FALSE, TRUE, 0 );
    addChats( state );

    /* Entry for editing text */
    state->entry = gtk_entry_new();
    gtk_box_pack_start( GTK_BOX(vbox), state->entry, FALSE, TRUE, 0 );
    gtk_entry_set_placeholder_text(GTK_ENTRY(state->entry), "Your message here");

    /* Buttons */
    GtkWidget* hbox = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 0 );
    gtk_box_pack_start( GTK_BOX(vbox), hbox, FALSE, TRUE, 0 );
    GtkWidget* sendButton = makeButton( "Send", (GCallback)onSend, state );
    gtk_box_pack_start( GTK_BOX(hbox), sendButton, FALSE, TRUE, 0 );
    GtkWidget* doneButton = makeButton( "Done", (GCallback)onDone, state );
    gtk_box_pack_start( GTK_BOX(hbox), doneButton, FALSE, TRUE, 0 );
    GtkWidget* clearButton = makeButton( "Delete all", (GCallback)onDelete, state );
    gtk_box_pack_start( GTK_BOX(hbox), clearButton, FALSE, TRUE, 0 );

    gtk_widget_show_all( state->dialog );
    gtk_main();

    gtk_widget_destroy( state->dialog );

    g_free( state );
    globals->chatOpenState = NULL;
}

void
launchChat( GtkGameGlobals* globals )
{
    if ( !!globals->chatOpenState ) {
        ChatState* cs = (ChatState*)globals->chatOpenState;
        addChats(cs);
    } else {
        launchChatOnce( globals );
    }
}

#endif
