/* -*-mode: C; fill-column: 77; c-basic-offset: 4; compile-command: "make ARCH=68K_ONLY MEMDEBUG=TRUE"; -*- */
/*
 * Copyright 1999 - 2006 by Eric House (xwords@eehouse.org).  All rights
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

#ifdef XWFEATURE_FIVEWAY
# include <Hs.h>
#endif

#include "prefsdlg.h"
#include "callback.h"
#include "palmutil.h"
#include "xwords4defines.h"

void localPrefsToGlobal( PalmAppGlobals* globals );
static void localPrefsToControls( PrefsDlgState* state );
static void drawPrefsTypeGadgets( PalmAppGlobals* globals );
static void showHidePrefsWidgets( PalmAppGlobals* globals, FormPtr form );
static void checkPrefsHiliteGadget( PalmAppGlobals* globals, UInt16 selGadget );
static void controlsToLocalPrefs( PrefsDlgState* state );
static XP_Bool ignoredUnlessNewgame( XP_U16 id );

Boolean
PrefsFormHandleEvent( EventPtr event )
{
    Boolean result = false;
    PalmAppGlobals* globals;
    PrefsDlgState* state;
    FormPtr form;
    Int16 chosen;
    XP_S16 selGadget;

    CALLBACK_PROLOGUE();
    globals = getFormRefcon();
    state = globals->prefsDlgState;

    switch ( event->eType ) {

    case frmOpenEvent:

        if ( !state ) {
            GlobalPrefsToLocal( globals );
            state = globals->prefsDlgState;
        }

        state->playerBdSizeList = 
            getActiveObjectPtr( XW_PREFS_BDSIZE_LIST_ID );
        state->phoniesList = 
            getActiveObjectPtr( XW_PREFS_PHONIES_LIST_ID );

    case frmUpdateEvent:
        form = FrmGetActiveForm();

        localPrefsToControls( state );

        showHidePrefsWidgets( globals, form );

        FrmDrawForm( form );

        drawPrefsTypeGadgets( globals );
        break;

    case penDownEvent:
        result = penInGadget( event, &selGadget );
        if ( result ) {
            checkPrefsHiliteGadget( globals, selGadget );
        }
        break;

#ifdef XWFEATURE_FIVEWAY
    case keyDownEvent:
        selGadget = getFocusOwner();
        if ( selGadget >= 0 ) {
            if ( tryRockerKey( event->data.keyDown.chr, selGadget, 
                               XW_PREFS_ALLGAMES_GADGET_ID, 
                               XW_PREFS_ONEGAME_GADGET_ID ) ) {
                checkPrefsHiliteGadget( globals, selGadget );
                result = XP_TRUE;
            } else if ( !globals->isNewGame
                        && vchrRockerCenter == event->data.keyDown.chr ) {
                result = ignoredUnlessNewgame( selGadget );
            }
        }
        break;

    case frmObjectFocusTakeEvent:
    case frmObjectFocusLostEvent:
        result = considerGadgetFocus( globals, event, XW_PREFS_ALLGAMES_GADGET_ID,
                                      XW_PREFS_ONEGAME_GADGET_ID );
        break;
#endif

    case ctlSelectEvent:
        result = true;
        switch ( event->data.ctlSelect.controlID ) {

#ifdef XWFEATURE_SEARCHLIMIT
        case XW_PREFS_NOHINTS_CHECKBOX_ID: {
            Boolean checked = getBooleanCtrl( XW_PREFS_NOHINTS_CHECKBOX_ID );
            disOrEnable( FrmGetActiveForm(), XW_PREFS_HINTRECT_CHECKBOX_ID, 
                         !checked );
        }
            break;
#endif
        case XW_PREFS_PHONIES_TRIGGER_ID:
            chosen = LstPopupList( state->phoniesList );
            if ( chosen >= 0 ) {
                setSelectorFromList( XW_PREFS_PHONIES_TRIGGER_ID, 
                                     state->phoniesList, chosen );
                state->phoniesAction = chosen;
            }
            break;

        case XW_PREFS_BDSIZE_SELECTOR_ID:
            XP_ASSERT( globals->isNewGame ); /* above disables otherwise */
            chosen = LstPopupList( state->playerBdSizeList );
            if ( chosen >= 0 ) {
                setSelectorFromList( XW_PREFS_BDSIZE_SELECTOR_ID, 
                                     state->playerBdSizeList, chosen );
                state->curBdSize = PALM_MAX_ROWS - (chosen*2);
            }
            break;

        case XW_PREFS_TIMERON_CHECKBOX_ID:
            XP_ASSERT( globals->isNewGame );
            form = FrmGetActiveForm();
            showHidePrefsWidgets( globals, form );
            break;

        case XW_PREFS_OK_BUTTON_ID:
            controlsToLocalPrefs( state );
            postEmptyEvent( prefsChangedEvent );
            globals->postponeDraw = true;

        case XW_PREFS_CANCEL_BUTTON_ID:
            FrmReturnToForm( 0 );
            break;
        }
        break;

    default:
        break;
    }

    CALLBACK_EPILOGUE();
    return result;
} /* prefsFormHandleEvent */

