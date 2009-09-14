/* -*-mode: C; fill-column: 77; c-basic-offset: 4; compile-command: "make ARCH=ARM_ONLY MEMDEBUG=TRUE"; -*- */
/* 
 * Copyright 2003-2008 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#if defined XWFEATURE_BLUETOOTH || defined XWFEATURE_RELAY

#include <NetMgr.h>

#include "callback.h"

#include "connsdlg.h"
#include "strutils.h"
#include "palmmain.h"
#include "palmutil.h"
#include "palmir.h"
#include "palmbt.h"
#include "LocalizedStrIncludes.h"

/* When user pops up via Host gadget, we want to get the port to listen on.
 * When pops up via the Guest gadget, we want to get the port and IP address
 * of the host AS WELL AS the local port that we'll tell it we're listening
 * on.  We need local state for both, since user can switch between them and
 * expect state to live as long as the parent dialog isn't exited.
 */

typedef struct XportEntry {
    XP_U16 resID;
    CommsConnType conType;
} XportEntry;

#define MAX_XPORTS 3

typedef struct ConnsDlgState {
    ListPtr connTypesList;
    ListData sLd;
    XportEntry xports[MAX_XPORTS];
    XP_U16 nXports;
    XP_U16 serverRole;
    XP_Bool isNewGame;
    CommsConnType conType;
    CommsAddrRec* addr;
    XP_BtAddr btAddr;           /* since there's no field, save it here */
#ifdef XWFEATURE_BLUETOOTH
    char hostName[PALM_BT_NAME_LEN];
#endif
} ConnsDlgState;

static void
ctlsFromState( PalmAppGlobals* globals )
{
    ConnsDlgState* state = globals->connState;
    CommsAddrRec* addr = state->addr;
    XP_Bool isNewGame = state->isNewGame;
    state->conType = addr->conType;

    if ( 0 ) {
#ifdef XWFEATURE_RELAY
    } else if ( addr->conType == COMMS_CONN_RELAY ) {
        XP_UCHAR buf[16];
        setFieldStr( XW_CONNS_RELAY_FIELD_ID, addr->u.ip_relay.hostName );
        setFieldEditable( XW_CONNS_RELAY_FIELD_ID, isNewGame );

        StrPrintF( buf, "%d", addr->u.ip_relay.port );
        setFieldStr( XW_CONNS_PORT_FIELD_ID, buf );
        setFieldEditable( XW_CONNS_PORT_FIELD_ID, isNewGame );

        setFieldStr( XW_CONNS_INVITE_FIELD_ID, addr->u.ip_relay.invite );
        setFieldEditable( XW_CONNS_INVITE_FIELD_ID, isNewGame );
#endif
#ifdef XWFEATURE_BLUETOOTH
    } else if ( addr->conType == COMMS_CONN_BT 
                /* It's ok to load the controls even if we won't use them */
                /* && state->serverRole == SERVER_ISCLIENT */ ) {
        ControlPtr ctrl = getActiveObjectPtr( XW_CONNS_BT_HOSTTRIGGER_ID );

        XP_MEMCPY( &state->btAddr, &state->addr->u.bt.btAddr,
                   sizeof(state->btAddr) );
        XP_MEMCPY( &state->hostName, &state->addr->u.bt.hostName,
                   sizeof(state->hostName) );

        if ( '\0' != addr->u.bt.hostName[0] ) {
            CtlSetLabel( ctrl, state->hostName );
        }
        CtlSetEnabled( ctrl, isNewGame );

#endif
    }

#ifdef XWFEATURE_BLUETOOTH
    /* Set up any controls that are not based on union fields regardless of
       conType, as user may change and we want the defaults to be right.
       This is a case where using a union breaks down: it'd be better to have
       access to all the defaults, but they may be corrupt if conType is
       wrong. */
    XP_ASSERT( !!globals->prefsDlgState );
    setBooleanCtrl( XW_CONNS_BTCONFIRM_CHECKBOX_ID,
                    globals->prefsDlgState->confirmBTConnect );
#endif

} /* ctlsFromState */

