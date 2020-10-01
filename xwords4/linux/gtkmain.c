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
findOpenGame( const GtkAppGlobals* apg, sqlite3_int64 rowid )
{
    GtkGameGlobals* result = NULL;
    GSList* iter;
    for ( iter = apg->cag.globalsList; !!iter; iter = iter->next ) {
        GtkGameGlobals* globals = (GtkGameGlobals*)iter->data;
        CommonGlobals* cGlobals = &globals->cGlobals;
        if ( cGlobals->rowid == rowid ) {
            result = globals;
            break;
        }
    }
    return result;
}

enum { ROW_ITEM, ROW_THUMB, NAME_ITEM, CREATED_ITEM, ROOM_ITEM, GAMEID_ITEM,
       SEED_ITEM, ROLE_ITEM, CONN_ITEM, RELAYID_ITEM, OVER_ITEM, TURN_ITEM,
       LOCAL_ITEM, NMOVES_ITEM, NTOTAL_ITEM, MISSING_ITEM, LASTTURN_ITEM,
       DUPTIMER_ITEM,

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
    addTextColumn( list, "Room", ROOM_ITEM );
    addTextColumn( list, "GameID", GAMEID_ITEM );
    addTextColumn( list, "Seed", SEED_ITEM );
    addTextColumn( list, "Role", ROLE_ITEM );
    addTextColumn( list, "Conn. via", CONN_ITEM );
    addTextColumn( list, "RelayID", RELAYID_ITEM );
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
                                              G_TYPE_STRING,  /* ROOM_ITEM */
                                              G_TYPE_INT,     /* GAMEID_ITEM */
                                              G_TYPE_INT,     /* SEED_ITEM */
                                              G_TYPE_INT,     /* ROLE_ITEM */
                                              G_TYPE_STRING,  /* CONN_ITEM */
                                              G_TYPE_STRING,  /*RELAYID_ITEM */
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

    gchar createdStr[64] = {0};
    if ( 0 != gib->created ) {
        formatSeconds( gib->created, createdStr, VSIZE(createdStr) );
    }
    gchar timeStr[64];
    formatSeconds( gib->lastMoveTime, timeStr, VSIZE(timeStr) );
    gchar timerStr[64] = {0};
    if ( gib->dupTimerExpires ) {
        formatSeconds( gib->dupTimerExpires, timerStr, VSIZE(timeStr) );
    }

