/* -*-mode: C; fill-column: 77; c-basic-offset: 4; -*- */
/* 
 * Copyright 1999 - 2009 by Eric House (xwords@eehouse.org).  All rights
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

#include <PalmTypes.h>
#include <Form.h>
#include <List.h>
#include <Chars.h>      /* for nextFieldChr */
#include <Graffiti.h>   /* for GrfSetState */
#include <Event.h>
#ifdef XWFEATURE_FIVEWAY
# include <Hs.h>
#endif

#include "callback.h"
#include "comtypes.h"
#include "palmmain.h"
#include "comms.h"
#include "strutils.h"
#include "newgame.h"
#include "xwords4defines.h"
#include "dictui.h"
#include "palmdict.h"
#include "palmutil.h"
#include "palmir.h"
#include "prefsdlg.h"
#include "connsdlg.h"
#include "LocalizedStrIncludes.h"

static void handlePasswordTrigger( PalmAppGlobals* globals, 
                                   UInt16 controlID );
static XP_Bool updatePlayerInfo( PalmAppGlobals* globals );
static void loadNewGameState( PalmAppGlobals* globals );
static void unloadNewGameState( PalmAppGlobals* globals );
static void setNameThatFits( PalmNewGameState* state );

static void palmEnableColProc( void* closure, XP_U16 player, 
                               NewGameColumn col, XP_TriEnable enable );
static void palmEnableAttrProc( void* closure, NewGameAttr attr, 
                                XP_TriEnable enable );
static void palmGetColProc( void* closure, XP_U16 player, NewGameColumn col, 
                            NgCpCallbk cpcb, const void* cbClosure );
static void palmSetColProc( void* closure, XP_U16 player, NewGameColumn col, 
                            const NGValue value );
static void palmSetAttrProc( void* closure, NewGameAttr attr, 
                             const NGValue value );
static void handleRobotChanged( PalmNewGameState* state, XP_U16 controlID,
                                XP_Bool on );

#ifndef XWFEATURE_STANDALONE_ONLY
static void handleRemoteChanged( PalmNewGameState* state, XP_U16 controlID,
                                 XP_Bool on );
static Boolean checkHiliteGadget(PalmAppGlobals* globals, 
                                 const EventType* event,
                                 PalmNewGameState* state );
static void drawConnectGadgets( PalmAppGlobals* globals );
static void changeGadgetHilite( PalmAppGlobals* globals, UInt16 hiliteID );

#else
# define checkHiliteGadget(globals, event, state) XP_FALSE
# define drawConnectGadgets( globals )
#endif

/*****************************************************************************
 *
 ****************************************************************************/
