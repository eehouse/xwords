/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 2001 by Eric House (fixin@peak.org).  All rights reserved.
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

#define MAX_SIZE_CHOICES 10

/* make the appropriate set of entries sensitive or not
 */
typedef struct ItemNumPair {
    GtkWidget* item;
    short index;
    gboolean found;
} ItemNumPair;

void
countBeforeSame( GtkWidget *widget, gpointer data )
{
    ItemNumPair* pair = (ItemNumPair*)data;
    if ( !pair->found ) {
	if ( pair->item == widget ) {
	    pair->found = TRUE;
	} else {
	    ++pair->index;
	}
    }
} /* countBeforeSame */

static void
setChildrenSensitivity( GtkWidget* hbox, gboolean enabling )
{
    gtk_widget_set_sensitive( hbox, enabling );
} /* setChildrenSensitivity */

static void
nplayers_menu_select( GtkWidget* item, GtkNewGameState* state )
{
    short prevNPlayers = state->nPlayers;
    short newNPlayers;
    GtkWidget* parent = item->parent;

    ItemNumPair pair;
    short high, low;
    gboolean enabling;

    pair.item = item;
    pair.index = 0;
    pair.found = FALSE;
    
    gtk_container_foreach( GTK_CONTAINER(parent), countBeforeSame, &pair );

    newNPlayers = pair.index + 1;

    low = XP_MIN( newNPlayers, prevNPlayers );
    high = XP_MAX( newNPlayers, prevNPlayers );
    enabling = newNPlayers > prevNPlayers;

    /* now loop through all the hboxes */
    while ( low < high ) {
	setChildrenSensitivity( state->playerEntries[low], enabling );
	++low;
    }
    state->nPlayers = newNPlayers;
} /* nplayers_menu_select */

static void
size_menu_select( GtkWidget* item, GtkNewGameState* state )
{
    ItemNumPair pair;    

    pair.item = item;
    pair.index = 0;
    pair.found = FALSE;

    gtk_container_foreach( GTK_CONTAINER(item->parent), countBeforeSame, &pair );

    XP_DEBUGF( "changing nCols from %d to %d\n", state->nCols, 
	       MAX_COLS - pair.index );
    state->nCols = MAX_COLS - pair.index;
} /* size_menu_select  */

typedef struct LoadPair {
    LocalPlayer* info;
    XP_U16 counter;
    MPSLOT
} LoadPair;

static void
loadCopyValues( GtkWidget* item, gpointer data )
{
    LoadPair* lp = (LoadPair*)data;
    LocalPlayer* player = lp->info;
    char* entryText;

    switch( lp->counter ) {
    case 0:			/* labels */
    case 2:
	break;
    case 1:			/* name field */
	entryText = gtk_entry_get_text( GTK_ENTRY(item) );
	player->name = copyString( MPPARM(lp->mpool) entryText );
	break;
    case 3:			/* passwd field */
	entryText = gtk_entry_get_text( GTK_ENTRY(item) );
	player->password = copyString( MPPARM(lp->mpool) entryText );
	break;
    case 4:			/* is Robot */
	player->isRobot = GTK_WIDGET_STATE( item ) == GTK_STATE_ACTIVE;
	break;
    case 5:			/* is local */
	player->isLocal = GTK_WIDGET_STATE( item ) == GTK_STATE_ACTIVE;
	break;
    default:
	XP_ASSERT( 0 );
    }
    ++lp->counter;
} /* loadCopyValues */

static void
handle_ok( GtkWidget* widget, void* closure )
{
    GtkNewGameState* state = (GtkNewGameState*)closure;
    CurGameInfo* gi = &state->globals->cGlobals.params->gi;
    short i;
    LoadPair lp;

    MPASSIGN( lp.mpool, state->globals->cGlobals.params->util->mpool );

    gi->nPlayers = state->nPlayers;
    gi->boardSize = state->nCols;	/* they're the same for now */

    for ( i = 0; i < state->nPlayers; ++i ) {
	LocalPlayer* player = &gi->players[i];
	GtkWidget* hbox = state->playerEntries[i];

	lp.info = player;
	lp.counter = 0;
	
	/* Read values out of the items in the hbox, which are, in order, the
	   name entry, passwd entry, isLocal box and isRobot box */
	gtk_container_foreach( GTK_CONTAINER(hbox), loadCopyValues, &lp );
    }

    state->cancelled = XP_FALSE;
    gtk_main_quit();
}

static void
handle_cancel( GtkWidget* widget, void* closure )
{
    GtkNewGameState* state = (GtkNewGameState*)closure;
    state->cancelled = XP_TRUE;
    gtk_main_quit();
}

