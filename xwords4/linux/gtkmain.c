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
#include "gtkmain.h"
#include "gamesdb.h"
#include "gtkboard.h"
#include "linuxmain.h"
#include "relaycon.h"
#include "gtkask.h"

static void onNewData( GtkAppGlobals* apg, sqlite3_int64 rowid, 
                       XP_Bool isNew );
static void updateButtons( GtkAppGlobals* apg );

static void
recordOpened( GtkAppGlobals* apg, GtkGameGlobals* globals )
{
    apg->globalsList = g_slist_prepend( apg->globalsList, globals );
    globals->apg = apg;
}

static void
recordClosed( GtkAppGlobals* apg, GtkGameGlobals* globals )
{
    apg->globalsList = g_slist_remove( apg->globalsList, globals );
}

static XP_Bool
gameIsOpen( GtkAppGlobals* apg, sqlite3_int64 rowid )
{
    XP_Bool found = XP_FALSE;
    GSList* iter;
    for ( iter = apg->globalsList; !!iter && !found; iter = iter->next ) {
        GtkGameGlobals* globals = (GtkGameGlobals*)iter->data;
        found = globals->cGlobals.selRow == rowid;
    }
    return found;
}

static GtkGameGlobals*
findOpenGame( const GtkAppGlobals* apg, sqlite3_int64 rowid )
{
    GtkGameGlobals* result = NULL;
    GSList* iter;
    for ( iter = apg->globalsList; !!iter; iter = iter->next ) {
        GtkGameGlobals* globals = (GtkGameGlobals*)iter->data;
        CommonGlobals* cGlobals = &globals->cGlobals;
        if ( cGlobals->selRow == rowid ) {
            result = globals;
            break;
        }
    }
    return result;
}

enum { ROW_ITEM, NAME_ITEM, ROOM_ITEM, SEED_ITEM, OVER_ITEM, TURN_ITEM, 
       NMOVES_ITEM, MISSING_ITEM, N_ITEMS };

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

static GtkWidget*
init_games_list( GtkAppGlobals* apg )
{
    GtkWidget* list = gtk_tree_view_new();

    addTextColumn( list, "Row", ROW_ITEM );
    addTextColumn( list, "Name", NAME_ITEM );
    addTextColumn( list, "Room", ROOM_ITEM );
    addTextColumn( list, "Seed", SEED_ITEM );
    addTextColumn( list, "Ended", OVER_ITEM );
    addTextColumn( list, "Turn", TURN_ITEM );
    addTextColumn( list, "NMoves", NMOVES_ITEM );
    addTextColumn( list, "NMissing", MISSING_ITEM );

    GtkListStore* store = gtk_list_store_new( N_ITEMS, 
                                              G_TYPE_INT64,   /* ROW_ITEM */
                                              G_TYPE_STRING,  /* NAME_ITEM */
                                              G_TYPE_STRING,  /* ROOM_ITEM */
                                              G_TYPE_INT,     /* SEED_ITEM */
                                              G_TYPE_BOOLEAN, /* OVER_ITEM */
                                              G_TYPE_INT,     /* TURN_ITEM */
                                              G_TYPE_INT,     /* NMOVES_ITEM */
                                              G_TYPE_INT      /* MISSING_ITEM */
                                              );
    gtk_tree_view_set_model( GTK_TREE_VIEW(list), GTK_TREE_MODEL(store) );
    g_object_unref( store );

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
    if ( isNew ) {
        gtk_list_store_append( store, &iter );
    } else {
        gboolean valid;
        for ( valid = gtk_tree_model_get_iter_first( model, &iter );
              valid;
              valid = gtk_tree_model_iter_next( model, &iter ) ) {
            sqlite3_int64 tmpid;
            gtk_tree_model_get( model, &iter, ROW_ITEM, &tmpid, -1 );
            if ( tmpid == rowid ) {
                break;
            }
        }
    }
    gtk_list_store_set( store, &iter, 
                        ROW_ITEM, rowid,
                        NAME_ITEM, gib->name,
                        ROOM_ITEM, gib->room,
                        SEED_ITEM, gib->seed,
                        OVER_ITEM, gib->gameOver,
                        TURN_ITEM, gib->turn,
                        NMOVES_ITEM, gib->nMoves,
                        MISSING_ITEM, gib->nMissing,
                        -1 );
    XP_LOGF( "DONE adding" );
}

