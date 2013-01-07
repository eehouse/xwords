/* -*- compile-command: "make MEMDEBUG=TRUE -j3"; -*- */
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

static void
recordOpened( GTKGamesGlobals* gg, GtkAppGlobals* globals )
{
    gg->globalsList = g_slist_prepend( gg->globalsList, globals );
    globals->gg = gg;
}

static void
recordClosed( GTKGamesGlobals* gg, GtkAppGlobals* globals )
{
    gg->globalsList = g_slist_remove( gg->globalsList, globals );
}

static XP_Bool
gameIsOpen( GTKGamesGlobals* gg, sqlite3_int64 rowid )
{
    XP_Bool found = XP_FALSE;
    GSList* iter;
    for ( iter = gg->globalsList; !!iter && !found; iter = iter->next ) {
        GtkAppGlobals* globals = (GtkAppGlobals*)iter->data;
        found = globals->cGlobals.selRow == rowid;
    }
    return found;
}

enum { ROW_ITEM, NAME_ITEM, N_ITEMS };

/* Prototype for selection handler callback */
static void 
tree_selection_changed_cb( GtkTreeSelection* selection, gpointer data )
{
    LOG_FUNC();
    GTKGamesGlobals* gg = (GTKGamesGlobals*)data;

    GtkTreeIter iter;
    GtkTreeModel *model;
    gchar *row;

    if ( gtk_tree_selection_get_selected( selection, &model, &iter ) ) {
        gtk_tree_model_get( model, &iter, ROW_ITEM, &row, -1 );
        sscanf( row, "%lld", &gg->selRow );
        g_print ("You selected row %s (parsed: %lld)\n", row, gg->selRow );
        g_free( row );
    }
}

static GtkWidget*
init_games_list( GTKGamesGlobals* gg )
{
    GtkWidget* list = gtk_tree_view_new();
    GtkCellRenderer* renderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn* column = 
        gtk_tree_view_column_new_with_attributes( "Row", renderer, "text", 
                                                  ROW_ITEM, NULL );
    gtk_tree_view_append_column( GTK_TREE_VIEW(list), column );

    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes( "Name", renderer, "text",
                                                       NAME_ITEM, NULL );
    gtk_tree_view_append_column( GTK_TREE_VIEW(list), column );

    GtkListStore* store = gtk_list_store_new( N_ITEMS, // G_TYPE_INT64, 
                                              G_TYPE_STRING, G_TYPE_STRING );
    gtk_tree_view_set_model( GTK_TREE_VIEW(list), GTK_TREE_MODEL(store) );
    g_object_unref( store );

    GtkTreeSelection* select = 
        gtk_tree_view_get_selection( GTK_TREE_VIEW (list) );
    gtk_tree_selection_set_mode( select, GTK_SELECTION_SINGLE );
    g_signal_connect( G_OBJECT(select), "changed",
                      G_CALLBACK (tree_selection_changed_cb), gg );
    return list;
}

static void
add_to_list( GtkWidget* list, sqlite3_int64* rowid, const gchar* str )
{
    GtkListStore* store = 
        GTK_LIST_STORE( gtk_tree_view_get_model(GTK_TREE_VIEW(list)));
    GtkTreeIter iter;
    gtk_list_store_append( store, &iter );
    XP_LOGF( "adding %lld, %s", *rowid, str );
    gchar buf[16];
    snprintf( buf, sizeof(buf), "%lld", *rowid );
    gtk_list_store_set( store, &iter, 
                        ROW_ITEM, buf,
                        NAME_ITEM, str,
                        -1 );
    XP_LOGF( "DONE adding" );
}

static void
handle_newgame_button( GtkWidget* XP_UNUSED(widget), void* closure )
{
    GTKGamesGlobals* gg = (GTKGamesGlobals*)closure;
    XP_LOGF( "%s called", __func__ );
    GtkAppGlobals* globals = malloc( sizeof(*globals) );
    gg->params->needsNewGame = XP_FALSE;
    initGlobals( globals, gg->params );
    if ( !makeNewGame( globals ) ) {
        freeGlobals( globals );
    } else {
        GtkWidget* gameWindow = globals->window;
        globals->cGlobals.pDb = gg->pDb;
        globals->cGlobals.selRow = -1;
        recordOpened( gg, globals );
        gtk_widget_show( gameWindow );
    }
}

