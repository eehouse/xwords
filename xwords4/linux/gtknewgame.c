/* -*-mode: C; fill-column: 78; c-basic-offset: 4; compile-command: "make MEMDEBUG=TRUE"; -*- */
/* 
 * Copyright 2001-2008 by Eric House (xwords@eehouse.org).  All rights
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

#include <stdarg.h>

#include "linuxutl.h"
#include "gtknewgame.h"
#include "strutils.h"
#include "nwgamest.h"
#include "gtkconnsdlg.h"
#include "gtkutils.h"

#define MAX_SIZE_CHOICES 10

typedef struct GtkNewGameState {
    GtkAppGlobals* globals;
    NewGameCtx* newGameCtxt;

    CommsAddrRec addr;

    DeviceRole role;
    gboolean revert;
    gboolean cancelled;
    XP_Bool loaded;
    XP_Bool fireConnDlg;
    gboolean isNewGame;
    short nCols;                /* for board size */

#ifndef XWFEATURE_STANDALONE_ONLY
    GtkWidget* remoteChecks[MAX_NUM_PLAYERS];
    GtkWidget* roleCombo;
    GtkWidget* settingsButton;
#endif
    GtkWidget* robotChecks[MAX_NUM_PLAYERS];
    GtkWidget* nameLabels[MAX_NUM_PLAYERS];
    GtkWidget* nameFields[MAX_NUM_PLAYERS];
    GtkWidget* passwdLabels[MAX_NUM_PLAYERS];
    GtkWidget* passwdFields[MAX_NUM_PLAYERS];
    GtkWidget* nPlayersCombo;
    GtkWidget* nPlayersLabel;
    GtkWidget* juggleButton;
} GtkNewGameState;

static void
nplayers_menu_changed( GtkComboBox* combo, GtkNewGameState* state )
{
    gint index = gtk_combo_box_get_active( GTK_COMBO_BOX(combo) );
    if ( index >= 0 ) {
        NGValue value = { .ng_u16 = index + 1 };
        newg_attrChanged( state->newGameCtxt, NG_ATTR_NPLAYERS, value );
    }
} /* nplayers_menu_changed */

#ifndef XWFEATURE_STANDALONE_ONLY
static void
role_combo_changed( GtkComboBox* combo, gpointer gp )
{
    GtkNewGameState* state = (GtkNewGameState*)gp;
    NGValue value;
    gint index = gtk_combo_box_get_active( GTK_COMBO_BOX(combo) );
 
    if ( index >= 0 ) {
        DeviceRole role = (DeviceRole)index;
        value.ng_role = role;
 
        if ( state->isNewGame ) {
            newg_attrChanged( state->newGameCtxt, NG_ATTR_ROLE, value );
        } else if ( state->loaded ) {
            /* put it back */
            gtk_combo_box_set_active( GTK_COMBO_BOX(combo), state->role );
        }

#ifndef XWFEATURE_STANDALONE_ONLY
        if ( state->loaded && SERVER_STANDALONE != role  ) {
            gtkConnsDlg( state->globals, &state->addr, role,
                         !state->isNewGame );
        }
#endif
    }
}

static void
handle_settings( GtkWidget* XP_UNUSED(item), GtkNewGameState* state )
{
    gtkConnsDlg( state->globals, &state->addr, state->role, !state->isNewGame );
}

#endif

static void
callChangedWithIndex( GtkNewGameState* state, GtkWidget* item, 
                      GtkWidget** items  )
{
    NGValue value;
    gint player;
    for ( player = 0; player < MAX_NUM_PLAYERS; ++player ) {
        if ( item == items[player] ) {
            break;
        }
    }
    XP_ASSERT( player < MAX_NUM_PLAYERS );

    value.ng_bool = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(item) );
    newg_colChanged( state->newGameCtxt, player );
} /* callChangedWithIndex */

static void
handle_robot_toggled( GtkWidget* item, GtkNewGameState* state )
{
    callChangedWithIndex( state, item, state->robotChecks );
}

static void
handle_juggle( GtkWidget* XP_UNUSED(item), GtkNewGameState* state )
{
    while ( !newg_juggle( state->newGameCtxt ) ) {
    }
}

#ifndef XWFEATURE_STANDALONE_ONLY
static void
handle_remote_toggled( GtkWidget* item, GtkNewGameState* state )
{
    callChangedWithIndex( state, item, state->remoteChecks );
}
#endif