static void
handle_revert( GtkWidget* widget, void* closure )
{
    GtkNewGameState* state = (GtkNewGameState*)closure;
    state->revert = TRUE;
    gtk_main_quit();
} /* handle_revert */

GtkWidget*
make_menu_item( gchar* name, GtkSignalFunc func, gpointer data )
{
    GtkWidget* item;
  
    item = gtk_menu_item_new_with_label( name );
    gtk_signal_connect( GTK_OBJECT(item), "activate", func, data );
    gtk_widget_show( item );

    return item;
} /* make_menu_item */

static GtkWidget*
makeButton( char* text, GtkSignalFunc func, gpointer data )
{
    GtkWidget* button = gtk_button_new_with_label( text );
    gtk_signal_connect( GTK_OBJECT(button), "clicked", func, data );
    gtk_widget_show( button );

    return button;
} /* makeButton */

static GtkWidget*
makeNewGameDialog( GtkNewGameState* state )
{
    GtkWidget* dialog;
    GtkWidget* vbox;
    GtkWidget* hbox;
    GtkWidget* item;
    GtkWidget* nPlayersMenu;
    GtkWidget* boardSizeMenu;
    GtkWidget* opt;
    CurGameInfo* gi;
    short i;

    dialog = gtk_dialog_new();
    gtk_window_set_modal( GTK_WINDOW( dialog ), TRUE );

    vbox = gtk_vbox_new( FALSE, 0 );

    hbox = gtk_hbox_new( FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(hbox), gtk_label_new("Number of players"),
			FALSE, TRUE, 0 );

    opt = gtk_option_menu_new();
    nPlayersMenu = gtk_menu_new();

    gi = &state->globals->cGlobals.params->gi;
    state->nPlayers = gi->nPlayers;

    for ( i = 0; i < MAX_NUM_PLAYERS; ++i ) {
	char buf[2];
	snprintf( buf, 2, "%d", i+1 );
	item = make_menu_item( buf, GTK_SIGNAL_FUNC(nplayers_menu_select),
			       state );
	gtk_menu_append( GTK_MENU(nPlayersMenu), item );
	if ( i+1 == state->nPlayers ) {
	    gtk_menu_set_active( GTK_MENU(nPlayersMenu), i );
	}
    }
    gtk_option_menu_set_menu( GTK_OPTION_MENU(opt), nPlayersMenu );

    gtk_widget_show( opt );
    gtk_box_pack_start( GTK_BOX(hbox), opt, FALSE, TRUE, 0 );
    gtk_widget_show( hbox );

    gtk_box_pack_start( GTK_BOX(vbox), hbox, FALSE, TRUE, 0 );

    for ( i = 0; i < MAX_NUM_PLAYERS; ++i ) {
	GtkWidget* label;
	GtkWidget* nameField = gtk_entry_new();
	GtkWidget* passwdField = gtk_entry_new_with_max_length( 6 );
	GtkWidget* robotCheck = gtk_check_button_new_with_label( "robot" );
	GtkWidget* localCheck = gtk_check_button_new_with_label( "is local" );
	hbox = gtk_hbox_new( FALSE, 0 );
	state->playerEntries[i] = hbox;

	label = gtk_label_new("name:");
	gtk_box_pack_start( GTK_BOX(hbox), label, FALSE, TRUE, 0 );
	gtk_widget_show( label );

	gtk_box_pack_start( GTK_BOX(hbox), nameField, FALSE, TRUE, 0 );
	gtk_widget_show( nameField );

	label = gtk_label_new("passwd:");
	gtk_box_pack_start( GTK_BOX(hbox), label, FALSE, TRUE, 0 );
	gtk_widget_show( label );

	gtk_box_pack_start( GTK_BOX(hbox), passwdField, FALSE, TRUE, 0 );
	gtk_widget_show( passwdField );

	gtk_box_pack_start( GTK_BOX(hbox), robotCheck, FALSE, TRUE, 0 );
	gtk_widget_show( robotCheck );

	gtk_box_pack_start( GTK_BOX(hbox), localCheck, FALSE, TRUE, 0 );
	gtk_widget_show( localCheck );

	if ( i < state->nPlayers ) {
	    XP_Bool isSet;
	    gtk_entry_set_text( 
		GTK_ENTRY(nameField), 
		gi->players[i].name );

	    isSet = gi->players[i].isRobot;
	    gtk_toggle_button_set_state( GTK_TOGGLE_BUTTON(robotCheck),
					 isSet );
	    XP_DEBUGF( "isRobot set to %d\n", isSet );
	    isSet = gi->players[i].isLocal;
	    gtk_toggle_button_set_state( GTK_TOGGLE_BUTTON(localCheck), 
					 isSet );
	    XP_DEBUGF( "isLocal set to %d\n", isSet );
	} else {
	    char buf[10];
	    snprintf( buf, sizeof(buf), "Player %d", i+1 );
	    gtk_entry_set_text( GTK_ENTRY(nameField), buf );

	    gtk_widget_set_sensitive( hbox, FALSE );
	    gtk_toggle_button_set_state( GTK_TOGGLE_BUTTON(localCheck), 
					 XP_TRUE );
	}
	gtk_box_pack_start( GTK_BOX(vbox), hbox, FALSE, TRUE, 0 );
	gtk_widget_show( hbox );
    }

    /* board size choices */
    hbox = gtk_hbox_new( FALSE, 0 );
    gtk_box_pack_start( GTK_BOX(hbox), gtk_label_new("Board size"),
			FALSE, TRUE, 0 );

    opt = gtk_option_menu_new();
    boardSizeMenu = gtk_menu_new();

    state->nCols = gi->boardSize;
    for ( i = 0; i < MAX_SIZE_CHOICES; ++i ) {
	char buf[10];
	XP_U16 siz = MAX_COLS - i;
	snprintf( buf, sizeof(buf), "%dx%d", siz, siz );
	item = make_menu_item( buf, GTK_SIGNAL_FUNC(size_menu_select),
			       state );
	gtk_menu_append( GTK_MENU(boardSizeMenu), item );
	if ( siz == state->nCols ) {
	    gtk_menu_set_active( GTK_MENU(boardSizeMenu), i );
	}
    }
    gtk_option_menu_set_menu( GTK_OPTION_MENU(opt), boardSizeMenu );

    gtk_widget_show( opt );
    gtk_box_pack_start( GTK_BOX(hbox), opt, FALSE, TRUE, 0 );

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
			makeButton( "Ok", handle_ok, state ),
			FALSE, TRUE, 0 );
    gtk_box_pack_start( GTK_BOX(hbox), 
			makeButton( "Revert", handle_revert, state ),
			FALSE, TRUE, 0 );
    gtk_box_pack_start( GTK_BOX(hbox), 
			makeButton( "Cancel", handle_cancel, state ),
			FALSE, TRUE, 0 );
    gtk_box_pack_start( GTK_BOX(vbox), hbox, FALSE, TRUE, 0 );


    gtk_widget_show( vbox );
    gtk_container_add( GTK_CONTAINER( GTK_DIALOG(dialog)->action_area), vbox);

    gtk_widget_show_all (dialog);

    return dialog;
} /* makeNewGameDialog */