Boolean
newGameHandleEvent( EventPtr event )
{
    Boolean result = false;
    PalmAppGlobals* globals;
    FormPtr form;
    CurGameInfo* gi;
    PalmNewGameState* state;
    Int16 chosen;
    XP_S16 itemId;
    Boolean on;

    CALLBACK_PROLOGUE();

    globals = getFormRefcon();
    gi = &globals->gameInfo;
    state = &globals->newGameState;

    switch ( event->eType ) {

    case frmOpenEvent:

        GlobalPrefsToLocal( globals );

        form = FrmGetActiveForm();

        XP_ASSERT( !state->ngc );
        XP_MEMSET( state, 0, sizeof(*state) );

        state->form = form;

        loadNewGameState( globals );
        if ( !globals->isNewGame ) {
            disOrEnableTri( form, XW_DICT_SELECTOR_ID, TRI_ENAB_DISABLED );
        }

        XP_ASSERT( !!state->dictName );
        setNameThatFits( state );

        XP_ASSERT( !!globals->game.server );
    case frmUpdateEvent:
        GrfSetState( false, false, false );
        FrmDrawForm( FrmGetActiveForm() );

        drawConnectGadgets( globals );

        result = true;
        break;

#ifdef XWFEATURE_FIVEWAY
        /* docs say to return HANDLED for both take and lost if the right
           object */
    case frmObjectFocusTakeEvent:
    case frmObjectFocusLostEvent:
        result = considerGadgetFocus( globals, event, XW_SOLO_GADGET_ID, 
                                      XW_CLIENT_GADGET_ID );
        break;
#endif

    case penDownEvent:
        result = checkHiliteGadget( globals, event, state );
        break;

#ifdef XWFEATURE_FIVEWAY
    case keyDownEvent:
        itemId = getFocusOwner();
        if ( itemId >= 0 ) {
            result = tryRockerKey( event->data.keyDown.chr, itemId,
                                   XW_SOLO_GADGET_ID, XW_CLIENT_GADGET_ID );
            if ( result ) {
                changeGadgetHilite( globals, itemId );
            }
        }
        break;
#endif

    case prefsChangedEvent:
        state->forwardChange = true;
        break;

    case ctlSelectEvent:
        result = true;
        itemId = event->data.ctlSelect.controlID;
        on = event->data.ctlSelect.on;
        switch ( itemId ) {

        case XW_ROBOT_1_CHECKBOX_ID:
        case XW_ROBOT_2_CHECKBOX_ID:
        case XW_ROBOT_3_CHECKBOX_ID:
        case XW_ROBOT_4_CHECKBOX_ID:
            handleRobotChanged( state, itemId, on );
            break;
#ifndef XWFEATURE_STANDALONE_ONLY
        case XW_REMOTE_1_CHECKBOX_ID:
        case XW_REMOTE_2_CHECKBOX_ID:
        case XW_REMOTE_3_CHECKBOX_ID:
        case XW_REMOTE_4_CHECKBOX_ID:
            handleRemoteChanged( state, itemId, on );
            break;
#endif

        case XW_NPLAYERS_SELECTOR_ID:
            XP_ASSERT( globals->isNewGame );
            chosen = LstPopupList( state->playerNumList );
            if ( chosen >= 0 ) {
                NGValue value;
                setSelectorFromList( XW_NPLAYERS_SELECTOR_ID,
                                     state->playerNumList,
                                     chosen );
                value.ng_u16 = chosen + 1;
                newg_attrChanged( state->ngc, NG_ATTR_NPLAYERS, value );
            }
            break;

        case XW_DICT_SELECTOR_ID:
            XP_ASSERT( globals->isNewGame );
            globals->dictuiForBeaming = false;
            FrmPopupForm( XW_DICTINFO_FORM );

            /* popup dict selection dialog -- or maybe just a list if there
               are no preferences to set. The results should all be
               cancellable, so don't delete the existing dictionary (if
               any) until OK is chosen */
            break;

        case XW_GINFO_JUGGLE_ID:
            while ( !newg_juggle( state->ngc ) ) {
            }
            (void)newg_juggle( state->ngc );
            break;

        case XW_OK_BUTTON_ID:
            if ( updatePlayerInfo( globals ) ) {
                /* if we put up the prefs form from within this one and the user
                   clicked ok, we need to make sure the main form gets the
                   notification so it can make use of any changes.  This event
                   needs to arrive before the newGame event so any changes will
                   be incorporated. */
                if ( state->forwardChange ) {
                    postEmptyEvent( prefsChangedEvent );
                    state->forwardChange = false;
                }

                if ( globals->isNewGame ) {
                    postEmptyEvent( newGameOkEvent );
                    globals->postponeDraw = true;
                }

                unloadNewGameState( globals );

                FrmReturnToForm( 0 );
            }
            break;

        case XW_CANCEL_BUTTON_ID:
            unloadNewGameState( globals );
            postEmptyEvent( newGameCancelEvent );
            FrmReturnToForm( 0 );
            break;

        case XW_PREFS_BUTTON_ID:
            /* bring up with the this-game tab selected */
            XP_ASSERT( !!globals->prefsDlgState );
            globals->prefsDlgState->stateTypeIsGlobal = false;
            FrmPopupForm( XW_PREFS_FORM );
            break;

        case XW_PLAYERPASSWD_1_TRIGGER_ID:
        case XW_PLAYERPASSWD_2_TRIGGER_ID:
        case XW_PLAYERPASSWD_3_TRIGGER_ID:
        case XW_PLAYERPASSWD_4_TRIGGER_ID:
            handlePasswordTrigger( globals, itemId );
            break;

        default:      /* one of the password selectors? */
            result = false;
        }
        break;

    case dictSelectedEvent:
        /* posted by the form we raise when user clicks Dictionary selector
           above. */
        if ( state->dictName != NULL ) {
            XP_FREE( globals->mpool, state->dictName );
        }
        state->dictName = 
            ((DictSelectedData*)&event->data.generic)->dictName;
        setNameThatFits( state );
        break;

    case appStopEvent:      /* I get these on older palms */
        unloadNewGameState( globals );
        result = false;

    default:
        break;
    }

    CALLBACK_EPILOGUE();
    return result;
} /* newGameHandleEvent */