static void
size_combo_changed( GtkComboBox* combo, gpointer gp )
{
    GtkNewGameState* state = (GtkNewGameState*)gp;
    gint index = gtk_combo_box_get_active( GTK_COMBO_BOX(combo) );
    if ( index >= 0 ) {
        state->nCols = MAX_COLS - index;
        XP_LOGF( "set nCols = %d", state->nCols );
    }
} /* size_combo_changed  */

static void
handle_ok( GtkWidget* XP_UNUSED(widget), gpointer closure )
{
    GtkNewGameState* state = (GtkNewGameState*)closure;
    state->cancelled = XP_FALSE;

    gtk_main_quit();
} /* handle_ok */

static void
handle_cancel( GtkWidget* XP_UNUSED(widget), void* closure )
{
    GtkNewGameState* state = (GtkNewGameState*)closure;
    state->cancelled = XP_TRUE;
    gtk_main_quit();
}

static void
handle_revert( GtkWidget* XP_UNUSED(widget), void* closure )
{
    GtkNewGameState* state = (GtkNewGameState*)closure;
    state->revert = TRUE;
    gtk_main_quit();
} /* handle_revert */

static gint
call_connsdlg_func( gpointer data )
{
    GtkNewGameState* state = (GtkNewGameState*)data;
    gtkConnsDlg( state->globals, &state->addr, state->role,
                 !state->isNewGame );
    return 0;
}

