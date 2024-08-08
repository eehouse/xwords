/* -*- compile-command: "make MEMDEBUG=TRUE -j5"; -*- */
/* 
 * Copyright 2000 - 2016 by Eric House (xwords@eehouse.org).  All rights
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

#include "strutils.h"
#include "smsproto.h"
#include "main.h"
#include "gtkmain.h"
#include "gamesdb.h"
#include "gtkboard.h"
#include "linuxmain.h"
#include "linuxutl.h"
#include "relaycon.h"
#include "mqttcon.h"
#include "linuxsms.h"
#include "gtkask.h"
#include "device.h"
#include "gtkkpdlg.h"
#include "gtknewgame.h"
#include "gtkrmtch.h"
#include "gsrcwrap.h"
#include "extcmds.h"

static void onNewData( GtkAppGlobals* apg, sqlite3_int64 rowid, 
                       XP_Bool isNew );
static void updateButtons( GtkAppGlobals* apg );
static void open_row( GtkAppGlobals* apg, sqlite3_int64 row, XP_Bool isNew );

static void
recordOpened( GtkAppGlobals* apg, GtkGameGlobals* globals )
{
    apg->cag.globalsList = g_slist_prepend( apg->cag.globalsList, globals );
    globals->apg = apg;
}

static void
recordClosed( GtkAppGlobals* apg, GtkGameGlobals* globals )
{
    apg->cag.globalsList = g_slist_remove( apg->cag.globalsList, globals );
}

static XP_Bool
gameIsOpen( GtkAppGlobals* apg, sqlite3_int64 rowid )
{
    XP_Bool found = XP_FALSE;
    GSList* iter;
    for ( iter = apg->cag.globalsList; !!iter && !found; iter = iter->next ) {
        GtkGameGlobals* globals = (GtkGameGlobals*)iter->data;
        found = globals->cGlobals.rowid == rowid;
    }
    return found;
}

static GtkGameGlobals*
findOpenGame( const GtkAppGlobals* apg, sqlite3_int64* rowid, XP_U32* gameID )
{
    GtkGameGlobals* result = NULL;
    GSList* iter;
    for ( iter = apg->cag.globalsList; !!iter; iter = iter->next ) {
        GtkGameGlobals* globals = (GtkGameGlobals*)iter->data;
        CommonGlobals* cGlobals = &globals->cGlobals;
        if ( *rowid && cGlobals->rowid == *rowid ) {
            result = globals;
            *gameID = cGlobals->gi->gameID;
            break;
        } else if ( gameID && cGlobals->gi->gameID == *gameID ) {
            result = globals;
            *rowid = cGlobals->rowid;
            break;
        }
    }
    return result;
}

enum { ROW_ITEM, ROW_THUMB, NAME_ITEM, CREATED_ITEM, GAMEID_ITEM,
       LANG_ITEM, SEED_ITEM, ROLE_ITEM, CHANNEL_ITEM, CONN_ITEM,
#ifdef XWFEATURE_RELAY
       RELAYID_ITEM,
#endif
       NPACKETS_ITEM, OVER_ITEM, TURN_ITEM,LOCAL_ITEM, NMOVES_ITEM,
       NTOTAL_ITEM, MISSING_ITEM, LASTTURN_ITEM, DUPTIMER_ITEM,

       N_ITEMS,
};

static void
foreachProc( GtkTreeModel* model, GtkTreePath* XP_UNUSED(path),
             GtkTreeIter* iter, gpointer data )
{
    GtkAppGlobals* apg = (GtkAppGlobals*)data;
    sqlite3_int64 rowid;
    gtk_tree_model_get( model, iter, ROW_ITEM, &rowid, -1 );
    apg->selRows = g_array_append_val( apg->selRows, rowid );
}

static void
tree_selection_changed_cb( GtkTreeSelection* selection, gpointer data )
{
    GtkAppGlobals* apg = (GtkAppGlobals*)data;

    apg->selRows = g_array_set_size( apg->selRows, 0 );
    gtk_tree_selection_selected_foreach( selection, foreachProc, apg );

    updateButtons( apg );
}

static void
row_activated_cb( GtkTreeView* tree_view, GtkTreePath* path,
                  GtkTreeViewColumn* XP_UNUSED(column), gpointer data )
{
    GtkAppGlobals* apg = (GtkAppGlobals*)data;
    XP_ASSERT( tree_view == GTK_TREE_VIEW(apg->listWidget) );
    GtkTreeModel* model = gtk_tree_view_get_model( tree_view );
    GtkTreeIter iter;
    if ( gtk_tree_model_get_iter( model, &iter, path ) ) {
        sqlite3_int64 rowid;
        gtk_tree_model_get( model, &iter, ROW_ITEM, &rowid, -1 );
        open_row( apg, rowid, XP_FALSE );
    }
}

static void
removeRow( GtkAppGlobals* apg, sqlite3_int64 rowid )
{
    GtkTreeModel* model = 
        gtk_tree_view_get_model(GTK_TREE_VIEW(apg->listWidget));
    GtkTreeIter iter;
    gboolean valid;
    for ( valid = gtk_tree_model_get_iter_first( model, &iter );
          valid;
          valid = gtk_tree_model_iter_next( model, &iter ) ) {
        sqlite3_int64 tmpid;
        gtk_tree_model_get( model, &iter, ROW_ITEM, &tmpid, -1 );
        if ( tmpid == rowid ) {
            gtk_list_store_remove( GTK_LIST_STORE(model), &iter );
            break;
        }
    }
}

static void
addTextColumn( GtkWidget* list, const gchar* title, int item )
{
    GtkCellRenderer* renderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn* column =
        gtk_tree_view_column_new_with_attributes( title, renderer, "text", 
                                                  item, NULL );
    gtk_tree_view_append_column( GTK_TREE_VIEW(list), column );
}

static void
addImageColumn( GtkWidget* list, const gchar* title, int item )
{
    GtkCellRenderer* renderer = gtk_cell_renderer_pixbuf_new();
    GtkTreeViewColumn* column =
        gtk_tree_view_column_new_with_attributes( title, renderer,
                                                  "pixbuf", item, NULL );
    gtk_tree_view_append_column( GTK_TREE_VIEW(list), column );
}

static GtkWidget*
init_games_list( GtkAppGlobals* apg )
{
    GtkWidget* list = gtk_tree_view_new();
    apg->listWidget = list;

    addTextColumn( list, "Row", ROW_ITEM );
    addImageColumn( list, "Snap", ROW_THUMB );
    addTextColumn( list, "Name", NAME_ITEM );
    addTextColumn( list, "Created", CREATED_ITEM );
    addTextColumn( list, "GameID", GAMEID_ITEM );
    addTextColumn( list, "Lang", LANG_ITEM );
    addTextColumn( list, "Seed", SEED_ITEM );
    addTextColumn( list, "Role", ROLE_ITEM );
    addTextColumn( list, "Channel", CHANNEL_ITEM );
    addTextColumn( list, "Conn. via", CONN_ITEM );
#ifdef XWFEATURE_RELAY
    addTextColumn( list, "RelayID", RELAYID_ITEM );
#endif
    addTextColumn( list, "nPackets", NPACKETS_ITEM );
    addTextColumn( list, "Ended", OVER_ITEM );
    addTextColumn( list, "Turn", TURN_ITEM );
    addTextColumn( list, "Local", LOCAL_ITEM );
    addTextColumn( list, "NMoves", NMOVES_ITEM );
    addTextColumn( list, "NTotal", NTOTAL_ITEM );
    addTextColumn( list, "NMissing", MISSING_ITEM );
    addTextColumn( list, "LastTurn", LASTTURN_ITEM );
    addTextColumn( list, "DupTimerFires", DUPTIMER_ITEM );

    GtkListStore* store = gtk_list_store_new( N_ITEMS, 
                                              G_TYPE_INT64,   /* ROW_ITEM */
                                              GDK_TYPE_PIXBUF,/* ROW_THUMB */
                                              G_TYPE_STRING,  /* NAME_ITEM */
                                              G_TYPE_STRING,  /* CREATED_ITEM */
                                              G_TYPE_STRING,  /* GAMEID_ITEM */
                                              G_TYPE_STRING,  /* LANG_ITEM */
                                              G_TYPE_INT,     /* SEED_ITEM */
                                              G_TYPE_INT,     /* ROLE_ITEM */
                                              G_TYPE_INT,     /* CHANNEL_ITEM */
                                              G_TYPE_STRING,  /* CONN_ITEM */
