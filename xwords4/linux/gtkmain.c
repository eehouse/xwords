/* 
 * Copyright 2000 - 2025 by Eric House (xwords@eehouse.org).  All rights
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
#include "linuxutl.h"
#include "mqttcon.h"
#include "linuxsms.h"
#include "gtkask.h"
#include "device.h"
#include "gtkkpdlg.h"
#include "gtknewgame.h"
#include "gtkrmtch.h"
#include "gsrcwrap.h"
#include "extcmds.h"
#include "gamemgr.h"
#include "gtkedalr.h"
#include "gtkaskgrp.h"
#include "dbgutil.h"

static void onNewData( GtkAppGlobals* apg, GameRef gr );
static void buildGamesList( GtkAppGlobals* apg );

static void updateButtons( GtkAppGlobals* apg );
static void open_row( GtkAppGlobals* apg, GameRef gr, XP_Bool isNew );

#ifdef XWFEATURE_DEVICE_STORES
static XP_Bool
gameIsOpen( GtkAppGlobals* apg, GameRef gr )
{
    XP_Bool found = NULL != globalsForGameRef( &apg->cag, gr, XP_FALSE );
    return found;
}
#else
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
#endif

static GtkGameGlobals*
findOpenGame( const GtkAppGlobals* apg, sqlite3_int64* rowid, XP_U32* gameID )
{
    XP_ASSERT(0);
    GtkGameGlobals* result = NULL;
    GSList* iter;
    for ( iter = apg->cag.globalsList; !!iter; iter = iter->next ) {
        GtkGameGlobals* globals = (GtkGameGlobals*)iter->data;
        CommonGlobals* cGlobals = &globals->cGlobals;
        if ( *rowid /* && cGlobals->rowid == *rowid*/ ) {
            result = globals;
            *gameID = cGlobals->gi->gameID;
            break;
        } else if ( gameID && cGlobals->gi->gameID == *gameID ) {
            result = globals;
            // *rowid = cGlobals->rowid;
            break;
        }
    }
    return result;
}

enum {
    ROW_THUMB,
#ifdef XWFEATURE_DEVICE_STORES
    GAMENAME_ITEM,
    GAMEREF_ITEM,
    CREATED_ITEM,
    STATUS_ITEM,
    GROUPNAME_ITEM,
    GROUP_ITEM,                 /* not shown */
#else
    ROW_ITEM,
    NAME_ITEM, CREATED_ITEM,
    SEED_ITEM, ROLE_ITEM,
    CONN_ITEM,
    OVER_ITEM, TURN_ITEM,LOCAL_ITEM, NMOVES_ITEM,
    MISSING_ITEM, LASTTURN_ITEM, DUPTIMER_ITEM,
#endif
    // GAMEID_ITEM,
    TURN_ITEM,
    NMOVES_ITEM,
    NPACKETS_ITEM,
    LASTMOVE_ITEM,
    LANG_ITEM,
    CHANNEL_ITEM,
    ROLE_ITEM,
#ifdef XWFEATURE_DEVICE_STORES
    CONTYPES_ITEM,
#endif
    NTOTAL_ITEM,
       
    N_ITEMS,
};

static GameRef
grFromIter( GtkTreeModel* model, GtkTreeIter* iter )
{
    gchar* str = NULL;
    gtk_tree_model_get( model, iter, GAMEREF_ITEM, &str, -1 );
    GameRef gr;
    sscanf( str, GR_FMT, &gr );
    g_free( str );
    return gr;
}

static GroupRef
grpFromIter( GtkTreeModel* model, GtkTreeIter* iter )
{
    gint val;
    gtk_tree_model_get( model, iter, GROUP_ITEM, &val, -1 );
    GroupRef grp = (GroupRef)val;
    XP_LOGFF( "got group: %d", grp );
    return grp;
}