static XP_Bool
ignoredUnlessNewgame( XP_U16 id )
{
    XP_Bool ignored = XP_FALSE;
    switch ( id ) {
    case XW_PREFS_NOHINTS_CHECKBOX_ID:
    case XW_PREFS_BDSIZE_SELECTOR_ID:
    case XW_PREFS_TIMERON_CHECKBOX_ID:
    case XW_PREFS_TIMER_FIELD_ID:
#ifdef FEATURE_TRAY_EDIT
    case XW_PREFS_PICKTILES_CHECKBOX_ID:
#endif
        ignored = XP_TRUE;
        break;
    }
    return ignored;
} /* ignoredUnlessNewgame */

void
GlobalPrefsToLocal( PalmAppGlobals* globals )
{
    PrefsDlgState* state = globals->prefsDlgState;

    if ( !state ) {
        state = globals->prefsDlgState = XP_MALLOC( globals->mpool,
                                                    sizeof(*state) );
    }

    state->curBdSize = !!globals->game.model? 
        model_numRows( globals->game.model ) : PALM_MAX_ROWS;

    state->showColors = globals->gState.showColors;
    state->smartRobot = globals->util.gameInfo->robotSmartness == SMART_ROBOT;
    state->showGrid = globals->gState.showGrid;
    state->showProgress = globals->gState.showProgress;
    XP_MEMCPY( &state->cp, &globals->gState.cp, sizeof(state->cp) );
    XP_ASSERT( !!globals->game.server );

    state->phoniesAction = globals->util.gameInfo->phoniesAction;
    state->hintsNotAllowed = globals->gameInfo.hintsNotAllowed;
#ifdef XWFEATURE_SEARCHLIMIT
    state->allowHintRect = globals->gameInfo.allowHintRect;
#endif
    state->timerEnabled = globals->util.gameInfo->timerEnabled;
    state->gameSeconds = globals->util.gameInfo->gameSeconds;
#ifdef FEATURE_TRAY_EDIT
    state->allowPickTiles = globals->util.gameInfo->allowPickTiles;
#endif

#ifdef XWFEATURE_BLUETOOTH
    state->confirmBTConnect = globals->util.gameInfo->confirmBTConnect;
#endif

    state->stateTypeIsGlobal = globals->stateTypeIsGlobal;
} /* GlobalPrefsToLocal */

XP_Bool
LocalPrefsToGlobal( PalmAppGlobals* globals )
{
    PrefsDlgState* state = globals->prefsDlgState;
    XP_Bool erase = XP_FALSE;

    /* curBdSize handled elsewhere */

    globals->gState.showColors = state->showColors;

    globals->util.gameInfo->robotSmartness = 
        state->smartRobot? SMART_ROBOT:DUMB_ROBOT;
    
    erase = globals->gState.showGrid != state->showGrid;
    globals->gState.showGrid = state->showGrid;

    XP_MEMCPY( &globals->gState.cp, &state->cp, sizeof(globals->gState.cp) );

    globals->gState.showProgress = state->showProgress;

    globals->util.gameInfo->phoniesAction = state->phoniesAction;

    globals->gameInfo.hintsNotAllowed = state->hintsNotAllowed;
#ifdef XWFEATURE_SEARCHLIMIT
    globals->gameInfo.allowHintRect = state->allowHintRect;
#endif
    globals->util.gameInfo->timerEnabled = state->timerEnabled;
    globals->util.gameInfo->gameSeconds = state->gameSeconds;

#ifdef FEATURE_TRAY_EDIT
    globals->util.gameInfo->allowPickTiles = state->allowPickTiles;
#endif

#ifdef XWFEATURE_BLUETOOTH
    globals->util.gameInfo->confirmBTConnect = state->confirmBTConnect;
#endif

    return erase;
} /* LocalPrefsToGlobal */