static void updateButtons( GtkAppGlobals* apg )
{
    guint count = apg->selRows->len;

    gtk_widget_set_sensitive( apg->openButton, 1 == count );
    gtk_widget_set_sensitive( apg->deleteButton, 1 <= count );
}

static void
handle_newgame_button( GtkWidget* XP_UNUSED(widget), void* closure )
{
    GtkAppGlobals* apg = (GtkAppGlobals*)closure;
    XP_LOGF( "%s called", __func__ );
    GtkGameGlobals* globals = malloc( sizeof(*globals) );
    apg->params->needsNewGame = XP_FALSE;
    initGlobals( globals, apg->params );
    if ( !makeNewGame( globals ) ) {
        freeGlobals( globals );
    } else {
        GtkWidget* gameWindow = globals->window;
        globals->cGlobals.pDb = apg->params->pDb;
        globals->cGlobals.selRow = -1;
        recordOpened( apg, globals );
        gtk_widget_show( gameWindow );
    }
}

static void
handle_open_button( GtkWidget* XP_UNUSED(widget), void* closure )
{
    GtkAppGlobals* apg = (GtkAppGlobals*)closure;
    sqlite3_int64 selRow = getSelRow( apg );
    if ( -1 != selRow && !gameIsOpen( apg, selRow ) ) {
        apg->params->needsNewGame = XP_FALSE;
        GtkGameGlobals* globals = malloc( sizeof(*globals) );
        initGlobals( globals, apg->params );
        globals->cGlobals.pDb = apg->params->pDb;
        globals->cGlobals.selRow = selRow;
        recordOpened( apg, globals );
        gtk_widget_show( globals->window );
    }
}