static void
setNameThatFits( PalmNewGameState* state ) 
{
    RectangleType rect;
    XP_U16 width;
    XP_U16 len = XP_STRLEN( (const char*)state->dictName );

    XP_MEMCPY( state->shortDictName, state->dictName, len + 1 );

    /* The width available is the cancel button's left minus ours */
    getObjectBounds( XW_CANCEL_BUTTON_ID, &rect );
    width = rect.topLeft.x;
    getObjectBounds( XW_DICT_SELECTOR_ID, &rect );
    width -= (rect.topLeft.x + 6);
    
    for ( ; FntCharsWidth( (const char*)state->dictName, len ) > width;  --len ) {
        /* do nothing */
    }

    state->shortDictName[len] = '\0';
    CtlSetLabel( getActiveObjectPtr( XW_DICT_SELECTOR_ID ), 
                 (const char*)state->shortDictName );
} /* setNameThatFits */

/* 
 * Copy the local state into global state.
 */
static XP_Bool
updatePlayerInfo( PalmAppGlobals* globals )
{
    CurGameInfo* gi;
    PalmNewGameState* state = &globals->newGameState;
    XP_Bool success;

    gi = &globals->gameInfo;
    success = newg_store( state->ngc, gi, XP_TRUE );
    if ( success ) {
        gi->boardSize = globals->prefsDlgState->curBdSize;

        replaceStringIfDifferent( globals->mpool, &gi->dictName, 
                                  globals->newGameState.dictName );
    }
    return success;
} /* updatePlayerInfo */

/* Frame 'em, draw their text, and highlight the one that's selected
 */
#ifndef XWFEATURE_STANDALONE_ONLY

static void
drawConnectGadgets( PalmAppGlobals* globals )
{
    XP_U16 hiliteItem = globals->newGameState.curServerHilite 
        + XW_SOLO_GADGET_ID ;
    ListPtr list = getActiveObjectPtr( XW_SERVERTYPES_LIST_ID );
    XP_ASSERT( !!list );

    drawGadgetsFromList( list, XW_SOLO_GADGET_ID, XW_CLIENT_GADGET_ID,
                         hiliteItem );
#ifdef XWFEATURE_FIVEWAY
    drawFocusRingOnGadget( globals, XW_SOLO_GADGET_ID, XW_CLIENT_GADGET_ID );
#endif
} /* drawConnectGadgets */

