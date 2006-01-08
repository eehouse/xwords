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
#include "xptypes.h"
#include "strutils.h"
#include "mempool.h"
}

#include "frankids.h"
#include "frankplayer.h"
#include "frankdict.h"


#define PLAYERCOUNT_MENU_ID 2000
#define NUMPLAYERS_POPUP_ID 2001
#define NAME_LABEL_ID 2002
#define ROBOT_LABEL_ID 2003
#define PASSWORD_LABEL_ID 2004
#define OK_BUTTON_ID 2005
#define CANCEL_BUTTON_ID 2006
#define REVERT_BUTTON_ID 2007
#define COUNT_LABEL_ID 2008
#define PLAYERDICT_MENU_ID 2009
#define DICTNAMES_POPUP_ID 2010
#define DICT_LABEL_ID 2011
#define SIZE_LABEL_ID 2012
#define BOARDSIZE_MENU_ID 2013
#define BOARDSIZE_POPUP_ID 2014
#define TIMER_CHECKBOX_ID 2015
#define TIMER_FIELD_ID 2016
#define PHONIES_MENU_ID 2017
#define PHONIES_POPUP_ID 2018

#define OK_BUTTON_COL 35
#define CANCEL_BUTTON_COL 95
#define REVERT_BUTTON_COL 125
/* These must be far enough apart that derivitaves will remain unique; make it
   10 for now.  Also, there can't be any overlap between their ranges and
   other ids!!!!*/
#define NAME_BASE 2060
#define ROBOT_BASE 2070
#define PASSWORD_BASE 2080

#define COUNTER_ROW 5
#define LABEL_ROW 20
#define FIRST_ROW (LABEL_ROW+20)
#define ROW_OFFSET 18
#define DICTMENU_ROW (FIRST_ROW + (ROW_OFFSET * MAX_NUM_PLAYERS) + 2)
#define SIZEMENU_ROW (DICTMENU_ROW + ROW_OFFSET + 1)
#define PHONIESMENU_ROW (SIZEMENU_ROW + ROW_OFFSET + 1)
#define TIMER_ROW (PHONIESMENU_ROW + ROW_OFFSET + 1)

#define BUTTON_ROW (TIMER_ROW + ROW_OFFSET + 3)
#define NAME_COL 5
#define NAME_WIDTH 105
#define NAME_HEIGHT 12
#define ROBOT_COL (NAME_COL + NAME_WIDTH + 10)
#define PASSWD_WIDTH 20
#define TIME_WIDTH 20
#define PASSWD_HEIGHT NAME_HEIGHT
#define PASSWORD_COL (ROBOT_COL + PASSWD_WIDTH + 10)
#define TIMER_FIELD_COL 120

/* Put up a window with a list for each player giving name and robotness,
 * and allowing for setting/changing a password.
 */