static void
handle_delete_button( GtkWidget* XP_UNUSED(widget), void* closure )
{
    GtkAppGlobals* apg = (GtkAppGlobals*)closure;
    LaunchParams* params = apg->params;
    guint len = apg->selRows->len;
    for ( guint ii = 0; ii < len; ++ii ) {
        sqlite3_int64 rowid = g_array_index( apg->selRows, sqlite3_int64, ii );

        GameInfo gib;
#ifdef DEBUG
        XP_Bool success = 
#endif
            getGameInfo( params->pDb, rowid, &gib );
        XP_ASSERT( success );
        XP_U32 clientToken = makeClientToken( rowid, gib.seed );
        removeRow( apg, rowid );
        deleteGame( params->pDb, rowid );

        XP_UCHAR devIDBuf[64] = {0};
        db_fetch( params->pDb, KEY_RDEVID, devIDBuf, sizeof(devIDBuf) );
        if ( '\0' != devIDBuf[0] ) {
            relaycon_deleted( params, devIDBuf, clientToken );
        } else {
            XP_LOGF( "%s: not calling relaycon_deleted: no relayID", __func__ );
        }
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
    for ( iter = apg->globalsList; !!iter; iter = iter->next ) {
        GtkGameGlobals* globals = (GtkGameGlobals*)iter->data;
        destroy_board_window( NULL, globals );
        // freeGlobals( globals );
    }
    g_slist_free( apg->globalsList );
    gtk_main_quit();
}

static void
handle_quit_button( GtkWidget* XP_UNUSED(widget), gpointer data )
{
    GtkAppGlobals* apg = (GtkAppGlobals*)data;
    handle_destroy( NULL, apg );
}

static GtkWidget*
addButton( gchar* label, GtkWidget* parent, GCallback proc, void* closure )
{
    GtkWidget* button = gtk_button_new_with_label( label );
    gtk_container_add( GTK_CONTAINER(parent), button );
    g_signal_connect( GTK_OBJECT(button), "clicked",
                      G_CALLBACK(proc), closure );
    gtk_widget_show( button );
    return button;
}

static GtkWidget* 
makeGamesWindow( GtkAppGlobals* apg )
{
    GtkWidget* window;

    window = gtk_window_new( GTK_WINDOW_TOPLEVEL );
    g_signal_connect( G_OBJECT(window), "destroy",
                      G_CALLBACK(handle_destroy), apg );

    if ( !!apg->params->dbName ) {
        gtk_window_set_title( GTK_WINDOW(window), apg->params->dbName );
    }
    
    GtkWidget* vbox = gtk_vbox_new( FALSE, 0 );
    gtk_container_add( GTK_CONTAINER(window), vbox );
    gtk_widget_show( vbox );
    GtkWidget* list = init_games_list( apg );
    apg->listWidget = list;
    gtk_container_add( GTK_CONTAINER(vbox), list );
    
    gtk_widget_show( list );

    GSList* games = listGames( apg->params->pDb );
    for ( GSList* iter = games; !!iter; iter = iter->next ) {
        sqlite3_int64* rowid = (sqlite3_int64*)iter->data;
        onNewData( apg, *rowid, XP_TRUE );
    }
    g_slist_free( games );

    GtkWidget* hbox = gtk_hbox_new( FALSE, 0 );
    gtk_widget_show( hbox );
    gtk_container_add( GTK_CONTAINER(vbox), hbox );

    (void)addButton( "New game", hbox, G_CALLBACK(handle_newgame_button), apg );
    apg->openButton = addButton( "Open", hbox, 
                                 G_CALLBACK(handle_open_button), apg );
    apg->deleteButton = addButton( "Delete", hbox, 
                                   G_CALLBACK(handle_delete_button), apg );
    (void)addButton( "Quit", hbox, G_CALLBACK(handle_quit_button), apg );
    updateButtons( apg );

    gtk_widget_show( window );
    return window;
}

static gint
freeGameGlobals( gpointer data )
{
    LOG_FUNC();
    GtkGameGlobals* globals = (GtkGameGlobals*)data;
    GtkAppGlobals* apg = globals->apg;
    recordClosed( apg, globals );
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
    if ( getGameInfo( apg->params->pDb, rowid, &gib ) ) {
        add_to_list( apg->listWidget, rowid, isNew, &gib );
    }
}

static gboolean
gtk_app_socket_proc( GIOChannel* source, GIOCondition condition, gpointer data )
{
    if ( 0 != (G_IO_IN & condition) ) {
        GtkAppGlobals* apg = (GtkAppGlobals*)data;
        int socket = g_io_channel_unix_get_fd( source );
        GList* iter;
        for ( iter = apg->sources; !!iter; iter = iter->next ) {
            SourceData* sd = (SourceData*)iter->data;
            if ( sd->channel == source ) {
                (*sd->proc)( sd->procClosure, socket );
                break;
            }
        }
        XP_ASSERT( !!iter );    /* didn't fail to find it */
    }
    return TRUE;
}

static void
gtkSocketChanged( void* closure, int newSock, int XP_UNUSED(oldSock), 
                  SockReceiver proc, void* procClosure )
{
#if 1
    GtkAppGlobals* apg = (GtkAppGlobals*)closure;
    SourceData* sd = g_malloc( sizeof(*sd) );
    sd->channel = g_io_channel_unix_new( newSock );
    sd->watch = g_io_add_watch( sd->channel, G_IO_IN | G_IO_ERR,
                                gtk_app_socket_proc, apg );
    sd->proc = proc;
    sd->procClosure = procClosure;
    apg->sources = g_list_append( apg->sources, sd );
#else
    GtkAppGlobals* globals = (GtkAppGlobals*)closure;
    SockInfo* info = (SockInfo*)*storage;
    XP_LOGF( "%s(old:%d; new:%d)", __func__, oldSock, newSock );

    if ( oldSock != -1 ) {
        XP_ASSERT( info != NULL );
        g_source_remove( info->watch );
        g_io_channel_unref( info->channel );
        XP_FREE( globals->cGlobals.params->util->mpool, info );
        *storage = NULL;
        XP_LOGF( "Removed socket %d from gtk's list of listened-to sockets", 
                 oldSock );
    }
    if ( newSock != -1 ) {
        info = (SockInfo*)XP_MALLOC( globals->cGlobals.params->util->mpool,
                                     sizeof(*info) );
        GIOChannel* channel = g_io_channel_unix_new( newSock );
        g_io_channel_set_close_on_unref( channel, TRUE );
        guint result = g_io_add_watch( channel,
                                       G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_PRI,
                                       newConnectionInput,
                                       globals );
        info->channel = channel;
        info->watch = result;
        if ( !!*storage ) {
            XP_FREE( globals->cGlobals.params->util->mpool, *storage );
        }
        *storage = info;
        XP_LOGF( "g_io_add_watch(%d) => %d", newSock, result );
    }
#ifdef XWFEATURE_RELAY
    globals->cGlobals.socket = newSock;
#endif
    /* A hack for the bluetooth case. */
    CommsCtxt* comms = globals->cGlobals.game.comms;
    if ( (comms != NULL) && (comms_getConType(comms) == COMMS_CONN_BT) ) {
        comms_resendAll( comms, XP_FALSE );
    }
#endif
    LOG_RETURN_VOID();
} /* gtk_socket_changed */

static void
gtkGotBuf( void* closure, const XP_U8* buf, XP_U16 len )
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

    XP_U16 seed = 0;
    GtkGameGlobals* globals = findOpenGame( apg, rowid );
    if ( !!globals ) {
        gameGotBuf( &globals->cGlobals, XP_TRUE, buf, len );
        seed = comms_getChannelSeed( globals->cGlobals.game.comms );
    } else {
        GtkGameGlobals tmpGlobals;
        if ( loadGameNoDraw( &tmpGlobals, apg->params, rowid ) ) {
            gameGotBuf( &tmpGlobals.cGlobals, XP_FALSE, buf, len );
            seed = comms_getChannelSeed( tmpGlobals.cGlobals.game.comms );
            saveGame( &tmpGlobals.cGlobals );
        }
        freeGlobals( &tmpGlobals );
    }
    XP_ASSERT( seed == 0 || gotSeed == seed );
    XP_USE( seed );
}