static void
foreachProc( GtkTreeModel* model, GtkTreePath* XP_UNUSED(path),
             GtkTreeIter* iter, gpointer data )
{
    GtkAppGlobals* apg = (GtkAppGlobals*)data;
    GameRef val = grFromIter( model, iter );
    apg->selRows = g_array_append_val( apg->selRows, val );
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
row_activated_cb( GtkTreeView* treeView, GtkTreePath* path,
                  GtkTreeViewColumn* XP_UNUSED(column), gpointer data )
{
    GtkAppGlobals* apg = (GtkAppGlobals*)data;
    GtkTreeModel* model = gtk_tree_view_get_model( treeView );
    GtkTreeIter iter;
    if ( gtk_tree_model_get_iter( model, &iter, path ) ) {
        GameRef gr = grFromIter( model, &iter );
        open_row( apg, gr, XP_FALSE );
    }
}

static bool
findFor( GtkAppGlobals* apg, GameRef gr, GtkTreeIter* foundP, GtkListStore** listP )
{
    bool valid = false;
    GList* children = gtk_container_get_children(GTK_CONTAINER(apg->treesBox));
    for( GList* child = children; !valid && child != NULL; child = g_list_next(child)) {
        if ( GTK_IS_TREE_VIEW(child->data) ) {
            GtkWidget* tree = child->data;
            GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(tree));
            GtkTreeIter iter;
            for ( valid = gtk_tree_model_get_iter_first( model, &iter );
                  valid;
                  valid = gtk_tree_model_iter_next( model, &iter ) ) {
                GameRef val = grFromIter( model, &iter );
                if ( val == gr ) {
                    /* Confirm it's the right group */
                    XW_DUtilCtxt* dutil = apg->cag.params->dutil;
                    GroupRef grp = gr_getGroup( dutil, gr, NULL_XWE );
                    GroupRef curGrp = grpFromIter( model, &iter );
                    if ( curGrp != grp ) {
                        XP_ASSERT(0); /* TODO: remove from model */
                        valid = false;
                    }
                    if ( !!foundP ) {
                        *foundP = iter;
                    }
                    if ( !!listP ) {
                        *listP = GTK_LIST_STORE( model );
                    }
                    break;
                }
            }
        }
    }
    g_list_free( children );
    return valid;
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
init_games_list( void* closure )
{
    GtkWidget* list = (GtkWidget*)GTK_TREE_VIEW(gtk_tree_view_new());
    
    addImageColumn( list, "Snap", ROW_THUMB );
#ifdef XWFEATURE_DEVICE_STORES
    addTextColumn( list, "Name", GAMENAME_ITEM );
    addTextColumn( list, "GameRef", GAMEREF_ITEM );
    addTextColumn( list, "Created", CREATED_ITEM );
    addTextColumn( list, "Status", STATUS_ITEM );
    addTextColumn( list, "Group", GROUPNAME_ITEM );
#else
    addTextColumn( list, "Row", ROW_ITEM );
    addTextColumn( list, "Name", NAME_ITEM );
    addTextColumn( list, "Created", CREATED_ITEM );
    addTextColumn( list, "Seed", SEED_ITEM );
    addTextColumn( list, "Role", ROLE_ITEM );
    addTextColumn( list, "Conn. via", CONN_ITEM );
    addTextColumn( list, "nPackets", NPACKETS_ITEM );
    addTextColumn( list, "Ended", OVER_ITEM );
    addTextColumn( list, "Turn", TURN_ITEM );
    addTextColumn( list, "Local", LOCAL_ITEM );
    addTextColumn( list, "NMoves", NMOVES_ITEM );
    addTextColumn( list, "NMissing", MISSING_ITEM );
    addTextColumn( list, "LastTurn", LASTTURN_ITEM );
    addTextColumn( list, "DupTimerFires", DUPTIMER_ITEM );
#endif
    // addTextColumn( list, "GameID", GAMEID_ITEM );
    addTextColumn( list, "Turn", TURN_ITEM );
    addTextColumn( list, "#moves", NMOVES_ITEM );
    addTextColumn( list, "#packets", NPACKETS_ITEM );
    addTextColumn( list, "Last move", LASTMOVE_ITEM );
    addTextColumn( list, "Lang", LANG_ITEM );
    addTextColumn( list, "Channel", CHANNEL_ITEM );
    addTextColumn( list, "Role", ROLE_ITEM );
#ifdef XWFEATURE_DEVICE_STORES
    addTextColumn( list, "ConTypes", CONTYPES_ITEM );
#endif
    addTextColumn( list, "NTotal", NTOTAL_ITEM );

    GtkListStore* store = gtk_list_store_new( N_ITEMS,
                                              GDK_TYPE_PIXBUF,/* ROW_THUMB */
#ifdef XWFEATURE_DEVICE_STORES
                                              G_TYPE_STRING,  /* GAMENAME_ITEM */
                                              G_TYPE_STRING,   /* GAMEREF_ITEM */
                                              G_TYPE_STRING,  /* CREATED_ITEM */
                                              G_TYPE_STRING,  /* STATUS_ITEM */
                                              G_TYPE_STRING,  /* GROUPNAME_ITEM */
                                              G_TYPE_INT,     /* GROUP_ITEM */
#else
                                              /* G_TYPE_INT64,   /\* ROW_ITEM *\/ */
                                              /* G_TYPE_STRING,  /\* NAME_ITEM *\/ */
                                              /* G_TYPE_STRING,  /\* CREATED_ITEM *\/ */
                                              /* G_TYPE_INT,     /\* SEED_ITEM *\/ */
                                              /* G_TYPE_INT,     /\* ROLE_ITEM *\/ */
                                              /* G_TYPE_STRING,  /\* CONN_ITEM *\/ */
                                              /* G_TYPE_INT,     /\* NPACKETS_ITEM *\/ */
                                              /* G_TYPE_BOOLEAN, /\* OVER_ITEM *\/ */
                                              /* G_TYPE_INT,     /\* TURN_ITEM *\/ */
                                              /* G_TYPE_STRING,  /\* LOCAL_ITEM *\/ */
                                              /* G_TYPE_INT,     /\* NMOVES_ITEM *\/ */
                                              /* G_TYPE_INT,     /\* MISSING_ITEM *\/ */
                                              /* G_TYPE_STRING,  /\* LASTTURN_ITEM *\/ */
                                              /* G_TYPE_STRING,  /\* DUPTIMER_ITEM *\/ */
#endif
                                              // G_TYPE_STRING,  /* GAMEID_ITEM */
                                              G_TYPE_STRING,  /* TURN_ITEM */
                                              G_TYPE_INT,     /* NMOVES_ITEM */
                                              G_TYPE_INT,     /* NPACKETS_ITEM */
                                              G_TYPE_STRING,  /* LASTMOVE_ITEM */
                                              G_TYPE_STRING,  /* LANG_ITEM */
                                              G_TYPE_INT,     /* CHANNEL_ITEM */
                                              G_TYPE_INT,     /* ROLE_ITEM */
#ifdef XWFEATURE_DEVICE_STORES
                                              G_TYPE_STRING,  /* CONTYPES_ITEM */
#endif
                                              G_TYPE_INT     /* NTOTAL_ITEM */
                                              );
    gtk_tree_view_set_model( GTK_TREE_VIEW(list), GTK_TREE_MODEL(store) );
    g_object_unref( store );

    g_signal_connect( G_OBJECT(list), "row-activated",
                      G_CALLBACK(row_activated_cb), closure );
 
    GtkTreeSelection* select =
        gtk_tree_view_get_selection( GTK_TREE_VIEW (list) );
    gtk_tree_selection_set_mode( select, GTK_SELECTION_MULTIPLE );
    g_signal_connect( G_OBJECT(select), "changed",
                      G_CALLBACK(tree_selection_changed_cb), closure );
    return list;
}

static GdkPixbuf*
getSnapData( LaunchParams* params, GameRef gr )
{
    GdkPixbuf* result = NULL;
    XW_DUtilCtxt* dutil = params->dutil;
    XWStreamCtxt* stream = dvc_makeStream( params->dutil );
    if ( gr_getThumbData( dutil, gr, NULL_XWE, stream ) ) {
        GInputStream* istr =
            g_memory_input_stream_new_from_data( strm_getPtr(stream),
                                                 strm_getSize(stream), NULL );
        result = gdk_pixbuf_new_from_stream( istr, NULL, NULL );
        g_object_unref( istr );
    }
    strm_destroy( stream );
    return result;
}

static void
updateRow( LaunchParams* params, GameRef gr,
           GtkListStore* store, GtkTreeIter* iter )
{
    const CurGameInfo* gi = gr_getGI( params->dutil, gr, NULL_XWE );
    const GameSummary* sum = gr_getSummary( params->dutil, gr, NULL_XWE );

    gchar gameIDStr[16];
    sprintf( gameIDStr, "%8X", gi->gameID );

    GdkPixbuf* snap = getSnapData( params, gr );

    gchar* state;
    if ( 0 <= sum->turn && 0 == sum->nMissing ) {      /* game's in play */
        state = "in play";
    } else if ( sum->gameOver ) {
        state = "game over";
    } else {
        state = "initing";
    }

    gchar createdStr[64] = {};
    if ( 0 != gi->created ) {
        formatSeconds( gi->created, createdStr, VSIZE(createdStr) );
    }
    gchar lastMoveStr[64] = {};
    if ( 0 != sum->lastMoveTime ) {
        formatSeconds( sum->lastMoveTime, lastMoveStr, VSIZE(lastMoveStr) );
    }

    GroupRef grp = gr_getGroup(params->dutil, gr, NULL_XWE );
    XP_UCHAR groupName[64];
    gmgr_getGroupName( params->dutil, NULL_XWE, grp,
                       groupName, VSIZE(groupName) );

    gchar conTypesBuf[32];
    snprintf( conTypesBuf, sizeof(conTypesBuf), "0X%X", gi->conTypes );

    gchar grStr[32];
    snprintf( grStr, sizeof(grStr), GR_FMT, gr );

    gtk_list_store_set( store, iter,
                        ROW_THUMB, snap,
                        GAMENAME_ITEM, gi->gameName,
                        GAMEREF_ITEM, grStr,
                        CREATED_ITEM, createdStr,
                        STATUS_ITEM, state,
                        GROUPNAME_ITEM, groupName,
                        GROUP_ITEM, grp,
                        // GAMEID_ITEM, gameIDStr,
                        TURN_ITEM, sum->turnIsLocal?"Local":"Remote",
                        NMOVES_ITEM, sum->nMoves,
                        NPACKETS_ITEM, sum->nPacketsPending,
                        LASTMOVE_ITEM, lastMoveStr,
                        LANG_ITEM, gi->isoCodeStr,
                        CHANNEL_ITEM, gi->forceChannel,
                        ROLE_ITEM, gi->deviceRole,
                        CONTYPES_ITEM, conTypesBuf,
                        NTOTAL_ITEM, gi->nPlayers,
                        -1 );
}

static GtkWidget*
add_to_list( LaunchParams* params, GtkWidget* list, GameRef gr,
             GtkWidget* container, void* closure )
{
    XP_ASSERT( gr );
    GtkListStore* store = NULL;
    GtkTreeIter iter;
    GtkAppGlobals* apg = (GtkAppGlobals*)params->cag;
    XP_Bool found = findFor( apg, gr, &iter, &store );

    if ( !found ) {
        XP_LOGFF( "making new row for " GR_FMT, gr );
        if ( !list ) {
            list = init_games_list( closure );
            gtk_container_add( GTK_CONTAINER(container), list );
            gtk_widget_show( list );
        }

        GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(list));
        store = GTK_LIST_STORE( model );
        gtk_list_store_append( store, &iter );
    } else {
        XP_LOGFF( "using existing row" );
    }

    updateRow( params, gr, store, &iter );
    return list;
}