static void
changeGadgetHilite( PalmAppGlobals* globals, UInt16 hiliteID )
{
    PalmNewGameState* state = &globals->newGameState;
    XP_Bool isNewGame = globals->isNewGame;

    hiliteID -= XW_SOLO_GADGET_ID;

    if ( hiliteID != state->curServerHilite ) {
        /* if it's not a new game, don't recognize the change */
        if ( isNewGame ) {
            NGValue value;

            state->curServerHilite = hiliteID;
            drawConnectGadgets( globals );

            value.ng_role = hiliteID;
            newg_attrChanged( state->ngc, NG_ATTR_ROLE, value );
        } else {
            beep();
        }
    }

#if defined XWFEATURE_BLUETOOTH || defined XWFEATURE_RELAY
    /* Even if it didn't change, pop the connections form.  It's only
       informational in the non-new-game case; nothing can be changed. */
    if ( hiliteID != SERVER_STANDALONE ) {
        if ( isNewGame || (hiliteID==globals->newGameState.curServerHilite) ) {
            PopupConnsForm( globals, hiliteID, &state->addr );
        }
    }
#endif
} /* changeGadgetHilite */

static Boolean
checkHiliteGadget( PalmAppGlobals* globals, const EventType* event,
                   PalmNewGameState* XP_UNUSED_DBG(state) )
{
    Boolean result = false;
    UInt16 selGadget;

    XP_ASSERT( &globals->newGameState == state );

    result = penInGadget( event, &selGadget );
    if ( result ) {
        changeGadgetHilite( globals, selGadget );
    }

    return result;
} /* checkHiliteGadget */
#endif

static void
showMaskedPwd( XP_U16 objectID, XP_Bool set )
{
    const char* label = set? "*" : "   ";
    /* control owns the string passed in */
    CtlSetLabel( getActiveObjectPtr( objectID ), label );
}

/* If there's currently no password set, just let 'em set one.  If there is
 * one set, they need to know the old before setting the new.
 */
static void
handlePasswordTrigger( PalmAppGlobals* globals, UInt16 controlID )
{
    UInt16 playerNum;
    PalmNewGameState* state = &globals->newGameState;
    XP_UCHAR* name;
    FieldPtr nameField;
    XP_U16 len;

    playerNum = (controlID - XW_PLAYERPASSWD_1_TRIGGER_ID) / NUM_PLAYER_COLS;
    XP_ASSERT( playerNum < MAX_NUM_PLAYERS );

    nameField = getActiveObjectPtr( XW_PLAYERNAME_1_FIELD_ID +
                                    (NUM_PLAYER_COLS * playerNum) );
    name = (XP_UCHAR*)FldGetTextPtr( nameField );

    len = sizeof(state->passwds[playerNum]);
    if ( askPassword( globals, name, true, state->passwds[playerNum], &len )) {
        showMaskedPwd( controlID, len > 0 );
    }
} /* handlePasswordTrigger */

static void
unloadNewGameState( PalmAppGlobals* globals )
{
    PalmNewGameState* state = &globals->newGameState;

    XP_FREE( globals->mpool, state->dictName );
    state->dictName = NULL;

    newg_destroy( state->ngc );
    state->ngc = NULL;
} /* unloadNewGameState */

static XP_U16
getBaseForCol( NewGameColumn col )
{
    XP_U16 resID = 0;
    switch ( col ) {
#ifndef XWFEATURE_STANDALONE_ONLY        
    case NG_COL_REMOTE:
        resID = XW_REMOTE_1_CHECKBOX_ID;
        break;
#endif
    case NG_COL_NAME:
        resID = XW_PLAYERNAME_1_FIELD_ID;
        break;
    case NG_COL_ROBOT:
        resID = XW_ROBOT_1_CHECKBOX_ID;
        break;
    case NG_COL_PASSWD:
        resID = XW_PLAYERPASSWD_1_TRIGGER_ID;
        break;
    default:
        XP_ASSERT( XP_FALSE );
    }
    XP_ASSERT( !!resID );
    return resID;
} /* getBaseForCol */

static XP_U16
objIDForCol( XP_U16 player, NewGameColumn col )
{
    XP_U16 base = getBaseForCol( col );
    return base + (NUM_PLAYER_COLS * player);
}

static ControlPtr
getControlForCol( XP_U16 player, NewGameColumn col )
{
    XP_U16 objID = objIDForCol( player, col );
    ControlPtr ctrl = getActiveObjectPtr( objID );
    return ctrl;
}