static GtkWidget*
makeNewGameDialog( GtkNewGameState* state )
{
    GtkWidget* dialog;
    GtkWidget* vbox;
    GtkWidget* hbox;
#ifndef XWFEATURE_STANDALONE_ONLY
    GtkWidget* roleCombo;
    char* roles[] = { "Standalone", "Host", "Guest" };
#endif
    GtkWidget* nPlayersCombo;
    GtkWidget* boardSizeCombo;
    CurGameInfo* gi;
    short ii;

    dialog = gtk_dialog_new();
    gtk_window_set_modal( GTK_WINDOW( dialog ), TRUE );

    vbox = gtk_vbox_new( FALSE, 0 );

#ifndef XWFEATURE_STANDALONE_ONLY
    hbox = gtk_hbox_new( FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(hbox), gtk_label_new("Role:"),
                        FALSE, TRUE, 0 );
    roleCombo = gtk_combo_box_new_text();
    state->roleCombo = roleCombo;

    for ( ii = 0; ii < VSIZE(roles); ++ii ) {
        gtk_combo_box_append_text( GTK_COMBO_BOX(roleCombo), roles[ii] );
    }
    gtk_box_pack_start( GTK_BOX(hbox), roleCombo, FALSE, TRUE, 0 );
    g_signal_connect( GTK_OBJECT(roleCombo), "changed", 
                      G_CALLBACK(role_combo_changed), state );

    state->settingsButton = makeButton( "Settings...", 
                                        GTK_SIGNAL_FUNC(handle_settings),
                                        state );
    gtk_box_pack_start( GTK_BOX(hbox), state->settingsButton, FALSE, TRUE, 0 );

    gtk_box_pack_start( GTK_BOX(vbox), hbox, FALSE, TRUE, 0 );
#endif

    /* NPlayers menu */
    hbox = gtk_hbox_new( FALSE, 0 );
    state->nPlayersLabel = gtk_label_new("");
    gtk_box_pack_start( GTK_BOX(hbox), state->nPlayersLabel, FALSE, TRUE, 0 );

    nPlayersCombo = gtk_combo_box_new_text();
    state->nPlayersCombo = nPlayersCombo;

    gi = &state->globals->cGlobals.params->gi;

    for ( ii = 0; ii < MAX_NUM_PLAYERS; ++ii ) {
        char buf[2] = { ii + '1', '\0' };
        gtk_combo_box_append_text( GTK_COMBO_BOX(nPlayersCombo), buf );
    }

    gtk_widget_show( nPlayersCombo );
    gtk_box_pack_start( GTK_BOX(hbox), nPlayersCombo, FALSE, TRUE, 0 );
    g_signal_connect( GTK_OBJECT(nPlayersCombo), "changed", 
                      G_CALLBACK(nplayers_menu_changed), state );

    state->juggleButton = makeButton( "Juggle", 
                                      GTK_SIGNAL_FUNC(handle_juggle),
                                      state );
    gtk_box_pack_start( GTK_BOX(hbox), state->juggleButton, FALSE, TRUE, 0 );
    gtk_widget_show( hbox );

    gtk_box_pack_start( GTK_BOX(vbox), hbox, FALSE, TRUE, 0 );

    for ( ii = 0; ii < MAX_NUM_PLAYERS; ++ii ) {
        GtkWidget* label = gtk_label_new("Name:");
#ifndef XWFEATURE_STANDALONE_ONLY
        GtkWidget* remoteCheck = gtk_check_button_new_with_label( "Remote" );
#endif
        GtkWidget* nameField = gtk_entry_new();
        GtkWidget* passwdField = gtk_entry_new_with_max_length( 6 );
        GtkWidget* robotCheck = gtk_check_button_new_with_label( "Robot" );

#ifndef XWFEATURE_STANDALONE_ONLY
        g_signal_connect( GTK_OBJECT(remoteCheck), "toggled", 
                          GTK_SIGNAL_FUNC(handle_remote_toggled), state );
#endif
        g_signal_connect( GTK_OBJECT(robotCheck), "toggled", 
                          GTK_SIGNAL_FUNC(handle_robot_toggled), state );

        hbox = gtk_hbox_new( FALSE, 0 );

#ifndef XWFEATURE_STANDALONE_ONLY
        gtk_box_pack_start( GTK_BOX(hbox), remoteCheck, FALSE, TRUE, 0 );
        gtk_widget_show( remoteCheck );
        state->remoteChecks[ii] = remoteCheck;
#endif
        
        gtk_box_pack_start( GTK_BOX(hbox), label, FALSE, TRUE, 0 );
        gtk_widget_show( label );
        state->nameLabels[ii] = label;

        gtk_box_pack_start( GTK_BOX(hbox), nameField, FALSE, TRUE, 0 );
        gtk_widget_show( nameField );
        state->nameFields[ii] = nameField;

        gtk_box_pack_start( GTK_BOX(hbox), robotCheck, FALSE, TRUE, 0 );
        gtk_widget_show( robotCheck );
        state->robotChecks[ii] = robotCheck;

        label = gtk_label_new("Passwd:");
        gtk_box_pack_start( GTK_BOX(hbox), label, FALSE, TRUE, 0 );
        gtk_widget_show( label );
        state->passwdLabels[ii] = label;

        gtk_box_pack_start( GTK_BOX(hbox), passwdField, FALSE, TRUE, 0 );
        gtk_widget_show( passwdField );
        state->passwdFields[ii] = passwdField;

        gtk_box_pack_start( GTK_BOX(vbox), hbox, FALSE, TRUE, 0 );
        gtk_widget_show( hbox );
    }

    /* board size choices */
    hbox = gtk_hbox_new( FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(hbox), gtk_label_new("Board size"),
                        FALSE, TRUE, 0 );

    boardSizeCombo = gtk_combo_box_new_text();
    if ( !state->isNewGame ) {
        gtk_widget_set_sensitive( boardSizeCombo, FALSE );
    }

    for ( ii = 0; ii < MAX_SIZE_CHOICES; ++ii ) {
        char buf[10];
        XP_U16 siz = MAX_COLS - ii;
        snprintf( buf, sizeof(buf), "%dx%d", siz, siz );
        gtk_combo_box_append_text( GTK_COMBO_BOX(boardSizeCombo), buf );
        if ( siz == state->nCols ) {
            gtk_combo_box_set_active( GTK_COMBO_BOX(boardSizeCombo), ii );
        }
    }

    g_signal_connect( GTK_OBJECT(boardSizeCombo), "changed", 
                      G_CALLBACK(size_combo_changed), state );

    gtk_widget_show( boardSizeCombo );
    gtk_box_pack_start( GTK_BOX(hbox), boardSizeCombo, FALSE, TRUE, 0 );

    gtk_box_pack_start( GTK_BOX(hbox), gtk_label_new("Dictionary: "),
                        FALSE, TRUE, 0 );

    if ( !!gi->dictName ) {
        gtk_box_pack_start( GTK_BOX(hbox), 
                            gtk_label_new(gi->dictName),
                            FALSE, TRUE, 0 );
    }

    gtk_widget_show( hbox );

    gtk_box_pack_start( GTK_BOX(vbox), hbox, FALSE, TRUE, 0 );

    /* buttons at the bottom */
    hbox = gtk_hbox_new( FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(hbox), 
                        makeButton( "Ok", GTK_SIGNAL_FUNC(handle_ok) , state ),
                        FALSE, TRUE, 0 );
    if ( state->isNewGame ) {
        gtk_box_pack_start( GTK_BOX(hbox), 
                            makeButton( "Revert", 
                                        GTK_SIGNAL_FUNC(handle_revert),
                                        state ),
                            FALSE, TRUE, 0 );
        gtk_box_pack_start( GTK_BOX(hbox), 
                            makeButton( "Cancel", 
                                        GTK_SIGNAL_FUNC(handle_cancel), 
                                        state ),
                            FALSE, TRUE, 0 );
    }
    gtk_box_pack_start( GTK_BOX(vbox), hbox, FALSE, TRUE, 0 );

    gtk_widget_show( vbox );
    gtk_container_add( GTK_CONTAINER( GTK_DIALOG(dialog)->action_area), vbox);

    gtk_widget_show_all (dialog);

    if ( state->fireConnDlg ) {
        (void)g_idle_add( call_connsdlg_func, state );
    }

    return dialog;
} /* makeNewGameDialog */