static XP_Bool
updateListFor( GtkAppGlobals* apg, GameRef gr )
{
    GtkTreeIter iter;
    GtkListStore* list;
    XP_Bool found = findFor( apg, gr, &iter, &list );
    XP_ASSERT( found );
    if ( found ) {
        updateRow( apg->cag.params, gr, list, &iter );
    }
    LOG_RETURNF( "%s", boolToStr(found) );
    return found;
}

/* This is supposed to check that at least one of the selected games is not
   in the archive. Later...  */
static bool
oneNotInArchive( GtkAppGlobals* apg )
{
    bool result = false;
    XW_DUtilCtxt* dutil = apg->cag.params->dutil;
    for ( int ii = 0; ii < apg->selRows->len && !result; ++ii ) {
        GameRef gr = g_array_index( apg->selRows, GameRef, ii );
        result = !gr_isArchived( dutil, gr, NULL_XWE );
    }

    return result;
}

static void updateButtons( GtkAppGlobals* apg )
{
    guint count = apg->selRows->len;

    gtk_widget_set_sensitive( apg->renameButton, 1 == count );
    gtk_widget_set_sensitive( apg->openButton, 1 <= count );
    gtk_widget_set_sensitive( apg->rematchButton, 1 == count );
    gtk_widget_set_sensitive( apg->deleteButton, 1 <= count );
    gtk_widget_set_sensitive( apg->moveToGroupButton, 1 <= count );
    gtk_widget_set_sensitive( apg->archiveButton, oneNotInArchive(apg) );
}

void
addGTKGame( LaunchParams* params, GameRef gr )
{
    GtkGameGlobals* globals = (GtkGameGlobals*)
        globalsForGameRef( params->cag, gr, XP_TRUE );
    params->needsNewGame = XP_FALSE;
    initBoardGlobalsGtk( globals, params, gr );

    GtkWidget* gameWindow = globals->window;
    gtk_widget_show( gameWindow );
}

static void
newGameIn( GtkAppGlobals* apg, GroupRef grp )
{
    LaunchParams* params = apg->cag.params;
    CurGameInfo gi = {};
    gi_copy( &gi, &params->pgi );

    CommsAddrRec selfAddr;
    makeSelfAddress( &selfAddr, params );

    if ( gtkNewGameDialog( params, &gi, &selfAddr, XP_TRUE, XP_FALSE ) ) {
        LOG_GI( &gi, __func__ );
        /*(void*)*/gmgr_newFor( params->dutil, NULL_XWE, grp, &gi, NULL );
    }
}

static void
handle_newgame_button( GtkWidget* XP_UNUSED(widget), void* closure )
{
    GtkAppGlobals* apg = (GtkAppGlobals*)closure;
    newGameIn( apg, GROUP_DEFAULT );
}

#ifdef XWFEATURE_GAMEREF_CONVERT
static void updateForConvert( GtkAppGlobals* apg );

static GroupRef
groupForRole(XW_DUtilCtxt* dutil, DeviceRole role)
{
    const char* name;
    switch ( role ) {
    case ROLE_STANDALONE:
        name = "Solo";
        break;
    case ROLE_ISHOST:
        name = "Host";
        break;
    case ROLE_ISGUEST:
        name = "Guest";
        break;
    default:
        XP_ASSERT(0);
    }
    GroupRef grp = gmgr_getGroup( dutil, NULL_XWE, name );
    if ( !grp ) {
        grp = gmgr_addGroup( dutil, NULL_XWE, name );
    }
    return grp;
}

static void
checkConvertImpl( GtkAppGlobals* apg, XP_Bool doAll )
{
    LaunchParams* params = apg->cag.params;
    XW_DUtilCtxt* dutil = params->dutil;

    bool done = false;
    GSList* games = gdb_listGames( params->pDb );
    for ( GSList* iter = games; !!iter && !done; iter = iter->next ) {
        sqlite3_int64 rowid = *(sqlite3_int64*)iter->data;

        XWStreamCtxt* stream = dvc_makeStream( params->dutil );
        DeviceRole role;
        GameRef gr = 0;
        if ( gdb_loadGame( stream, params->pDb, &role, rowid ) ) {
            GroupRef grp = groupForRole(dutil, role);
            XP_UCHAR* name = g_strdup_printf("Game %lld", rowid );

            gr = gmgr_convertGame( dutil, NULL_XWE, grp, name, stream );
        }
        strm_destroy( stream );
        /* exit after first success */
        if ( !doAll && !!gr ) {
            break;
        }
    }
    gdb_freeGamesList( games );

    updateForConvert( apg );
}

