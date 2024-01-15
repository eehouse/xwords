/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/*
 * Copyright 2024 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#include "gtkrmtch.h"
#include "dbgutil.h"
#include "gtkutils.h"

struct {
    const gchar* txt;
    RematchOrder ro;
} sROData[] = {
    { "Keep existing", RO_SAME },
    { "Low score first", RO_LOW_SCORE_FIRST },
    { "High score first", RO_HIGH_SCORE_FIRST },
    { "Juggle", RO_JUGGLE },
#ifdef XWFEATURE_RO_BYNAME
    { "Alphabetical", RO_BY_NAME },
#endif
};

#define OK_RESULT 1000

typedef struct _State {
    const CommonGlobals* cGlobals;
    NewOrder* nop;
    GtkWidget* dialog;
    GtkWidget* radios[RO_NUM_ROS];
    int curSel;
    GtkWidget* nameField;
} State;

static void
toggled( GtkToggleButton* togglebutton, gpointer user_data )
{
    State* state = (State*)user_data;
    gboolean active = gtk_toggle_button_get_active( togglebutton );
    if ( active ) {
        for ( int ii = 0; ii < VSIZE(sROData); ++ii ) {
            if ( state->radios[ii] == GTK_WIDGET(togglebutton) ) {
                state->curSel = ii;

                server_figureOrder( state->cGlobals->game.server, sROData[ii].ro,
                                    state->nop );

                const CurGameInfo* gi = state->cGlobals->gi;
                const gchar* arr[gi->nPlayers + 1];
                for ( int ii = 0; ii < gi->nPlayers; ++ii ) {
                    arr[ii] = gi->players[state->nop->order[ii]].name;
                }
                arr[gi->nPlayers] = NULL;
                gchar* namesstr = g_strjoinv( " vs. ", (gchar**)arr );
                gtk_entry_set_text( GTK_ENTRY(state->nameField), namesstr );
                g_free( namesstr );

                break;
            }
        }
    }
}

XP_Bool
gtkask_rematch( const CommonGlobals* cGlobals, NewOrder* nop,
                gchar* gameName, int* nameLen )
{
    XP_USE( gameName );
    XP_USE( nameLen );

    State state = { .cGlobals = cGlobals, .nop = nop, };

    state.dialog = gtk_dialog_new();
    gtk_window_set_modal( GTK_WINDOW( state.dialog ), TRUE );

    GtkWidget* vbox = gtk_box_new( GTK_ORIENTATION_VERTICAL, 0 );

    GtkWidget* tmp = makeLabeledField( "Game Name", &state.nameField, "nothing" );
    gtk_box_pack_start( GTK_BOX(vbox), GTK_WIDGET(tmp), FALSE, TRUE, 0 );

    GtkWidget* prev = NULL;
    for ( int ii = 0; ii < VSIZE(sROData); ++ii ) {
        const gchar* txt = sROData[ii].txt;
        GtkWidget* radio = state.radios[ii]
            = gtk_radio_button_new_with_label_from_widget( GTK_RADIO_BUTTON(prev), txt );
        g_signal_connect( radio, "toggled", G_CALLBACK(toggled), &state );
        gtk_box_pack_start( GTK_BOX(vbox), GTK_WIDGET(radio), FALSE, TRUE, 0 );
        prev = radio;
    }    
    g_signal_emit_by_name(state.radios[0], "toggled");

    gtk_dialog_add_action_widget( GTK_DIALOG(state.dialog), vbox, 0 );

    gtk_dialog_add_button( GTK_DIALOG(state.dialog), "OK", OK_RESULT );
    
    gtk_widget_show_all( state.dialog );

    gint dlgResult = gtk_dialog_run( GTK_DIALOG(state.dialog) );

    gtk_widget_destroy( state.dialog );

    XP_Bool success = dlgResult == OK_RESULT;
    LOG_RETURNF( "%s", boolToStr(success) );
    return success;
}