static void
palmEnableColProc( void* closure, XP_U16 player, NewGameColumn col, 
                   XP_TriEnable enable )
{
    PalmAppGlobals* globals = (PalmAppGlobals*)closure;
    PalmNewGameState* state = &globals->newGameState;
    XP_U16 objID;

    /* If it's a field, there need to be three states */
    objID = objIDForCol( player, col );
    disOrEnableTri( state->form, objID, enable );
}
 
/* Palm doesn't really do "disabled."  Things are visible or not.  But we
 * want the player count dropdown in particular visible since it give
 * information.  So different objects get treated differently.  The code
 * handling ctlEnterEvent can disable for non-newgame dialogs even if a
 * control is technically enabled.
 */
static void
palmEnableAttrProc(void* closure, NewGameAttr attr, XP_TriEnable ngEnable )
{
    PalmAppGlobals* globals = (PalmAppGlobals*)closure;
    PalmNewGameState* state = &globals->newGameState;
    XP_U16 objID = 0;

    switch ( attr ) {
#ifndef XWFEATURE_STANDALONE_ONLY
    case NG_ATTR_ROLE:
        /* always enabled */
        break;
    case NG_ATTR_REMHEADER:
        objID = XW_LOCAL_LABEL_ID;
        break;
    case NG_ATTR_CANCONFIG:     /* not needed on palm */
        break;
#endif
    case NG_ATTR_NPLAYERS:
        objID = XW_NPLAYERS_SELECTOR_ID;
        break;
    case NG_ATTR_NPLAYHEADER:
        break;
    case NG_ATTR_CANJUGGLE:
        objID = XW_GINFO_JUGGLE_ID;
        break;
    }

    if ( objID != 0 ) {
        disOrEnableTri( state->form, objID, ngEnable );
    }
} /* palmEnableAttrProc */

static void
palmGetColProc( void* closure, XP_U16 player, NewGameColumn col, 
                NgCpCallbk cpcb, const void* cbClosure )
{
    PalmAppGlobals* globals;
    PalmNewGameState* state;
    NGValue value;
    XP_U16 objID = objIDForCol( player, col );

    switch ( col ) {
#ifndef XWFEATURE_STANDALONE_ONLY
    case NG_COL_REMOTE:
#endif
    case NG_COL_ROBOT:
        value.ng_bool = getBooleanCtrl( objID );
        break;
    case NG_COL_NAME:
        value.ng_cp = FldGetTextPtr( getActiveObjectPtr( objID ) );
        break;
    case NG_COL_PASSWD:
        globals = (PalmAppGlobals*)closure;
        state = &globals->newGameState;
        value.ng_cp = state->passwds[player];
        break;
    default:
        XP_ASSERT(0);
    }

    (*cpcb)( value, cbClosure );
}

static void
palmSetColProc( void* closure, XP_U16 player, NewGameColumn col, 
                const NGValue value )
{
    PalmAppGlobals* globals = (PalmAppGlobals*)closure;
    PalmNewGameState* state = &globals->newGameState;
    ControlPtr ctrl;
    XP_U16 objID = objIDForCol( player, col );

    switch ( col ) {
#ifndef XWFEATURE_STANDALONE_ONLY
    case NG_COL_REMOTE:
#endif
    case NG_COL_ROBOT:
        ctrl = getControlForCol( player, col );
        CtlSetValue( ctrl, value.ng_bool );
        break;
    case NG_COL_NAME:
        setFieldStr( objID, value.ng_cp );
        break;
    case NG_COL_PASSWD:
        if ( !!value.ng_cp ) {
            XP_SNPRINTF( state->passwds[player], sizeof(state->passwds[player]),
                         "%s", value.ng_cp );
            showMaskedPwd( objID, *value.ng_cp != '\0' );
        }

    default:                    /* shut up compiler */
        break;
    }
} /* palmSetColProc */