CPlayersWindow::CPlayersWindow( MPFORMAL CurGameInfo* gi, FrankDictList* dlist, 
                                BOOL isNew, BOOL allowCancel, BOOL* cancelledP )
    : CWindow( PLAYERS_WINDOW_ID, 2, 12, 196, 226, "Game setup", !isNew,
	       FALSE, FALSE )
{
    fLocalGI = *gi;
    fGIRef = gi;
    fDList = dlist;
    this->resultP = cancelledP;
    fIsNew = isNew;
    MPASSIGN( this->mpool, mpool );

    /* numplayers counter */
    CLabel* label = new CLabel( COUNT_LABEL_ID, "Number of players:" );
    this->AddChild( label, NAME_COL, COUNTER_ROW );
    this->countMenu = new CMenu(PLAYERCOUNT_MENU_ID, 0, 0, 0, 0, 0 );
    this->countMenu->SetNumRows( MAX_NUM_PLAYERS );

    char* base = (char*)fNumsBuf;
    for ( U16 i = 0 ; i < MAX_NUM_PLAYERS; ++i ) {
        snprintf( base, 2, "%d", i+1 );
        this->countMenu->SetRow( i, 2000+i, base );
        base += 2;
    }

    CPopupTrigger *trigger = new CPopupTrigger( NUMPLAYERS_POPUP_ID, 0, 0, 
                                                this->countMenu, 0 );
    trigger->SetCurrentRow( fLocalGI.nPlayers-1 );
    this->AddChild( trigger, NAME_COL+130, COUNTER_ROW );
    if ( !isNew ) {
        DisOrEnable( NUMPLAYERS_POPUP_ID, FALSE );
    }

    /* Column labels */
    label = new CLabel( NAME_LABEL_ID, "Name" );
    this->AddChild( label, NAME_COL, LABEL_ROW );
    label = new CLabel( ROBOT_LABEL_ID, "Rbt" );
    this->AddChild( label, ROBOT_COL, LABEL_ROW );
    label = new CLabel( PASSWORD_LABEL_ID, "Pwd" );
    this->AddChild( label, PASSWORD_COL, LABEL_ROW );

    /* build a row of controls for each potential player.  Disable those below
       the point determined by the number of players we have. */
    for ( U16 i = 0; i < MAX_NUM_PLAYERS; ++i ) {
        LocalPlayer* fp = &fLocalGI.players[i];

        CTextEdit* name = new CTextEdit( NAME_BASE + i, NAME_WIDTH, 
                                         NAME_HEIGHT, TEXTOPTION_ONELINE );
        name->SetText( (char*)fp->name );	
        this->AddChild( name, NAME_COL, FIRST_ROW + (ROW_OFFSET*i) );

        CCheckbox *robot_check = new CCheckbox( ROBOT_BASE + i, 0, 0, "" ); 
        robot_check->SetDownStatus( fp->isRobot );
        this->AddChild( robot_check, ROBOT_COL, FIRST_ROW + (ROW_OFFSET*i) );

        CTextEdit* passwd = new CTextEdit( PASSWORD_BASE + i, 
                                           PASSWD_WIDTH, PASSWD_HEIGHT,
                                           TEXTOPTION_PASSWORD 
                                           | TEXTOPTION_ONELINE);
        this->AddChild( passwd, PASSWORD_COL, FIRST_ROW + (ROW_OFFSET*i) );
        const char* password = (const char*)fp->password;
        if ( !!password && !!password[0] ) {
            passwd->SetText( password );
        }
    }

    this->makeDictMenu();

    this->makeSizeMenu();

    this->makePhoniesMenu();

    /* the timer checkbox */
    fTimerEnabled = new CCheckbox( TIMER_CHECKBOX_ID, 0, 0, 
                                   "Timer enabled" ); 
    fTimerEnabled->SetDownStatus( fLocalGI.timerEnabled );
    AddChild( fTimerEnabled, NAME_COL, TIMER_ROW );
	if ( !isNew ) {
		fTimerEnabled->Disable();
	}

    /* the timer field (hidden if checkbox not checked) */
    fTimerField = new CTextEdit( TIMER_FIELD_ID, TIME_WIDTH, 
                                 PASSWD_HEIGHT, TEXTOPTION_ONELINE );
    char buf[10];
    sprintf( buf, "%d", fLocalGI.gameSeconds / 60 );
    fTimerField->SetText( buf );
    AddChild( fTimerField, TIMER_FIELD_COL, TIMER_ROW );
    if ( !fLocalGI.timerEnabled || !isNew ) {
        fTimerField->Disable();
    }

    /* the buttons at the bottom */
    U16 okCol = OK_BUTTON_COL;
    CButton* button = new CButton( OK_BUTTON_ID, 0, 0, "Ok" );
    if ( !(isNew && allowCancel) ) {
        U16 buttonWidth = button->GetWidth();
        U16 windowWidth = this->GetWidth();
        okCol = (windowWidth - buttonWidth) / 2;
    }
    this->AddChild( button, okCol, BUTTON_ROW );

    if ( isNew && allowCancel ) {
        button = new CButton( CANCEL_BUTTON_ID, 0, 0, "Cancel" );
        this->AddChild( button, CANCEL_BUTTON_COL, BUTTON_ROW );
    }

	adjustVisibility();
    XP_DEBUGF( "CPlayersWindow done" );
} // CPlayersWindow

CPlayersWindow::~CPlayersWindow()
{
    delete( this->countMenu );
    delete( this->dictMenu );
    delete( this->sizeMenu );
} /* ~CPlayersWindow */

void
CPlayersWindow::DisOrEnable( U16 id, BOOL enable )
{
    CViewable* child = this->GetChildID( id );
    if ( enable ) {
	XP_DEBUGF( "enabling child id=%d\n", id );
	child->Enable();
    } else {
	XP_DEBUGF( "disabling child id=%d\n", id );
	child->Disable();
    }
} /* DisOrEnable */


static BOOL
checkAllDigits( CTextEdit* te )
{
    char* text = te->GetText();
    char ch;
    while ( (ch=*text++) != '\0' ) {
	if ( !isdigit(ch) ) {
	    return false;
	}
    }
    return true;
} /* checkAllDigits */