static gint
requestMsgs( gpointer data )
{
    GtkAppGlobals* apg = (GtkAppGlobals*)data;
    XP_UCHAR devIDBuf[64] = {0};
    db_fetch( apg->params->pDb, KEY_RDEVID, devIDBuf, sizeof(devIDBuf) );
    if ( '\0' != devIDBuf[0] ) {
        relaycon_requestMsgs( apg->params, devIDBuf );
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
gtkDevIDChanged( void* closure, const XP_UCHAR* devID )
{
    GtkAppGlobals* apg = (GtkAppGlobals*)closure;
    LaunchParams* params = apg->params;
    if ( !!devID ) {
        XP_LOGF( "%s(devID=%s)", __func__, devID );
        db_store( params->pDb, KEY_RDEVID, devID );
    } else {
        XP_LOGF( "%s: bad relayid", __func__ );
        db_remove( params->pDb, KEY_RDEVID );

        DevIDType typ;
        const XP_UCHAR* devID = linux_getDevID( params, &typ );
        relaycon_reg( params, devID, typ );
    }
}

static void
gtkErrorMsgRcvd( void* closure, const XP_UCHAR* msg )
{
    GtkAppGlobals* apg = (GtkAppGlobals*)closure;
    (void)gtkask( apg->window, msg, GTK_BUTTONS_OK );
}

void
onGameSaved( void* closure, sqlite3_int64 rowid, 
             XP_Bool firstTime )
{
    GtkGameGlobals* globals = (GtkGameGlobals*)closure;
    GtkAppGlobals* apg = globals->apg;
    /* May not be recorded */
    if ( !!apg ) {
        onNewData( apg, rowid, firstTime );
    }
}

sqlite3_int64
getSelRow( const GtkAppGlobals* apg )
{
    sqlite3_int64 result = -1;
    guint len = apg->selRows->len;
    if ( 1 == len ) {
        result = g_array_index( apg->selRows, sqlite3_int64, 0 );
    }
    return result;
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

    g_globals_for_signal = &apg;

    struct sigaction act = { .sa_handler = handle_sigintterm };
    sigaction( SIGINT, &act, NULL );
    sigaction( SIGTERM, &act, NULL );

    apg.selRows = g_array_new( FALSE, FALSE, sizeof( sqlite3_int64 ) );
    apg.params = params;
    params->pDb = openGamesDB( params->dbName );

    RelayConnProcs procs = {
        .msgReceived = gtkGotBuf,
        .msgNoticeReceived = gtkNoticeRcvd,
        .devIDChanged = gtkDevIDChanged,
        .msgErrorMsg = gtkErrorMsgRcvd,
        .socketChanged = gtkSocketChanged,
    };

    relaycon_init( params, &procs, &apg, 
                   params->connInfo.relay.relayName,
                   params->connInfo.relay.defaultSendPort );

    DevIDType typ;
    const XP_UCHAR* devID = linux_getDevID( params, &typ );
    relaycon_reg( params, devID, typ );

    apg.window = makeGamesWindow( &apg );
    gtk_main();

    closeGamesDB( params->pDb );
    relaycon_cleanup( params );

    return 0;
} /* gtkmain */

#endif /* PLATFORM_GTK */
