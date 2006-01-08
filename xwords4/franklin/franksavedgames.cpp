// -*-mode: C; fill-column: 78; c-basic-offset: 4; -*-
/* 
 * Copyright 1999-2001 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#include <assert.h>
#include <ctype.h>
#include "sys.h"
#include "gui.h"
#include "ebm_object.h"

extern "C" {
#include "comtypes.h"
}

#include "franksavedgames.h"
#include "frankids.h"

#define RENAME_BUTTON_ID 1000
#define DUP_BUTTON_ID 1001
#define DELETE_BUTTON_ID 1002
#define DONE_BUTTON_ID 1003
#define OPEN_BUTTON_ID 1004
#define NAME_FIELD_ID 1005

#define ROW_HEIGHT 12
#define GAMES_NUM_VISROWS 6
#define GAMES_ROW_WIDTH 150
#define LIST_TOP 5
#define LIST_HEIGHT (GAMES_NUM_VISROWS*ROW_HEIGHT)
#define FIELD_TOP (LIST_TOP+LIST_HEIGHT+8)
#define LIST_LEFT 5
#define FIELD_HEIGHT 15
#define FIELD_WIDTH 100
#define BUTTON_TOP (FIELD_TOP+FIELD_HEIGHT+8)

class GamesList : public CList {
private:
    CGamesDB* gamesDB;

 public:
    GamesList( CGamesDB* gamesDB, U16 numRows, U16 startRow ); 
    
    U16 GetRowHeight( S32 row ) { return ROW_HEIGHT; }
    void DrawRow( RECT *rect, S32 row );
};

GamesList::GamesList( CGamesDB* gamesDB, U16 numRows, U16 startRow )
    : CList( 1001, GAMES_ROW_WIDTH, LIST_HEIGHT,
	     numRows, LISTOPTION_ALWAYS_HIGHLIGHT )
{
    this->gamesDB = gamesDB;

    this->SetCurrentRow(startRow);
}

void GamesList::DrawRow( RECT *rect, S32 row )
{
    XP_UCHAR* name = this->gamesDB->getNthName( row+1 );
    if ( !name ) {
	name = (XP_UCHAR*)"untitled";
    }

    CWindow* window = this->GetWindow();
    window->DrawText( (char*)name, rect->x, rect->y );
} /* GamesList::DrawRow */

/*****************************************************************************
 * The class itself
 ****************************************************************************/
CSavedGamesWindow::CSavedGamesWindow( CGamesDB* gamesDB, U16* toOpen, 
				      U16* curIndex  )
    : CWindow( SAVEDGAMES_WINDOW_ID, 2, 90, 196, 148, "Saved games", TRUE,
	       FALSE, FALSE /* no closebox */ )
{
    this->gamesDB = gamesDB;
    this->toOpenP = toOpen;	/* what we'll say to open */
    this->curIndexP = curIndex;	/* where we'll say current's moved to */
    this->curIndex = *curIndex;	/* save current (move when delete/dup) */
    this->displayIndex = this->curIndex; /* start display at current */
    this->gamesList = (GamesList*)NULL;

    CTextEdit* field = new CTextEdit( NAME_FIELD_ID, FIELD_WIDTH, 
				     FIELD_HEIGHT, TEXTOPTION_HAS_FOCUS
				     | TEXTOPTION_ONELINE );
    this->nameField = field;
    field->SetText( (char*)gamesDB->getNthName( this->displayIndex ) );
    this->AddChild( field, LIST_LEFT, FIELD_TOP );

    CButton* button = new CButton( RENAME_BUTTON_ID, 0, 0, "Rename" );
    U16 result = this->AddChild( button, 130, FIELD_TOP );

    button = new CButton( DUP_BUTTON_ID, 0, 0, "Dup" );
    result = this->AddChild( button, 5, BUTTON_TOP );

    button = new CButton( DELETE_BUTTON_ID, 0, 0, "Delete" );
    result = this->AddChild( button, 40, BUTTON_TOP );
    this->deleteButton = button;
    checkDisableDelete();

    button = new CButton( OPEN_BUTTON_ID, 0, 0, "Open" );
    result = this->AddChild( button, 90, BUTTON_TOP );

    button = new CButton( DONE_BUTTON_ID, 0, 0, "Done" );
    result = this->AddChild( button, 130, BUTTON_TOP );

    reBuildGamesList();
} // CSavedGamesWindow

void
CSavedGamesWindow::reBuildGamesList() 
{
    if ( !!this->gamesList ) {
	this->DeleteChild( this->gamesList );
	delete this->gamesList;
    }

    U16 numRows = this->gamesDB->countRecords() - 1; /* skip prefs */
    GamesList* list = new GamesList( gamesDB, numRows, this->curIndex-1 );
    this->gamesList = list;
    this->AddChild( list, LIST_LEFT, LIST_TOP );
    list->SetCurrentRow( this->displayIndex-1 );
} /* reBuildGamesList */

void
CSavedGamesWindow::checkDisableDelete()
{
    BOOL disable = this->displayIndex == this->curIndex;
    CButton* button = this->deleteButton;
    if ( disable != button->IsDisabled() ) {
	if ( disable ) {
	    button->Disable();
	} else {
	    button->Enable();
	}
    }
} /* checkDisableDelete */

S32
CSavedGamesWindow::MsgHandler( MSG_TYPE type, CViewable *object, S32 data )
{
    S32 result = 0;
    XP_UCHAR* name;
    U16 newID;

    switch (type) {
    case MSG_BUTTON_SELECT:	// there's only one button....
	switch (object->GetID()) {

	case RENAME_BUTTON_ID:
	    name = (XP_UCHAR*)this->nameField->GetText();
	    this->gamesDB->putNthName( this->displayIndex, name );
	    this->gamesList->Draw();
	    break;

	case DUP_BUTTON_ID:
	    newID = this->gamesDB->duplicateNthRecord( this->displayIndex );
	    this->displayIndex = newID;
	    reBuildGamesList();
	    this->gamesList->Draw();
	    checkDisableDelete();
	    break;

	case DELETE_BUTTON_ID:
	    /* disable button instead of checking here */
	    XP_ASSERT( this->displayIndex != this->curIndex );
	    if ( 1 == GUI_Alert( ALERT_OK,
				 "Are you sure you want to delete"
				 " the selected game?" ) ) {
		this->gamesDB->removeNthRecord( this->displayIndex );
	    
		if ( this->displayIndex < this->curIndex ) {
		    --this->curIndex;
		}

		if ( this->displayIndex == this->gamesDB->countRecords() ) {
		    --this->displayIndex;
		}

		reBuildGamesList();
		this->gamesList->Draw();
		checkDisableDelete();
	    }
	    break;

	case DONE_BUTTON_ID:
	    this->displayIndex = this->curIndex; /* restore to saved so next
						    line's does nothing */
	/* FALLTHRU */
	case OPEN_BUTTON_ID:
	    *this->curIndexP = this->curIndex;
	    *this->toOpenP = this->displayIndex;
	    this->Close();
	    break;
	}
	result = 1;
	break;
	
    case MSG_ROW_SELECT:
	this->displayIndex = (U16)data + 1;
	nameField->SetText( (char*)gamesDB->getNthName( this->displayIndex ) );
	checkDisableDelete();
	result = 1;
	break;

    default:
	break;
    }

    if ( result == 0 ) {
	result = CWindow::MsgHandler( type, object, data );
    }
    return result;
} // MsgHandler