S32
CPlayersWindow::MsgHandler( MSG_TYPE type, CViewable *from, S32 data )
{
    S32 result = 0;
    S32 id;
    U16 row;

    switch ( type ) {
    case MSG_MENU_SELECT:	/* the num-players trigger */
        XP_DEBUGF( "MSG_MENU_SELECT: data=%ld\n", data );
        switch ( from->GetID()) {
        case NUMPLAYERS_POPUP_ID:
            row = this->countMenu->GetCurrentRow();
            fLocalGI.nPlayers = row + 1; /* GetCurrentRow is 0-based */
            adjustVisibility();
            GUI_NeedUpdate();
            result = 1;
            break;
            /* 	case DICTNAMES_POPUP_ID: */
            /* 	    row = this->dictMenu->GetCurrentRow(); */
            /* 	    break; */
        }
        break;

    case MSG_TEXT_CHANGED:
        if ( (from->GetID() == TIMER_FIELD_ID)
             && !checkAllDigits( (CTextEdit*)from ) ) {
            result = TEXTEDIT_PLEASE_UNDO;
        }
        break;

    case MSG_BUTTON_SELECT:
        result = 1;
        id = from->GetID();
        switch ( id ) {

        case TIMER_CHECKBOX_ID:
            DisOrEnable( TIMER_FIELD_ID, fTimerEnabled->GetDownStatus() );
            break;

        case OK_BUTTON_ID:
            for ( U16 i = 0; i < fLocalGI.nPlayers; ++i ) {
                copyIDString( NAME_BASE+i, &fLocalGI.players[i].name );
                copyIDString( PASSWORD_BASE+i, 
                              &fLocalGI.players[i].password );
            }
            if ( !!dictMenu ) {
                fLocalGI.dictName = 
                    copyString( MPPARM(mpool) 
                                fDList->GetNthName(dictMenu->GetCurrentRow()));
            } else {
                fLocalGI.dictName = (XP_UCHAR*)NULL;
            }

            if ( fIsNew ) {
                fLocalGI.boardSize = 15 - this->sizeMenu->GetCurrentRow();
                fLocalGI.phoniesAction = fPhoniesMenu->GetCurrentRow();
            }

            fLocalGI.timerEnabled = fTimerEnabled->GetDownStatus();
            if ( fLocalGI.timerEnabled ) {
                char* text = fTimerField->GetText();
                fLocalGI.gameSeconds = atoi(text) * 60;
            }

            *fGIRef = fLocalGI; /* copy changes to caller */
        case CANCEL_BUTTON_ID:
            *this->resultP = id == CANCEL_BUTTON_ID;
            this->Close();
            result = 1;
            break;
        default:		/* probably one of our synthetic IDs */
            if ( id >= ROBOT_BASE && id < ROBOT_BASE+MAX_NUM_PLAYERS ) {
                U16 playerNum = id - ROBOT_BASE;
                BOOL isRobot = ((CButton*)from)->GetDownStatus();
                fLocalGI.players[playerNum].isRobot = isRobot;
                adjustVisibility();
            } else if (id >= NAME_BASE && id < NAME_BASE + MAX_NUM_PLAYERS ){
            } else {
                result = 0;
            }
        }
    default:
        break;
    }

    if ( result == 0 ) {
        result = CWindow::MsgHandler( type, from, data );
    }
    return result;
} // CPlayersWindow::MsgHandler

/* This will create a dictionary of the dict listed first in the initial.mom
 * file
 */
void
CPlayersWindow::makeDictMenu()
{
    XP_U16 nDicts = fDList->GetDictCount();

    U16 startRow;
    if ( !!fLocalGI.dictName ) {
        startRow = fDList->IndexForName( fLocalGI.dictName);
    } else {
        startRow = 0;
    }

    XP_ASSERT( nDicts > 0 );

    CMenu* menu = new CMenu( PLAYERDICT_MENU_ID, 0, 0, 0, 0, 0 );
    menu->SetNumRows( nDicts );

    for ( U16 i = 0; i < nDicts; ++i ) {
        menu->SetRow( i, 3000+i, (char*)fDList->GetNthName(i) );
    }

    CPopupTrigger *trigger = new CPopupTrigger( DICTNAMES_POPUP_ID, 0, 0,
                                                menu, 0 );
    trigger->SetCurrentRow(startRow);
    menu->SetCurrentRow(startRow);

    CLabel* label = new CLabel( DICT_LABEL_ID, "Dictnry:" );
    this->AddChild( label, NAME_COL, DICTMENU_ROW );
    U16 labelWidth = label->GetWidth();
    this->AddChild( trigger, NAME_COL+labelWidth+10, DICTMENU_ROW );

    if ( !fIsNew ) {
        DisOrEnable( DICTNAMES_POPUP_ID, FALSE );
    }

    this->dictMenu = menu;
} /* CPlayersWindow::makeDictMenu */