static GtkWidget*
widgetForCol( const GtkNewGameState* state, XP_U16 player, NewGameColumn col )
{
    GtkWidget* widget = NULL;
    if ( col == NG_COL_NAME ) {
        widget = state->nameFields[player];
    } else if ( col == NG_COL_PASSWD ) {
        widget = state->passwdFields[player];
#ifndef XWFEATURE_STANDALONE_ONLY
    } else if ( col == NG_COL_REMOTE ) {
        widget = state->remoteChecks[player];
#endif
    } else if ( col == NG_COL_ROBOT ) {
        widget = state->robotChecks[player];
    } 
    XP_ASSERT( !!widget );
    return widget;
} /* widgetForCol */

static GtkWidget*
labelForCol( const GtkNewGameState* state, XP_U16 player, NewGameColumn col )
{
    GtkWidget* widget = NULL;
    if ( col == NG_COL_NAME ) {
        widget = state->nameLabels[player];
    } else if ( col == NG_COL_PASSWD ) {
        widget = state->passwdLabels[player];
    } 
    return widget;
} /* labelForCol */

static void
gtk_newgame_col_enable( void* closure, XP_U16 player, NewGameColumn col, 
                        XP_TriEnable enable )
{
    GtkNewGameState* state = (GtkNewGameState*)closure;
    GtkWidget* widget = widgetForCol( state, player, col );
    GtkWidget* label = labelForCol( state, player, col );

    gtk_widget_show( widget );
    gtk_widget_set_sensitive( widget, enable == TRI_ENAB_ENABLED );
    if ( !!label ) {
        gtk_widget_show( label );
        gtk_widget_set_sensitive( label, enable == TRI_ENAB_ENABLED );
    }
} /* gtk_newgame_col_enable */

static void
gtk_newgame_attr_enable( void* closure, NewGameAttr attr, XP_TriEnable enable )
{
    GtkNewGameState* state = (GtkNewGameState*)closure;
    GtkWidget* widget = NULL;
    if ( attr == NG_ATTR_NPLAYERS ) {
        widget = state->nPlayersCombo;
#ifndef XWFEATURE_STANDALONE_ONLY
    } else if ( attr == NG_ATTR_CANCONFIG ) {
        widget = state->settingsButton;
    } else if ( attr == NG_ATTR_ROLE ) {
        /* NG_ATTR_ROLE always enabled */
/*         widget = state->roleCombo; */
#endif
    } else if ( attr == NG_ATTR_CANJUGGLE ) {
        widget = state->juggleButton;
    }

    if ( !!widget ) {
        gtk_widget_set_sensitive( widget, enable == TRI_ENAB_ENABLED );
    }
}

static void
gtk_newgame_col_set( void* closure, XP_U16 player, NewGameColumn col, 
                     NGValue value )
{
    GtkNewGameState* state = (GtkNewGameState*)closure;
    GtkWidget* widget = widgetForCol( state, player, col );
    const XP_UCHAR* cp;

    switch ( col ) {
    case NG_COL_NAME:
    case NG_COL_PASSWD:
        cp = value.ng_cp? value.ng_cp : "";
        gtk_entry_set_text( GTK_ENTRY(widget), cp );
        break;
#ifndef XWFEATURE_STANDALONE_ONLY
    case NG_COL_REMOTE:
#endif
    case NG_COL_ROBOT:
        gtk_toggle_button_set_state( GTK_TOGGLE_BUTTON(widget),
                                     value.ng_bool );
        break;
    }
} /* gtk_newgame_set */

