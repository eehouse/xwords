/* -*- compile-command: "make MEMDEBUG=TRUE -j3"; -*- */
/* 
 * Copyright 2001 - 2021 by Eric House (xwords@eehouse.org).  All rights
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
#include "linuxmain.h"
#include "gtknewgame.h"
#include "strutils.h"
#include "nwgamest.h"
#include "gtkconnsdlg.h"
#include "gtkutils.h"
#include "gtkask.h"

#define MIN_BOARD_SIZE 11
#define MAX_BOARD_SIZE 23

#define BINGO_THRESHOLD "Bingo threshold"
#define TRAY_SIZE "Tray size"

typedef struct GtkNewGameState {
    GtkGameGlobals* globals;
    CurGameInfo* gi;
    NewGameCtx* newGameCtxt;

    CommsAddrRec addr;

    DeviceRole role;
    XWPhoniesChoice phoniesAction;
    gboolean revert;
    gboolean cancelled;
    XP_Bool loaded;
    XP_Bool fireConnDlg;
    gboolean isNewGame;
    short nCols;                /* for board size */
    int nTrayTiles;
    int bingoMin;
    gchar* dict;

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
    GtkWidget* timerField;
    GtkWidget* duplicateCheck;
} GtkNewGameState;

static void
nplayers_menu_changed( GtkComboBox* combo, GtkNewGameState* state )
{
    gint index = gtk_combo_box_get_active( GTK_COMBO_BOX(combo) );
    if ( index >= 0 ) {
        NGValue value = { .ng_u16 = index + 1 };
        newg_attrChanged( state->newGameCtxt, NULL_XWE, NG_ATTR_NPLAYERS, value );
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
            newg_attrChanged( state->newGameCtxt, NULL_XWE, NG_ATTR_ROLE, value );
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
phonies_combo_changed( GtkComboBox* combo, gpointer gp )
{
    GtkNewGameState* state = (GtkNewGameState*)gp;
    gint index = gtk_combo_box_get_active( combo );
    state->phoniesAction = (XWPhoniesChoice)index;
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
    gint player;
    for ( player = 0; player < MAX_NUM_PLAYERS; ++player ) {
        if ( item == items[player] ) {
            break;
        }
    }
    XP_ASSERT( player < MAX_NUM_PLAYERS );

    (void)gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(item) );
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
        state->nCols = MIN_BOARD_SIZE + (index * 2);
        XP_LOGF( "set nCols = %d", state->nCols );
    }
} /* size_combo_changed  */

static void
tray_size_changed( GtkComboBox* combo, gpointer gp )
{
    GtkNewGameState* state = (GtkNewGameState*)gp;
    gint index = gtk_combo_box_get_active( GTK_COMBO_BOX(combo) );
    if ( index >= 0 ) {
        state->nTrayTiles = 7 + index;
    }
}

static void
bingo_changed( GtkComboBox* combo, gpointer gp )
{
    GtkNewGameState* state = (GtkNewGameState*)gp;
    gint index = gtk_combo_box_get_active( GTK_COMBO_BOX(combo) );
    if ( index >= 0 ) {
        state->bingoMin = 7 + index;
    }
}

static void
dict_combo_changed( GtkComboBox* combo, gpointer gp )
{
    GtkNewGameState* state = (GtkNewGameState*)gp;
    state->dict =
        gtk_combo_box_text_get_active_text( GTK_COMBO_BOX_TEXT( combo ) );
    XP_LOGF( "got dict: %s", state->dict );
} /* dict_combo_changed  */

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

static void
addPhoniesCombo( GtkNewGameState* state, GtkWidget* parent )
{
    GtkWidget* hbox = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 0 );
    gtk_box_pack_start( GTK_BOX(hbox), gtk_label_new("Phonies"),
                        FALSE, TRUE, 0 );
    GtkWidget* phoniesCombo = gtk_combo_box_text_new();

    const char* ptxts[] = { "IGNORE", "WARN", "LOSE TURN", "BLOCK" };

    for ( int ii = 0; ii < VSIZE(ptxts); ++ii ) {
        gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT(phoniesCombo),
                                        ptxts[ii] );
    }

    g_signal_connect( phoniesCombo, "changed",
                      G_CALLBACK(phonies_combo_changed), state );
    XWPhoniesChoice startChoice = state->globals->cGlobals.params->pgi.phoniesAction;
    gtk_combo_box_set_active( GTK_COMBO_BOX(phoniesCombo), startChoice );
    gtk_widget_show( phoniesCombo );
    gtk_box_pack_start( GTK_BOX(hbox), phoniesCombo, FALSE, TRUE, 0 );
    gtk_widget_show( hbox );

    gtk_box_pack_start( GTK_BOX(parent), hbox, FALSE, TRUE, 0 );
}

