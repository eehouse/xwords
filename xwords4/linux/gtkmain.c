/* -*-mode: C; fill-column: 78; c-basic-offset: 4;  compile-command: "make MEMDEBUG=TRUE -j3"; -*- */
/* 
 * Copyright 2000-2013 by Eric House (xwords@eehouse.org).  All rights
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

#include "main.h"
#include "gamesdb.h"
#include "gtkboard.h"
#include "linuxmain.h"

enum { NAME_ITEM, N_ITEMS };

typedef struct _GTKGamesGlobals {
    sqlite3* pDb;
    GtkAppGlobals globals;
    LaunchParams* params;
} GTKGamesGlobals;


static void
init_games_list( GtkWidget* list )
{
    GtkCellRenderer* renderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn* column = 
        gtk_tree_view_column_new_with_attributes( "Games", renderer, 
                                                  "text", NAME_ITEM, NULL );
    gtk_tree_view_append_column( GTK_TREE_VIEW(list), column );
    GtkListStore* store = gtk_list_store_new( N_ITEMS, G_TYPE_STRING );
    gtk_tree_view_set_model( GTK_TREE_VIEW(list), GTK_TREE_MODEL(store) );
    g_object_unref( store );
}

static void
handle_newgame_button( GtkWidget* XP_UNUSED(widget), void* closure )
{
    GTKGamesGlobals* gg = (GTKGamesGlobals*)closure;
    XP_LOGF( "%s called", __func__ );
    GtkAppGlobals* globals = malloc( sizeof(*globals) );
    initGlobals( globals, gg->params );
    if ( !makeNewGame( globals ) ) {
        freeGlobals( globals );
    } else {
        GtkWidget* gameWindow = globals->window;
        gtk_widget_show( gameWindow );
    }
}

static GtkWidget* 
makeGamesWindow( GTKGamesGlobals* gg ) 
{
    GtkWidget* window;

    window = gtk_window_new( GTK_WINDOW_TOPLEVEL );

    GtkWidget* vbox = gtk_vbox_new( FALSE, 0 );
    gtk_container_add( GTK_CONTAINER(window), vbox );
    gtk_widget_show( vbox );
    GtkWidget* list = gtk_tree_view_new();
    gtk_container_add( GTK_CONTAINER(vbox), list );
    init_games_list( list );
    GtkWidget* button = gtk_button_new_with_label( "New Game" );
    gtk_container_add( GTK_CONTAINER(vbox), button );
    g_signal_connect( GTK_OBJECT(button), "clicked",
                      G_CALLBACK(handle_newgame_button), gg );
    gtk_widget_show( button );

    gtk_widget_show( window );
    return window;
}

int
gtkmain( LaunchParams* params )
{
    GTKGamesGlobals gg = {0};
    gg.params = params;
    XP_LOGF( "%s: I'M HERE!!! (calling makeGamesDB())", __func__ );
    gg.pDb = openGamesDB();

    (void)makeGamesWindow( &gg );
    gtk_main();

    closeGamesDB( gg.pDb );

    XP_LOGF( "%s: I'M BACK!!!", __func__ );
    return 0;
} /* gtkmain */

#endif /* PLATFORM_GTK */