#ifdef XWFEATURE_RELAY
                                              G_TYPE_STRING,  /*RELAYID_ITEM */
#endif
                                              G_TYPE_INT,     /* NPACKETS_ITEM */
                                              G_TYPE_BOOLEAN, /* OVER_ITEM */
                                              G_TYPE_INT,     /* TURN_ITEM */
                                              G_TYPE_STRING,  /* LOCAL_ITEM */
                                              G_TYPE_INT,     /* NMOVES_ITEM */
                                              G_TYPE_INT,     /* NTOTAL_ITEM */
                                              G_TYPE_INT,     /* MISSING_ITEM */
                                              G_TYPE_STRING,  /* LASTTURN_ITEM */
                                              G_TYPE_STRING  /* DUPTIMER_ITEM */
                                              );
    gtk_tree_view_set_model( GTK_TREE_VIEW(list), GTK_TREE_MODEL(store) );
    g_object_unref( store );

    g_signal_connect( G_OBJECT(list), "row-activated",
                      G_CALLBACK(row_activated_cb), apg );
 
    GtkTreeSelection* select =
        gtk_tree_view_get_selection( GTK_TREE_VIEW (list) );
    gtk_tree_selection_set_mode( select, GTK_SELECTION_MULTIPLE );
    g_signal_connect( G_OBJECT(select), "changed",
                      G_CALLBACK(tree_selection_changed_cb), apg );
    return list;
}

static void
add_to_list( GtkWidget* list, sqlite3_int64 rowid, XP_Bool isNew, 
             const GameInfo* gib )
{
    GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(list));
    GtkListStore* store = GTK_LIST_STORE( model );
    GtkTreeIter iter;
    XP_Bool found = XP_FALSE;
    if ( !isNew ) {
        for ( gboolean valid = gtk_tree_model_get_iter_first( model, &iter );
              valid;
              valid = gtk_tree_model_iter_next( model, &iter ) ) {
            sqlite3_int64 tmpid;
            gtk_tree_model_get( model, &iter, ROW_ITEM, &tmpid, -1 );
            if ( tmpid == rowid ) {
                found = XP_TRUE;
                break;
            }
        }
    }

    if ( !found ) {
        gtk_list_store_append( store, &iter );
    }

    gchar* localString = 0 <= gib->turn ? gib->turnLocal ? "YES"
        : "NO" : "";

    gchar createdStr[64] = {};
    if ( 0 != gib->created ) {
        formatSeconds( gib->created, createdStr, VSIZE(createdStr) );
    }
    gchar timeStr[64];
    formatSeconds( gib->lastMoveTime, timeStr, VSIZE(timeStr) );
    gchar timerStr[64] = {};
    if ( gib->dupTimerExpires ) {
        formatSeconds( gib->dupTimerExpires, timerStr, VSIZE(timeStr) );
    }

    gchar gameIDStr[16];
    sprintf( gameIDStr, "%8X", gib->gameID );

    gtk_list_store_set( store, &iter, 
                        ROW_ITEM, rowid,
                        ROW_THUMB, gib->snap,
                        NAME_ITEM, gib->name,
                        CREATED_ITEM, createdStr,
                        GAMEID_ITEM, gameIDStr,
                        LANG_ITEM, gib->isoCode,
                        SEED_ITEM, gib->seed,
                        ROLE_ITEM, gib->role,
                        CHANNEL_ITEM, gib->channelNo,
                        CONN_ITEM, gib->conn,
#ifdef XWFEATURE_RELAY
                        RELAYID_ITEM, gib->relayID,
#endif
                        TURN_ITEM, gib->turn,
                        NPACKETS_ITEM, gib->nPending,
                        OVER_ITEM, gib->gameOver,
                        LOCAL_ITEM, localString,
                        NMOVES_ITEM, gib->nMoves,
                        NTOTAL_ITEM, gib->nTotal,
                        MISSING_ITEM, gib->nMissing,
                        LASTTURN_ITEM, timeStr,
                        DUPTIMER_ITEM, timerStr,
                        -1 );
    XP_LOGF( "DONE adding" );
}

