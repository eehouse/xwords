/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/****************************************************************************
 *									    *
 *	Copyright 1999, 2001 by Eric House (xwords@eehouse.org).  All rights reserved.           *
 *									    *
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
 ****************************************************************************/

#include <PalmTypes.h>
#include <Form.h>
#include <DLServer.h>

#include "callback.h"
#include "strutils.h"
#include "palmsavg.h"
#include "palmmain.h"
#include "palmutil.h"
#include "gameutil.h"

#include "xwords4defines.h"
#include "LocalizedStrIncludes.h"

/* Prototypes */
static void populateGameList( SavedGamesState* state );
static void setFieldToSelText( SavedGamesState* state );
static void listDrawFunc(Int16 index, RectanglePtr bounds, char** itemsText);

/*****************************************************************************
 * Handler for dictionary info form.
 ****************************************************************************/
#define USE_POPULATE 1
Boolean 
savedGamesHandleEvent( EventPtr event ) 
{
    Boolean result;
    PalmAppGlobals* globals;
    SavedGamesState* state;
    XP_S16 newGameIndex;
    Int16* curGameIndexP;
    char* newName;

    CALLBACK_PROLOGUE();

    result = false;
    globals = getFormRefcon();
    state = globals->savedGamesState;

    curGameIndexP = &globals->gState.curGameIndex;

    switch ( event->eType ) {
    case frmOpenEvent:

        if ( !state ) {
            state = globals->savedGamesState = XP_MALLOC( globals->mpool,
                                                          sizeof(*state) );
        }
        XP_MEMSET( state, 0, sizeof(*state) );

        state->globals = globals;
        state->form = FrmGetActiveForm();

        /* dictionary list setup */
        state->gamesList = getActiveObjectPtr( XW_SAVEDGAMES_LIST_ID );
        state->nameField = getActiveObjectPtr( XW_SAVEDGAMES_NAME_FIELD );
        state->displayGameIndex = globals->gState.curGameIndex;
        XP_ASSERT( state->displayGameIndex < countGameRecords(globals) );

        /* must preceed drawing calls so they have a valid window */
        FrmDrawForm( state->form );

        /* LstSetDrawFunction must follow FrmDrawForm since populateGameList
           must be called before listDrawFunc has valid globals; can't let
           listDrawFunc get called from FrmDrawForm until set up correctly. */
        LstSetDrawFunction( state->gamesList, listDrawFunc );
        populateGameList( state );
        setFieldToSelText( state );
        break;

    case frmUpdateEvent:
        FrmDrawForm( state->form );
        /* don't update field here!  The keyboard may have been the form whose
           disappearance triggered the frmUpdateEvent, and resetting the field
           would undo the user's edits.  Also causes a crash in Keyboard.c in
           the ROMs for reasons I don't understand. */
        break;

    case lstSelectEvent:
        state->displayGameIndex = LstGetSelection( state->gamesList );
        setFieldToSelText( state );
        result = true;
        break;

    case ctlSelectEvent:
        result = true;
        switch ( event->data.ctlEnter.controlID ) {

        case XW_SAVEDGAMES_USE_BUTTON: /* write the new name to the selected
                                          record */
            newName = FldGetTextPtr( state->nameField );
            if ( !!newName && (*newName != '\0') ) {
                XP_U16 len = FldGetTextLength( state->nameField );
                writeNameToGameRecord( globals, state->displayGameIndex,
                                       newName, len );

                populateGameList( state );
                setFieldToSelText( state );
            }
            break;

        case XW_SAVEDGAMES_DUPE_BUTTON:	/* copy the selected record */
            newGameIndex = duplicateGameRecord( globals, 
                                                state->displayGameIndex );
            state->displayGameIndex = newGameIndex;
            if ( *curGameIndexP >= newGameIndex ) {
                ++*curGameIndexP;
            }
            populateGameList( state );
            setFieldToSelText( state );
            break;

        case XW_SAVEDGAMES_DELETE_BUTTON: /* delete the selected record.
                                             Refuse if it's open. */
            if ( state->displayGameIndex == *curGameIndexP ) {
                beep();
            } else if ( palmaskFromStrId( globals, STR_CONFIRM_DEL_GAME, -1) ) {
                XP_S16 index = state->displayGameIndex;
                deleteGameRecord( globals, index );
                if ( *curGameIndexP > index ) {
                    --*curGameIndexP;
                }
                if ( index == countGameRecords(globals) ) {
                    --index;
                }
                state->displayGameIndex = index;
                populateGameList( state );
            }
            break;

        case XW_SAVEDGAMES_OPEN_BUTTON:	/* open the selected db if not already
                                           open. */
            if ( *curGameIndexP != state->displayGameIndex ) {
                EventType eventToPost = { .eType = openSavedGameEvent };
                ((OpenSavedGameData*)&eventToPost.data.generic)->newGameIndex
                    = state->displayGameIndex;
                EvtAddEventToQueue( &eventToPost );
                globals->postponeDraw = true;
            }

        case XW_SAVEDGAMES_DONE_BUTTON:
            /* Update the app's idea of which record to save the current game
               into -- in case any with lower IDs have been deleted. */
            FrmReturnToForm( 0 );

            freeSavedGamesData( MPPARM(globals->mpool) state );

            break;
        }

    default:
        break;
    } /* switch */

    CALLBACK_EPILOGUE();
    return result;
} /* savedGamesHandleEvent */