static void
on_timer_changed( GtkEditable *editable, gpointer data )
{
    GtkNewGameState* state = (GtkNewGameState*)data;
    XP_ASSERT( GTK_ENTRY(state->timerField) == GTK_ENTRY(editable) );
    gchar* text = gtk_editable_get_chars( editable, 0, -1 );
    NGValue value = { .ng_u16 = atoi(text) };
    newg_attrChanged( state->newGameCtxt, NULL_XWE, NG_ATTR_TIMER, value );
}

static void
addTimerWidget( GtkNewGameState* state, GtkWidget* parent )
{
    GtkWidget* hbox = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 0 );
    gtk_box_pack_start( GTK_BOX(parent), hbox, FALSE, TRUE, 0 );
    GtkWidget* label = gtk_label_new( "Timer seconds (per move in duplicate case):" );
    gtk_box_pack_start( GTK_BOX(hbox), label, FALSE, TRUE, 0 );
    state->timerField = gtk_entry_new();
    g_signal_connect( state->timerField, "changed",
                      (GCallback)on_timer_changed, state );
    gtk_entry_set_input_purpose( GTK_ENTRY(state->timerField), GTK_INPUT_PURPOSE_NUMBER );
    gtk_box_pack_start( GTK_BOX(hbox), state->timerField, FALSE, TRUE, 0 );
}

static void
handle_duplicate_toggled( GtkWidget* item, GtkNewGameState* state )
{
    NGValue value = { .ng_bool = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(item) ) };
    newg_attrChanged( state->newGameCtxt, NULL_XWE, NG_ATTR_DUPLICATE, value );
}

static void
addDuplicateCheckbox( GtkNewGameState* state, GtkWidget* parent )
{
    GtkWidget* duplicateCheck = gtk_check_button_new_with_label( "Duplicate mode" );
    state->duplicateCheck = duplicateCheck;
    g_signal_connect( duplicateCheck, "toggled",
                      (GCallback)handle_duplicate_toggled, state );
    gtk_widget_show( duplicateCheck );
    gtk_box_pack_start( GTK_BOX(parent), duplicateCheck, FALSE, TRUE, 0 );
}