static void updateButtons( GtkAppGlobals* apg )
{
    guint count = apg->selRows->len;

    gtk_widget_set_sensitive( apg->openButton, 1 <= count );
    gtk_widget_set_sensitive( apg->rematchButton, 1 == count );
    gtk_widget_set_sensitive( apg->deleteButton, 1 <= count );
}

static void
handle_newgame_button( GtkWidget* XP_UNUSED(widget), void* closure )
{
    GtkAppGlobals* apg = (GtkAppGlobals*)closure;
    LOG_FUNC();
    GtkGameGlobals* globals = calloc( 1, sizeof(*globals) );
    apg->cag.params->needsNewGame = XP_FALSE;
    initBoardGlobalsGtk( globals, apg->cag.params, NULL );

    if ( gtkNewGameDialog( globals, globals->cGlobals.gi,
                           &globals->cGlobals.selfAddr,
                           XP_TRUE, XP_FALSE ) ) {
        GtkWidget* gameWindow = globals->window;
        globals->cGlobals.rowid = -1;
        recordOpened( apg, globals );
        gtk_widget_show( gameWindow );
    } else {
        freeGlobals( globals );
    }
}

static void
open_row( GtkAppGlobals* apg, sqlite3_int64 row, XP_Bool isNew )
{
    if ( -1 != row && !gameIsOpen( apg, row ) ) {
        if ( isNew ) {
            onNewData( apg, row, XP_TRUE );
        }

        apg->cag.params->needsNewGame = XP_FALSE;
        GtkGameGlobals* globals = malloc( sizeof(*globals) );
        initBoardGlobalsGtk( globals, apg->cag.params, NULL );
        globals->cGlobals.rowid = row;
        recordOpened( apg, globals );
        gtk_widget_show( globals->window );
    }
}

static void
handle_open_button( GtkWidget* XP_UNUSED(widget), void* closure )
{
    GtkAppGlobals* apg = (GtkAppGlobals*)closure;

    GArray* selRows = apg->selRows;
    for ( int ii = 0; ii < selRows->len; ++ii ) {
        sqlite3_int64 row = g_array_index( selRows, sqlite3_int64, ii );
        open_row( apg, row, XP_FALSE );
    }
}

void
make_rematch( GtkAppGlobals* apg, const CommonGlobals* cGlobals )
{
    XP_Bool canRematch = server_canRematch( cGlobals->game.server, NULL );
    XP_ASSERT( canRematch );

    if ( canRematch ) {
        gchar gameName[128];
        int nameLen = VSIZE(gameName);
        NewOrder no;
        if ( gtkask_rematch( cGlobals, &no, gameName, &nameLen ) ) {
            LaunchParams* params = apg->cag.params;
            GtkGameGlobals* newGlobals = calloc( 1, sizeof(*newGlobals) );
            initBoardGlobalsGtk( newGlobals, params, NULL );

            XW_UtilCtxt* util = newGlobals->cGlobals.util;
            const CommonPrefs* cp = &newGlobals->cGlobals.cp;
            XP_UCHAR buf[64];
            snprintf( buf, VSIZE(buf), "Game %lX", XP_RANDOM() % 256 );
            game_makeRematch( &cGlobals->game, NULL_XWE, util, cp,
                              &newGlobals->cGlobals.procs,
                              &newGlobals->cGlobals.game, buf, &no );

            linuxSaveGame( &newGlobals->cGlobals );
            sqlite3_int64 rowid = newGlobals->cGlobals.rowid;
            freeGlobals( newGlobals );

            open_row( apg, rowid, XP_TRUE );
        }
    }
} /* make_rematch */

static void
handle_rematch_button( GtkWidget* XP_UNUSED(widget), void* closure )
{
    GtkAppGlobals* apg = (GtkAppGlobals*)closure;
    GArray* selRows = apg->selRows;
    for ( int ii = 0; ii < selRows->len; ++ii ) {
        sqlite3_int64 rowid = g_array_index( selRows, sqlite3_int64, ii );
        GtkGameGlobals tmpGlobals;
        if ( loadGameNoDraw( &tmpGlobals, apg->cag.params, rowid ) ) {
            make_rematch( apg, &tmpGlobals.cGlobals );
        }
        freeGlobals( &tmpGlobals );
    }
}

