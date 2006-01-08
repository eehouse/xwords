/* -*-mode: C; fill-column: 77; c-basic-offset: 4; -*- */
/* 
 * Copyright 2003 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#ifdef BEYOND_IR

#include <NetMgr.h>

#include "callback.h"

#include "connsdlg.h"
#include "palmmain.h"
#include "palmutil.h"
#include "palmir.h"

/* When user pops up via Host gadget, we want to get the port to listen on.
 * When pops up via the Guest gadget, we want to get the port and IP address
 * of the host AS WELL AS the local port that we'll tell it we're listening
 * on.  We need local state for both, since user can switch between them and
 * expect state to live as long as the parent dialog isn't exited.
 */

typedef struct ConnsDlgState {
    ListPtr connTypesList;
    XP_U16 serverRole;
    XP_Bool isNewGame;
    CommsAddrRec* addr;
} ConnsDlgState;

static void
fieldFromStr( XP_U16 id, XP_UCHAR* buf, XP_Bool editable )
{
    FieldPtr field = getActiveObjectPtr( id );
    UInt16 len = FldGetTextLength( field );
    FldSetSelection( field, 0, len );
    FldInsert( field, buf, XP_STRLEN(buf) );
    setFieldEditable( field, editable );
} /* fieldFromStr */

static void
strFromField( XP_U16 id, XP_UCHAR* buf, XP_U16 max )
{
    FieldPtr field = getActiveObjectPtr( id );
    XP_UCHAR* str = FldGetTextPtr( field );
    XP_U16 len = FldGetTextLength( field );
    if ( len > max-1 ) {
        len = max - 1;
    }
    XP_MEMCPY( buf, str, len );
    buf[len] = '\0';
} /* strFromField */

#if 0
static void
stateFromGlobals( PalmAppGlobals* globals, ConnsDlgState* state )
{
    comms_getAddr( globals->game.comms, &state->targetAddr,
                   &state->myPort );

    state->isNewGame = globals->isNewGame;
} /* stateFromGlobals */

static void
globalsFromState( PalmAppGlobals* globals, ConnsDlgState* state )
{
    comms_setAddr( globals->game.comms, &state->targetAddr,
                   state->myPort );
} /* globalsFromState */
#endif

static void
ctlsFromState( PalmAppGlobals* globals, FormPtr form, ConnsDlgState* state )
{
    XP_Bool isNewGame = state->isNewGame;
    XP_UCHAR buf[16];
    CommsAddrRec* addr = state->addr;

    fieldFromStr( XW_CONNS_RELAY_FIELD_ID, 
                  addr->u.ip_relay.hostName, isNewGame );

    StrPrintF( buf, "%d", addr->u.ip_relay.port );
    fieldFromStr( XW_CONNS_PORT_FIELD_ID, buf, isNewGame );

    fieldFromStr( XW_CONNS_COOKIE_FIELD_ID, addr->u.ip_relay.cookie, isNewGame );
} /* ctlsFromState */

static XP_Bool
stateFromCtls( PalmAppGlobals* globals, ConnsDlgState* state )
{
    XP_Bool ok = XP_TRUE;
    XP_UCHAR buf[16];
    CommsAddrRec* addr = state->addr;

    strFromField( XW_CONNS_RELAY_FIELD_ID, addr->u.ip_relay.hostName,
                  sizeof(addr->u.ip_relay.hostName) );

    strFromField( XW_CONNS_PORT_FIELD_ID, buf, sizeof(buf) );
    addr->u.ip_relay.port = StrAToI( buf );        

    strFromField( XW_CONNS_COOKIE_FIELD_ID, addr->u.ip_relay.cookie,
                  sizeof(addr->u.ip_relay.cookie) );
    return ok;
} /* stateFromCtls */

static void
updateFormCtls( FormPtr form, ConnsDlgState* state )
{
    const XP_U16 relayCtls[] = {
        XW_CONNS_RELAY_LABEL_ID ,
        XW_CONNS_RELAY_FIELD_ID,
        XW_CONNS_PORT_LABEL_ID,
        XW_CONNS_PORT_FIELD_ID,
        XW_CONNS_COOKIE_LABEL_ID,
        XW_CONNS_COOKIE_FIELD_ID,
        0
    };
    
    disOrEnableSet( form, relayCtls, 
                    state->addr->conType == COMMS_CONN_RELAY );
} /* updateFormCtls */

static void
cleanupExit( PalmAppGlobals* globals )
{
    XP_FREE( globals->mpool, globals->connState );
    globals->connState = NULL;
    FrmReturnToForm( 0 );
} /* cleanupExit */

Boolean
ConnsFormHandleEvent( EventPtr event )
{
    Boolean result;
    PalmAppGlobals* globals;
    ConnsDlgState* state;
    FormPtr form;
    XP_S16 chosen;

    CALLBACK_PROLOGUE();

    globals = getFormRefcon();
    state = globals->connState;
    if ( !state ) {
        state = globals->connState = XP_MALLOC( globals->mpool,
                                                sizeof(*state) );
        XP_MEMSET( state, 0, sizeof(*state) );
    }

    form = FrmGetActiveForm();

    switch ( event->eType ) {
    case frmOpenEvent:

        state->serverRole = 
            (Connectedness)globals->dlgParams[CONNS_PARAM_ROLE_INDEX];
        state->addr = 
            (CommsAddrRec*)globals->dlgParams[CONNS_PARAM_ADDR_INDEX];
        state->isNewGame = globals->isNewGame;

        /* setup connection popup */
        state->connTypesList = getActiveObjectPtr( XW_CONNS_TYPE_LIST_ID );
        XP_ASSERT( state->addr->conType == COMMS_CONN_IR
                   || state->addr->conType == COMMS_CONN_RELAY );
        setSelectorFromList( XW_CONNS_TYPE_TRIGGER_ID, state->connTypesList,
                             state->addr->conType == COMMS_CONN_IR? 0:1 );

        ctlsFromState( globals, form, state );

        updateFormCtls( form, state );

    case frmUpdateEvent:
        FrmDrawForm( form );
        result = true;
        break;

    case ctlSelectEvent:
        result = true;
        switch ( event->data.ctlSelect.controlID ) {

        case XW_CONNS_TYPE_TRIGGER_ID:
            if ( state->isNewGame ) {
                chosen = LstPopupList( state->connTypesList );
                if ( chosen >= 0 ) {
                    setSelectorFromList( XW_CONNS_TYPE_TRIGGER_ID, 
                                         state->connTypesList, chosen );
                    state->addr->conType = 
                        chosen == 0? COMMS_CONN_IR : COMMS_CONN_RELAY;
                    updateFormCtls( form, state );
                }
            }
            break;

        case XW_CONNS_OK_BUTTON_ID:
            if ( !state->isNewGame ) {
                /* do nothing; same as cancel */
            } else if ( !stateFromCtls( globals, state ) ) {
                beep();
                break;
            } else {
                EventType eventToPost;
                eventToPost.eType = connsSettingChgEvent;
                EvtAddEventToQueue( &eventToPost );
            }

        case XW_CONNS_CANCEL_BUTTON_ID:
            cleanupExit( globals );
            break;
        }
        break;
    default:
        result = false;
    }

    CALLBACK_EPILOGUE();
    return result;
} /* ConnsFormHandleEvent */

#endif /* BEYOND_IR */