static int
checkConvertOne( void* closure )
{
    checkConvertImpl( (GtkAppGlobals*)closure, XP_FALSE );
    return 0;                   /* don't run again */
}

static int
checkConvertAll( void* closure )
{
    checkConvertImpl( (GtkAppGlobals*)closure, XP_TRUE );
    return 0;                   /* don't run again */
}

static void
handle_convert_button( GtkWidget* XP_UNUSED(widget), void* closure )
{
    (void)g_idle_add( checkConvertOne, closure );
}

static void
handle_convertAll_button( GtkWidget* XP_UNUSED(widget), void* closure )
{
    (void)g_idle_add( checkConvertAll, closure );
}

static void
handle_deleteOld_button( GtkWidget* XP_UNUSED(widget), void* closure )
{
    LOG_FUNC();
    XP_USE(closure);
    updateForConvert( (GtkAppGlobals*)closure );
}
#endif

static void
handle_rename_button( GtkWidget* XP_UNUSED(widget), void* closure )
{
    GtkAppGlobals* apg = (GtkAppGlobals*)closure;
    XW_DUtilCtxt* dutil = apg->cag.params->dutil;
    GameRef gr = g_array_index( apg->selRows, GameRef, 0 );
    const CurGameInfo* gi = gr_getGI( dutil, gr, NULL_XWE );

    XP_UCHAR name[64];
    str2ChrArray( name, gi->gameName );
    if ( gtkEditAlert( apg->window, "Edit this game's name",
                       name, VSIZE(name) ) ) {
        gr_setGameName( dutil, gr, NULL_XWE, name );
    }
}

static void
handle_newgroup_button( GtkWidget* XP_UNUSED(widget), void* closure )
{
    GtkAppGlobals* apg = (GtkAppGlobals*)closure;
    LaunchParams* params = apg->cag.params;

    XP_UCHAR name[33] = "New Group";
    if ( gtkEditAlert( apg->window, "Name of new group", name, VSIZE(name) ) ) {
        gmgr_addGroup( params->dutil, NULL_XWE, name );
    }
}

static void
open_row( GtkAppGlobals* apg, GameRef gr, XP_Bool isNew )
{
    if ( !!gr && !gameIsOpen( apg, gr ) ) {
        LaunchParams* params = apg->cag.params;
        const XP_UCHAR* missingNames[5] = {};
        XP_U16 count = VSIZE(missingNames);
        gr_missingDicts( params->dutil, gr, NULL_XWE, missingNames, &count );
        if ( count == 0 ) {
            if ( isNew ) {
                onNewData( apg, gr );
            }

            params->needsNewGame = XP_FALSE;

            GtkGameGlobals* globals = (GtkGameGlobals*)
                globalsForGameRef( params->cag, gr, XP_TRUE );
            initBoardGlobalsGtk( globals, params, gr );
            gtk_widget_show( globals->window );
        } else {
            AskPair pairs[] = { {.txt = "Delete", .result = 1},
                              {.txt = "Download", .result = 2},
                              {.txt = "Ok", .result = 3},
                              { NULL, 0 }
            };
            gchar* lst = g_strjoinv(", ", (gchar**)missingNames );
            gchar* msg = g_strdup_printf( "This game is missing wordlist[s] %s.", lst );
            switch ( gtkask( apg->window, msg, GTK_BUTTONS_NONE, pairs ) ) {
            case 1:             /* delete */
                gmgr_deleteGame( params->dutil, NULL_XWE, gr );
                break;
            case 2:
            case 3:
                break;
            }
            g_free( lst );
            g_free( msg );
        }
    }
}

static void
handle_open_button( GtkWidget* XP_UNUSED(widget), void* closure )
{
    GtkAppGlobals* apg = (GtkAppGlobals*)closure;

    GArray* selRows = apg->selRows;
    for ( int ii = 0; ii < selRows->len; ++ii ) {
#ifdef XWFEATURE_DEVICE_STORES
        GameRef row = g_array_index( selRows, GameRef, ii );
#else
        sqlite3_int64 row = g_array_index( selRows, sqlite3_int64, ii );
#endif
        open_row( apg, row, XP_FALSE );
    }
}

void
make_rematch( GtkAppGlobals* apg, GameRef parent, XP_Bool archiveAfter,
              XP_Bool deleteAfter )
{
    XW_DUtilCtxt* dutil = apg->cag.params->dutil;
    XP_Bool canRematch = gr_canRematch( dutil, parent, NULL_XWE, NULL );

    if ( canRematch ) {
        gchar gameName[128];
        int nameLen = VSIZE(gameName);
        RematchOrder ro;
        if ( gtkask_rematch( dutil, apg->window, parent, &ro,
                             gameName, &nameLen ) ) {
            GameRef gr = gr_makeRematch( dutil, parent, NULL_XWE, gameName,
                                         ro, archiveAfter, deleteAfter );
            addGTKGame( apg->cag.params, gr );
            open_row( apg, gr, XP_TRUE );
        }
    }
} /* make_rematch */

static void
handle_rematch_button( GtkWidget* XP_UNUSED(widget), void* closure )
{
    GtkAppGlobals* apg = (GtkAppGlobals*)closure;
    GArray* selRows = apg->selRows;
    for ( int ii = 0; ii < selRows->len; ++ii ) {
        GameRef gr = g_array_index( selRows, GameRef, ii );
        make_rematch( apg, gr, XP_FALSE, XP_FALSE );
    }
}

static void
handle_movetogroup_button( GtkWidget* XP_UNUSED(widget), void* closure )
{
    GtkAppGlobals* apg = (GtkAppGlobals*)closure;

    /* If we don't have selected games, skip */
    guint nGames = apg->selRows->len;
    if ( 0 < nGames ) {
        GameRef grs[nGames];
        for ( guint ii = 0; ii < nGames; ++ii ) {
            grs[ii] = g_array_index( apg->selRows, GameRef, ii );
        }

        XW_DUtilCtxt* dutil = apg->cag.params->dutil;
        XP_U32 nGroups = gmgr_countGroups( dutil, NULL_XWE );
        GroupRef grps[nGroups];
        XP_UCHAR bufs[nGroups][32];
        XP_UCHAR* names[nGroups];
        for ( int ii = 0; ii < nGroups; ++ii ) {
            grps[ii] = gmgr_getNthGroup( dutil, NULL_XWE, ii );
            gmgr_getGroupName( dutil, NULL_XWE, grps[ii],
                               bufs[ii], VSIZE(bufs[ii]) );
            names[ii] = bufs[ii];
        }

        XP_U16 sel;
        if ( gtkAskGroup( apg->window, names, nGroups, &sel ) ) {
            gmgr_moveGames( dutil, NULL_XWE, grps[sel], grs, nGames );
        }
    }
}