static void
delete_game( GtkAppGlobals* apg, sqlite3_int64 rowid )
{
    LaunchParams* params = apg->cag.params;
    GameInfo gib;
#ifdef DEBUG
    XP_Bool success =
#endif
        gdb_getGameInfoForRow( params->pDb, rowid, &gib );
    XP_ASSERT( success );
#ifdef XWFEATURE_RELAY
    XP_U32 clientToken = makeClientToken( rowid, gib.seed );
#endif
    removeRow( apg, rowid );
    gdb_deleteGame( params->pDb, rowid );

#ifdef XWFEATURE_RELAY
    XP_UCHAR devIDBuf[64] = {};
    gdb_fetch_safe( params->pDb, KEY_RDEVID, NULL, devIDBuf, sizeof(devIDBuf) );
    if ( '\0' != devIDBuf[0] ) {
        relaycon_deleted( params, devIDBuf, clientToken );
    } else {
        XP_LOGF( "%s: not calling relaycon_deleted: no relayID", __func__ );
    }
#endif
    g_object_unref( gib.snap );
}

static void
handle_delete_button( GtkWidget* XP_UNUSED(widget), void* closure )
{
    GtkAppGlobals* apg = (GtkAppGlobals*)closure;
    guint len = apg->selRows->len;
    for ( guint ii = 0; ii < len; ++ii ) {
        sqlite3_int64 rowid = g_array_index( apg->selRows, sqlite3_int64, ii );
        delete_game( apg, rowid );
    }
    apg->selRows = g_array_set_size( apg->selRows, 0 );
    updateButtons( apg );
    /* Need now to update the selection and sync the buttons */
}

static gint
quitIdle( gpointer XP_UNUSED(data) )
{
    gtk_main_quit();
    return 0;
}

static void
handle_destroy( GtkWidget* XP_UNUSED(widget), gpointer data )
{
    LOG_FUNC();
    GtkAppGlobals* apg = (GtkAppGlobals*)data;
    GSList* iter;
    for ( iter = apg->cag.globalsList; !!iter; iter = iter->next ) {
        GtkGameGlobals* globals = (GtkGameGlobals*)iter->data;
        destroy_board_window( NULL, globals );
        // freeGlobals( globals );
    }
    g_slist_free( apg->cag.globalsList );

    saveSize( &apg->lastConfigure, apg->cag.params->pDb, KEY_WIN_LOC );

    /* Quit via an idle proc, because other shutdown code inside
       destroy_board_window() has posted idles to clean up memory and they
       need to run first. Last posted should be last run, I hope. */
    (void)g_idle_add( quitIdle, NULL );
}

static void
handle_quit_button( GtkWidget* XP_UNUSED(widget), gpointer data )
{
    GtkAppGlobals* apg = (GtkAppGlobals*)data;
    handle_destroy( NULL, apg );
}

static gboolean
on_window_configured( GtkWidget* XP_UNUSED(widget),
                      GdkEventConfigure* event, GtkAppGlobals* apg )
{
    /* XP_LOGFF( "(x=%d, y=%d, width=%d, height=%d)",  */
    /*           event->x, event->y, event->width, event->height ); */
    apg->lastConfigure = *event;
    return FALSE;
}

static GtkWidget*
addButton( gchar* label, GtkWidget* parent, GCallback proc, void* closure )
{
    GtkWidget* button = gtk_button_new_with_label( label );
    gtk_container_add( GTK_CONTAINER(parent), button );
    g_signal_connect( button, "clicked",
                      G_CALLBACK(proc), closure );
    gtk_widget_show( button );
    return button;
}

static void
setWindowTitle( GtkAppGlobals* apg )
{
    GtkWidget* window = apg->window;
    LaunchParams* params = apg->cag.params;

    gchar title[128] = {};
    if ( !!params->dbName ) {
        strcat( title, params->dbName );
    }
#ifdef XWFEATURE_SMS
    int len = strlen( title );
    snprintf( &title[len], VSIZE(title) - len, " (phone: %s, port: %d)", 
              params->connInfo.sms.myPhone, params->connInfo.sms.port );
#endif
#ifdef XWFEATURE_RELAY
    XP_U32 relayID = linux_getDevIDRelay( params );
    len = strlen( title );
    snprintf( &title[len], VSIZE(title) - len, " (relayid: %d)", relayID );
#endif
    len = strlen( title );
    MQTTDevID devID;
    dvc_getMQTTDevID( params->dutil, NULL_XWE, &devID );
    XP_UCHAR didBuf[32];
    snprintf( &title[len], VSIZE(title) - len, " (mqtt: %s)",
              formatMQTTDevID( &devID, didBuf, VSIZE(didBuf) ) );

    gtk_window_set_title( GTK_WINDOW(window), title );
}

#define COORDS_FORMAT "x:%d;y:%d;w:%d;h:%d"
void
resizeFromSaved( GtkWidget* window, sqlite3* pDb, const gchar* key )
{
    gint xx, yy, width, height;
    gchar buf[64];
    if ( gdb_fetch_safe( pDb, key, NULL, buf, sizeof(buf)) ) {
        sscanf( buf, COORDS_FORMAT, &xx, &yy, &width, &height );
    } else {
        xx = yy = 100;
        width = height = 500;
    }
    gtk_window_resize( GTK_WINDOW(window), width, height );
    gtk_window_move( GTK_WINDOW(window), xx, yy );
}

static void
formatCoords( gchar* buf, const GdkEventConfigure* lastSize )
{
    sprintf( buf, COORDS_FORMAT, lastSize->x, lastSize->y,
             lastSize->width, lastSize->height );
}

void
saveSize( const GdkEventConfigure* lastSize, sqlite3* pDb, const gchar* key )
{
    gchar buf[64];
    formatCoords( buf, lastSize );
    gdb_store( pDb, key, buf );
}
#undef COORDS_FORMAT

static void
handle_known_players( GtkWidget* XP_UNUSED(widget), GtkAppGlobals* apg )
{
    gtkkp_show( apg, GTK_WINDOW(apg->window) );
}

