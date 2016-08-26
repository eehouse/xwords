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

#include "strutils.h"
#include "main.h"
#include "gtkmain.h"
#include "gamesdb.h"
#include "gtkboard.h"
#include "linuxmain.h"
#include "relaycon.h"
#include "linuxsms.h"
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

enum { ROW_ITEM, ROW_THUMB, NAME_ITEM, ROOM_ITEM, GAMEID_ITEM, SEED_ITEM,
       CONN_ITEM, OVER_ITEM, TURN_ITEM, NMOVES_ITEM, NTOTAL_ITEM, MISSING_ITEM,
       LASTTURN_ITEM, N_ITEMS };

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
    addTextColumn( list, "Room", ROOM_ITEM );
    addTextColumn( list, "GameID", GAMEID_ITEM );
    addTextColumn( list, "Seed", SEED_ITEM );
    addTextColumn( list, "Conn. via", CONN_ITEM );
    addTextColumn( list, "Ended", OVER_ITEM );
    addTextColumn( list, "Turn", TURN_ITEM );
    addTextColumn( list, "NMoves", NMOVES_ITEM );
    addTextColumn( list, "NTotal", NTOTAL_ITEM );
    addTextColumn( list, "NMissing", MISSING_ITEM );
    addTextColumn( list, "LastTurn", LASTTURN_ITEM );

    GtkListStore* store = gtk_list_store_new( N_ITEMS, 
                                              G_TYPE_INT64,   /* ROW_ITEM */
                                              GDK_TYPE_PIXBUF,/* ROW_THUMB */
                                              G_TYPE_STRING,  /* NAME_ITEM */
                                              G_TYPE_STRING,  /* ROOM_ITEM */
                                              G_TYPE_INT,     /* GAMEID_ITEM */
                                              G_TYPE_INT,     /* SEED_ITEM */
                                              G_TYPE_STRING,  /* CONN_ITEM */
                                              G_TYPE_BOOLEAN, /* OVER_ITEM */
                                              G_TYPE_INT,     /* TURN_ITEM */
                                              G_TYPE_INT,     /* NMOVES_ITEM */
                                              G_TYPE_INT,     /* NTOTAL_ITEM */
                                              G_TYPE_INT,     /* MISSING_ITEM */
                                              G_TYPE_INT      /* LASTTURN_ITEM */
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
                        ROW_THUMB, gib->snap,
                        NAME_ITEM, gib->name,
                        ROOM_ITEM, gib->room,
                        GAMEID_ITEM, gib->gameID,
                        SEED_ITEM, gib->seed,
                        CONN_ITEM, gib->conn,
                        OVER_ITEM, gib->gameOver,
                        TURN_ITEM, gib->turn,
                        NMOVES_ITEM, gib->nMoves,
                        NTOTAL_ITEM, gib->nTotal,
                        MISSING_ITEM, gib->nMissing,
                        LASTTURN_ITEM, gib->lastMoveTime,
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
    GtkGameGlobals* globals = malloc( sizeof(*globals) );
    apg->params->needsNewGame = XP_FALSE;
    initGlobals( globals, apg->params, NULL );
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

void
open_row( GtkAppGlobals* apg, sqlite3_int64 row, XP_Bool isNew )
{
    if ( -1 != row && !gameIsOpen( apg, row ) ) {
        if ( isNew ) {
            onNewData( apg, row, XP_TRUE );
        }

        apg->params->needsNewGame = XP_FALSE;
        GtkGameGlobals* globals = malloc( sizeof(*globals) );
        initGlobals( globals, apg->params, NULL );
        globals->cGlobals.pDb = apg->params->pDb;
        globals->cGlobals.selRow = row;
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
    // LaunchParams* params = apg->params;
    XWStreamCtxt* stream = mem_stream_make( MPPARM(cGlobals->util->mpool)
                                            cGlobals->params->vtMgr,
                                            NULL, CHANNEL_NONE, NULL );

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
    game_saveNewGame( MPPARM(cGlobals->util->mpool) &gi, 
                      cGlobals->util, &cGlobals->cp, stream );

    sqlite3_int64 rowID = writeNewGameToDB( stream, cGlobals->pDb );
    stream_destroy( stream );
    gi_disposePlayerInfo( MPPARM(cGlobals->util->mpool) &gi );

    /* If it's a multi-device game, save enough information with it than when
       opened it can invite the other device[s] join the rematch. */
    if ( !!comms ) {
        XWStreamCtxt* stream = mem_stream_make( MPPARM(cGlobals->util->mpool)
                                                cGlobals->params->vtMgr,
                                                NULL, CHANNEL_NONE, NULL );
        CommsAddrRec addr;
        comms_getAddr( comms, &addr );
        addrToStream( stream, &addr );

        CommsAddrRec addrs[4];
        XP_U16 nRecs = VSIZE(addrs);
        comms_getAddrs( comms, addrs, &nRecs );

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
        saveInviteAddrs( stream, cGlobals->pDb, rowID );
        stream_destroy( stream );
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
        if ( loadGameNoDraw( &tmpGlobals, apg->params, rowid ) ) {
            make_rematch( apg, &tmpGlobals.cGlobals );
        }
        freeGlobals( &tmpGlobals );
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
    for ( iter = apg->globalsList; !!iter; iter = iter->next ) {
        GtkGameGlobals* globals = (GtkGameGlobals*)iter->data;
        destroy_board_window( NULL, globals );
        // freeGlobals( globals );
    }
    g_slist_free( apg->globalsList );

    gchar buf[64];
    sprintf( buf, "%d:%d:%d:%d", apg->lastConfigure.x,
             apg->lastConfigure.y, apg->lastConfigure.width,
             apg->lastConfigure.height );
    db_store( apg->params->pDb, KEY_WIN_LOC, buf );

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
    LaunchParams* params = apg->params;

    gchar title[128] = {0};
    if ( !!params->dbName ) {
        strcat( title, params->dbName );
    }
#ifdef XWFEATURE_SMS
    int len = strlen( title );
    snprintf( &title[len], VSIZE(title) - len, " (phone: %s, port: %d)", 
              params->connInfo.sms.phone, params->connInfo.sms.port );
#endif
#ifdef XWFEATURE_RELAY
    XP_U32 relayID = linux_getDevIDRelay( params );
    len = strlen( title );
    snprintf( &title[len], VSIZE(title) - len, " (relayid: %d)", relayID );
#endif
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
    if ( db_fetch( apg->params->pDb, KEY_WIN_LOC, buf, sizeof(buf)) ) {
        sscanf( buf, "%d:%d:%d:%d", &xx, &yy, &width, &height );
    }

    gtk_window_resize( GTK_WINDOW(apg->window), width, height );
    gtk_window_move (GTK_WINDOW(apg->window), xx, yy );
}

static void
makeGamesWindow( GtkAppGlobals* apg )
{
    GtkWidget* window;
    LaunchParams* params = apg->params;

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
    GtkWidget* list = init_games_list( apg );
    gtk_container_add( GTK_CONTAINER(vbox), list );
    
    gtk_widget_show( list );

    if ( !!params->pDb ) {
        GSList* games = listGames( params->pDb );
        for ( GSList* iter = games; !!iter; iter = iter->next ) {
            sqlite3_int64* rowid = (sqlite3_int64*)iter->data;
            onNewData( apg, *rowid, XP_TRUE );
        }
        g_slist_free( games );
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
    initGlobals( globals, apg->params, NULL );

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
    if ( getGameInfo( apg->params->pDb, rowid, &gib ) ) {
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
        if ( loadGameNoDraw( &tmpGlobals, apg->params, rowid ) ) {
            gameGotBuf( &tmpGlobals.cGlobals, XP_FALSE, buf, len, from );
            seed = comms_getChannelSeed( tmpGlobals.cGlobals.game.comms );
            saveGame( &tmpGlobals.cGlobals );
        }
        freeGlobals( &tmpGlobals );
    }
    return seed;
}

static void
gtkSocketAdded( void* closure, int newSock, GIOFunc proc )
{
    GIOChannel* channel = g_io_channel_unix_new( newSock );
    (void)g_io_add_watch( channel, G_IO_IN | G_IO_ERR, proc, closure );
    LOG_RETURN_VOID();
} /* gtk_socket_changed */


static void
relayInviteReceived( void* closure, NetLaunchInfo* invite )
{
    GtkAppGlobals* apg = (GtkAppGlobals*)closure;
    LaunchParams* params = apg->params;

    XP_U32 gameID = invite->gameID;
    sqlite3_int64 rowids[1];
    int nRowIDs = VSIZE(rowids);
    getRowsForGameID( apg->params->pDb, gameID, rowids, &nRowIDs );
    
    if ( 0 < nRowIDs ) {
        gtktell( apg->window, "Duplicate invite rejected" );
    } else {
        CurGameInfo gi = {0};
        gi_copy( MPPARM(params->mpool) &gi, &params->pgi );

        gi_setNPlayers( &gi, invite->nPlayersT, invite->nPlayersH );
        gi.gameID = gameID;
        gi.dictLang = invite->lang;
        gi.forceChannel = invite->forceChannel;
        replaceStringIfDifferent( params->mpool, &gi.dictName, invite->dict );

        GtkGameGlobals* globals = malloc( sizeof(*globals) );
        params->needsNewGame = XP_FALSE;
        initGlobals( globals, params, &gi );

        nli_makeAddrRec( invite, &globals->cGlobals.addr );
        // globals->cGlobals.addr = *returnAddr;

        GtkWidget* gameWindow = globals->window;
        globals->cGlobals.pDb = apg->params->pDb;
        globals->cGlobals.selRow = -1;
        recordOpened( apg, globals );
        gtk_widget_show( gameWindow );

        gi_disposePlayerInfo( MPPARM(params->mpool) &gi );
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
smsInviteReceived( void* closure, const XP_UCHAR* XP_UNUSED_DBG(gameName), 
                   XP_U32 gameID, XP_U16 dictLang, const XP_UCHAR* dictName,
                   XP_U16 nPlayers, XP_U16 nHere, XP_U16 forceChannel,
                   const CommsAddrRec* returnAddr )
{
    GtkAppGlobals* apg = (GtkAppGlobals*)closure;
    LaunchParams* params = apg->params;
    XP_LOGF( "%s(gameName=%s, gameID=%d, dictName=%s, nPlayers=%d, nHere=%d)",
             __func__, gameName, gameID, dictName, nPlayers, nHere );

    CurGameInfo gi = {0};
    gi_copy( MPPARM(params->mpool) &gi, &params->pgi );

    gi_setNPlayers( &gi, nPlayers, nHere );
    gi.gameID = gameID;
    gi.dictLang = dictLang;
    gi.forceChannel = forceChannel;
    replaceStringIfDifferent( params->mpool, &gi.dictName, dictName );

    GtkGameGlobals* globals = malloc( sizeof(*globals) );
    params->needsNewGame = XP_FALSE;
    initGlobals( globals, params, &gi );
    globals->cGlobals.addr = *returnAddr;

    GtkWidget* gameWindow = globals->window;
    globals->cGlobals.pDb = apg->params->pDb;
    globals->cGlobals.selRow = -1;
    recordOpened( apg, globals );
    gtk_widget_show( gameWindow );

    gi_disposePlayerInfo( MPPARM(params->mpool) &gi );
}

static void
smsMsgReceivedGTK( void* closure, const CommsAddrRec* from, XP_U32 gameID, 
                   const XP_U8* buf, XP_U16 len )
{
    LOG_FUNC();
    GtkAppGlobals* apg = (GtkAppGlobals*)closure;
    LaunchParams* params =  apg->params;

    sqlite3_int64 rowids[4];
    int nRowIDs = VSIZE(rowids);
    getRowsForGameID( params->pDb, gameID, rowids, &nRowIDs );
    XP_LOGF( "%s: found %d rows for gameID %d", __func__, nRowIDs, gameID );
    for ( int ii = 0; ii < nRowIDs; ++ii ) {
        feedBufferGTK( apg, rowids[ii], buf, len, from );
    }
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
    LaunchParams* params = apg->params;
    if ( !!devID ) {
        XP_LOGF( "%s(devID=%s)", __func__, devID );
        db_store( params->pDb, KEY_RDEVID, devID );
        (void)g_timeout_add_seconds( maxInterval, keepalive_timer, apg );

        setWindowTitle( apg );
    } else {
        XP_LOGF( "%s: bad relayid", __func__ );
        db_remove( params->pDb, KEY_RDEVID );

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
    XP_ASSERT( !!params->dbName || params->dbFileName );
    if ( !!params->dbName ) {
        params->pDb = openGamesDB( params->dbName );

        /* Check if we have a local ID already.  If we do and it's
           changed, we care. */
        XP_Bool idIsNew = linux_setupDevidParams( params );

        if ( params->useUdp ) {
            RelayConnProcs procs = {
                .msgReceived = gtkGotBuf,
                .msgNoticeReceived = gtkNoticeRcvd,
                .devIDReceived = gtkDevIDReceived,
                .msgErrorMsg = gtkErrorMsgRcvd,
                .socketAdded = gtkSocketAdded,
                .inviteReceived = relayInviteReceived,
            };

            relaycon_init( params, &procs, &apg, 
                           params->connInfo.relay.relayName,
                           params->connInfo.relay.defaultSendPort );

            linux_doInitialReg( params, idIsNew );
        }

#ifdef XWFEATURE_SMS
        gchar buf[32];
        const gchar* myPhone = params->connInfo.sms.phone;
        if ( !!myPhone ) {
            db_store( params->pDb, KEY_SMSPHONE, myPhone );
        } else if ( !myPhone && db_fetch( params->pDb, KEY_SMSPHONE, buf, VSIZE(buf) ) ) {
            params->connInfo.sms.phone = myPhone = buf;
        }
        XP_U16 myPort = params->connInfo.sms.port;
        gchar portbuf[8];
        if ( 0 < myPort ) {
            sprintf( portbuf, "%d", myPort );
            db_store( params->pDb, KEY_SMSPORT, portbuf );
        } else if ( db_fetch( params->pDb, KEY_SMSPORT, portbuf, VSIZE(portbuf) ) ) {
            params->connInfo.sms.port = myPort = atoi( portbuf );
        }
        if ( !!myPhone && 0 < myPort ) {
            SMSProcs smsProcs = {
                .socketAdded = gtkSocketAdded,
                .inviteReceived = smsInviteReceived,
                .msgReceived = smsMsgReceivedGTK,
            };
            linux_sms_init( params, myPhone, myPort, &smsProcs, &apg );
        } else {
            XP_LOGF( "not activating SMS: I don't have a phone" );
        }


#endif
        makeGamesWindow( &apg );
    } else if ( !!params->dbFileName ) {
        apg.window = openDBFile( &apg );
    }

    gtk_main();

    closeGamesDB( params->pDb );
    relaycon_cleanup( params );
#ifdef XWFEATURE_SMS
    linux_sms_cleanup( params );
#endif
    return 0;
} /* gtkmain */

#endif /* PLATFORM_GTK */