static void
addSizesRow( GtkNewGameState* state, GtkWidget* parent )
{
    LOG_FUNC();
    GtkWidget* hbox = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 0 );

    /* Tray size */
    gtk_box_pack_start( GTK_BOX(hbox), gtk_label_new(TRAY_SIZE ":"), FALSE, TRUE, 0 );
    GtkWidget* traySizeCombo = gtk_combo_box_text_new();
    for ( int ii = MIN_TRAY_TILES; ii <= MAX_TRAY_TILES; ++ii ) {
        char buf[10];
        snprintf( buf, sizeof(buf), "%d", ii );
        gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT(traySizeCombo), buf );
    }
    gtk_combo_box_set_active( GTK_COMBO_BOX(traySizeCombo),
                              state->nTrayTiles - MIN_TRAY_TILES );

    g_signal_connect( traySizeCombo, "changed", G_CALLBACK(tray_size_changed), state );
    gtk_widget_show( traySizeCombo );
    gtk_box_pack_start( GTK_BOX(hbox), traySizeCombo, FALSE, TRUE, 0 );

    /* Bingo threshold */
    gtk_box_pack_start( GTK_BOX(hbox), gtk_label_new(BINGO_THRESHOLD":"), FALSE, TRUE, 0 );
    GtkWidget* bingoCombo = gtk_combo_box_text_new();
    for ( int ii = MIN_TRAY_TILES; ii <= MAX_TRAY_TILES; ++ii ) {
        char buf[10];
        snprintf( buf, sizeof(buf), "%d", ii );
        gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT(bingoCombo), buf );
    }
    gtk_combo_box_set_active( GTK_COMBO_BOX(bingoCombo), state->bingoMin - MIN_TRAY_TILES );

    g_signal_connect( bingoCombo, "changed", G_CALLBACK(bingo_changed), state );
    gtk_widget_show( bingoCombo );
    gtk_box_pack_start( GTK_BOX(hbox), bingoCombo, FALSE, TRUE, 0 );

    /* board size choices */
    gtk_box_pack_start( GTK_BOX(hbox), gtk_label_new("Board size:"),
                        FALSE, TRUE, 0 );

    GtkWidget* boardSizeCombo = gtk_combo_box_text_new();
    if ( !state->isNewGame ) {
        gtk_widget_set_sensitive( boardSizeCombo, FALSE );
    }

    int curEntry = 0;
    for ( int siz = MIN_BOARD_SIZE; siz <= MAX_BOARD_SIZE; siz += 2 ) {
        char buf[10];
        // XP_U16 siz = MAX_COLS - ii;
        snprintf( buf, sizeof(buf), "%dx%d", siz, siz );
        gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT(boardSizeCombo), buf );
        if ( siz == state->nCols ) {
            gtk_combo_box_set_active( GTK_COMBO_BOX(boardSizeCombo), curEntry );
        }
        ++curEntry;
    }

    g_signal_connect( boardSizeCombo, "changed",
                      G_CALLBACK(size_combo_changed), state );

    gtk_widget_show( boardSizeCombo );
    gtk_box_pack_start( GTK_BOX(hbox), boardSizeCombo, FALSE, TRUE, 0 );

    gtk_box_pack_start( GTK_BOX(parent), hbox, FALSE, TRUE, 0 );
} /* addSizesRow */

static void
addDictsRow( GtkNewGameState* state, GtkWidget* parent )
{
    GtkWidget* hbox = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 0 );

    /* Dictionary combo */
    gtk_box_pack_start( GTK_BOX(hbox), gtk_label_new("Dictionary: "),
                        FALSE, TRUE, 0 );
    GtkWidget* dictCombo = gtk_combo_box_text_new();
    g_signal_connect( dictCombo, "changed",
                      G_CALLBACK(dict_combo_changed), state );
    gtk_widget_show( dictCombo );
    gtk_box_pack_start( GTK_BOX(hbox), dictCombo, FALSE, TRUE, 0 );

	GSList* dicts = listDicts( state->globals->cGlobals.params );
    GSList* iter = dicts;
    for ( int ii = 0; !!iter; iter = iter->next, ++ii ) {
        const gchar* name = iter->data;
        gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT(dictCombo), name );
        if ( !!state->gi->dictName ) {
            if ( !strcmp( name, state->gi->dictName ) ) {
                gtk_combo_box_set_active( GTK_COMBO_BOX(dictCombo), ii );
            }
        } else if ( 0 == ii ) {
            gtk_combo_box_set_active( GTK_COMBO_BOX(dictCombo), ii );
        }
    }
	g_slist_free( dicts );

    addPhoniesCombo( state, hbox );

    gtk_box_pack_start( GTK_BOX(parent), hbox, FALSE, TRUE, 0 );
} /* addDictsRow */

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

    dialog = gtk_dialog_new();
    gtk_window_set_modal( GTK_WINDOW( dialog ), TRUE );

    vbox = gtk_box_new( GTK_ORIENTATION_VERTICAL, 0 );

#ifndef XWFEATURE_STANDALONE_ONLY
    hbox = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 0 );
    gtk_box_pack_start( GTK_BOX(hbox), gtk_label_new("Role:"),
                        FALSE, TRUE, 0 );
    roleCombo = gtk_combo_box_text_new();
    state->roleCombo = roleCombo;

    for ( int ii = 0; ii < VSIZE(roles); ++ii ) {
        gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT(roleCombo),
                                        roles[ii] );
    }
    gtk_box_pack_start( GTK_BOX(hbox), roleCombo, FALSE, TRUE, 0 );
    g_signal_connect( roleCombo, "changed", 
                      G_CALLBACK(role_combo_changed), state );

    state->settingsButton = makeButton( "Settings...", 
                                        (GCallback)handle_settings,
                                        state );
    gtk_box_pack_start( GTK_BOX(hbox), state->settingsButton, FALSE, TRUE, 0 );

    gtk_box_pack_start( GTK_BOX(vbox), hbox, FALSE, TRUE, 0 );