static void
handle_archive_button( GtkWidget* XP_UNUSED(widget), void* closure )
{
    GtkAppGlobals* apg = (GtkAppGlobals*)closure;
    guint nGames = apg->selRows->len;
    if ( 0 < nGames ) {
        GameRef grs[nGames];
        for ( guint ii = 0; ii < nGames; ++ii ) {
            grs[ii] = g_array_index( apg->selRows, GameRef, ii );
        }
        XW_DUtilCtxt* dutil = apg->cag.params->dutil;
        gchar* msg = "Are you sure you want to archive the selected game[s]? "
            "Archived games do not send or receive messages in the background.";
        if ( GTK_RESPONSE_YES == gtkask( apg->window, msg, GTK_BUTTONS_YES_NO, NULL ) ) {
            gmgr_moveGames( dutil, NULL_XWE, GROUP_ARCHIVE, grs, nGames );
        }
    }
}

static void
delete_game( GtkAppGlobals* apg, GameRef gr )
{
    LaunchParams* params = apg->cag.params;
    gmgr_deleteGame( params->dutil, NULL_XWE, gr );
}

static void
handle_delete_button( GtkWidget* XP_UNUSED(widget), void* closure )
{
    GtkAppGlobals* apg = (GtkAppGlobals*)closure;
    guint len = apg->selRows->len;

    XP_UCHAR buf[128];
    XP_SNPRINTF( buf, VSIZE(buf), "Are you sure you want to delete the %d "
                 "selected games", len );
    if ( GTK_RESPONSE_YES == gtkask( apg->window, buf,
                                     GTK_BUTTONS_YES_NO, NULL ) ) {
        for ( guint ii = 0; ii < len; ++ii ) {
            GameRef item = g_array_index( apg->selRows, GameRef, ii );
            delete_game( apg, item );
        }
        apg->selRows = g_array_set_size( apg->selRows, 0 );
        updateButtons( apg );
    }
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
        cg_unref( &globals->cGlobals );
    }
    g_slist_free( apg->cag.globalsList );

    int new_width, new_height;
    gtk_window_get_size( GTK_WINDOW(apg->window), &new_width, &new_height);
    apg->lastConfigure.width = new_width;
    apg->lastConfigure.height = new_height;
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
removeAllFrom( GtkWidget* container )
{
    GList* children = gtk_container_get_children(GTK_CONTAINER(container));
    for( GList* iter = children; iter != NULL; iter = g_list_next(iter)) {
        gtk_container_remove( GTK_CONTAINER(container), GTK_WIDGET(iter->data) );
    }
    g_list_free(children);
}

#ifdef XWFEATURE_GAMEREF_CONVERT
static void
getCounts( GtkAppGlobals* apg, int* nToConvert, int* nToDelete )
{
    LaunchParams* params = apg->cag.params;
    *nToDelete = *nToConvert = 0;
    GSList* games = gdb_listGames( params->pDb );
    for ( GSList* iter = games; !!iter; iter = iter->next ) {
        sqlite3_int64 rowid = *(sqlite3_int64*)iter->data;

        XWStreamCtxt* stream = dvc_makeStream( params->dutil );
        DeviceRole role;
        if ( gdb_loadGame( stream, params->pDb, &role, rowid ) ) {
            GameRef gr = gmgr_figureGR( stream );
            XP_LOGFF( "row %llx => " GR_FMT, rowid, gr );
            if ( gmgr_gameExists( params->dutil, NULL_XWE, gr ) ) {
                XP_LOGFF( GR_FMT " exists", gr );
                ++*nToDelete;
            } else {
                XP_LOGFF( GR_FMT " needs converting", gr );
                ++*nToConvert;
            }
        }
        strm_destroy( stream );
    }
    gdb_freeGamesList( games );
}

static void
updateForConvert( GtkAppGlobals* apg )
{
    GtkWidget* hbox = apg->convertBox;

    removeAllFrom( hbox );

    int needsConvert = 0;
    int needsDelete = 0;
    getCounts( apg, &needsConvert, &needsDelete );

    if ( needsConvert || needsDelete ) {
        if ( needsConvert ) {
            gchar msg[128];
            sprintf( msg, "There are %d old-format games to convert", needsConvert );
            XP_LOGFF( "%s", msg );
            GtkWidget* label = gtk_label_new( msg );
            gtk_container_add( GTK_CONTAINER(hbox), label );

            (void)addButton( "Convert one", hbox, G_CALLBACK(handle_convert_button), apg );
            (void)addButton( "Convert all", hbox, G_CALLBACK(handle_convertAll_button), apg );
        }
        if ( needsDelete ) {
            gchar msg[128];
            sprintf( msg, "There are %d converted old-format games to delete", needsDelete );
            XP_LOGFF( "%s", msg );
            GtkWidget* label = gtk_label_new( msg );
            gtk_container_add( GTK_CONTAINER(hbox), label );

            (void)addButton( "Delete old", hbox, G_CALLBACK(handle_deleteOld_button), apg );
        }
    }
    gtk_widget_show_all( hbox );
}

static gint
updateConvertIdle( gpointer closure )
{
    GtkAppGlobals* apg = (GtkAppGlobals*)closure;
    updateForConvert( apg );
    return 0;
}

static void
addForConvert( GtkAppGlobals* apg, GtkWidget* parent )
{
    apg->convertBox = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 0 );
    gtk_container_add( GTK_CONTAINER(parent), apg->convertBox );

    (void)g_idle_add( updateConvertIdle, apg );
}
#endif

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
    snprintf( &title[len], VSIZE(title) - len, " (mqtt: %s, host: %s)",
              formatMQTTDevID( &devID, didBuf, VSIZE(didBuf) ),
              params->connInfo.mqtt.hostName);

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