    gtk_list_store_set( store, &iter, 
                        ROW_ITEM, rowid,
                        ROW_THUMB, gib->snap,
                        NAME_ITEM, gib->name,
                        CREATED_ITEM, createdStr,
                        ROOM_ITEM, gib->room,
                        GAMEID_ITEM, gib->gameID,
                        SEED_ITEM, gib->seed,
                        ROLE_ITEM, gib->role,
                        CONN_ITEM, gib->conn,
                        RELAYID_ITEM, gib->relayID,
                        TURN_ITEM, gib->turn,
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
    XP_LOGF( "%s called", __func__ );
    GtkGameGlobals* globals = calloc( 1, sizeof(*globals) );
    apg->cag.params->needsNewGame = XP_FALSE;
    initBoardGlobalsGtk( globals, apg->cag.params, NULL );
    if ( !makeNewGame( globals ) ) {
        freeGlobals( globals );
    } else {
        GtkWidget* gameWindow = globals->window;
        globals->cGlobals.rowid = -1;
        recordOpened( apg, globals );
        gtk_widget_show( gameWindow );
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
    LaunchParams* params = apg->cag.params;
    XP_ASSERT( params == cGlobals->params );
    XWStreamCtxt* stream = mem_stream_make_raw( MPPARM(cGlobals->util->mpool)
                                                params->vtMgr );

    /* Create new game. But has no addressing info, so need to set that
       aside for later. */
    const CommsCtxt* comms = cGlobals->game.comms;
    CurGameInfo gi = {0};
    gi_copy( MPPARM(cGlobals->util->mpool) &gi, cGlobals->gi );
    gi.gameID = 0;          /* clear so will get generated */
    if ( !!comms ) {
        gi.serverRole = SERVER_ISSERVER;
        gi.forceChannel = 0;
    }
    game_saveNewGame( MPPARM(cGlobals->util->mpool) NULL_XWE, &gi,
                      cGlobals->util, &cGlobals->cp, stream );

    sqlite3_int64 rowID = gdb_writeNewGame( stream, params->pDb );
    stream_destroy( stream, NULL_XWE );
    gi_disposePlayerInfo( MPPARM(cGlobals->util->mpool) &gi );

    /* If it's a multi-device game, save enough information with it than when
       opened it can invite the other device[s] join the rematch. */
    if ( !!comms ) {
        XWStreamCtxt* stream = mem_stream_make_raw( MPPARM(cGlobals->util->mpool)
                                                    params->vtMgr );
        CommsAddrRec addr;
        comms_getAddr( comms, &addr );
        addrToStream( stream, &addr );

        CommsAddrRec addrs[4];
        XP_U16 nRecs = VSIZE(addrs);
        comms_getAddrs( comms, NULL_XWE, addrs, &nRecs );

        stream_putU8( stream, nRecs );
        for ( int ii = 0; ii < nRecs; ++ii ) {
            XP_UCHAR relayID[32];
            XP_U16 len = sizeof(relayID);
            comms_formatRelayID( comms, ii, relayID, &len );
            XP_LOGF( "%s: adding relayID: %s", __func__, relayID );
            stringToStream( stream, relayID );
            if ( addr_hasType( &addrs[ii], COMMS_CONN_RELAY ) ) {
                /* copy over room name */
                XP_STRCAT( addrs[ii].u.ip_relay.invite, addr.u.ip_relay.invite );
            }
            addrToStream( stream, &addrs[ii] );
        }
        gdb_saveInviteAddrs( stream, params->pDb, rowID );
        stream_destroy( stream, NULL_XWE );
    }

    open_row( apg, rowID, XP_TRUE );
}

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
handle_delete_button( GtkWidget* XP_UNUSED(widget), void* closure )
{
    GtkAppGlobals* apg = (GtkAppGlobals*)closure;
    LaunchParams* params = apg->cag.params;
    guint len = apg->selRows->len;
    for ( guint ii = 0; ii < len; ++ii ) {
        sqlite3_int64 rowid = g_array_index( apg->selRows, sqlite3_int64, ii );

        GameInfo gib;
#ifdef DEBUG
        XP_Bool success = 
#endif
            gdb_getGameInfo( params->pDb, rowid, &gib );
        XP_ASSERT( success );
        XP_U32 clientToken = makeClientToken( rowid, gib.seed );
        removeRow( apg, rowid );
        gdb_deleteGame( params->pDb, rowid );

        XP_UCHAR devIDBuf[64] = {0};
        gdb_fetch_safe( params->pDb, KEY_RDEVID, NULL, devIDBuf, sizeof(devIDBuf) );
        if ( '\0' != devIDBuf[0] ) {
            relaycon_deleted( params, devIDBuf, clientToken );
        } else {
            XP_LOGF( "%s: not calling relaycon_deleted: no relayID", __func__ );
        }
        g_object_unref( gib.snap );
    }
    apg->selRows = g_array_set_size( apg->selRows, 0 );
    updateButtons( apg );
    /* Need now to update the selection and sync the buttons */
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

    gchar buf[64];
    sprintf( buf, "%d:%d:%d:%d", apg->lastConfigure.x,
             apg->lastConfigure.y, apg->lastConfigure.width,
             apg->lastConfigure.height );
    gdb_store( apg->cag.params->pDb, KEY_WIN_LOC, buf );

    gtk_main_quit();
}

static void
handle_quit_button( GtkWidget* XP_UNUSED(widget), gpointer data )
{
    GtkAppGlobals* apg = (GtkAppGlobals*)data;
    handle_destroy( NULL, apg );
}

static gboolean
window_configured( GtkWidget* XP_UNUSED(widget),
                   GdkEventConfigure* event, GtkAppGlobals* apg )
{
    /* XP_LOGF( "%s(x=%d, y=%d, width=%d, height=%d)", __func__, */
    /*          event->x, event->y, event->width, event->height ); */
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

    gchar title[128] = {0};
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

static void
trySetWinConfig( GtkAppGlobals* apg )
{
    int xx = 20;                /* defaults */
    int yy = 20;
    int width = 600;
    int height = 400;

    gchar buf[64];
    if ( gdb_fetch_safe( apg->cag.params->pDb, KEY_WIN_LOC, NULL, buf, sizeof(buf)) ) {
        sscanf( buf, "%d:%d:%d:%d", &xx, &yy, &width, &height );
    }

    gtk_window_resize( GTK_WINDOW(apg->window), width, height );
    gtk_window_move (GTK_WINDOW(apg->window), xx, yy );
}

static void
handle_movescheck( GtkWidget* XP_UNUSED(widget), GtkAppGlobals* apg )
{
    LaunchParams* params = apg->cag.params;
    relaycon_checkMsgs( params );
}

static void
handle_known_players( GtkWidget* XP_UNUSED(widget), GtkAppGlobals* apg )
{
    gtkkp_show( apg );
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
                      G_CALLBACK(window_configured), apg );

    setWindowTitle( apg );

    trySetWinConfig( apg );

    GtkWidget* swin = gtk_scrolled_window_new( NULL, NULL );
    gtk_container_add( GTK_CONTAINER(window), swin );
    gtk_widget_show( swin );
    
    GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add( GTK_CONTAINER(swin), vbox );
    gtk_widget_show( vbox );

    // add menubar here
    GtkWidget* menubar = gtk_menu_bar_new();
    GtkWidget* netMenu = makeAddSubmenu( menubar, "Menu" );
    (void)createAddItem( netMenu, "Known Players",
                         (GCallback)handle_known_players, apg );
    if ( params->useHTTP ) {
        (void)createAddItem( netMenu, "Check for moves",
                             (GCallback)handle_movescheck, apg );
    }
    (void)createAddItem( netMenu, "copy relayid",
                         (GCallback)handle_relayid_to_clip, apg );
    (void)createAddItem( netMenu, "copy mqtt devid",
                         (GCallback)handle_mqttid_to_clip, apg );
    gtk_widget_show( menubar );
    gtk_box_pack_start( GTK_BOX(vbox), menubar, FALSE, TRUE, 0 );

    GtkWidget* list = init_games_list( apg );
    gtk_container_add( GTK_CONTAINER(vbox), list );
    
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
    GtkGameGlobals* globals = malloc( sizeof(*globals) );
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
    if ( gdb_getGameInfo( apg->cag.params->pDb, rowid, &gib ) ) {
        add_to_list( apg->listWidget, rowid, isNew, &gib );
        g_object_unref( gib.snap );
    }
}

static XP_U16
feedBufferGTK( GtkAppGlobals* apg, sqlite3_int64 rowid, 
               const XP_U8* buf, XP_U16 len, const CommsAddrRec* from )
{
    XP_U16 seed = 0;
    GtkGameGlobals* globals = findOpenGame( apg, rowid );

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
gameFromInvite( GtkAppGlobals* apg, const NetLaunchInfo* invite,
                const CommsAddrRec* returnAddr )
{
    LaunchParams* params = apg->cag.params;
    CurGameInfo gi = {0};
    gi_copy( MPPARM(params->mpool) &gi, &params->pgi );

    gi_setNPlayers( &gi, invite->nPlayersT, invite->nPlayersH );
    ensureLocalPlayerNames( params, &gi );

    gi.gameID = invite->gameID;
    gi.dictLang = invite->lang;
    gi.forceChannel = invite->forceChannel;
    gi.inDuplicateMode = invite->inDuplicateMode;
    gi.serverRole = SERVER_ISCLIENT; /* recipient of invitation is client */
    replaceStringIfDifferent( params->mpool, &gi.dictName, invite->dict );

    GtkGameGlobals* globals = malloc( sizeof(*globals) );
    params->needsNewGame = XP_FALSE;
    initBoardGlobalsGtk( globals, params, &gi );

    if ( !!returnAddr ) {
        globals->cGlobals.addr = *returnAddr;
    } else {
        nli_makeAddrRec( invite, &globals->cGlobals.addr );
    }

    GtkWidget* gameWindow = globals->window;
    globals->cGlobals.rowid = -1;
    recordOpened( apg, globals );
    gtk_widget_show( gameWindow );

    gi_disposePlayerInfo( MPPARM(params->mpool) &gi );
}

void
relayInviteReceivedGTK( void* closure, const NetLaunchInfo* invite )
{
    GtkAppGlobals* apg = (GtkAppGlobals*)closure;

    XP_U32 gameID = invite->gameID;
    sqlite3_int64 rowids[1];
    int nRowIDs = VSIZE(rowids);
    gdb_getRowsForGameID( apg->cag.params->pDb, gameID, rowids, &nRowIDs );

    bool doIt = 0 == nRowIDs;
    if ( ! doIt ) {
        doIt = GTK_RESPONSE_YES == gtkask( apg->window,
                                           "Duplicate invitation received. Accept anyway?",
                                           GTK_BUTTONS_YES_NO, NULL );
    }
    if ( doIt ) {
        gameFromInvite( apg, invite, NULL );
    }
}

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
    XP_UCHAR devIDBuf[64] = {0};
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

static void
smsInviteReceived( void* closure, const NetLaunchInfo* nli,
                   const CommsAddrRec* returnAddr )
{
    GtkAppGlobals* apg = (GtkAppGlobals*)closure;
    XP_LOGF( "%s(gameName=%s, gameID=%d, dictName=%s, nPlayers=%d, "
             "nHere=%d, forceChannel=%d)", __func__, nli->gameName,
             nli->gameID, nli->dict, nli->nPlayersT,
             nli->nPlayersH, nli->forceChannel );

    gameFromInvite( apg, nli, returnAddr );
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
    XP_LOGF( "%s: found %d rows for gameID %d", __func__, nRowIDs, gameID );
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
    gchar buf[64];
    snprintf( buf, VSIZE(buf), "game %d has been deleted on a remote device", gameID );
    gtktell( apg->window, buf );
}

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

int
gtkmain( LaunchParams* params )
{
    GtkAppGlobals apg = {0};
    params->appGlobals = &apg;

    g_globals_for_signal = &apg;

    struct sigaction act = { .sa_handler = handle_sigintterm };
    sigaction( SIGINT, &act, NULL );
    sigaction( SIGTERM, &act, NULL );

    apg.selRows = g_array_new( FALSE, FALSE, sizeof( sqlite3_int64 ) );
    apg.cag.params = params;
    XP_ASSERT( !!params->dbName || params->dbFileName );
    if ( !!params->dbName ) {
        /* params->pDb = openGamesDB( params->dbName ); */

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
                .inviteReceived = relayInviteReceivedGTK,
            };

            relaycon_init( params, &procs, &apg, 
                           params->connInfo.relay.relayName,
                           params->connInfo.relay.defaultSendPort );

            linux_doInitialReg( params, idIsNew );

            mqttc_init( params );
        }

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
    dvc_store( params->dutil, NULL_XWE );
    /* closeGamesDB( params->pDb ); */
    /* params->pDb = NULL; */
    relaycon_cleanup( params );
#ifdef XWFEATURE_SMS
    linux_sms_cleanup( params );
#endif
    mqttc_cleanup( params );
    return 0;
} /* gtkmain */

#endif /* PLATFORM_GTK */