#ifdef XWFEATURE_RELAY
static void
handle_movescheck( GtkWidget* XP_UNUSED(widget), GtkAppGlobals* apg )
{
    LaunchParams* params = apg->cag.params;
    relaycon_checkMsgs( params );
}

static void
handle_relayid_to_clip( GtkWidget* XP_UNUSED(widget), GtkAppGlobals* apg )
{
    LaunchParams* params = apg->cag.params;
    XP_U32 relayID = linux_getDevIDRelay( params );
    gchar str[32];
    snprintf( &str[0], VSIZE(str), "%d", relayID );
    GtkClipboard* clipboard = gtk_clipboard_get( GDK_SELECTION_CLIPBOARD );
    gtk_clipboard_set_text( clipboard, str, strlen(str) );
}
#endif

static void
handle_mqttid_to_clip( GtkWidget* XP_UNUSED(widget), GtkAppGlobals* apg )
{
    LaunchParams* params = apg->cag.params;
    const gchar* devIDStr = mqttc_getDevIDStr( params );
    GtkClipboard* clipboard = gtk_clipboard_get( GDK_SELECTION_CLIPBOARD );
    gtk_clipboard_set_text( clipboard, devIDStr, strlen(devIDStr) );
}

static void
makeGamesWindow( GtkAppGlobals* apg )
{
    GtkWidget* window;
    LaunchParams* params = apg->cag.params;

    apg->window = window = gtk_window_new( GTK_WINDOW_TOPLEVEL );
    g_signal_connect( G_OBJECT(window), "destroy",
                      G_CALLBACK(handle_destroy), apg );
    g_signal_connect( window, "configure_event",
                      G_CALLBACK(on_window_configured), apg );

    setWindowTitle( apg );

    resizeFromSaved( window, params->pDb, KEY_WIN_LOC );

    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add( GTK_CONTAINER(window), vbox );
    gtk_widget_show( vbox );

    // add menubar here
    GtkWidget* menubar = gtk_menu_bar_new();
    GtkWidget* netMenu = makeAddSubmenu( menubar, "Menu" );
    (void)createAddItem( netMenu, "Known Players",
                         (GCallback)handle_known_players, apg );
#ifdef XWFEATURE_RELAY
    if ( params->useHTTP ) {
        (void)createAddItem( netMenu, "Check for moves",
                             (GCallback)handle_movescheck, apg );
    }
    (void)createAddItem( netMenu, "copy relayid",
                         (GCallback)handle_relayid_to_clip, apg );
#endif
    (void)createAddItem( netMenu, "copy mqtt devid",
                         (GCallback)handle_mqttid_to_clip, apg );
    gtk_widget_show( menubar );
    gtk_box_pack_start( GTK_BOX(vbox), menubar, FALSE, TRUE, 0 );

    GtkWidget* swin = gtk_scrolled_window_new( NULL, NULL );
    gboolean expand = TRUE;  // scrollable window gets all extra space
    gtk_box_pack_start( GTK_BOX(vbox), swin, expand, TRUE, 0 );
    gtk_widget_show( swin );

    GtkWidget* list = init_games_list( apg );
    gtk_container_add( GTK_CONTAINER(swin), list );
    
    gtk_widget_show( list );

    if ( !!params->pDb ) {
        GSList* games = gdb_listGames( params->pDb );
        for ( GSList* iter = games; !!iter; iter = iter->next ) {
            sqlite3_int64* rowid = (sqlite3_int64*)iter->data;
            onNewData( apg, *rowid, XP_TRUE );
        }
        gdb_freeGamesList( games );
    }

    GtkWidget* hbox = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 0 );
    gtk_widget_show( hbox );
    gtk_container_add( GTK_CONTAINER(vbox), hbox );

    (void)addButton( "New game", hbox, G_CALLBACK(handle_newgame_button), apg );
    apg->openButton = addButton( "Open", hbox, 
                                 G_CALLBACK(handle_open_button), apg );
    apg->rematchButton = addButton( "Rematch", hbox, 
                                    G_CALLBACK(handle_rematch_button), apg );
    apg->deleteButton = addButton( "Delete", hbox, 
                                   G_CALLBACK(handle_delete_button), apg );
    (void)addButton( "Quit", hbox, G_CALLBACK(handle_quit_button), apg );
    updateButtons( apg );

    gtk_widget_show( window );
}

static GtkWidget* 
openDBFile( GtkAppGlobals* apg )
{
    GtkGameGlobals* globals = calloc( 1, sizeof(*globals) );
    initBoardGlobalsGtk( globals, apg->cag.params, NULL );

    GtkWidget* window = globals->window;
    gtk_widget_show( window );
    return window;
}

static gint
freeGameGlobals( gpointer data )
{
    LOG_FUNC();
    GtkGameGlobals* globals = (GtkGameGlobals*)data;
    GtkAppGlobals* apg = globals->apg;
    if ( !!apg ) {
        recordClosed( apg, globals );
    }
    freeGlobals( globals );
    return 0;                   /* don't run again */
}

void
windowDestroyed( GtkGameGlobals* globals )
{
    /* schedule to run after we're back to main loop */
    (void)g_idle_add( freeGameGlobals, globals );
}

static void
onNewData( GtkAppGlobals* apg, sqlite3_int64 rowid, XP_Bool isNew )
{
    GameInfo gib;
    if ( gdb_getGameInfoForRow( apg->cag.params->pDb, rowid, &gib ) ) {
        add_to_list( apg->listWidget, rowid, isNew, &gib );
        g_object_unref( gib.snap );
    }
}

