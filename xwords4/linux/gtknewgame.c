/* -*-mode: C; fill-column: 78; c-basic-offset: 4; compile-command: "make MEMDEBUG=TRUE"; -*- */
/* 
 * Copyright 2001-2006 by Eric House (xwords@eehouse.org).  All rights
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

#include "gtknewgame.h"
#include "strutils.h"
#include "nwgamest.h"

#define MAX_SIZE_CHOICES 10

typedef struct GtkNewGameState {
    GtkAppGlobals* globals;
    NewGameCtx* newGameCtxt;

    gboolean revert;
    gboolean cancelled;
    short nCols;                /* for board size */

#ifndef XWFEATURE_STANDALONE_ONLY
    GtkWidget* remoteChecks[MAX_NUM_PLAYERS];
#endif
    GtkWidget* robotChecks[MAX_NUM_PLAYERS];
    GtkWidget* nameLabels[MAX_NUM_PLAYERS];
    GtkWidget* nameFields[MAX_NUM_PLAYERS];
    GtkWidget* passwdLabels[MAX_NUM_PLAYERS];
    GtkWidget* passwdFields[MAX_NUM_PLAYERS];
    GtkWidget* nPlayersCombo;
    GtkWidget* roleCombo;
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

static void
role_combo_changed( GtkComboBox* combo, gpointer gp )
{
    NGValue value;
    GtkNewGameState* state = (GtkNewGameState*)gp;
    gint index = gtk_combo_box_get_active( GTK_COMBO_BOX(combo) );
    if ( index >= 0 ) {    
        value.ng_role = (DeviceRole)index;
        newg_attrChanged( state->newGameCtxt, NG_ATTR_ROLE, value );
    }
} /* role_combo_changed */

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

static GtkWidget*
makeButton( char* text, GCallback func, gpointer data )
{
    GtkWidget* button = gtk_button_new_with_label( text );
    g_signal_connect( GTK_OBJECT(button), "clicked", func, data );
    gtk_widget_show( button );

    return button;
} /* makeButton */