static XP_Bool
stateFromCtls( PalmAppGlobals* globals, XP_Bool* prefsChanged )
{
    ConnsDlgState* state = globals->connState;
    CommsAddrRec addr;
    XP_Bool addrOk;

    *prefsChanged = XP_FALSE;

    XP_MEMCPY( &addr, state->addr, sizeof(addr) );
    addr.conType = state->conType;

    if ( 0 ) {
#ifdef XWFEATURE_RELAY
    } else if ( addr.conType == COMMS_CONN_RELAY ) {
        XP_UCHAR buf[16];
        getFieldStr( XW_CONNS_RELAY_FIELD_ID, addr.u.ip_relay.hostName,
                      sizeof(addr.u.ip_relay.hostName) );

        getFieldStr( XW_CONNS_PORT_FIELD_ID, buf, sizeof(buf) );
        addr.u.ip_relay.port = StrAToI( buf );        

        getFieldStr( XW_CONNS_INVITE_FIELD_ID, addr.u.ip_relay.invite,
                      sizeof(addr.u.ip_relay.invite) );
#endif
#ifdef XWFEATURE_BLUETOOTH
    } else if ( addr.conType == COMMS_CONN_BT 
                && state->serverRole == SERVER_ISCLIENT ) {
        XP_Bool confirmBTConnect;
        /* Not exactly from a control... */
        /* POSE is flagging this as reading from a bad address, but it
           looks ok inside the debugger */
        XP_MEMCPY( addr.u.bt.hostName, state->hostName, 
                   sizeof(addr.u.bt.hostName) );
        XP_MEMCPY( &addr.u.bt.btAddr, &state->btAddr, 
                   sizeof(addr.u.bt.btAddr) );
        LOG_HEX( &addr.u.bt.btAddr, sizeof(addr.u.bt.btAddr), __func__ );

        confirmBTConnect = getBooleanCtrl( XW_CONNS_BTCONFIRM_CHECKBOX_ID );
        XP_ASSERT( !!globals->prefsDlgState );
        if ( confirmBTConnect != globals->prefsDlgState->confirmBTConnect ) {
            globals->prefsDlgState->confirmBTConnect = confirmBTConnect;
            *prefsChanged = XP_TRUE;
        }
#endif
    }

    addrOk = comms_checkAddr( state->serverRole, &addr, &globals->util );
    if ( addrOk ) {
        XP_MEMCPY( state->addr, &addr, sizeof( *state->addr) );
    }

    return addrOk;
} /* stateFromCtls */

static void
updateFormCtls( FormPtr form, ConnsDlgState* state )
{
    const XP_U16 relayCtls[] = {
#ifdef XWFEATURE_RELAY
        XW_CONNS_RELAY_LABEL_ID,
        XW_CONNS_RELAY_FIELD_ID,
        XW_CONNS_PORT_LABEL_ID,
        XW_CONNS_PORT_FIELD_ID,
        XW_CONNS_INVITE_LABEL_ID,
        XW_CONNS_INVITE_FIELD_ID,
#endif
        0
    };
    const XP_U16 btGuestCtls[] = {
#ifdef XWFEATURE_BLUETOOTH
        XW_CONNS_BT_HOSTNAME_LABEL_ID,
        XW_CONNS_BT_HOSTTRIGGER_ID,
        XW_CONNS_BTCONFIRM_CHECKBOX_ID,
#endif
        0
    };
    const XP_U16* allCtls[] = {
        relayCtls, btGuestCtls
    };
    const XP_U16* on;
    XP_U16 i;

    if ( state->conType == COMMS_CONN_RELAY ) {
        on = relayCtls;
    } else if ( state->conType == COMMS_CONN_BT
#ifdef XWFEATURE_BLUETOOTH
                && state->serverRole == SERVER_ISCLIENT 
#endif
                ) {
        on = btGuestCtls;
    } else {
        on = NULL;
    }

    for ( i = 0; i < VSIZE(allCtls); ++i ) {
        const XP_U16* cur = allCtls[i];
        if ( cur != on ) {
            disOrEnableSet( form, cur, XP_FALSE );
        }
    }
    if ( on != NULL ) {
        disOrEnableSet( form, on, XP_TRUE );
    }

} /* updateFormCtls */

static CommsConnType
selToConType( const ConnsDlgState* state, XP_U16 sel )
{
    XP_ASSERT( sel < state->nXports );
    return state->xports[sel].conType;
} /* selToConType */

#ifdef XWFEATURE_BLUETOOTH
static void
browseForDeviceName( PalmAppGlobals* globals )
{
    ConnsDlgState* state = globals->connState;
    XP_BtAddr btAddr;
    if ( palm_bt_browse_device( globals, &btAddr, 
                                state->hostName, sizeof(state->hostName) ) ) {
        CtlSetLabel( getActiveObjectPtr( XW_CONNS_BT_HOSTTRIGGER_ID ),
                     state->hostName );
        XP_MEMCPY( &state->btAddr, &btAddr, sizeof(state->btAddr) );
        LOG_HEX( &state->btAddr, sizeof(state->btAddr), __func__ );
    }
} /* browseForDeviceName */
#endif

