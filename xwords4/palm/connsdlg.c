/* -*-mode: C; fill-column: 77; c-basic-offset: 4; -*- */
/* 
 * Copyright 2003 by Eric House (fixin@peak.org).  All rights reserved.
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
    ConnDlgAddrs* addrState;
    XP_UCHAR localIPStr[16];
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
    ConnDlgAddrs* addrState = state->addrState;

    NetLibAddrINToA( globals->nlStuff.netLibRef, 
                     addrState->remoteIP, buf );
    fieldFromStr( XW_CONNS_TARGET_FIELD_ID, buf, isNewGame );

    StrPrintF( buf, "%d", addrState->remotePort );
    fieldFromStr( XW_CONNS_TPORT_FIELD_ID, buf, isNewGame );

    StrPrintF( buf, "%d", addrState->localPort );
    fieldFromStr( XW_CONNS_MYPORT_FIELD_ID, buf, isNewGame );

    fieldFromStr( XW_CONNS_HOSTIP_FIELD_ID, state->localIPStr, XP_FALSE );
} /* ctlsFromState */

static XP_Bool
stateFromCtls( PalmAppGlobals* globals, ConnsDlgState* state )
{
    XP_Bool ok = XP_TRUE;
    XP_UCHAR buf[16];
    XP_U32 tmpAddr;
    ConnDlgAddrs* addrState = state->addrState;
    XP_Bool rejectBadIP = addrState->conType == COMMS_CONN_IP;

    strFromField( XW_CONNS_TARGET_FIELD_ID, buf, sizeof(buf) );
    tmpAddr = NetLibAddrAToIN( globals->nlStuff.netLibRef, buf );
    if ( tmpAddr != -1L ) {
        addrState->remoteIP = tmpAddr;
    } else if ( rejectBadIP ) {
        NetLibAddrINToA( globals->nlStuff.netLibRef, addrState->remoteIP,
                         buf );
        fieldFromStr( XW_CONNS_TARGET_FIELD_ID, buf, state->isNewGame );
        ok = XP_FALSE;
    }

    strFromField( XW_CONNS_TPORT_FIELD_ID, buf, sizeof(buf) );
    addrState->remotePort = StrAToI( buf );        

    strFromField( XW_CONNS_MYPORT_FIELD_ID, buf, sizeof(buf) );
    addrState->localPort = StrAToI( buf );        

    return ok;
} /* stateFromCtls */

/* Adjust the set of visible form controls based on state.  There are two
 * variables here: whether we're showing for a Host or a Guest, and whether
 * the connection method is IR or IP.  Currently IR means no controls needed,
 * either way.  IP means on set if launched from host button, another if
 * launched from Guest.
 */
static void
updateFormCtls( FormPtr form, ConnsDlgState* state )
{
    const XP_U16 ipCtlsBoth[] = {
        XW_CONNS_MYPORT_LABEL_ID,
        XW_CONNS_MYPORT_FIELD_ID, 
        XW_CONNS_HOSTIP_LABEL_ID,
        XW_CONNS_HOSTIP_FIELD_ID,
        0
    };
    const XP_U16 ipCtlsGuest[] = {
        XW_CONNS_TARGET_LABEL_ID,
        XW_CONNS_TARGET_FIELD_ID,
        XW_CONNS_TPORT_LABEL_ID,
        XW_CONNS_TPORT_FIELD_ID,
        0
    };

    if ( state->addrState->conType == COMMS_CONN_IR ) {
        disOrEnableSet( form, ipCtlsBoth, XP_FALSE );
        disOrEnableSet( form, ipCtlsGuest, XP_FALSE );
    } else {
        disOrEnableSet( form, ipCtlsBoth, XP_TRUE );
        disOrEnableSet( form, ipCtlsGuest, 
                        state->serverRole == SERVER_ISCLIENT );
        setFieldEditable( getActiveObjectPtr(XW_CONNS_HOSTIP_FIELD_ID), 
                          XP_FALSE );
    }

} /* updateFormCtls */

static void
cleanupExit( PalmAppGlobals* globals )
{
    XP_FREE( globals->mpool, globals->connState );
    globals->connState = NULL;
    FrmReturnToForm( 0 );
} /* cleanupExit */

static XP_U32
figureLocalIP( PalmAppGlobals* globals, XP_UCHAR* buf )
{
    Err err;
    XP_U32 ipAddr = 0L;
    XP_U16 netLibRef = globals->nlStuff.netLibRef;
    XP_U16 index;

    for ( index = 0; ; ++index ) {
        UInt32 creator;
        UInt16 instance;
        err = NetLibIFGet( netLibRef, index,
                           &creator, &instance );

        /* Docs say to iterate until get netErrInvalidInterface, but I'm
           never getting that, getting netErrInterfaceNotFound instead  */
        if ( (err == netErrInvalidInterface) || 
             (err == netErrInterfaceNotFound) ) {
            break;              /* we're done */
        } else if ( err == errNone ) {
            XP_U8 up;
            UInt16 siz = sizeof(up);
            err = NetLibIFSettingGet( netLibRef, creator, instance,
                                      netIFSettingUp, &up, &siz );
            if ( (err == errNone) && (up != 0) ) {

                siz = sizeof(ipAddr);
                /* use this interface?? */
                err = NetLibIFSettingGet( netLibRef, creator, instance,
                                          netIFSettingReqIPAddr,
                                          &ipAddr, &siz );
                XP_ASSERT( siz == 4 );
            }
        }
    }

    if ( !!buf ) {
        NetLibAddrINToA( globals->nlStuff.netLibRef, ipAddr, buf );
        XP_LOGF( "got local addr: %s", buf );
    }
    return ipAddr;
} /* figureLocalIP */

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

        if ( !openNetLibIfNot( globals ) ) {
            beep();
            cleanupExit( globals );
            result = true;
            break;
        }

        state->serverRole = 
            (Connectedness)globals->dlgParams[CONNS_PARAM_ROLE_INDEX];
        state->addrState = 
            (ConnDlgAddrs*)globals->dlgParams[CONNS_PARAM_ADDR_INDEX];
        state->isNewGame = globals->isNewGame;
        (void)figureLocalIP( globals, state->localIPStr );

        /* setup connection popup */
        state->connTypesList = getActiveObjectPtr( XW_CONNS_TYPE_LIST_ID );
        XP_ASSERT( state->addrState->conType == COMMS_CONN_IR
                   || state->addrState->conType == COMMS_CONN_IP );
        setSelectorFromList( XW_CONNS_TYPE_TRIGGER_ID, state->connTypesList,
                             state->addrState->conType == COMMS_CONN_IR? 0:1 );

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
                    state->addrState->conType = 
                        chosen == 0? COMMS_CONN_IR : COMMS_CONN_IP;
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
