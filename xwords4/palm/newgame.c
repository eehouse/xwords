/* -*-mode: C; fill-column: 77; c-basic-offset: 4; -*- */
/* 
 * Copyright 1999 - 2001 by Eric House (fixin@peak.org).  All rights reserved.
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
#include <Chars.h>		/* for nextFieldChr */
#include <Graffiti.h>		/* for GrfSetState */
#include <Event.h>
#ifdef HS_DUO_SUPPORT
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

static void handlePasswordTrigger( PalmAppGlobals* globals, 
                                   UInt16 controlID );
static void adjustVisibility( PalmAppGlobals* globals, XP_Bool canDraw );
static void setNPlayersAndAdjust( PalmAppGlobals* globals, Int16 chosen );
static void updatePlayerInfo( PalmAppGlobals* globals );
static XP_Bool tryFieldNavigationKey( XP_U16 key );
static void loadNewGameState( PalmAppGlobals* globals );
static void unloadNewGameState( PalmAppGlobals* globals );
static void setNameThatFits( PalmNewGameState* state );
#ifdef HS_DUO_SUPPORT
static XP_Bool tryDuoRockerKey( PalmAppGlobals* globals,XP_U16 key );
static XP_Bool considerGadgetFocus( PalmNewGameState* state, EventType* event );
#else
# define tryDuoRockerKey(g,key) XP_FALSE
#endif

#ifndef XWFEATURE_STANDALONE_ONLY
static Boolean checkHiliteGadget(PalmAppGlobals* globals, EventType* event,
                                 PalmNewGameState* state );
static void drawConnectGadgets( PalmAppGlobals* globals );
static void changeGadgetHilite( PalmAppGlobals* globals, UInt16 hiliteID );

#else
# define checkHiliteGadget(globals, event, state) XP_FALSE
# define drawConnectGadgets( globals )
#endif

#define IS_SERVER_GADGET(id) \
     ((id) >= XW_SOLO_GADGET_ID && (id) <= XW_CLIENT_GADGET_ID)


/*****************************************************************************
 *
 ****************************************************************************/