static void
numToField( UInt16 id, XP_S16 num )
{
    FieldPtr field;
    char buf[16];

    StrIToA( buf, num );
    field = getActiveObjectPtr( id );
    FldInsert( field, buf, StrLen(buf) );
} /* numToField */

static void
localPrefsToControls( PrefsDlgState* state ) 
{
    setSelectorFromList( XW_PREFS_BDSIZE_SELECTOR_ID, 
                         state->playerBdSizeList, 
                         (PALM_MAX_ROWS - state->curBdSize) / 2 );

    setSelectorFromList( XW_PREFS_PHONIES_TRIGGER_ID, state->phoniesList, 
                         state->phoniesAction );

    setBooleanCtrl( XW_PREFS_PLAYERCOLORS_CHECKBOX_ID, state->showColors );
    setBooleanCtrl( XW_PREFS_PROGRESSBAR_CHECKBOX_ID, state->showProgress );
    setBooleanCtrl( XW_PREFS_NOHINTS_CHECKBOX_ID, state->hintsNotAllowed );
#ifdef XWFEATURE_SEARCHLIMIT
    setBooleanCtrl( XW_PREFS_HINTRECT_CHECKBOX_ID, state->allowHintRect );
#endif
    setBooleanCtrl( XW_PREFS_ROBOTSMART_CHECKBOX_ID, state->smartRobot );
    setBooleanCtrl( XW_PREFS_SHOWGRID_CHECKBOX_ID, state->showGrid );
    setBooleanCtrl( XW_PREFS_SHOWARROW_CHECKBOX_ID, state->cp.showBoardArrow );
    setBooleanCtrl( XW_PREFS_ROBOTSCORE_CHECKBOX_ID,
                    state->cp.showRobotScores );
    setBooleanCtrl( XW_PREFS_HIDETRAYVAL_CHECKBOX_ID, 
                    state->cp.hideTileValues );
    
    setBooleanCtrl( XW_PREFS_TIMERON_CHECKBOX_ID, state->timerEnabled );

#ifdef FEATURE_TRAY_EDIT
    setBooleanCtrl( XW_PREFS_PICKTILES_CHECKBOX_ID, state->allowPickTiles );
#endif

#ifdef XWFEATURE_BLUETOOTH
    setBooleanCtrl( XW_PREFS_BTCONFIRM_CHECKBOX_ID, state->confirmBTConnect );
#endif

    numToField( XW_PREFS_TIMER_FIELD_ID, state->gameSeconds/60 );
} /* localPrefsToControls */

static XP_S16
fieldToNum( UInt16 id )
{
    FieldPtr field;
    char* txt;

    field = getActiveObjectPtr( id );
    txt = FldGetTextPtr( field );
    return StrAToI( txt );
} /* fieldToNum */

static void
controlsToLocalPrefs( PrefsDlgState* state )
{
    state->showColors = getBooleanCtrl( XW_PREFS_PLAYERCOLORS_CHECKBOX_ID );
    state->smartRobot = getBooleanCtrl( XW_PREFS_ROBOTSMART_CHECKBOX_ID );
    state->showGrid = getBooleanCtrl( XW_PREFS_SHOWGRID_CHECKBOX_ID );
    state->cp.showBoardArrow = 
        getBooleanCtrl( XW_PREFS_SHOWARROW_CHECKBOX_ID );
    state->cp.showRobotScores = 
        getBooleanCtrl( XW_PREFS_ROBOTSCORE_CHECKBOX_ID );
    state->cp.hideTileValues = 
        getBooleanCtrl( XW_PREFS_HIDETRAYVAL_CHECKBOX_ID );

    state->showProgress = getBooleanCtrl( XW_PREFS_PROGRESSBAR_CHECKBOX_ID );

    /* trapping ctlEnterEvent should mean it can't have changed, so no need
       to test before grabbing the value. */
    state->hintsNotAllowed = getBooleanCtrl( XW_PREFS_NOHINTS_CHECKBOX_ID );
#ifdef XWFEATURE_SEARCHLIMIT
    state->allowHintRect = getBooleanCtrl( XW_PREFS_HINTRECT_CHECKBOX_ID );
#endif

    state->timerEnabled = getBooleanCtrl( XW_PREFS_TIMERON_CHECKBOX_ID );
    state->gameSeconds = fieldToNum( XW_PREFS_TIMER_FIELD_ID ) * 60;

#ifdef FEATURE_TRAY_EDIT
    state->allowPickTiles = 
        getBooleanCtrl( XW_PREFS_PICKTILES_CHECKBOX_ID );
#endif

#ifdef XWFEATURE_BLUETOOTH
    state->confirmBTConnect = getBooleanCtrl( XW_PREFS_BTCONFIRM_CHECKBOX_ID );
#endif

} /* controlsToLocalPrefs */