static GtkWidget*
makeNewGameDialog( GtkNewGameState* state, XP_Bool isNewGame )
{
    GtkWidget* dialog;
    GtkWidget* vbox;
    GtkWidget* hbox;
    GtkWidget* roleCombo;
    GtkWidget* nPlayersCombo;
    GtkWidget* boardSizeCombo;
    CurGameInfo* gi;
    short i;
    char* roles[] = { "Standalone", "Host", "Guest" };

    dialog = gtk_dialog_new();
    gtk_window_set_modal( GTK_WINDOW( dialog ), TRUE );

    vbox = gtk_vbox_new( FALSE, 0 );

    hbox = gtk_hbox_new( FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(hbox), gtk_label_new("Role:"),
                        FALSE, TRUE, 0 );
    roleCombo = gtk_combo_box_new_text();
    state->roleCombo = roleCombo;

    for ( i = 0; i < sizeof(roles)/sizeof(roles[0]); ++i ) {
        gtk_combo_box_append_text( GTK_COMBO_BOX(roleCombo), roles[i] );
    }
    g_signal_connect( GTK_OBJECT(roleCombo), "changed", 
                      G_CALLBACK(role_combo_changed), state );

    gtk_box_pack_start( GTK_BOX(hbox), roleCombo, FALSE, TRUE, 0 );
    gtk_box_pack_start( GTK_BOX(vbox), hbox, FALSE, TRUE, 0 );

    /* NPlayers menu */
    hbox = gtk_hbox_new( FALSE, 0 );
    state->nPlayersLabel = gtk_label_new("");
    gtk_box_pack_start( GTK_BOX(hbox), state->nPlayersLabel, FALSE, TRUE, 0 );

    nPlayersCombo = gtk_combo_box_new_text();
    state->nPlayersCombo = nPlayersCombo;

    gi = &state->globals->cGlobals.params->gi;

    for ( i = 0; i < MAX_NUM_PLAYERS; ++i ) {
        char buf[2] = { i + '1', '\0' };
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

    for ( i = 0; i < MAX_NUM_PLAYERS; ++i ) {
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
        state->remoteChecks[i] = remoteCheck;
#endif
        
        gtk_box_pack_start( GTK_BOX(hbox), label, FALSE, TRUE, 0 );
        gtk_widget_show( label );
        state->nameLabels[i] = label;

        gtk_box_pack_start( GTK_BOX(hbox), nameField, FALSE, TRUE, 0 );
        gtk_widget_show( nameField );
        state->nameFields[i] = nameField;

        gtk_box_pack_start( GTK_BOX(hbox), robotCheck, FALSE, TRUE, 0 );
        gtk_widget_show( robotCheck );
        state->robotChecks[i] = robotCheck;

        label = gtk_label_new("Passwd:");
        gtk_box_pack_start( GTK_BOX(hbox), label, FALSE, TRUE, 0 );
        gtk_widget_show( label );
        state->passwdLabels[i] = label;

        gtk_box_pack_start( GTK_BOX(hbox), passwdField, FALSE, TRUE, 0 );
        gtk_widget_show( passwdField );
        state->passwdFields[i] = passwdField;

        gtk_box_pack_start( GTK_BOX(vbox), hbox, FALSE, TRUE, 0 );
        gtk_widget_show( hbox );
    }

    /* board size choices */
    hbox = gtk_hbox_new( FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(hbox), gtk_label_new("Board size"),
                        FALSE, TRUE, 0 );

    boardSizeCombo = gtk_combo_box_new_text();
    if ( !isNewGame ) {
        gtk_widget_set_sensitive( boardSizeCombo, FALSE );
    }

    for ( i = 0; i < MAX_SIZE_CHOICES; ++i ) {
        char buf[10];
        XP_U16 siz = MAX_COLS - i;
        snprintf( buf, sizeof(buf), "%dx%d", siz, siz );
        gtk_combo_box_append_text( GTK_COMBO_BOX(boardSizeCombo), buf );
        if ( siz == state->nCols ) {
            gtk_combo_box_set_active( GTK_COMBO_BOX(boardSizeCombo), i );
        }
    }

    g_signal_connect( GTK_OBJECT(boardSizeCombo), "changed", 
                      G_CALLBACK(size_combo_changed), state );

    gtk_widget_show( boardSizeCombo );
    gtk_box_pack_start( GTK_BOX(hbox), boardSizeCombo, FALSE, TRUE, 0 );

    gtk_box_pack_start( GTK_BOX(hbox), gtk_label_new("Dictionary: "),
                        FALSE, TRUE, 0 );

    XP_ASSERT( gi->dictName );
    gtk_box_pack_start( GTK_BOX(hbox), 
                        gtk_label_new(gi->dictName),
                        FALSE, TRUE, 0 );

    gtk_widget_show( hbox );

    gtk_box_pack_start( GTK_BOX(vbox), hbox, FALSE, TRUE, 0 );

    /* buttons at the bottom */
    hbox = gtk_hbox_new( FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(hbox), 
                        makeButton( "Ok", GTK_SIGNAL_FUNC(handle_ok) , state ),
                        FALSE, TRUE, 0 );
    gtk_box_pack_start( GTK_BOX(hbox), 
                        makeButton( "Revert", GTK_SIGNAL_FUNC(handle_revert),
                                    state ),
                        FALSE, TRUE, 0 );
    gtk_box_pack_start( GTK_BOX(hbox), 
                        makeButton( "Cancel", GTK_SIGNAL_FUNC(handle_cancel), 
                                    state ),
                        FALSE, TRUE, 0 );
    gtk_box_pack_start( GTK_BOX(vbox), hbox, FALSE, TRUE, 0 );


    gtk_widget_show( vbox );
    gtk_container_add( GTK_CONTAINER( GTK_DIALOG(dialog)->action_area), vbox);

    gtk_widget_show_all (dialog);

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
} /* widgetForCol */

static void
gtk_newgame_col_enable( void* closure, XP_U16 player, NewGameColumn col, 
                        XP_TriEnable enable )
{
    GtkNewGameState* state = (GtkNewGameState*)closure;
    GtkWidget* widget = widgetForCol( state, player, col );
    GtkWidget* label = labelForCol( state, player, col );

    if ( enable == TRI_ENAB_HIDDEN ) {
        gtk_widget_hide( widget );
        if ( !!label ) {
            gtk_widget_hide( label );
        }
    } else {
        gtk_widget_show( widget );
        gtk_widget_set_sensitive( widget, enable == TRI_ENAB_ENABLED );
        if ( !!label ) {
            gtk_widget_show( label );
        gtk_widget_set_sensitive( label, enable == TRI_ENAB_ENABLED );
        }
    }
} /* gtk_newgame_col_enable */

static void
gtk_newgame_attr_enable( void* closure, NewGameAttr attr, XP_TriEnable enable )
{
    GtkNewGameState* state = (GtkNewGameState*)closure;
    GtkWidget* widget = NULL;
    if ( attr == NG_ATTR_NPLAYERS ) {
        widget = state->nPlayersCombo;
    } else if ( attr == NG_ATTR_ROLE ) {
        widget = state->roleCombo;
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
        XP_U16 i = value.ng_u16;
        XP_LOGF( "%s: setting menu %d", __FUNCTION__, i-1 );
        gtk_combo_box_set_active( GTK_COMBO_BOX(state->nPlayersCombo), i-1 );
    } else if ( attr == NG_ATTR_ROLE ) {
        gtk_combo_box_set_active( GTK_COMBO_BOX(state->roleCombo), 
                                  value.ng_role );
    } else if ( attr == NG_ATTR_REMHEADER ) {
        /* ignored on GTK: no headers at all */
    } else if ( attr == NG_ATTR_NPLAYHEADER ) {
        gtk_label_set_text( GTK_LABEL(state->nPlayersLabel), value.ng_cp );
    }
}

gboolean
newGameDialog( GtkAppGlobals* globals, XP_Bool isNewGame )
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

    /* returns when button handler calls gtk_main_quit */
    do {
        GtkWidget* dialog;

        state.revert = FALSE;
        state.nCols = globals->cGlobals.params->gi.boardSize;

        dialog = makeNewGameDialog( &state, isNewGame );

        newg_load( state.newGameCtxt, 
                   &globals->cGlobals.params->gi );

        gtk_main();
        if ( !state.cancelled && !state.revert ) {
            newg_store( state.newGameCtxt, &globals->cGlobals.params->gi );
            globals->cGlobals.params->gi.boardSize = state.nCols;
        }

        gtk_widget_destroy( dialog );
    } while ( state.revert );

    newg_destroy( state.newGameCtxt );

    return !state.cancelled;
} /* newGameDialog */

#endif /* PLATFORM_GTK */