static void
setupXportList( PalmAppGlobals* globals )
{
    ConnsDlgState* state = globals->connState;
    ListData* sLd = &state->sLd;
    XP_U16 ii;
    XP_S16 selSel = -1;
    const XP_UCHAR* selName = NULL;

    XP_ASSERT( state->nXports >= 1 );
    state->connTypesList = getActiveObjectPtr( XW_CONNS_TYPE_LIST_ID );

    initListData( MPPARM(globals->mpool) sLd, state->nXports );
    for ( ii = 0; ii < state->nXports; ++ii ) {
        XportEntry* xports = &state->xports[ii];
        const XP_UCHAR* xname = getResString( globals, xports->resID );
        addListTextItem( MPPARM(globals->mpool) sLd, xname );
        if ( state->conType == xports->conType ) {
            selName = xname;
            selSel = ii;
        }
    }

    XP_ASSERT( !!selName );
    setListSelection( sLd, selName );
    setListChoices( sLd, state->connTypesList, NULL );

    setSelectorFromList( XW_CONNS_TYPE_TRIGGER_ID, state->connTypesList,
                         selSel );
} /* setupXportList */

static void
buildXportData( ConnsDlgState* state )
{
    XportEntry* xports = state->xports;
    XP_ASSERT( 0 == state->nXports );

#ifdef XWFEATURE_IR
    xports->conType = COMMS_CONN_IR;
    xports->resID = STR_IR_XPORTNAME;
    ++xports;
#endif
#ifdef XWFEATURE_BLUETOOTH
    xports->conType = COMMS_CONN_BT;
    xports->resID = STR_BT_XPORTNAME;
    ++xports;
#endif
#ifdef XWFEATURE_RELAY
    xports->conType = COMMS_CONN_RELAY;
    xports->resID = STR_RELAY_XPORTNAME;
    ++xports;
#endif
    state->nXports = xports - state->xports;
} /* buildXportData */

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
            (DeviceRole)globals->dlgParams[CONNS_PARAM_ROLE_INDEX];
        state->addr = 
            (CommsAddrRec*)globals->dlgParams[CONNS_PARAM_ADDR_INDEX];
        state->isNewGame = globals->isNewGame;

        ctlsFromState( globals );

        /* setup connection popup */
        buildXportData( state );
        setupXportList( globals );

        updateFormCtls( form, state );

    case frmUpdateEvent:
        FrmDrawForm( form );
        result = true;
        break;

    case ctlSelectEvent:
        result = true;
        switch ( event->data.ctlSelect.controlID ) {

#ifdef XWFEATURE_BLUETOOTH
        case XW_CONNS_BT_HOSTTRIGGER_ID:
            if ( state->isNewGame ) {
                browseForDeviceName( globals );
            }
            break;
#endif

        case XW_CONNS_TYPE_TRIGGER_ID:
            if ( state->isNewGame ) {
                chosen = LstPopupList( state->connTypesList );
                if ( chosen >= 0 ) {
                    setSelectorFromList( XW_CONNS_TYPE_TRIGGER_ID, 
                                         state->connTypesList, chosen );
                    state->conType = selToConType( state, chosen );
                    updateFormCtls( form, state );
                }
            }
            break;

        case XW_CONNS_OK_BUTTON_ID:
            if ( !state->isNewGame ) {
                /* do nothing; same as cancel */
            } else {
                XP_Bool prefsChanged;
                if ( !stateFromCtls( globals, &prefsChanged ) ) {
                    break;      /* refuse to exit */
                }
                if ( prefsChanged ) {
                    postEmptyEvent( prefsChangedEvent );
                }
            }
            /* FALLTHRU */
        case XW_CONNS_CANCEL_BUTTON_ID:
            freeListData( MPPARM(globals->mpool) &globals->connState->sLd );
            XP_FREE( globals->mpool, globals->connState );
            globals->connState = NULL;
            FrmReturnToForm( 0 );
            break;
        }
        break;
    default:
        result = false;
    }

    CALLBACK_EPILOGUE();
    return result;
} /* ConnsFormHandleEvent */

#endif /* #if defined XWFEATURE_BLUETOOTH || defined XWFEATURE_RELAY */