#endif

    /* NPlayers menu */
    hbox = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 0 );
    state->nPlayersLabel = gtk_label_new("");
    gtk_box_pack_start( GTK_BOX(hbox), state->nPlayersLabel, FALSE, TRUE, 0 );

    GtkWidget* nPlayersCombo = gtk_combo_box_text_new();
    state->nPlayersCombo = nPlayersCombo;

    for ( int ii = 0; ii < MAX_NUM_PLAYERS; ++ii ) {
        char buf[2] = { ii + '1', '\0' };
        gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT(nPlayersCombo),
                                        buf );
    }

    gtk_widget_show( nPlayersCombo );
    gtk_box_pack_start( GTK_BOX(hbox), nPlayersCombo, FALSE, TRUE, 0 );
    g_signal_connect( nPlayersCombo, "changed", 
                      G_CALLBACK(nplayers_menu_changed), state );

    state->juggleButton = makeButton( "Juggle", 
                                      (GCallback)handle_juggle,
                                      state );
    gtk_box_pack_start( GTK_BOX(hbox), state->juggleButton, FALSE, TRUE, 0 );
    gtk_widget_show( hbox );

    gtk_box_pack_start( GTK_BOX(vbox), hbox, FALSE, TRUE, 0 );

    for ( int ii = 0; ii < MAX_NUM_PLAYERS; ++ii ) {
        GtkWidget* label = gtk_label_new("Name:");
#ifndef XWFEATURE_STANDALONE_ONLY
        GtkWidget* remoteCheck = gtk_check_button_new_with_label( "Remote" );
#endif
        GtkWidget* nameField = gtk_entry_new();
        GtkWidget* passwdField = gtk_entry_new();
        gtk_entry_set_max_length( GTK_ENTRY(passwdField), 6 );
        GtkWidget* robotCheck = gtk_check_button_new_with_label( "Robot" );

#ifndef XWFEATURE_STANDALONE_ONLY
        g_signal_connect( remoteCheck, "toggled", 
                          (GCallback)handle_remote_toggled, state );
#endif
        g_signal_connect( robotCheck, "toggled", 
                          (GCallback)handle_robot_toggled, state );

        hbox = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 0 );

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

    addSizesRow( state, vbox );
    addDictsRow( state, vbox );

    gtk_widget_show( hbox );
    gtk_box_pack_start( GTK_BOX(vbox), hbox, FALSE, TRUE, 0 );

    addTimerWidget( state, vbox );
    addDuplicateCheckbox( state, vbox );

    /* buttons at the bottom */
    hbox = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 0 );
    gtk_box_pack_start( GTK_BOX(hbox), 
                        makeButton( "Ok", (GCallback)handle_ok, state ),
                        FALSE, TRUE, 0 );
    if ( state->isNewGame ) {
        gtk_box_pack_start( GTK_BOX(hbox), 
                            makeButton( "Revert", 
                                        (GCallback)handle_revert,
                                        state ),
                            FALSE, TRUE, 0 );
        gtk_box_pack_start( GTK_BOX(hbox), 
                            makeButton( "Cancel", 
                                        (GCallback)handle_cancel,
                                        state ),
                            FALSE, TRUE, 0 );
    }
    gtk_box_pack_start( GTK_BOX(vbox), hbox, FALSE, TRUE, 0 );

    gtk_widget_show( vbox );
    gtk_dialog_add_action_widget( GTK_DIALOG(dialog), vbox, 0 );

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
    case NG_COL_PASSWD: {
        gchar buf[32];
        cp = !!value.ng_cp ? value.ng_cp : "";
        if ( NG_COL_NAME == col && '\0' == cp[0] ) {
            sprintf( buf, "Linuser %d", 1 + player );
            cp = buf;
        }
        gtk_entry_set_text( GTK_ENTRY(widget), cp );
        break;
    }