static void
gtk_newgame_col_get( void* closure, XP_U16 player, NewGameColumn col, 
                     NgCpCallbk cpcb, const void* cbClosure )
{
    GtkNewGameState* state = (GtkNewGameState*)closure;
    NGValue value;

    GtkWidget* widget = widgetForCol( state, player, col );
    switch ( col ) {
#ifndef XWFEATURE_STANDALONE_ONLY        
    case NG_COL_REMOTE:
#endif
    case NG_COL_ROBOT:
        value.ng_bool =
            gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(widget));
        break;
    case NG_COL_PASSWD:
    case NG_COL_NAME:
        value.ng_cp = gtk_entry_get_text( GTK_ENTRY(widget) );
        break;
    }

    (*cpcb)( value, cbClosure );
} /* gtk_newgame_col_get */

static void
gtk_newgame_attr_set( void* closure, NewGameAttr attr, NGValue value )
{
    GtkNewGameState* state = (GtkNewGameState*)closure;
    if ( attr == NG_ATTR_NPLAYERS ) {
        XP_U16 ii = value.ng_u16;
        XP_LOGF( "%s: setting menu %d", __func__, ii-1 );
        gtk_combo_box_set_active( GTK_COMBO_BOX(state->nPlayersCombo), ii-1 );
#ifndef XWFEATURE_STANDALONE_ONLY
    } else if ( attr == NG_ATTR_ROLE ) {
        gtk_combo_box_set_active( GTK_COMBO_BOX(state->roleCombo), 
                                  value.ng_role );
    } else if ( attr == NG_ATTR_REMHEADER ) {
        /* ignored on GTK: no headers at all */
#endif
    } else if ( attr == NG_ATTR_NPLAYHEADER ) {
        gtk_label_set_text( GTK_LABEL(state->nPlayersLabel), value.ng_cp );
    }
}

gboolean
newGameDialog( GtkAppGlobals* globals, CommsAddrRec* addr, XP_Bool isNewGame,
               XP_Bool fireConnDlg )
{
    GtkNewGameState state;
    XP_MEMSET( &state, 0, sizeof(state) );

    state.globals = globals;
    state.newGameCtxt = newg_make( MPPARM(globals->cGlobals.params
                                          ->util->mpool) 
                                   isNewGame,
                                   globals->cGlobals.params->util,
                                   gtk_newgame_col_enable,
                                   gtk_newgame_attr_enable,
                                   gtk_newgame_col_get,
                                   gtk_newgame_col_set,
                                   gtk_newgame_attr_set,
                                   &state );
    state.isNewGame = isNewGame;
    state.fireConnDlg = fireConnDlg;

    /* returns when button handler calls gtk_main_quit */
    do {
        GtkWidget* dialog;

        state.revert = FALSE;
        state.loaded = XP_FALSE;
        state.nCols = globals->cGlobals.params->gi.boardSize;
        state.role = globals->cGlobals.params->gi.serverRole;

        XP_MEMCPY( &state.addr, addr, sizeof(state.addr) );

        dialog = makeNewGameDialog( &state );

        newg_load( state.newGameCtxt, 
                   &globals->cGlobals.params->gi );
        state.loaded = XP_TRUE;

        gtk_main();
        if ( !state.cancelled && !state.revert ) {
            if ( newg_store( state.newGameCtxt, &globals->cGlobals.params->gi,
                             XP_TRUE ) ) {
                globals->cGlobals.params->gi.boardSize = state.nCols;
            } else {
                /* Do it again if we warned user of inconsistency. */
                state.revert = XP_TRUE;
            }
        }

        gtk_widget_destroy( dialog );
        state.fireConnDlg = XP_FALSE;
    } while ( state.revert );

    newg_destroy( state.newGameCtxt );

    if ( !state.cancelled ) {
        XP_MEMCPY( addr, &state.addr, sizeof(state.addr) );
    }
    return !state.cancelled;
} /* newGameDialog */

#endif /* PLATFORM_GTK */