void
onGameChangedGTK( LaunchParams* params, GameRef gr, GameChangeEvents gces )
{
    CommonAppGlobals* cag = params->cag;
    GtkAppGlobals* apg = (GtkAppGlobals*)cag;
    if ( !updateListFor( apg, gr ) ) {
        buildGamesList( apg );
    }

    CommonGlobals* cGlobals = globalsForGameRef( cag, gr, XP_FALSE );
    if ( !!cGlobals ) {
        onGameChanged( (GtkGameGlobals*)cGlobals, gces );
    }
}

void
onGroupChangedGTK( LaunchParams* params, GroupRef XP_UNUSED_DBG(grp),
                   GroupChangeEvents XP_UNUSED_DBG(gces) )
{
    XP_LOGFF( "grp: %d, gces: %x", grp, gces );
    GtkAppGlobals* apg = (GtkAppGlobals*)params->cag;
    buildGamesList( apg );
}

static gint
buildGamesListIdle( gpointer data )
{
    GtkAppGlobals* apg = (GtkAppGlobals*)data;
    buildGamesList( apg );
    return 0;                   /* don't run again */
}

void
onGTKMissingDictAdded( LaunchParams* params, GameRef XP_UNUSED_DBG(gr),
                       const XP_UCHAR* XP_UNUSED_DBG(dictName) )
{
    XP_LOGFF( "(gr=" GR_FMT ", dictName=%s)", gr, dictName );
    XP_USE(params);
}

void
onGTKDictGone( LaunchParams* params, GameRef XP_UNUSED_DBG(gr),
               const XP_UCHAR* XP_UNUSED_DBG(dictName) )
{
    XP_LOGFF( "(gr=" GR_FMT ", dictName=%s)", gr, dictName );
    XP_USE(params);
}

void
informMoveGTK( LaunchParams* params, GameRef gr, XWStreamCtxt* expl,
               XWStreamCtxt* words )
{
    GtkWidget* parent;
    CommonAppGlobals* cag = params->cag;
    CommonGlobals* cGlobals = globalsForGameRef( cag, gr, XP_FALSE );

    if ( !!cGlobals ) {
        parent = ((GtkGameGlobals*)cGlobals)->window;
    } else {
        GtkAppGlobals* globals = (GtkAppGlobals*)cag;
        parent = globals->window;
    }

    char* explStr = strFromStream( expl );
    gchar* msg = g_strdup_printf( "informMove():\nexpl: %s", explStr );
    if ( !!words ) {
        char* wordsStr = strFromStream( words );
        gchar* prev = msg;
        gchar* postfix = g_strdup_printf( "words: %s", wordsStr );
        free( wordsStr );
        msg = g_strconcat( msg, postfix, NULL );
        g_free( prev );
        g_free( postfix );
    }
    (void)gtktell( parent, msg );
    free( explStr );
    g_free( msg );
}

void
informGameOverGTK( LaunchParams* params, GameRef gr, XP_S16 quitter )
{
    CommonGlobals* cGlobals = globalsForGameRef( params->cag, gr, XP_FALSE );
    if ( !!cGlobals ) {
        GtkGameGlobals* globals = (GtkGameGlobals*)cGlobals;

        if ( params->printHistory ) {
            catGameHistory( params, gr );
        }

        catFinalScores( params, gr, quitter );

        if ( params->quitAfter >= 0 ) {
            sleep( params->quitAfter );
            destroy_board_window( NULL, globals );
        } else if ( params->undoWhenDone ) {
            XW_DUtilCtxt* dutil = params->dutil;
            gr_handleUndo( dutil, cGlobals->gr, NULL_XWE, 0 );
            gr_draw( dutil, cGlobals->gr, NULL_XWE );
        } else if ( !params->skipGameOver ) {
            gtkShowFinalScores( globals, XP_TRUE );
        }
    }
}

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
handle_getUrl( GtkWidget* XP_UNUSED(widget), GtkAppGlobals* apg )
{
    gchar* txt = gtkask_gettext( apg->window, "Paste URL here" );
    if ( !!txt ) {
        LaunchParams* params = apg->cag.params;
        dvc_parseUrl( params->dutil, NULL_XWE, txt, strlen(txt), NULL, NULL );
        g_free( txt );
    }
}

#ifdef XWFEATURE_DEVICE_STORES
/* static void */
/* onGameProc( GameRef gr, XWEnv XP_UNUSED(xwe), void* closure ) */
/* { */
/*     GtkAppGlobals* apg = (GtkAppGlobals*)closure; */
/*     onNewGameRef( apg, gr, XP_TRUE ); */
/* } */
#endif

static void
showGamesToggle( GtkWidget* toggle, void* closure )
{
    GtkAppGlobals* apg = (GtkAppGlobals*)closure;
    LaunchParams* params = apg->cag.params;
    params->showGames = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(toggle) );
}

typedef struct _GroupState {
    GtkAppGlobals* apg;
    LaunchParams* params;
    GroupRef grp;
} GroupState;

static void
hideCheckToggled( GtkWidget* toggle, void* closure )
{
    GroupState* gs = (GroupState*)closure;
    bool collapse = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(toggle) );
    gmgr_setGroupCollapsed( gs->params->dutil, NULL_XWE, gs->grp, collapse );
}


static void
mkDefaultGroupProc( GtkWidget* XP_UNUSED(widget), void* closure )
{
    GroupState* gs = (GroupState*)closure;
    gmgr_makeGroupDefault( gs->params->dutil, NULL_XWE, gs->grp );
}

static void
addToGroupProc( GtkWidget* XP_UNUSED(widget), void* closure )
{
    GroupState* gs = (GroupState*)closure;
    GtkAppGlobals* apg = gs->apg;
    newGameIn( apg, gs->grp );
}

static void
renameGroupProc( GtkWidget* XP_UNUSED(widget), void* closure )
{
    GroupState* gs = (GroupState*)closure;
    XP_UCHAR name[64];
    gmgr_getGroupName( gs->params->dutil, NULL_XWE, gs->grp,
                       name, sizeof(name) );
    if ( gtkEditAlert( gs->apg->window, "Edit your group's name", name, VSIZE(name) ) ) {
        gmgr_setGroupName( gs->params->dutil, NULL_XWE, gs->grp, name );
    }
}

static void
upGroupProc( GtkWidget* XP_UNUSED(widget), void* closure )
{
    GroupState* gs = (GroupState*)closure;
    gmgr_raiseGroup( gs->params->dutil, NULL_XWE, gs->grp );
}

static void
downGroupProc( GtkWidget* XP_UNUSED(widget), void* closure )
{
    GroupState* gs = (GroupState*)closure;
    gmgr_lowerGroup( gs->params->dutil, NULL_XWE, gs->grp );
}

static void
sortGroupProc( GtkWidget* XP_UNUSED(widget), void* XP_UNUSED(closure) )
{
    LOG_FUNC();
}