static XP_U16
feedBufferGTK( GtkAppGlobals* apg, sqlite3_int64 rowid, 
               const XP_U8* buf, XP_U16 len, const CommsAddrRec* from )
{
    XP_U16 seed = 0;
    XP_U32 ignored = 0;
    GtkGameGlobals* globals = findOpenGame( apg, &rowid, &ignored );

    if ( !!globals ) {
        gameGotBuf( &globals->cGlobals, XP_TRUE, buf, len, from );
        seed = comms_getChannelSeed( globals->cGlobals.game.comms );
    } else {
        GtkGameGlobals tmpGlobals;
        if ( loadGameNoDraw( &tmpGlobals, apg->cag.params, rowid ) ) {
            gameGotBuf( &tmpGlobals.cGlobals, XP_FALSE, buf, len, from );
            seed = comms_getChannelSeed( tmpGlobals.cGlobals.game.comms );
            linuxSaveGame( &tmpGlobals.cGlobals );
        }
        freeGlobals( &tmpGlobals );
    }
    return seed;
}

/* Stuff common to receiving invitations */
static void
gameFromInvite( GtkAppGlobals* apg, const NetLaunchInfo* nli )
{
    LOG_FUNC();

    GtkGameGlobals* globals = calloc( 1, sizeof(*globals) );
    CommonGlobals* cGlobals = &globals->cGlobals;

    LaunchParams* params = apg->cag.params;
    initBoardGlobalsGtk( globals, params, NULL );

    CommsAddrRec selfAddr;
    makeSelfAddress( &selfAddr, params );

    CommonPrefs* cp = &cGlobals->cp;
    game_makeFromInvite( &cGlobals->game, NULL_XWE, nli,
                         &selfAddr, cGlobals->util, cGlobals->draw,
                         cp, &cGlobals->procs );

    linuxSaveGame( cGlobals );
    sqlite3_int64 rowid = cGlobals->rowid;
    freeGlobals( globals );

    open_row( apg, rowid, XP_TRUE );

    LOG_RETURN_VOID();
}

void
inviteReceivedGTK( void* closure, const NetLaunchInfo* invite )
{
    GtkAppGlobals* apg = (GtkAppGlobals*)closure;

    XP_U32 gameID = invite->gameID;
    sqlite3_int64 rowids[1];
    int nRowIDs = VSIZE(rowids);
    gdb_getRowsForGameID( apg->cag.params->pDb, gameID, rowids, &nRowIDs );

    bool doIt = 0 == nRowIDs;
    if ( ! doIt ) {
        doIt = XP_FALSE;
        XP_LOGFF( "duplicate invite; not creating game" );
        doIt = GTK_RESPONSE_YES == gtkask( apg->window,
                                           "Duplicate invitation received. Accept anyway?",
                                           GTK_BUTTONS_YES_NO, NULL );
    }
    if ( doIt ) {
        gameFromInvite( apg, invite );
    }
}

#ifdef XWFEATURE_RELAY
static void
gtkGotBuf( void* closure, const CommsAddrRec* from, 
           const XP_U8* buf, XP_U16 len )
{
    LOG_FUNC();
    GtkAppGlobals* apg = (GtkAppGlobals*)closure;
    XP_U32 clientToken;
    XP_ASSERT( sizeof(clientToken) < len );
    XP_MEMCPY( &clientToken, &buf[0], sizeof(clientToken) );
    buf += sizeof(clientToken);
    len -= sizeof(clientToken);

    sqlite3_int64 rowid;
    XP_U16 gotSeed;
    rowidFromToken( ntohl( clientToken ), &rowid, &gotSeed );

    XP_U16 seed = feedBufferGTK( apg, rowid, buf, len, from );
    XP_ASSERT( seed == 0 || gotSeed == seed );
    XP_USE( seed );
}

static void
gtkGotMsgForRow( void* closure, const CommsAddrRec* from,
                 sqlite3_int64 rowid, const XP_U8* buf, XP_U16 len )
{
    XP_LOGF( "%s(): got msg of len %d for row %lld", __func__, len, rowid );
    GtkAppGlobals* apg = (GtkAppGlobals*)closure;
    // LaunchParams* params = apg->cag.params;
    (void)feedBufferGTK( apg, rowid, buf, len, from );
    LOG_RETURN_VOID();
}

static gint
requestMsgs( gpointer data )
{
    GtkAppGlobals* apg = (GtkAppGlobals*)data;
    XP_UCHAR devIDBuf[64] = {};
    gdb_fetch_safe( apg->cag.params->pDb, KEY_RDEVID, NULL, devIDBuf, sizeof(devIDBuf) );
    if ( '\0' != devIDBuf[0] ) {
        relaycon_requestMsgs( apg->cag.params, devIDBuf );
    } else {
        XP_LOGF( "%s: not requesting messages as don't have relay id", __func__ );
    }
    return 0;                   /* don't run again */
}

static void 
gtkNoticeRcvd( void* closure )
{
    LOG_FUNC();
    GtkAppGlobals* apg = (GtkAppGlobals*)closure;
    (void)g_idle_add( requestMsgs, apg );
}
#endif

static void
smsInviteReceived( void* closure, const NetLaunchInfo* nli )
{
    GtkAppGlobals* apg = (GtkAppGlobals*)closure;
    XP_LOGFF( "(gameName=%s, gameID=%d, dictName=%s, nPlayers=%d, "
              "nHere=%d, forceChannel=%d)", nli->gameName,
              nli->gameID, nli->dict, nli->nPlayersT,
              nli->nPlayersH, nli->forceChannel );

    gameFromInvite( apg, nli );
}