static void
drawPrefsTypeGadgets( PalmAppGlobals* globals )
{
    ListPtr list;
    UInt16 active;

    list = getActiveObjectPtr( XW_PREFS_TYPES_LIST_ID );
    XP_ASSERT( !!list );
    XP_ASSERT( !!globals->prefsDlgState );

    active = globals->prefsDlgState->stateTypeIsGlobal ?
        XW_PREFS_ALLGAMES_GADGET_ID : XW_PREFS_ONEGAME_GADGET_ID;

    drawGadgetsFromList( list, XW_PREFS_ALLGAMES_GADGET_ID,
                         XW_PREFS_ONEGAME_GADGET_ID, active );
#ifdef XWFEATURE_FIVEWAY
    drawFocusRingOnGadget( globals, XW_PREFS_ALLGAMES_GADGET_ID,
                           XW_PREFS_ONEGAME_GADGET_ID );
#endif
    LOG_RETURN_VOID();
} /* drawPrefsTypeGadgets */

static void
doOneSet( FormPtr form, XP_U16 first, XP_U16 last, XP_Bool enable, 
          XP_Bool isNewGame )
{
    while ( first <= last ) {
        XP_TriEnable stat = enable? TRI_ENAB_ENABLED : TRI_ENAB_HIDDEN;

        if ( enable && !isNewGame && ignoredUnlessNewgame( first ) ) {
            stat = TRI_ENAB_DISABLED;
        }
        disOrEnableTri( form, first, stat );

        ++first;
    }
} /* doOneSet */

/* Which set of controls is supposed to be visible changes depending on which
 * gadget is selected.
 */
static void
showHidePrefsWidgets( PalmAppGlobals* globals, FormPtr form )
{
    XP_Bool isGlobal;
    XP_Bool isNewGame = globals->isNewGame;
    XP_U16 firstToEnable, lastToEnable, firstToDisable, lastToDisable;

    isGlobal = globals->prefsDlgState->stateTypeIsGlobal;

    /* Need to do the disabling first */
    if ( isGlobal ) {
        firstToEnable = XW_PREFS_FIRST_GLOBAL_ID;
        lastToEnable =  XW_PREFS_LAST_GLOBAL_ID;
        firstToDisable = XW_PREFS_FIRST_PERGAME_ID;
        lastToDisable =  XW_PREFS_LAST_PERGAME_ID;
    } else {
        firstToDisable = XW_PREFS_FIRST_GLOBAL_ID;
        lastToDisable =  XW_PREFS_LAST_GLOBAL_ID;
        firstToEnable = XW_PREFS_FIRST_PERGAME_ID;
        lastToEnable =  XW_PREFS_LAST_PERGAME_ID;
    }

    doOneSet( form, firstToDisable, lastToDisable, XP_FALSE, isNewGame );
    doOneSet( form, firstToEnable, lastToEnable, XP_TRUE, isNewGame );

    if ( !isGlobal ) {
        Boolean on = getBooleanCtrl( XW_PREFS_TIMERON_CHECKBOX_ID );
        disOrEnable( form, XW_PREFS_TIMER_FIELD_ID, on );
    }
} /* showHidePrefsWidgets */

static void
checkPrefsHiliteGadget( PalmAppGlobals* globals, UInt16 selGadget )
{
    FormPtr form = FrmGetActiveForm();
    Boolean result = false;

    XP_Bool globalChosen = selGadget == XW_PREFS_ALLGAMES_GADGET_ID;
    XP_LOGF( "%s: selGadget=%d", __func__, selGadget );

    result = globalChosen != globals->prefsDlgState->stateTypeIsGlobal;
	    
    if ( result ) {
        globals->prefsDlgState->stateTypeIsGlobal = globalChosen;

        showHidePrefsWidgets( globals, form );
        drawPrefsTypeGadgets( globals );
    }
} /* checkPrefsHiliteGadget */
