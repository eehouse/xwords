/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 2025 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#include "gtksort.h"
#include "gtkmain.h"
#include "gamemgr.h"
#include "dbgutil.h"

typedef struct _SortState {
    GtkAppGlobals* apg;
    GtkWidget* dialog;
    GtkWidget* vbox;
    GtkWidget* elemsBox;
    GroupRef grp;
    SortOrderElem soes[SO_NSOS];
    GtkWidget* elems[SO_NSOS];  /* rows within elemsBox */
    GtkWidget* radios[SO_NSOS]; /* radio buttons within elems */
    XP_U16 nActive;
    XP_U16 nTotal;
    bool showAvail;
} SortState;

static gchar*
nameForSO( SORT_ORDER so )
{
    gchar* result = NULL;
    switch ( so ) {
    case SO_TURNLOCAL: result = "TURNLOCAL"; break;
    case SO_CREATED: result = "CREATED"; break;
    case SO_LASTMOVE: result = "LAST MOVE"; break;
    case SO_LASTMOVE_TS: result = "LAST MOVE TS"; break;
    case SO_OTHERS_NAMES: result = "OTHER NAMES"; break;
    case SO_GAMENAME: result = "NAME"; break;
    case SO_CREATED_TS: result = "CREATED"; break;
    case SO_GAMESTATE: result = "GAME STATE"; break;
    case SO_HASCHAT: result = "HAS CHAT"; break;
    case SO_LANGUAGE: result = "LANGUAGE"; break;
    default:
        XP_ASSERT(0);
    }
    return result;
}

static void
checkToggled( GtkWidget* toggle, void* closure )
{
    LOG_FUNC();
    SortState* ss = (SortState*)closure;
    for ( int ii = 0; ii < ss->nTotal; ++ii ) {
        if ( hasAsChild( GTK_CONTAINER(ss->elems[ii]), toggle ) ) {
            ss->soes[ii].inverted = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(toggle) );
            break;
        }
    }
}

static void
readdElements( SortState* ss, gint selected )
{
    removeAllFrom( ss->elemsBox );

    GtkWidget* prev = NULL;
    const int count = ss->showAvail ? ss->nTotal : ss->nActive;
    for ( int ii = 0; ii < count; ++ii ) {
        GtkWidget* hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, /*spacing=*/0);
        ss->elems[ii] = hbox;

        if ( ss->showAvail ) {
            prev = gtk_check_button_new();
            gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(prev), ii < ss->nActive );
        } else {
            prev = gtk_radio_button_new_from_widget( GTK_RADIO_BUTTON(prev) );
        }
        ss->radios[ii] = prev;

        SortOrderElem* soe = &ss->soes[ii];
        const gchar* txt = nameForSO(soe->so);
        GtkWidget* label = gtk_label_new(txt);
        gtk_container_add( GTK_CONTAINER(prev), label );
        gtk_box_pack_start(GTK_BOX(hbox), prev, FALSE, FALSE, 0);

        GtkWidget* check = gtk_check_button_new_with_label( "inverted" );
        gtk_box_pack_start(GTK_BOX(hbox), check, FALSE, FALSE, 0);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), soe->inverted );

        gtk_box_pack_start(GTK_BOX(ss->elemsBox), hbox, FALSE, FALSE, 0);
        g_signal_connect( check, "toggled", G_CALLBACK(checkToggled), ss );
    }
    gtk_widget_show_all( ss->elemsBox );
    if ( !ss->showAvail ) {
        gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(ss->radios[selected]), true );
    }
}

static void
handleMoveBy( SortState* ss, int by )
{
    int index = -1;
    for ( int ii = 0; index < 0 && ii < ss->nActive; ++ii ) {
        GtkWidget* radio = ss->radios[ii];
        if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(radio) ) ) {
            index = ii;
        }
    }
    XP_ASSERT( 0 <= index && index < ss->nActive );
    if ( by < 0 && index == 0 ) {
        XP_LOGFF( "can't move up" );
    } else if ( 0 < by && index == ss->nActive - 1 ) {
        XP_LOGFF( "can't move down" );
    } else {
        int dest = index + by;
        SortOrderElem tmp = ss->soes[index];
        ss->soes[index] = ss->soes[dest];
        ss->soes[dest] = tmp;
        readdElements( ss, dest );
    }
}