void
CPlayersWindow::makeSizeMenu()
{
    CMenu* menu = new CMenu( BOARDSIZE_MENU_ID, 0, 0, 0, 0, 0 );
    menu->SetNumRows( NUM_SIZES );
    for ( U16 i = 0; i < NUM_SIZES; ++i ) {
        U16 siz = 15-i;
        snprintf( (char*)this->sizeNames[i], sizeof(this->sizeNames[i]),
                  "%dx%d", siz, siz );
        menu->SetRow( i, 4000+i, (char*)this->sizeNames[i] );
    }
    CPopupTrigger* trigger = new CPopupTrigger( BOARDSIZE_POPUP_ID, 0, 0, 
                                                menu, 0 );
    U16 curSize = 15-fLocalGI.boardSize;
    trigger->SetCurrentRow(curSize);
    menu->SetCurrentRow(curSize);

    CLabel* label = new CLabel( SIZE_LABEL_ID, "Board size:" );
    this->AddChild( label, NAME_COL, SIZEMENU_ROW );
    U16 labelWidth = label->GetWidth();
    this->AddChild( trigger, NAME_COL+labelWidth+10, SIZEMENU_ROW );

    if ( !fIsNew ) {
        DisOrEnable( BOARDSIZE_POPUP_ID, FALSE );
    }

    this->sizeMenu = menu;
} /* CPlayersWindow::makeSizeMenu */

void
CPlayersWindow::adjustVisibility()
{
	/* disable everything greater than the number of players.  Before that,
	   disable passwords if robot */

	U16 nPlayers = fLocalGI.nPlayers;
	for ( U16 i = 0; i < MAX_NUM_PLAYERS; ++i ) {
		XP_Bool disableAll = i >= nPlayers;
		BOOL enable;

		/* name */
		enable = !disableAll;
		DisOrEnable( NAME_BASE + i, enable );

		/* robot check */
		/* enable's the same as above */
		DisOrEnable( ROBOT_BASE + i, enable );

		/* passwd */
		enable = !disableAll && !fLocalGI.players[i].isRobot;
		DisOrEnable( PASSWORD_BASE + i, enable );
	}
} /* adjustVisibility */

void
CPlayersWindow::makePhoniesMenu()
{
    CMenu* menu = new CMenu( PHONIES_MENU_ID, 0, 0, 0, 0, 0 );
    menu->SetNumRows( 3 );
    menu->SetRow( 0, 5000, "Ignore" );
    menu->SetRow( 1, 5001, "Warn" );
    menu->SetRow( 2, 5002, "Disallow" );

    CPopupTrigger* trigger = new CPopupTrigger( PHONIES_POPUP_ID, 0, 0, 
						menu, 0 );

    XWPhoniesChoice phoniesAction = fLocalGI.phoniesAction;
    trigger->SetCurrentRow(phoniesAction);
    menu->SetCurrentRow(phoniesAction);

    CLabel* label = new CLabel( SIZE_LABEL_ID, "Phonies:" );
    this->AddChild( label, NAME_COL, PHONIESMENU_ROW );
    U16 labelWidth = label->GetWidth();
    this->AddChild( trigger, NAME_COL+labelWidth+10, PHONIESMENU_ROW );

    fPhoniesMenu = menu;
} /* CPlayersWindow::makePhoniesMenu */

void
CPlayersWindow::copyIDString( U16 id, XP_UCHAR** where )
{
    if ( *where ) {
	XP_DEBUGF( "freeing string " );
	XP_DEBUGF( "%s\n", *where );
	XP_FREE( mpool, *where );
	XP_DEBUGF( "done freeing string\n" );
    }

    XP_UCHAR* str = (XP_UCHAR*)NULL;
    CTextEdit* te = (CTextEdit*)this->GetChildID( id );
    XP_UCHAR* name = (XP_UCHAR*)te->GetText();
    U16 len = te->TextLength();
    if ( len > 0 ) {
	str = (XP_UCHAR*)XP_MALLOC( mpool, len + 1 );
	memcpy( str, name, len );
	str[len] = '\0';
    }
    *where = str;
} /* CPlayersWindow::copyIDString */