static void
setFieldToSelText( SavedGamesState* state ) 
{
    FieldPtr field = state->nameField;
    char name[MAX_GAMENAME_LENGTH];
    nameFromRecord( state->globals, state->displayGameIndex, name );
    XP_ASSERT( XP_STRLEN(name) < MAX_GAMENAME_LENGTH );
    XP_ASSERT( XP_STRLEN(name) > 0 );

    FldSetSelection( field, 0, FldGetTextLength(field) );

    FldInsert( field, name, XP_STRLEN(name) );
    FldSetSelection( field, 0, FldGetTextLength(field) );
#ifdef XWFEATURE_FIVEWAY
    setFormFocus( state->form, XW_SAVEDGAMES_NAME_FIELD );
#endif
    FldDrawField( field );
} /* setFieldToSelText */

void
freeSavedGamesData( MPFORMAL SavedGamesState* state )
{
    if ( !!state->stringPtrs ) {
        XP_FREE( mpool, state->stringPtrs );
        state->stringPtrs = NULL;
    }
} /* freeSavedGamesData */

static void
populateGameList( SavedGamesState* state )
{
    XP_U16 nRecords;
    PalmAppGlobals* globals = state->globals;
    ListPtr gamesList = state->gamesList;

    nRecords = countGameRecords( globals );
    if ( state->nStrings != nRecords ) {
        char** stringPtrs;
        XP_U16 i;

        LstEraseList( gamesList );

        freeSavedGamesData( MPPARM(globals->mpool) state );

        state->nStrings = nRecords;
        state->stringPtrs = stringPtrs = 
            XP_MALLOC( globals->mpool, 
                       (nRecords+1) * sizeof(state->stringPtrs[0] ) );

        stringPtrs[0] = (char*)globals;
        for ( i = 1; i <= nRecords; ++i ) {
            stringPtrs[i] = "";
        }

        LstSetListChoices( gamesList, &state->stringPtrs[1], nRecords );
        LstSetHeight( gamesList, XP_MIN(nRecords, 9) );
    }

    LstSetSelection( gamesList, state->displayGameIndex );
    XP_ASSERT( state->displayGameIndex < nRecords );
    LstMakeItemVisible( gamesList, state->displayGameIndex );
    LstDrawList( gamesList );
} /* populateGameList */

static void 
listDrawFunc( Int16 index, RectanglePtr bounds, char** itemsText )
{
    char buf[MAX_GAMENAME_LENGTH];
    XP_U16 len;
    PalmAppGlobals* globals;

    CALLBACK_PROLOGUE();

    globals = (PalmAppGlobals*)itemsText[-1];

    nameFromRecord( globals, index, buf );
    len = XP_STRLEN( buf );
    XP_ASSERT( len > 0 );
    WinDrawChars( buf, len, bounds->topLeft.x, bounds->topLeft.y );

    CALLBACK_EPILOGUE();
} /* listDrawFunc */