Boolean
newGameHandleEvent( EventPtr event )
{
    Boolean result = false;
    EventType eventToPost;	/* used only with OK button */
    PalmAppGlobals* globals;
    FormPtr form;
    LocalPlayer* lp;
    CurGameInfo* gi;
    PalmNewGameState* state;
    Int16 chosen;
    XP_U16 i;
    XP_U16 controlID;
    XP_U16 index;
    Boolean on;
    Boolean canEdit;

    CALLBACK_PROLOGUE();

    globals = getFormRefcon();
    gi = &globals->gameInfo;
    state = &globals->newGameState;

    switch ( event->eType ) {

    case frmOpenEvent:

        GlobalPrefsToLocal( globals );

        loadNewGameState( globals );

        form = FrmGetActiveForm();
#ifndef XWFEATURE_STANDALONE_ONLY
        sizeGadgetsForStrings( form,
                               getActiveObjectPtr( XW_SERVERTYPES_LIST_ID ),
                               XW_SOLO_GADGET_ID );
#endif
        state->playerNumList = 
            getActiveObjectPtr( XW_NPLAYERS_LIST_ID );
        XP_ASSERT( state->playerNumList != NULL );

        setSelectorFromList( XW_NPLAYERS_SELECTOR_ID, 
                             state->playerNumList,
                             gi->nPlayers - 1 );

        XP_ASSERT( !!state->dictName );
        setNameThatFits( state );

        XP_ASSERT( !!globals->game.server );

        canEdit = state->curServerHilite == SERVER_STANDALONE
            || globals->isNewGame;

        /* load the fields from what we already have */
        for ( lp = gi->players, i = 0; i < MAX_NUM_PLAYERS; ++lp, ++i ) {
            XP_U16 offset = i * NUM_PLAYER_COLS;
            ControlPtr check;
            XP_UCHAR* name;
            FieldPtr nameField;

#ifndef XWFEATURE_STANDALONE_ONLY
            Boolean isLocal = lp->isLocal;
            check = getActiveObjectPtr(XW_REMOTE_1_CHECKBOX_ID + offset);
            CtlSetValue( check, !isLocal );
#endif
            check = getActiveObjectPtr(XW_ROBOT_1_CHECKBOX_ID+offset);
            CtlSetValue( check, lp->isRobot );

            nameField = getActiveObjectPtr(XW_PLAYERNAME_1_FIELD_ID+offset);
            name = lp->name;
            if ( !!name && !!*name ) {
                FldInsert( nameField, (const char*)name, 
                           XP_STRLEN((const char*)name) );
            }
            setFieldEditable( nameField, canEdit );

            /* set up the password */
            if ( !!lp->password ) {
                CtlSetLabel( getActiveObjectPtr( 
                                XW_PLAYERPASSWD_1_TRIGGER_ID+offset ),
                             "*" );
            }
        }
        /* 	form = FrmGetActiveForm(); */
        FrmSetFocus(form, FrmGetObjectIndex(form, XW_PLAYERNAME_1_FIELD_ID));
	
    case frmUpdateEvent:
        adjustVisibility( globals, XP_FALSE );

        GrfSetState( false, false, false );
        FrmDrawForm( FrmGetActiveForm() );

        drawConnectGadgets( globals );

        result = true;
        break;

#ifdef BEYOND_IR
    case connsSettingChgEvent:
        XP_ASSERT( globals->isNewGame );
        state->connsSettingChanged = XP_TRUE;
        break;
#endif

#ifdef HS_DUO_SUPPORT
    case frmObjectFocusTakeEvent:
    case frmObjectFocusLostEvent:
        result = considerGadgetFocus( state, event );
        break;
#endif

    case penDownEvent:
        result = checkHiliteGadget( globals, event, state );
        break;

    case keyDownEvent:
        result = tryFieldNavigationKey( event->data.keyDown.chr )
            || tryDuoRockerKey( globals, event->data.keyDown.chr );
        break;

    case prefsChangedEvent:
        state->forwardChange = true;
        break;

    case ctlEnterEvent:
        switch ( event->data.ctlEnter.controlID ) {
#ifndef XWFEATURE_STANDALONE_ONLY
        case XW_REMOTE_1_CHECKBOX_ID:
        case XW_REMOTE_2_CHECKBOX_ID:
        case XW_REMOTE_3_CHECKBOX_ID:
        case XW_REMOTE_4_CHECKBOX_ID:
#endif
        case XW_DICT_SELECTOR_ID:
        case XW_NPLAYERS_SELECTOR_ID:
            if ( !globals->isNewGame ) {
                result = true;
                beep();
            }
        }
        break;

    case ctlSelectEvent:
        result = true;
        controlID = event->data.ctlSelect.controlID;
        on = event->data.ctlSelect.on;
        switch ( controlID ) {

        case XW_ROBOT_1_CHECKBOX_ID:
        case XW_ROBOT_2_CHECKBOX_ID:
        case XW_ROBOT_3_CHECKBOX_ID:
        case XW_ROBOT_4_CHECKBOX_ID:
            index = (controlID - XW_ROBOT_1_CHECKBOX_ID) / NUM_PLAYER_COLS;
            state->isRobot[index] = on;
            adjustVisibility( globals, XP_TRUE );
            break;
#ifndef XWFEATURE_STANDALONE_ONLY
        case XW_REMOTE_1_CHECKBOX_ID:
        case XW_REMOTE_2_CHECKBOX_ID:
        case XW_REMOTE_3_CHECKBOX_ID:
        case XW_REMOTE_4_CHECKBOX_ID:
            XP_ASSERT( state->curServerHilite == SERVER_ISSERVER );
            index = (controlID - XW_REMOTE_1_CHECKBOX_ID) / NUM_PLAYER_COLS;
            state->isLocal[index] = !on;
            state->curNPlayersLocal += on? -1:1;
            adjustVisibility( globals, XP_TRUE );
            break;
#endif

        case XW_NPLAYERS_SELECTOR_ID:
            XP_ASSERT( globals->isNewGame );
            chosen = LstPopupList( state->playerNumList );
            if ( chosen >= 0 ) {
                setSelectorFromList( XW_NPLAYERS_SELECTOR_ID, 
                                     state->playerNumList,
                                     chosen );
                ++chosen;	/* chosen is 0-based */
                if (state->curServerHilite==SERVER_ISCLIENT) {
                    state->curNPlayersLocal = chosen;
                    XP_ASSERT( state->curNPlayersLocal <= MAX_NUM_PLAYERS );
                } else {
                    state->curNPlayersTotal = chosen;
                }
                setNPlayersAndAdjust( globals, chosen );
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

        case XW_OK_BUTTON_ID:

            /* if we put up the prefs form from within this one and the user
               clicked ok, we need to make sure the main form gets the
               notification so it can make use of any changes.  This event
               needs to arrive before the newGame event so any changes will
               be incorporated. */
            if ( state->forwardChange ) {
                eventToPost.eType = prefsChangedEvent;
                EvtAddEventToQueue( &eventToPost );
                state->forwardChange = false;
            }

            if ( globals->isNewGame ) {
                updatePlayerInfo( globals );

                eventToPost.eType = newGameOkEvent;
                EvtAddEventToQueue( &eventToPost );
                globals->postponeDraw = true;

            } else if ( state->curServerHilite
                        == SERVER_STANDALONE ) {
                updatePlayerInfo( globals );
            }

            unloadNewGameState( globals );

            FrmReturnToForm( 0 );
            break;

        case XW_CANCEL_BUTTON_ID:
            unloadNewGameState( globals );
            eventToPost.eType = newGameCancelEvent;
            EvtAddEventToQueue( &eventToPost );
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
            handlePasswordTrigger( globals, controlID );
            break;

        default:		/* one of the password selectors? */
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

static XP_U16
countLocalIn( PalmNewGameState* state, XP_U16 nPlayers )
{
    XP_U16 nLocal = 0;
    XP_U16 i;

    for ( i = 0; i < nPlayers; ++i ) {
        if ( state->isLocal[i] ) {
            ++nLocal;
        }
    }

    return nLocal;
} /* countLocalIn */

/* If we're in GUEST mode, only local players are visible, and so this means
 * an increase in the number of local players.  If we're in a different mode
 * then it means a simple increase in all players.  Only the first case is
 * difficult, because if the number's getting larger we need to confirm that
 * there's another local player to show, and if there's not we need to
 * convert the first non-local player.
 */
static void
setNPlayersAndAdjust( PalmAppGlobals* globals, Int16 chosen )
{
#ifndef XWFEATURE_STANDALONE_ONLY
    PalmNewGameState* state = &globals->newGameState;

    if ( state->curServerHilite == SERVER_ISCLIENT ) {
        XP_U16 i;
        XP_S16 nRemote = 0;
        XP_S16 nLocal = 0;

        /* find the first non-local player */
        for ( i = 0; i < MAX_NUM_PLAYERS; ++i ) {
            if ( state->isLocal[i] ) {
                ++nLocal;
            } else {
                ++nRemote;
            }
        }

        /* Make as many local as necessary */
        for ( i = 0; nLocal < chosen && i < MAX_NUM_PLAYERS; ++i ) {
            if ( !state->isLocal[i] ) {
                state->isLocal[i] = true;
                setBooleanCtrl( XW_REMOTE_1_CHECKBOX_ID +
                                (i * NUM_PLAYER_COLS), false );
                ++nLocal;
                --nRemote;
            }
        }

        state->curNPlayersLocal = chosen;
        XP_ASSERT( state->curNPlayersLocal <= MAX_NUM_PLAYERS );
        state->curNPlayersTotal = chosen + nRemote;
        XP_ASSERT( state->curNPlayersTotal <= MAX_NUM_PLAYERS );
    } else {
        state->curNPlayersTotal = chosen;
        state->curNPlayersLocal = countLocalIn( state, chosen );
        XP_ASSERT( state->curNPlayersLocal <= MAX_NUM_PLAYERS );
    }
#endif

    adjustVisibility( globals, XP_TRUE );
} /* setNPlayersAndAdjust */

static Boolean
tryFieldNavigationKey( XP_U16 key )
{
    FormPtr form;
    Int16 curFocus, nextFocus, change;
    UInt16 nObjects;

    if ( key == prevFieldChr ) {
        change = -1;
    } else if ( key == nextFieldChr ) {
        change = 1;
    } else {
        return false;
    }

    form = FrmGetActiveForm();
    curFocus = nextFocus = FrmGetFocus( form );
    nObjects = FrmGetNumberOfObjects(form);

    /* find the next (in either direction) usable field */
    for ( ; ; ) {
        nextFocus += change;

        if ( nextFocus == nObjects ) {
            nextFocus = 0;
        } else if ( nextFocus < 0 ) {
            nextFocus = nObjects-1;
        }

        if ( nextFocus == curFocus ) {
            break;
        } else if ( FrmGetObjectType(form, nextFocus) != frmFieldObj ) {
            continue;
        } else {
            FieldPtr field = FrmGetObjectPtr( form, nextFocus );
            FieldAttrType attrs;
            FldGetAttributes( field, &attrs );
            if ( attrs.usable ) {
                break;
            }
        }
    }

    FrmSetFocus( form, nextFocus );
    return true;
} /* tryFieldNavigationKey */

#ifdef HS_DUO_SUPPORT
#ifdef DEBUG
static XP_UCHAR*
keyToStr( XP_U16 key )
{
#define keyCase(k)  case (k): return #k
    switch( key ) {
        keyCase(vchrRockerUp);
        keyCase(vchrRockerDown);
        keyCase(vchrRockerLeft);
        keyCase(vchrRockerRight);
        keyCase(vchrRockerCenter);
    default:
        return "huh?";
    }
#undef keyCase
}
#endif

static XP_Bool
tryDuoRockerKey( PalmAppGlobals* globals, XP_U16 key )
{
    XP_Bool result = XP_FALSE;
    XP_U16 focusID;
    FormPtr form;

    switch( key ) {
    case vchrRockerUp:
    case vchrRockerDown:
    case vchrRockerLeft:
    case vchrRockerRight:
        XP_LOGF( "got rocker key: %s", keyToStr(key) );
        result = XP_FALSE;
        break;
    case vchrRockerCenter:
        /* if one of the gadgets is focussed, "tap" it. */
        XP_LOGF( "got rocker key: %s", keyToStr(key) );
        form = FrmGetActiveForm();
        focusID = FrmGetObjectId( form, FrmGetFocus( form ) );
        if ( IS_SERVER_GADGET( focusID ) ) {
            changeGadgetHilite( globals, focusID );
            result = XP_TRUE;
        } else {
            XP_LOGF( "%d not server gadget", focusID );
        }
        break;
    default:
        break;
    }
    return result;
} /* tryDuoRockerKey */
#endif

static void
adjustVisibility( PalmAppGlobals* globals, XP_Bool canDraw )
{
    FormPtr form = FrmGetActiveForm();
    short i;
    PalmNewGameState* state = &globals->newGameState;
    XP_Bool isNewGame = globals->isNewGame;

    Connectedness curServerHilite = state->curServerHilite;
    Boolean canShowRemote = curServerHilite != SERVER_STANDALONE;
    Boolean isClient = curServerHilite == SERVER_ISCLIENT;
    XP_U16 nShown = 0;
    Boolean canEdit = (curServerHilite == SERVER_STANDALONE) || isNewGame;
    XP_U16 nToShow = (isClient && isNewGame)? 
        state->curNPlayersLocal:state->curNPlayersTotal;

    /* It's illegal for there to be 0 players selected.  So if that ever
       happens, make the first player local.  And beep? */
    if ( nToShow == 0 ) {
        XP_ASSERT( isClient );	/* the only way this can happen is if someone
                                   sets type to SERVER_ISCLIENT when there
                                   are no local players. */
        state->isLocal[0] = true;
        nToShow = state->curNPlayersLocal = 1;
        setBooleanCtrl( XW_REMOTE_1_CHECKBOX_ID, false );
    }

    if ( canShowRemote && isClient ) {
        canShowRemote = !isNewGame;
    }

#ifndef XWFEATURE_STANDALONE_ONLY
    disOrEnable( form, XW_LOCAL_LABEL_ID, canShowRemote );
    if ( canShowRemote ) {
        disOrEnable( form, XW_TOTALP_LABEL_ID, XP_TRUE );
    } else {
        disOrEnable( form, XW_LOCALP_LABEL_ID, XP_TRUE );
    }
#endif

    for ( i = 0; i < MAX_NUM_PLAYERS; ++i ) {
        short offset = NUM_PLAYER_COLS * i;
        Boolean lineVisible = nShown < nToShow;
        Boolean isLocal;
#ifndef XWFEATURE_STANDALONE_ONLY
        Boolean showRemote;
        Boolean remoteChecked = !state->isLocal[i];

        if ( isClient && lineVisible && remoteChecked && isNewGame ) {
            lineVisible = false;
        }
#endif
        /* Since the user of a device can change at will whether a local
           player is a robot, we don't show that attribute except when it's
           for a local player; don't keep track of what's going on on the
           other device. */
        isLocal = lineVisible;
#ifndef XWFEATURE_STANDALONE_ONLY
        showRemote = lineVisible && canShowRemote;
        isLocal = isLocal && 
            (curServerHilite == SERVER_STANDALONE
             || (showRemote && !remoteChecked)
             || (isClient && isNewGame) );

        /* show local/remote checkbox if not standalone */
        disOrEnable( form, XW_REMOTE_1_CHECKBOX_ID + offset, 
                     lineVisible && showRemote );
#endif
        /* show name no matter what (if line's showing) */
        disOrEnable( form, XW_PLAYERNAME_1_FIELD_ID + offset, 
                     lineVisible && (isLocal || !isNewGame) );

        if ( lineVisible ) {
            FieldPtr nameField = 
                getActiveObjectPtr(XW_PLAYERNAME_1_FIELD_ID + offset);
            setFieldEditable( nameField, canEdit );
            if ( canDraw ) {
                FldDrawField( nameField );
            }
        }

        /* show robot checkbox if player is local */
        disOrEnable( form, XW_ROBOT_1_CHECKBOX_ID + offset, isLocal );

        /* and show password if not a robot (and if local) */
        disOrEnable( form, XW_PLAYERPASSWD_1_TRIGGER_ID + offset, 
                     isLocal && !state->isRobot[i] );

        if ( lineVisible ) {
            ++nShown;
        }
        XP_ASSERT( nShown <= MAX_NUM_PLAYERS );
    }

#ifndef XWFEATURE_STANDALONE_ONLY
    XP_ASSERT( nShown > 0 );
    setSelectorFromList( XW_NPLAYERS_SELECTOR_ID, 
                         state->playerNumList, nShown - 1 );
#endif
} /* adjustVisibility */

/* 
 * Copy the local state into global state.
 */
static void
updatePlayerInfo( PalmAppGlobals* globals )
{
    UInt16 i;
    CurGameInfo* gi;
    LocalPlayer* lp;
    PalmNewGameState* state = &globals->newGameState;
    Connectedness curServerHilite = globals->newGameState.curServerHilite;

    gi = &globals->gameInfo;

    gi->nPlayers = curServerHilite == SERVER_ISCLIENT?
        state->curNPlayersLocal: state->curNPlayersTotal;
    XP_ASSERT( gi->nPlayers <= MAX_NUM_PLAYERS );

    gi->boardSize = globals->prefsDlgState->curBdSize;
    gi->serverRole = curServerHilite;

    replaceStringIfDifferent( MPPARM(globals->mpool) &gi->dictName, 
                              globals->newGameState.dictName );

    for ( i = 0, lp = gi->players; i < MAX_NUM_PLAYERS; ++i, ++lp ) {
        XP_UCHAR* name = NULL;
        XP_UCHAR* passwd = NULL;
        short offset = NUM_PLAYER_COLS * i;
        XP_Bool isLocal = state->isLocal[i];

        if ( isLocal ) {
            MemPtr p = getActiveObjectPtr( offset + 
                                           XW_PLAYERNAME_1_FIELD_ID );
            name = (XP_UCHAR*)FldGetTextPtr( p );
                
            if ( name == NULL ) {
                name = (XP_UCHAR*)"";
            }
            passwd = globals->newGameState.passwds[i];
        }

        lp->isRobot = state->isRobot[i];
        lp->isLocal = isLocal;

        replaceStringIfDifferent(MPPARM(globals->mpool) &lp->name, name);
        replaceStringIfDifferent(MPPARM(globals->mpool) &lp->password, passwd);
    }

#ifdef BEYOND_IR
    if ( state->connsSettingChanged ) {
        CommsAddrRec addr;
        XP_U16 localPort;

        comms_getAddr( globals->game.comms, &addr, &localPort );

        addr.conType = state->connAddrs.conType;
        if ( addr.conType == COMMS_CONN_IP ) {
            addr.u.ip.port = state->connAddrs.remotePort;
            addr.u.ip.ipAddr = state->connAddrs.remoteIP;
            localPort = state->connAddrs.localPort;
        }
        comms_setAddr( globals->game.comms, &addr, localPort );
    }
#endif

} /* updatePlayerInfo */

void
drawOneGadget( UInt16 id, char* text, Boolean hilite )
{
    RectangleType divRect;
    XP_U16 len = XP_STRLEN(text);
    XP_U16 width = FntCharsWidth( text, len );
    XP_U16 left;

    getObjectBounds( id, &divRect );
    WinDrawRectangleFrame( rectangleFrame, &divRect );
    WinEraseRectangle( &divRect, 0 );
    left = divRect.topLeft.x;
    left += (divRect.extent.x - width) / 2;
    WinDrawChars( text, len, left, divRect.topLeft.y );
    if ( hilite ) {
        WinInvertRectangle( &divRect, 0 );
    }
} /* drawOneGadget */

/* Frame 'em, draw their text, and highlight the one that's selected
 */
#ifndef XWFEATURE_STANDALONE_ONLY

#ifdef HS_DUO_SUPPORT
static void
drawFocusRingOnGadget()
{
    FormPtr form = FrmGetActiveForm();
    XP_U16 focusID = FrmGetObjectId( form, FrmGetFocus(form) );
    if ( IS_SERVER_GADGET(focusID) ) {
        Err err;
        RectangleType rect;

        getObjectBounds( focusID, &rect );

        err = HsNavDrawFocusRing( form, focusID, 0, &rect,
                                  hsNavFocusRingStyleObjectTypeDefault,
                                  false );
        XP_ASSERT( err == errNone );
    }
} /* drawFocusRingOnGadget */
#endif

static void
drawConnectGadgets( PalmAppGlobals* globals )
{
    UInt16 i;
    ListPtr list = getActiveObjectPtr( XW_SERVERTYPES_LIST_ID );
    XP_ASSERT( !!list );

    for ( i = 0; i < 3; ++i ) {
        char* text = LstGetSelectionText( list, i );
        Boolean hilite = i == globals->newGameState.curServerHilite;
        drawOneGadget( i + XW_SOLO_GADGET_ID, text, hilite );
    }

#ifdef HS_DUO_SUPPORT
    drawFocusRingOnGadget();
#endif

} /* drawConnectGadgets */

#ifdef HS_DUO_SUPPORT
static XP_Bool
considerGadgetFocus( PalmNewGameState* state, EventType* event )
{
    XP_Bool result = XP_FALSE;
    XP_Bool isTake;
    XP_U16 eType = event->eType;
    FormPtr form = FrmGetActiveForm();
    XP_U16 objectID;

    XP_ASSERT( eType == frmObjectFocusTakeEvent
               || eType == frmObjectFocusLostEvent );
    XP_ASSERT( event->data.frmObjectFocusTake.formID == FrmGetActiveFormID() );


    isTake = eType == frmObjectFocusTakeEvent;
    if ( isTake ) {
        objectID = event->data.frmObjectFocusTake.objectID;
    } else {
        objectID = event->data.frmObjectFocusLost.objectID;
    }

    /* docs say to return HANDLED for both take and lost */
    result = IS_SERVER_GADGET( objectID );

    if ( result ) {
        Err err;
        if ( isTake ) {

            FrmSetFocus(form, FrmGetObjectIndex(form, objectID));
            drawFocusRingOnGadget();
            result = XP_TRUE;
/*         } else { */
/*             err = HsNavRemoveFocusRing( form ); */
        }
    }

    return result;
} /* considerGadgetFocus */
#endif

static void
changeGadgetHilite( PalmAppGlobals* globals, UInt16 hiliteID )
{
    PalmNewGameState* state = &globals->newGameState;
    XP_Bool isNewGame = globals->isNewGame;

    hiliteID -= XW_SOLO_GADGET_ID;

    if ( hiliteID != state->curServerHilite ) {
        /* if it's not a new game, don't recognize the change */
        if ( isNewGame ) {
            state->curServerHilite = hiliteID;
            drawConnectGadgets( globals );
            adjustVisibility( globals, XP_TRUE );
        } else {
            beep();
        }
    }

#ifdef BEYOND_IR
    /* Even if it didn't change, pop the connections form */
    if ( hiliteID != SERVER_STANDALONE ) {
        if ( isNewGame || hiliteID==globals->newGameState.curServerHilite ) {
            PopupConnsForm( globals, hiliteID, &state->connAddrs );
        }
    }
#endif
} /* changeGadgetHilite */

static Boolean
checkHiliteGadget( PalmAppGlobals* globals, EventType* event,
                   PalmNewGameState* state )
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

/* If there's currently no password set, just let 'em set one.  If there is
 * one set, they need to know the old before setting the new.
 */
static void
handlePasswordTrigger( PalmAppGlobals* globals, UInt16 controlID )
{
    UInt16 playerNum;
    PalmNewGameState* state = &globals->newGameState;
    XP_UCHAR** password;
    XP_UCHAR* name;
    FieldPtr nameField;
    XP_U16 len;
    XP_UCHAR buf[32];
    char* label;

    playerNum = (controlID - XW_PLAYERPASSWD_1_TRIGGER_ID) / NUM_PLAYER_COLS;
    XP_ASSERT( playerNum < MAX_NUM_PLAYERS );

    password = &state->passwds[playerNum];
    nameField = getActiveObjectPtr( XW_PLAYERNAME_1_FIELD_ID +
                                    (NUM_PLAYER_COLS * playerNum) );
    name = (XP_UCHAR*)FldGetTextPtr( nameField );

    len = sizeof(buf);
    if ( askPassword( globals, name, true, buf, &len ) ) {

        if ( len == 0 ) {
            buf[0] = '\0';
            label = "   ";
        } else {
            label = "*";
        }
        replaceStringIfDifferent(MPPARM(globals->mpool) password, 
                                 (unsigned char*)buf);

        /* control owns the string passed in */
        CtlSetLabel( getActiveObjectPtr( controlID ), label );
    }
} /* handlePasswordTrigger */

static void
unloadNewGameState( PalmAppGlobals* globals )
{
    XP_U16 i;
    XP_UCHAR** passwd;
    PalmNewGameState* state = &globals->newGameState;

    for ( passwd = state->passwds, i = 0;
          i < MAX_NUM_PLAYERS; ++i, ++passwd ) {
        if ( !!*passwd ) {
            XP_FREE( globals->mpool, *passwd );
            *passwd = NULL;
        }
    }
    /*     XP_WARNF( "freeing string 0x%lx", state->dictName ); */
    XP_FREE( globals->mpool, state->dictName );
    state->dictName = NULL;
} /* unloadNewGameState */

static void
loadNewGameState( PalmAppGlobals* globals )
{
    CurGameInfo* gi = &globals->gameInfo;
    PalmNewGameState* state = &globals->newGameState;
    XP_U16 i;
    LocalPlayer* lp;

    XP_MEMSET( state, 0, sizeof(*state) );

    state->dictName = copyString( MPPARM(globals->mpool) gi->dictName );
    state->curServerHilite = gi->serverRole;

    for ( i = 0, lp=gi->players; i < MAX_NUM_PLAYERS; ++i, ++lp ) {
        state->isLocal[i] = lp->isLocal;
        state->isRobot[i] = lp->isRobot;
        state->passwds[i] = copyString( MPPARM(globals->mpool) 
                                        lp->password );
    }

    state->curNPlayersTotal = gi->nPlayers;
    state->curNPlayersLocal = countLocalIn( state, gi->nPlayers );
    XP_ASSERT( state->curNPlayersLocal <= MAX_NUM_PLAYERS );

#ifdef BEYOND_IR
    {
        CommsAddrRec addr;
        comms_getAddr( globals->game.comms, &addr, 
                       &state->connAddrs.localPort );
        state->connAddrs.remotePort = addr.u.ip.port;
        state->connAddrs.remoteIP = addr.u.ip.ipAddr;
        state->connAddrs.conType = addr.conType;
    }
#endif

} /* loadNewGameState */