void
msgReceivedGTK( void* closure, const CommsAddrRec* from, XP_U32 gameID,
                const XP_U8* buf, XP_U16 len )
{
    LOG_FUNC();
    GtkAppGlobals* apg = (GtkAppGlobals*)closure;
    LaunchParams* params =  apg->cag.params;

    sqlite3_int64 rowids[4];
    int nRowIDs = VSIZE(rowids);
    gdb_getRowsForGameID( params->pDb, gameID, rowids, &nRowIDs );
    XP_LOGFF( "found %d rows for gameID %d", nRowIDs, gameID );
    if ( 0 == nRowIDs ) {
        mqttc_notifyGameGone( params, &from->u.mqtt.devID, gameID );
    } else {
        for ( int ii = 0; ii < nRowIDs; ++ii ) {
            feedBufferGTK( apg, rowids[ii], buf, len, from );
        }
    }
}

void
gameGoneGTK( void* closure, const CommsAddrRec* XP_UNUSED(from), XP_U32 gameID )
{
    GtkAppGlobals* apg = (GtkAppGlobals*)closure;
    LaunchParams* params =  apg->cag.params;

    /* Do we have this game locally still? If not, ignore message */
    sqlite3_int64 rowids[4];
    int nRowIDs = VSIZE(rowids);
    gdb_getRowsForGameID( params->pDb, gameID, rowids, &nRowIDs );
    if ( 0 == nRowIDs ) {
        XP_LOGFF( "Old msg? Game %X no longer here", gameID );
    } else {
        gchar buf[128];
        snprintf( buf, VSIZE(buf), "Game %X has been deleted on a remote device. "
                  "Do you want to delete it?", gameID );
        if ( GTK_RESPONSE_YES == gtkask( apg->window, buf, GTK_BUTTONS_YES_NO, NULL ) ) {
            XP_ASSERT( 1 == nRowIDs );
            delete_game( apg, rowids[0] );
        } else {
            XP_LOGFF( "we need to call comms_setQuashed() here but don't have comms" );
        }
    }
}

#ifdef XWFEATURE_RELAY
static gboolean
keepalive_timer( gpointer data )
{
    LOG_FUNC();
    requestMsgs( data );
    return TRUE;
}

static void
gtkDevIDReceived( void* closure, const XP_UCHAR* devID, XP_U16 maxInterval )
{
    GtkAppGlobals* apg = (GtkAppGlobals*)closure;
    LaunchParams* params = apg->cag.params;
    if ( !!devID ) {
        XP_LOGF( "%s(devID=%s)", __func__, devID );
        gdb_store( params->pDb, KEY_RDEVID, devID );
        (void)g_timeout_add_seconds( maxInterval, keepalive_timer, apg );

        setWindowTitle( apg );
    } else {
        XP_LOGF( "%s: bad relayid", __func__ );
        gdb_remove( params->pDb, KEY_RDEVID );

        DevIDType typ;
        const XP_UCHAR* devID = linux_getDevID( params, &typ );
        relaycon_reg( params, NULL, typ, devID );
    }
}

static void
gtkErrorMsgRcvd( void* closure, const XP_UCHAR* msg )
{
    GtkAppGlobals* apg = (GtkAppGlobals*)closure;
    (void)gtkask( apg->window, msg, GTK_BUTTONS_OK, NULL );
}
#endif

void
gtkOnGameSaved( void* closure, sqlite3_int64 rowid, XP_Bool firstTime )
{
    GtkGameGlobals* globals = (GtkGameGlobals*)closure;
    GtkAppGlobals* apg = globals->apg;
    /* May not be recorded */
    if ( !!apg ) {
        onNewData( apg, rowid, firstTime );
    }
}

static GtkAppGlobals* g_globals_for_signal = NULL;

static void
handle_sigintterm( int XP_UNUSED(sig) )
{
    LOG_FUNC();
    handle_destroy( NULL, g_globals_for_signal );
}

static XP_Bool
newGameWrapper( void* closure, CurGameInfo* gi, XP_U32* newGameIDP )
{
    GtkAppGlobals* apg = (GtkAppGlobals*)closure;
    LaunchParams* params = apg->cag.params;
    GtkGameGlobals* globals = calloc( 1, sizeof(*globals) );
    initBoardGlobalsGtk( globals, params, NULL );

    gi_copy( MPPARM(params->mpool) globals->cGlobals.gi, gi );

    GtkWidget* gameWindow = globals->window;
    globals->cGlobals.rowid = -1;
    recordOpened( apg, globals );
    gtk_widget_show( gameWindow );
    *newGameIDP = globals->cGlobals.gi->gameID;
    return XP_TRUE;
}

static void
quitWrapper( void* closure )
{
    handle_quit_button( NULL, closure );
}

static XP_Bool
makeMoveIfWrapper( void* closure, XP_U32 gameID, XP_Bool tryTrade )
{
    GtkAppGlobals* apg = (GtkAppGlobals*)closure;
    sqlite3_int64 rowid = 0;
    GtkGameGlobals* globals = findOpenGame( apg, &rowid, &gameID );
    XP_Bool success = XP_FALSE;

    if ( !!globals ) {
        success = linux_makeMoveIf( &globals->cGlobals, tryTrade );
    } else {
        GtkGameGlobals tmpGlobals = {};
        int nRowIDs = 1;
        gdb_getRowsForGameID( apg->cag.params->pDb, gameID, &rowid, &nRowIDs );

        success = 1 == nRowIDs
            && loadGameNoDraw( &tmpGlobals, apg->cag.params, rowid )
            && linux_makeMoveIf( &tmpGlobals.cGlobals, tryTrade );
        freeGlobals( &tmpGlobals );
    }
    return success;
}