static void
deleteGroupProc( GtkWidget* XP_UNUSED(widget), void* closure )
{
    GroupState* gs = (GroupState*)closure;
    XW_DUtilCtxt* dutil = gs->params->dutil;
    if ( gs->grp == gmgr_getDefaultGroup( dutil ) ) {
        gtktell( gs->apg->window, "The default group cannot be deleted." );
    } else {
        XP_U32 count = gmgr_getGroupGamesCount( dutil, NULL_XWE, gs->grp );
        XP_UCHAR name[32];
        gmgr_getGroupName( dutil, NULL_XWE, gs->grp, name, VSIZE(name) );
        XP_UCHAR msg[256];
        XP_SNPRINTF( msg, VSIZE(msg), "Are you sure you want to delete the group %s and its %d games",
                     name, count );
        if ( GTK_RESPONSE_YES == gtkask( gs->apg->window, msg, GTK_BUTTONS_YES_NO, NULL ) ) {
            gmgr_deleteGroup( dutil, NULL_XWE, gs->grp );
        }
    }
}

static void
addGroupMenuItem(GtkWidget* menu, const gchar* label,
                 GCallback proc, void* closure )
{
    GtkWidget* item = gtk_menu_item_new_with_label( label );
    gtk_widget_show(item);
    g_signal_connect( item, "activate", G_CALLBACK(proc), closure );
    gtk_menu_shell_append( GTK_MENU_SHELL(menu), item );
}

static GtkWidget*
mkGroupHeader( GtkAppGlobals* apg, GroupRef grp )
{
    LaunchParams* params = apg->cag.params;
    GroupState* gs = g_malloc0( sizeof(*gs) );
    gs->apg = apg;
    gs->params = params;
    gs->grp = grp;

    GtkWidget* hbox = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 0 );

    XP_U32 nGames = gmgr_getGroupGamesCount( params->dutil, NULL_XWE, grp );
    XP_UCHAR name[32];
    gmgr_getGroupName( params->dutil, NULL_XWE, grp, name, VSIZE(name) );
    GroupRef dflt = gmgr_getDefaultGroup( params->dutil );
    XP_Bool isDefault = dflt == grp;
    gchar all[128];
    XP_SNPRINTF( all, VSIZE(all), "Group: %s; nGames: %d; grp: %d; dflt: %s ",
                 name, nGames, grp, isDefault?"true":"false" );
    GtkWidget* label = gtk_label_new( all );

    gtk_container_add( GTK_CONTAINER(hbox), label );
    gtk_widget_show( label );

    GtkWidget* menuButton = gtk_menu_button_new();
    GtkWidget* menu = gtk_menu_new();
    addGroupMenuItem( menu, "Make default", (GCallback)mkDefaultGroupProc, gs );
    addGroupMenuItem( menu, "Add game", (GCallback)addToGroupProc, gs );
    addGroupMenuItem( menu, "Rename group", (GCallback)renameGroupProc, gs );
    addGroupMenuItem( menu, "Delete group", (GCallback)deleteGroupProc, gs );
    addGroupMenuItem( menu, "Move group up", (GCallback)upGroupProc, gs );
    addGroupMenuItem( menu, "Move group down", (GCallback)downGroupProc, gs );
    addGroupMenuItem( menu, "Set group sort order", (GCallback)sortGroupProc, gs );
    gtk_menu_button_set_popup( GTK_MENU_BUTTON(menuButton), menu );

    gtk_box_pack_end( GTK_BOX(hbox), menuButton, true, true, 0 );
    gtk_widget_show( menuButton );

    GtkWidget* hideCheck = gtk_check_button_new_with_label( "Collapse" );
    gtk_box_pack_end( GTK_BOX(hbox), hideCheck, true, true, 0 );

    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(hideCheck),
                                  gmgr_getGroupCollapsed(params->dutil,
                                                         NULL_XWE, grp) );
    g_signal_connect( hideCheck, "toggled",
                      G_CALLBACK(hideCheckToggled), gs );
    gtk_widget_show( hideCheck );

    gtk_widget_show( hbox );
    return hbox;
}

typedef struct _BuildListData {
    GtkWidget* list;
    GtkAppGlobals* apg;
    GtkWidget* treesBox;
} BuildListData;

static ForEachAct
addOneProc( void* elem, void* closure, XWEnv XP_UNUSED(xwe) )
{
    BuildListData* bld = (BuildListData*)closure;
    GLItemRef ir = (GLItemRef)elem;
    /* XP_LOGFF( "(" GR_FMT ")", ir ); */

    if ( gmgr_isGame( ir ) ) {
        if ( !bld->list ) {
            bld->list = init_games_list( bld->apg );
            gtk_container_add( GTK_CONTAINER(bld->treesBox), bld->list );
            gtk_widget_show( bld->list );
        }
        LaunchParams* params = bld->apg->cag.params;
        bld->list = add_to_list( params, bld->list, gmgr_toGame( ir ),
                                 bld->treesBox, bld->apg );
    } else {
        bld->list = NULL;   /* clear so any additional games will get new list */
        GtkWidget* header = mkGroupHeader( bld->apg, gmgr_toGroup(ir) );
        gtk_container_add( GTK_CONTAINER(bld->treesBox), header );
    }

    return FEA_OK;
}