static void
palmSetAttrProc( void* closure, NewGameAttr attr, const NGValue value )
{
    PalmAppGlobals* globals = (PalmAppGlobals*)closure;
    PalmNewGameState* state = &globals->newGameState;

    switch ( attr ) {
#ifndef XWFEATURE_STANDALONE_ONLY
    case NG_ATTR_ROLE:
        state->curServerHilite = value.ng_role;
        break;
    case NG_ATTR_REMHEADER:
        break;
#endif
    case NG_ATTR_NPLAYERS:
        setSelectorFromList( XW_NPLAYERS_SELECTOR_ID, 
                             state->playerNumList, value.ng_u16 - 1 );
        break;
    case NG_ATTR_NPLAYHEADER: {
        FieldPtr field = getActiveObjectPtr( XW_TOTALP_FIELD_ID );
        char* cur = FldGetTextPtr( field );
        /* Need to update to get it drawn and that flashes the whole dialog.
           So avoid if possible. */
        if ( !cur || (0 != StrCompare( cur, value.ng_cp ) ) ) {
            FldSetTextPtr( field, (char*)value.ng_cp );
            FrmUpdateForm( 0, frmRedrawUpdateCode );
        }
    }
        break;
    case NG_ATTR_CANJUGGLE:
        XP_ASSERT(0);           /* doesn't make sense */
    case NG_ATTR_CANCONFIG:     /* not needed on palm */
        break;
    }
} /* palmSetAttrProc */

static XP_U16
palmPlayerFromID( XP_U16 id, XP_U16 base )
{
    XP_U16 player;
    player = (id - base) / NUM_PLAYER_COLS;
    return player;
} /* palmPlayerFromID */

static void
handleRobotChanged( PalmNewGameState* state, XP_U16 controlID, XP_Bool on )
{
    XP_U16 player;
    NGValue value;

    player = palmPlayerFromID( controlID, XW_ROBOT_1_CHECKBOX_ID );
    value.ng_bool = on;
    newg_colChanged( state->ngc, player );
}

#ifndef XWFEATURE_STANDALONE_ONLY
static void
handleRemoteChanged( PalmNewGameState* state, XP_U16 controlID, XP_Bool on )
{
    XP_U16 player;
    NGValue value;

    XP_LOGF( "%s: controlID=%d", __func__, controlID );

    player = palmPlayerFromID( controlID, XW_REMOTE_1_CHECKBOX_ID );
    value.ng_bool = on;
    newg_colChanged( state->ngc, player );
}
#endif

XP_U16
countXPorts( const PalmAppGlobals* globals )
{
    XP_U16 xports = 0;
#ifdef XWFEATURE_IR
    ++xports;
#endif
#ifdef XWFEATURE_BLUETOOTH
    if ( globals->hasBTLib ) {
        ++xports;
    }
#endif
#ifdef XWFEATURE_RELAY
    ++xports;
#endif
    return xports;
}

static void
loadNewGameState( PalmAppGlobals* globals )
{
    CurGameInfo* gi = &globals->gameInfo;
    PalmNewGameState* state = &globals->newGameState;

    state->dictName = copyString( globals->mpool, gi->dictName );
    state->playerNumList = getActiveObjectPtr( XW_NPLAYERS_LIST_ID );
    state->nXPorts = countXPorts( globals );

    XP_ASSERT( !state->ngc );
    state->ngc = newg_make( MPPARM(globals->mpool)
                            globals->isNewGame, 
                            &globals->util,
                            palmEnableColProc, 
                            palmEnableAttrProc, 
                            palmGetColProc,
                            palmSetColProc,
                            palmSetAttrProc,
                            globals );
    newg_load( state->ngc, gi );

#if defined XWFEATURE_BLUETOOTH || defined XWFEATURE_RELAY
    if ( globals->game.comms ) {
        comms_getAddr( globals->game.comms, &state->addr );
    } else {
        comms_getInitialAddr( &state->addr );
    }
#elif defined XWFEATURE_IR
    state->addr.conType = COMMS_CONN_IR;
#endif
} /* loadNewGameState */