#ifndef XWFEATURE_STANDALONE_ONLY
    case NG_COL_REMOTE:
#endif
    case NG_COL_ROBOT:
        gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(widget),
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
    switch( attr ) {
    case NG_ATTR_NPLAYERS: {
        XP_U16 ii = value.ng_u16;
        XP_LOGF( "%s: setting menu %d", __func__, ii-1 );
        gtk_combo_box_set_active( GTK_COMBO_BOX(state->nPlayersCombo), ii-1 );
    }
        break;
#ifndef XWFEATURE_STANDALONE_ONLY
    case NG_ATTR_ROLE:
        gtk_combo_box_set_active( GTK_COMBO_BOX(state->roleCombo), 
                                  value.ng_role );
        break;
    case NG_ATTR_REMHEADER:
        /* ignored on GTK: no headers at all */
        break;
#endif
    case NG_ATTR_NPLAYHEADER:
        gtk_label_set_text( GTK_LABEL(state->nPlayersLabel), value.ng_cp );
        break;
    case NG_ATTR_TIMER: {
        gchar buf[32];
        snprintf( buf, VSIZE(buf), "%d", value.ng_u16 );
        gtk_label_set_text( GTK_LABEL(state->timerField), buf );
    }
        break;
    case NG_ATTR_DUPLICATE:
        gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(state->duplicateCheck),
                                      value.ng_bool );
        break;
    default:
        XP_ASSERT(0);
        break;
    }
}

static void
setDefaults( CurGameInfo* gi )
{
    if ( 0 == gi->nPlayers ) {
        gi->nPlayers = 2;
        gi->players[0].isLocal = XP_FALSE;
    }
}

static void
checkAndWarn( GtkNewGameState* state, GtkWidget* dialog )
{
    if ( state->nTrayTiles < state->bingoMin ) {
        gchar buf[128];
        XP_SNPRINTF( buf, VSIZE(buf),"\"%s\" cannot be greater than \"%s\"",
                     BINGO_THRESHOLD, TRAY_SIZE );
        gtktell( dialog, buf );
        state->revert = XP_TRUE;
    }
}

gboolean
gtkNewGameDialog( GtkGameGlobals* globals, CurGameInfo* gi, CommsAddrRec* addr,
                  XP_Bool isNewGame, XP_Bool fireConnDlg )
{
    GtkNewGameState state;
    XP_MEMSET( &state, 0, sizeof(state) );

    state.globals = globals;
    state.gi = gi;
    state.newGameCtxt = newg_make( MPPARM(globals->cGlobals.util->mpool) 
                                   isNewGame,
                                   globals->cGlobals.util,
                                   gtk_newgame_col_enable,
                                   gtk_newgame_attr_enable,
                                   gtk_newgame_col_get,
                                   gtk_newgame_col_set,
                                   gtk_newgame_attr_set,
                                   &state );
    state.isNewGame = isNewGame;
    state.fireConnDlg = fireConnDlg;

    setDefaults( gi );

    /* returns when button handler calls gtk_main_quit */
    do {
        GtkWidget* dialog;

        state.revert = FALSE;
        state.loaded = XP_FALSE;
        state.nCols = gi->boardSize;
        state.nTrayTiles = gi->traySize;
        state.bingoMin = gi->bingoMin;
        if ( 0 == state.nCols ) {
            state.nCols = globals->cGlobals.params->pgi.boardSize;
        }
        state.role = gi->serverRole;

        XP_MEMCPY( &state.addr, addr, sizeof(state.addr) );

        dialog = makeNewGameDialog( &state );

        newg_load( state.newGameCtxt, NULL_XWE, gi );
        state.loaded = XP_TRUE;

        gtk_main();

        checkAndWarn( &state, dialog );
        if ( !state.cancelled && !state.revert ) {
            if ( newg_store( state.newGameCtxt, NULL_XWE, gi, XP_TRUE ) ) {
                gi->boardSize = state.nCols;
                gi->traySize = state.nTrayTiles;
                gi->bingoMin = state.bingoMin;
                replaceStringIfDifferent( globals->cGlobals.util->mpool,
                                          &gi->dictName, state.dict );
                gi->phoniesAction = state.phoniesAction;
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