static void
handleUp( GtkWidget* XP_UNUSED(widget), void* closure )
{
    handleMoveBy( closure, -1 );
}

static void
handleDown( GtkWidget* XP_UNUSED(widget), void* closure )
{
    handleMoveBy( closure, 1 );
}

/* Reorder, moving all the checked elems above the active line and the rest
   below. */
static void
harvestActive( SortState* ss )
{
    SortOrderElem activeSoes[SO_NSOS];
    SortOrderElem inactiveSoes[SO_NSOS];
    int nActive = 0;
    int nInActive = 0;

    for ( int ii = 0; ii < ss->nTotal; ++ii ) {
        if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(ss->radios[ii]) ) ) {
            activeSoes[nActive++] = ss->soes[ii];
        } else {
            inactiveSoes[nInActive++] = ss->soes[ii];
        }
    }
    memmove( &ss->soes[0], activeSoes, nActive * sizeof(activeSoes[0]) );
    ss->nActive = nActive;
    memmove( &ss->soes[nActive], inactiveSoes, nInActive * sizeof(activeSoes[0]) );
}

static void
disEnableApply( SortState* ss )
{
    GtkWidget *button =
        gtk_dialog_get_widget_for_response(GTK_DIALOG(ss->dialog),
                                           GTK_RESPONSE_YES);
    gtk_widget_set_sensitive(button, !ss->showAvail);
}

static void
onShowClicked( GtkWidget* toggle, void* closure )
{
    SortState* ss = (SortState*)closure;
    ss->showAvail = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(toggle) );
    XP_LOGFF( "showAvail now %s", boolToStr(ss->showAvail) );
    if ( !ss->showAvail ) {
        harvestActive( ss );
    }
    disEnableApply( ss );
    readdElements( ss, 0 );
}

static void
addButton( GtkWidget* hbox, const gchar* label, GCallback proc, void* closure )
{
    GtkWidget* button = gtk_button_new_with_label( label );
    gtk_container_add( GTK_CONTAINER(hbox), button );
    g_signal_connect( button, "clicked", G_CALLBACK(proc), closure );
    // gtk_widget_show( button );
}

static void
addButtons( SortState* ss )
{
    GtkWidget* hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, /*spacing=*/0);
    addButton( hbox, "Up", G_CALLBACK(handleUp), ss );
    addButton( hbox, "Down", G_CALLBACK(handleDown), ss );
    gtk_box_pack_start(GTK_BOX(ss->vbox), hbox, FALSE, FALSE, 0);
}

void
gtkSortDialog( GtkAppGlobals* apg, LaunchParams* params, GroupRef grp )
{
    SortState ss = {.apg = apg,
                    .grp = grp,
    };

    ss.nTotal = VSIZE(ss.soes);
    gmgr_getSortOrder( params->dutil, NULL_XWE, grp, XP_FALSE,
                       &ss.nActive, &ss.nTotal, ss.soes );
    
    XP_LOGFF( "nActive: %d; nTotal: %d", ss.nActive, ss.nTotal );
    
    ss.dialog = gtk_dialog_new_with_buttons(
        "Net state",
        GTK_WINDOW(apg->window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "Apply", GTK_RESPONSE_YES,
        "Cancel", GTK_RESPONSE_CANCEL,
        NULL
    );

    GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(ss.dialog));
    ss.vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add( GTK_CONTAINER(content), ss.vbox );

    GtkWidget* button = gtk_check_button_new_with_label( "Show available" );
    gtk_container_add( GTK_CONTAINER(ss.vbox), button );
    g_signal_connect( button, "clicked", G_CALLBACK(onShowClicked), &ss );

    ss.elemsBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add( GTK_CONTAINER(ss.vbox), ss.elemsBox );

    readdElements( &ss, 0 );
    addButtons( &ss );

    gtk_widget_show_all(ss.dialog);
    
    gint response = gtk_dialog_run(GTK_DIALOG(ss.dialog));
    if ( GTK_RESPONSE_YES == response ) {
        gmgr_setSortOrder( params->dutil, NULL_XWE, grp, ss.nActive, ss.soes );
    }
    
    gtk_widget_destroy(ss.dialog);
}

#endif /* PLATFORM_GTK */