static XP_Bool
sendChatWrapper( void* closure, XP_U32 gameID, const char* msg )
{
    GtkAppGlobals* apg = (GtkAppGlobals*)closure;
    sqlite3_int64 rowid = 0;
    GtkGameGlobals* globals = findOpenGame( apg, &rowid, &gameID );
    XP_Bool success;

    if ( !!globals ) {
        board_sendChat( globals->cGlobals.game.board, NULL_XWE, msg );
        success = XP_TRUE;
    } else {
        GtkGameGlobals tmpGlobals = {};
        int nRowIDs = 1;
        gdb_getRowsForGameID( apg->cag.params->pDb, gameID, &rowid, &nRowIDs );

        success = 1 == nRowIDs
            && loadGameNoDraw( &tmpGlobals, apg->cag.params, rowid );
        if ( success ) {
            board_sendChat( tmpGlobals.cGlobals.game.board, NULL_XWE, msg );
        }
        freeGlobals( &tmpGlobals );
    }
    return success;
}

static void
addInvitesWrapper( void* closure, XP_U32 gameID, XP_U16 nRemotes,
                   const CommsAddrRec destAddrs[] )
{
    GtkAppGlobals* apg = (GtkAppGlobals*)closure;
    sqlite3_int64 rowid = 0;
    GtkGameGlobals* globals = findOpenGame( apg, &rowid, &gameID );
    if ( !!globals ) {
        linux_addInvites( &globals->cGlobals, nRemotes, destAddrs );
    } else {
        int nRowIDs = 1;
        gdb_getRowsForGameID( apg->cag.params->pDb, gameID, &rowid, &nRowIDs );

        GtkGameGlobals tmpGlobals = {};
        if ( 1 == nRowIDs
             && loadGameNoDraw( &tmpGlobals, apg->cag.params, rowid ) ) {
            linux_addInvites( &tmpGlobals.cGlobals, nRemotes, destAddrs );
        }
        freeGlobals( &tmpGlobals );
    }
}

static const CommonGlobals*
getForGameIDWrapper( void* closure, XP_U32 gameID )
{
    CommonGlobals* result = NULL;
    GtkAppGlobals* apg = (GtkAppGlobals*)closure;

    for ( ; ; ) {
        sqlite3_int64 rowid = 0;
        GtkGameGlobals* globals = findOpenGame( apg, &rowid, &gameID );
        if ( !!globals ) {
            result = &globals->cGlobals;
            break;
        }

        int nRowIDs = 1;
        gdb_getRowsForGameID( apg->cag.params->pDb, gameID, &rowid, &nRowIDs );
        XP_ASSERT( 1 == nRowIDs );
        open_row( apg, rowid, XP_FALSE );
        XP_LOGFF( "at bottom" );
    }
    return result;
}

int
gtkmain( LaunchParams* params )
{
    GtkAppGlobals apg = {};
    params->appGlobals = &apg;

    g_globals_for_signal = &apg;

    struct sigaction act = { .sa_handler = handle_sigintterm };
    sigaction( SIGINT, &act, NULL );
    sigaction( SIGTERM, &act, NULL );

    apg.selRows = g_array_new( FALSE, FALSE, sizeof( sqlite3_int64 ) );
    apg.cag.params = params;

    CmdWrapper wr = {
        .params = params,
        .closure = &apg,
        .procs = {
            .newGame = newGameWrapper,
            .quit = quitWrapper,
            .makeMoveIf = makeMoveIfWrapper,
            .sendChat = sendChatWrapper,
            .addInvites = addInvitesWrapper,
            .getForGameID = getForGameIDWrapper,
        },
    };
    GSocketService* cmdService = cmds_addCmdListener( &wr );

    XP_ASSERT( !!params->dbName || params->dbFileName );
    if ( !!params->dbName ) {
        /* params->pDb = openGamesDB( params->dbName ); */

#ifdef XWFEATURE_RELAY
        /* Check if we have a local ID already.  If we do and it's
           changed, we care. */
        XP_Bool idIsNew = linux_setupDevidParams( params );

        if ( params->useUdp ) {
            RelayConnProcs procs = {
                .msgReceived = gtkGotBuf,
                .msgForRow = gtkGotMsgForRow,
                .msgNoticeReceived = gtkNoticeRcvd,
                .devIDReceived = gtkDevIDReceived,
                .msgErrorMsg = gtkErrorMsgRcvd,
                .inviteReceived = inviteReceivedGTK,
            };

            relaycon_init( params, &procs, &apg, 
                           params->connInfo.relay.relayName,
                           params->connInfo.relay.defaultSendPort );

            linux_doInitialReg( params, idIsNew );
        }
#endif

        mqttc_init( params );

#ifdef XWFEATURE_SMS
        gchar* myPhone;
        XP_U16 myPort;
        if ( parseSMSParams( params, &myPhone, &myPort ) ) {
            SMSProcs smsProcs = {
                .inviteReceived = smsInviteReceived,
                .msgReceived = msgReceivedGTK,
            };
            linux_sms_init( params, myPhone, myPort, &smsProcs, &apg );
        } else {
            XP_LOGF( "not activating SMS: I don't have a phone" );
        }

        if ( params->runSMSTest ) {
            CommonGlobals cGlobals = {.params = params };
            setupUtil( &cGlobals );
            smsproto_runTests( params->mpool, NULL_XWE, cGlobals.params->dutil );
            linux_util_vt_destroy( cGlobals.util );
            free( cGlobals.util );
        }
#endif
        makeGamesWindow( &apg );
    } else if ( !!params->dbFileName ) {
        apg.window = openDBFile( &apg );
    }

    gtk_main();

    g_object_unref( cmdService );

#ifdef XWFEATURE_RELAY
    relaycon_cleanup( params );
#endif
#ifdef XWFEATURE_SMS
    linux_sms_cleanup( params );
#endif
    mqttc_cleanup( params );
    g_array_free( apg.selRows, true );

    return 0;
} /* gtkmain */

#endif /* PLATFORM_GTK */