static void
handle_open_button( GtkWidget* XP_UNUSED(widget), void* closure )
{
    GTKGamesGlobals* gg = (GTKGamesGlobals*)closure;
    if ( -1 != gg->selRow && !gameIsOpen( gg, gg->selRow ) ) {
        gg->params->needsNewGame = XP_FALSE;
        GtkAppGlobals* globals = malloc( sizeof(*globals) );
        initGlobals( globals, gg->params );
        globals->cGlobals.pDb = gg->pDb;
        globals->cGlobals.selRow = gg->selRow;
        recordOpened( gg, globals );
        gtk_widget_show( globals->window );
    }
}

static void
handle_quit_button( GtkWidget* XP_UNUSED(widget), gpointer data )
{
    GTKGamesGlobals* gg = (GTKGamesGlobals*)data;
    gg = gg;
    gtk_main_quit();
}

static void
handle_destroy( GtkWidget* XP_UNUSED(widget), gpointer data )
{
    LOG_FUNC();
    GTKGamesGlobals* gg = (GTKGamesGlobals*)data;
    GSList* iter;
    for ( iter = gg->globalsList; !!iter; iter = iter->next ) {
        GtkAppGlobals* globals = (GtkAppGlobals*)iter->data;
        freeGlobals( globals );
    }
    g_slist_free( gg->globalsList );
    gtk_main_quit();
}

static void
addButton( gchar* label, GtkWidget* parent, GCallback proc, void* closure )
{
    GtkWidget* button = gtk_button_new_with_label( label );
    gtk_container_add( GTK_CONTAINER(parent), button );
    g_signal_connect( GTK_OBJECT(button), "clicked",
                      G_CALLBACK(proc), closure );
    gtk_widget_show( button );
}

static GtkWidget* 
makeGamesWindow( GTKGamesGlobals* gg )
{
    GtkWidget* window;

    window = gtk_window_new( GTK_WINDOW_TOPLEVEL );
    g_signal_connect( G_OBJECT(window), "destroy",
                      G_CALLBACK(handle_destroy), gg );
    
    GtkWidget* vbox = gtk_vbox_new( FALSE, 0 );
    gtk_container_add( GTK_CONTAINER(window), vbox );
    gtk_widget_show( vbox );
    GtkWidget* list = init_games_list( gg );
    gg->listWidget = list;
    gtk_container_add( GTK_CONTAINER(vbox), list );
    
    gtk_widget_show( list );

    GSList* games = listGames( gg );
    for ( GSList* iter = games; !!iter; iter = iter->next ) {
        XP_UCHAR name[128];
        sqlite3_int64* rowid = (sqlite3_int64*)iter->data;
        getGameName( gg, rowid, name, VSIZE(name) );
        add_to_list( list, rowid, name );
    }
    g_slist_free( games );

    GtkWidget* hbox = gtk_hbox_new( FALSE, 0 );
    gtk_widget_show( hbox );
    gtk_container_add( GTK_CONTAINER(vbox), hbox );

    addButton( "New game", hbox, G_CALLBACK(handle_newgame_button), gg );
    addButton( "Open", hbox, G_CALLBACK(handle_open_button), gg );
    addButton( "Quit", hbox, G_CALLBACK(handle_quit_button), gg );

    gtk_widget_show( window );
    return window;
}

static gint
freeGameGlobals( gpointer data )
{
    LOG_FUNC();
    GtkAppGlobals* globals = (GtkAppGlobals*)data;
    GTKGamesGlobals* gg = globals->gg;
    recordClosed( gg, globals );
    freeGlobals( globals );
    return 0;                   /* don't run again */
}

void
windowDestroyed( GtkAppGlobals* globals )
{
    /* schedule to run after we're back to main loop */
    (void)g_idle_add( freeGameGlobals, globals );
}

void
newGameSaved( void* closure )
{
    GtkAppGlobals* globals = (GtkAppGlobals*)closure;
    GTKGamesGlobals* gg = globals->gg;
    CommonGlobals* cGlobals = &globals->cGlobals;
    XP_UCHAR buf[128];
    getGameName( gg, &cGlobals->selRow, buf, sizeof(buf) );
    add_to_list( gg->listWidget, &cGlobals->selRow, buf );
}

int
gtkmain( LaunchParams* params )
{
    GTKGamesGlobals gg = {0};
    gg.selRow = -1;
    gg.params = params;
    XP_LOGF( "%s: I'M HERE!!! (calling makeGamesDB())", __func__ );
    gg.pDb = openGamesDB( params->dbName );

    (void)makeGamesWindow( &gg );
    gtk_main();

    closeGamesDB( gg.pDb );

    XP_LOGF( "%s: I'M BACK!!!", __func__ );
    return 0;
} /* gtkmain */

#endif /* PLATFORM_GTK */