static void
buildGamesList( GtkAppGlobals* apg )
{
    GtkWidget* treesBox = apg->treesBox;
    LaunchParams* params = apg->cag.params;

    removeAllFrom( treesBox );

    XWArray* positions = gmgr_getPositions( params->dutil, NULL_XWE );
    BuildListData bld = { .apg = apg, .treesBox = treesBox,
    };
    arr_map( positions, NULL_XWE, addOneProc, &bld );
    arr_destroyp( &positions );
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
    (void)createAddItem( netMenu, "Paste URL", (GCallback)handle_getUrl, apg );
    gtk_widget_show( menubar );
    gtk_box_pack_start( GTK_BOX(vbox), menubar, FALSE, TRUE, 0 );

    {
        GtkWidget* hbox = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 0 );
        gtk_widget_show( hbox );
        gtk_container_add( GTK_CONTAINER(vbox), hbox );
        {
            GtkWidget* showNewcheck = gtk_check_button_new_with_label( "Show new games" );
            gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(showNewcheck),
                                          params->showGames );
            g_signal_connect( showNewcheck, "toggled", G_CALLBACK(showGamesToggle),
                              apg );
            gtk_widget_show( showNewcheck );
            gtk_container_add( GTK_CONTAINER(hbox), showNewcheck );
        }
    }

    GtkWidget* swin = gtk_scrolled_window_new( NULL, NULL );
    gboolean expand = TRUE;  // scrollable window gets all extra space
    gtk_box_pack_start( GTK_BOX(vbox), swin, expand, TRUE, 0 );
    gtk_widget_show( swin );

    apg->treesBox = gtk_box_new( GTK_ORIENTATION_VERTICAL, 0 );
    gtk_widget_show( apg->treesBox );
    gtk_container_add( GTK_CONTAINER(swin), apg->treesBox );

    if ( !!params->pDb ) {
        (void)g_idle_add( buildGamesListIdle, apg );
    }

    GtkWidget* hbox = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 0 );
    gtk_widget_show( hbox );
    gtk_container_add( GTK_CONTAINER(vbox), hbox );

    (void)addButton( "New game", hbox, G_CALLBACK(handle_newgame_button), apg );
    (void)addButton( "New group", hbox, G_CALLBACK(handle_newgroup_button), apg );
    apg->renameButton = addButton( "Rename", hbox,
                                   G_CALLBACK(handle_rename_button), apg );
    apg->openButton = addButton( "Open", hbox, 
                                 G_CALLBACK(handle_open_button), apg );
    apg->rematchButton = addButton( "Rematch", hbox, 
                                    G_CALLBACK(handle_rematch_button), apg );
    apg->moveToGroupButton = addButton( "Move to group", hbox,
                                        G_CALLBACK(handle_movetogroup_button), apg );
    apg->archiveButton = addButton( "Archive", hbox,
                                    G_CALLBACK(handle_archive_button), apg );
    apg->deleteButton = addButton( "Delete", hbox, 
                                   G_CALLBACK(handle_delete_button), apg );
    (void)addButton( "Quit", hbox, G_CALLBACK(handle_quit_button), apg );
    updateButtons( apg );

#ifdef XWFEATURE_GAMEREF_CONVERT
    addForConvert( apg, vbox );
#endif

    gtk_widget_show( window );
}

static GtkWidget* 
openDBFile( GtkAppGlobals* apg )
{
    XP_ASSERT(0);
    GtkGameGlobals* globals = calloc( 1, sizeof(*globals) );
    initBoardGlobalsGtk( globals, apg->cag.params, 0 );

    GtkWidget* window = globals->window;
    gtk_widget_show( window );
    return window;
}

static gint
freeGameGlobals( gpointer data )
{
    LOG_FUNC();

    GtkGameGlobals* globals = (GtkGameGlobals*)data;
    CommonGlobals* cGlobals = &globals->cGlobals;
    GtkAppGlobals* apg = (GtkAppGlobals*)cGlobals->params->cag;
    if ( !!apg ) {
        forgetGameGlobals( &apg->cag, cGlobals );
    }
    cg_unref( cGlobals );
    return 0;                   /* don't run again */
}

void
windowDestroyed( GtkGameGlobals* globals )
{
    /* schedule to run after we're back to main loop */
    (void)g_idle_add( freeGameGlobals, globals );
}

static void
onNewData( GtkAppGlobals* apg, GameRef gr )
{
    LaunchParams* params = apg->cag.params;
    add_to_list( params, NULL, gr, apg->treesBox, apg );
}

/* Stuff common to receiving invitations */
static void
gameFromInvite( GtkAppGlobals* apg, const NetLaunchInfo* XP_UNUSED(nli) )
{
#ifdef XWFEATURE_DEVICE_STORES
    XP_ASSERT(0);
    XP_USE(apg);
#else
    LOG_FUNC();

    GtkGameGlobals* globals = calloc( 1, sizeof(*globals) );
    CommonGlobals* cGlobals = &globals->cGlobals;

    LaunchParams* params = apg->cag.params;
    initBoardGlobalsGtk( globals, params, NULL );

    CommsAddrRec selfAddr;
    makeSelfAddress( &selfAddr, params );

    // CommonPrefs* cp = &cGlobals->cp;
    XP_ASSERT(0);
    // gr_makeFromInvite( cGlobals->gameRef, NULL_XWE, nli,
    // &selfAddr, cGlobals->util, cGlobals->draw,
    // cp, &cGlobals->procs );

    linuxSaveGame( cGlobals );
    sqlite3_int64 rowid = cGlobals->rowid;
    freeGlobals( globals );

    open_row( apg, rowid, XP_TRUE );

    LOG_RETURN_VOID();
#endif
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

void
gameGoneGTK( void* closure, const CommsAddrRec* XP_UNUSED(from), XP_U32 gameID )
{
    GtkAppGlobals* apg = (GtkAppGlobals*)closure;
    LaunchParams* params =  apg->cag.params;

    /* Do we have this game locally still? If not, ignore message */
#ifdef XWFEATURE_DEVICE_STORES
    XP_USE(params);
    XP_USE(gameID);
    XP_ASSERT(0);
#else
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
#endif
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
    GameRef gr = gmgr_newFor( params->dutil, NULL_XWE, GROUP_DEFAULT, gi, NULL );
    initBoardGlobalsGtk( globals, params, gr );

    GtkWidget* gameWindow = globals->window;
    XP_ASSERT( globals->cGlobals.gr == gr );
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

    XW_DUtilCtxt* dutil = apg->cag.params->dutil;
    if ( !!globals ) {
        gr_sendChat( dutil, globals->cGlobals.gr, NULL_XWE, msg );
        success = XP_TRUE;
    } else {
        GtkGameGlobals tmpGlobals = {};
        int nRowIDs = 1;
        gdb_getRowsForGameID( apg->cag.params->pDb, gameID, &rowid, &nRowIDs );

        success = 1 == nRowIDs
            && loadGameNoDraw( &tmpGlobals, apg->cag.params, rowid );
        if ( success ) {
            gr_sendChat( dutil, tmpGlobals.cGlobals.gr, NULL_XWE, msg );
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
#ifdef XWFEATURE_DEVICE_STORES
    XP_ASSERT(0);
    XP_USE(closure);
    XP_USE(gameID);
    return NULL;
#else
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
#endif
}

int
gtkmain( LaunchParams* params )
{
    GtkAppGlobals apg = {};
    params->cag = &apg.cag;

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

#ifdef XWFEATURE_SMS
        gchar* myPhone;
        XP_U16 myPort;
        if ( parseSMSParams( params, &myPhone, &myPort ) ) {
            linux_sms_init( params, myPhone, myPort );
        } else {
            XP_LOGF( "not activating SMS: I don't have a phone" );
        }

        if ( params->runSMSTest ) {
            cnk_runTests( params->dutil, NULL_XWE );
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