gboolean
newGameDialog( GtkAppGlobals* globals/* , GtkGameInfo* gameInfo */ )
{
    GtkNewGameState state;
    state.globals = globals;

    /* returns when button handler calls gtk_main_quit */
    do {
	GtkWidget* dialog = makeNewGameDialog( &state );
	state.revert = FALSE;
	gtk_main();
	gtk_widget_destroy( dialog );
    } while ( state.revert );

    return !state.cancelled;
} /* newGameDialog */

#if 0
gint
gtkask( GtkAppGlobals* globals, gchar *message, gint numButtons,
	char* button1, ... )
{
    GtkWidget* dialog;
    GtkWidget* label;
    GtkWidget* button;
    short i;
    gboolean* results = g_malloc( numButtons * sizeof(results[0]) );
    char** butList = &button1;

    /* Create the widgets */
    dialog = gtk_dialog_new();
    gtk_window_set_modal( GTK_WINDOW( dialog ), TRUE );

    label = gtk_label_new( message );

    for ( i = 0; i < numButtons; ++i ) {
	button = gtk_button_new_with_label( *butList );

	results[i] = 0;
	gtk_signal_connect( GTK_OBJECT( button ), "clicked",
			    GTK_SIGNAL_FUNC(button_event), &results[i] );
	
	gtk_container_add( GTK_CONTAINER( GTK_DIALOG(dialog)->action_area),
			   button );

	++butList;
    }
    
    /* Add the label, and show everything we've added to the dialog. */
    gtk_container_add (GTK_CONTAINER (GTK_DIALOG(dialog)->vbox),
                       label);
    gtk_widget_show_all (dialog);

    /* returns when button handler calls gtk_main_quit */
    gtk_main();

    gtk_widget_destroy( dialog );

    for ( i = 0; i < numButtons; ++i ) {
	if ( results[i] ) {
	    break;
	}
    }
    g_free( results );
    return i;
 } /* gtkask */

#endif

#endif /* PLATFORM_GTK */
