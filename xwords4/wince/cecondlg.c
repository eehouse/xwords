/* -*-mode: C; fill-column: 77; c-basic-offset: 4; -*- */
/* 
 * Copyright 2005-2009 by Eric House (xwords@eehouse.org).  All rights
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

#ifndef XWFEATURE_STANDALONE_ONLY

#include "cecondlg.h"
#include "ceutil.h"
#include "debhacks.h"

typedef struct _ConnDlgPair {
    CommsConnType conType;
    wchar_t* str;
} ConnDlgPair;

typedef struct _CeConnDlgState {
    CeDlgHdr dlgHdr;
    CommsAddrRec addrRec;
    DeviceRole role;
    XP_U16 connComboId;
    ConnDlgPair* types;
    XP_Bool userCancelled;
    XP_Bool forShowOnly;
} CeConnDlgState;

static CommsConnType indexToConType( const CeConnDlgState* state, 
                                     XP_U16 index );

static void
ceControlsToAddrRec( HWND hDlg, CeConnDlgState* state )
{
    XP_U16 len;

    if ( state->addrRec.conType == COMMS_CONN_RELAY ) {
#ifdef XWFEATURE_RELAY
        len = sizeof(state->addrRec.u.ip_relay.hostName);
        ceGetDlgItemText( hDlg, RELAYNAME_EDIT, 
                          state->addrRec.u.ip_relay.hostName, &len );
        state->addrRec.u.ip_relay.port = 
            (XP_U16)ceGetDlgItemNum( hDlg, RELAYPORT_EDIT );
        len = sizeof(state->addrRec.u.ip_relay.cookie);
        ceGetDlgItemText( hDlg, COOKIE_EDIT, state->addrRec.u.ip_relay.cookie, 
                          &len );
#endif
    } else if ( state->addrRec.conType == COMMS_CONN_IP_DIRECT ) {
#ifdef XWFEATURE_IP_DIRECT
        len = sizeof(state->addrRec.u.ip.hostName_ip);
        ceGetDlgItemText( hDlg, IPNAME_EDIT, state->addrRec.u.ip.hostName_ip,
                          &len );
#endif
    } else if ( state->addrRec.conType == COMMS_CONN_BT ) {
#ifdef XWFEATURE_BLUETOOTH
        if ( state->role == SERVER_ISCLIENT ) {
            len = sizeof(state->addrRec.u.bt.hostName);
            ceGetDlgItemText( hDlg, IDC_BLUET_ADDR_EDIT, 
                              state->addrRec.u.bt.hostName, &len );
        }
#endif
    } else if ( state->addrRec.conType == COMMS_CONN_SMS ) {
#ifdef XWFEATURE_SMS
        len = sizeof(state->addrRec.u.sms.phone);
        ceGetDlgItemText( hDlg, IDC_SMS_PHONE_EDIT, state->addrRec.u.sms.phone,
                          &len );
        state->addrRec.u.sms.port = (XP_U16)ceGetDlgItemNum( hDlg, 
                                                             IDC_SMS_PORT_EDIT );
#endif
    } else {
        XP_ASSERT(0);
    }
} /* ceControlsToAddrRec */

static void
adjustForConnType( HWND hDlg, CeConnDlgState* state, XP_Bool useFromState )
{
    XP_U16 relayIds[] = { 
        IDC_COOKIE_LAB,
#ifdef XWFEATURE_RELAY
        COOKIE_EDIT,IDC_CRELAYHINT_LAB,IDC_CRELAYNAME_LAB,RELAYNAME_EDIT, 
        IDC_CRELAYPORT_LAB, RELAYPORT_EDIT,
#endif
        0 };
    XP_U16 directIds[] = {
        IDC_IPNAME_LAB,
        IPNAME_EDIT,
        0
    };
    XP_U16 smsIds[] = {
        IDC_SMS_PHONE_LAB, 
        IDC_SMS_PHONE_EDIT,
        IDC_SMS_PORT_LAB,
        IDC_SMS_PORT_EDIT,
        0
    };
    XP_U16* allIDs[] = { relayIds, directIds, smsIds };
    XP_U16* on = NULL;
    XP_U16 ii;
    CommsConnType conType;

    if ( !useFromState ) {
        XP_S16 sel;
        sel = SendDlgItemMessage( hDlg, state->connComboId, 
                                  GETCURSEL(state->dlgHdr.globals), 0, 0L );
        state->addrRec.conType = indexToConType( state, sel );
    }

    conType = state->addrRec.conType;

    if ( 0 ) {
#ifdef XWFEATURE_RELAY
    } else if ( conType == COMMS_CONN_RELAY ) {
        on = relayIds;
#endif
#ifdef XWFEATURE_IP_DIRECT
    } else if ( COMMS_CONN_IP_DIRECT == conType ) {
        on = directIds;
#endif
#ifdef XWFEATURE_SMS
    } else if ( COMMS_CONN_SMS == conType ) {
        on = smsIds;
#endif
#ifdef XWFEATURE_BLUETOOTH
    } else if ( COMMS_CONN_BT == conType ) {
        on = btIds;
#endif
    }

    for ( ii = 0; ii < VSIZE(allIDs); ++ii ) {
        XP_U16* ids = allIDs[ii];
        XP_Bool enable = ids == on;
        while ( *ids != 0 ) {
            ceShowOrHide( hDlg, *(ids++), enable );
        }
    }
#ifdef _WIN32_WCE
        if ( IS_SMARTPHONE(state->dlgHdr.globals) ) {
            SendMessage( hDlg, DM_RESETSCROLL, (WPARAM)TRUE, (LPARAM)TRUE );
        }
#endif
} /* adjustForConnType */

static XP_U16
conTypeToIndex( const CeConnDlgState* state, CommsConnType conType )
{
    XP_U16 ii;
    for ( ii = 0; state->types[ii].conType != COMMS_CONN_NONE; ++ii ) {
        if ( conType == state->types[ii].conType ) {
            break;
        }
    }
    XP_ASSERT( state->types[ii].conType != COMMS_CONN_NONE );
    return ii;
}

static CommsConnType
indexToConType( const CeConnDlgState* state, XP_U16 index )
{
    CommsConnType conType = state->types[index].conType;
    XP_ASSERT( conTypeToIndex( state, conType ) == index );
    return conType;
} /* indexToConType */

static void
ceControlsFromAddrRec( HWND hDlg, const CeConnDlgState* state )
{
    XP_U16 ii;
    CEAppGlobals* globals = state->dlgHdr.globals;
    CommsConnType conType;
    XP_U16 ids[32];
    XP_S16 nIds = 0;

    for ( ii = 0; ; ++ii ) {
        ConnDlgPair* type = &state->types[ii];
        if ( type->conType == COMMS_CONN_NONE ) {
            break;
        }
        /* make sure tables are in sync */
        XP_ASSERT( ii == conTypeToIndex( state, type->conType ) );
        SendDlgItemMessage( hDlg, state->connComboId, ADDSTRING(globals), 
                            0, (LPARAM)type->str );
    }

    SendDlgItemMessage( hDlg, state->connComboId, SETCURSEL(globals), 
                        conTypeToIndex(state, state->addrRec.conType), 0L );
    ids[nIds++] = state->connComboId;

    conType = state->addrRec.conType;
    if ( state->addrRec.conType == COMMS_CONN_RELAY ) {
#ifdef XWFEATURE_RELAY
        ceSetDlgItemText( hDlg, RELAYNAME_EDIT, 
                          state->addrRec.u.ip_relay.hostName );
        ids[nIds++] = RELAYNAME_EDIT;
        ceSetDlgItemNum( hDlg, RELAYPORT_EDIT, 
                         state->addrRec.u.ip_relay.port );
        ids[nIds++] = RELAYPORT_EDIT;
        ceSetDlgItemText( hDlg, COOKIE_EDIT, 
                          state->addrRec.u.ip_relay.cookie );
        ids[nIds++] = COOKIE_EDIT;
#endif
    } else if ( state->addrRec.conType == COMMS_CONN_IP_DIRECT ) {
#ifdef XWFEATURE_IP_DIRECT
        ceSetDlgItemText( hDlg, IPNAME_EDIT, state->addrRec.u.ip.hostName_ip );
        ids[nIds++] = IPNAME_EDIT;
#endif
    } else if ( state->addrRec.conType == COMMS_CONN_BT ) {
#ifdef XWFEATURE_BLUETOOTH
        if ( state->role == SERVER_ISCLIENT ) {
            ceSetDlgItemText( hDlg, IDC_BLUET_ADDR_EDIT, 
                              state->addrRec.u.bt.hostName );
            ids[nIds++] = IDC_BLUET_ADDR_EDIT;
        }
#endif
    } else if ( state->addrRec.conType == COMMS_CONN_SMS ) {
#ifdef XWFEATURE_SMS
        ceSetDlgItemText( hDlg, IDC_SMS_PHONE_EDIT, state->addrRec.u.sms.phone );
        ids[nIds++] = IDC_SMS_PHONE_EDIT;
        ceSetDlgItemNum( hDlg, IDC_SMS_PORT_EDIT, state->addrRec.u.sms.port );
        ids[nIds++] = IDC_SMS_PORT_EDIT;
#endif
    } else {
        XP_ASSERT(0);
    }

    XP_ASSERT( nIds < VSIZE(ids) );
    if ( state->forShowOnly ) {
        while ( nIds-- > 0 ) {
            ceEnOrDisable( hDlg, ids[nIds], XP_FALSE );
        }
    }

} /* ceControlsFromAddrRec */

static LRESULT CALLBACK
ConnsDlg( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    LRESULT result = FALSE;

    CeConnDlgState* state;

    if ( message == WM_INITDIALOG ) {
        SetWindowLongPtr( hDlg, GWL_USERDATA, lParam );
        state = (CeConnDlgState*)lParam;

        state->connComboId = LB_IF_PPC(state->dlgHdr.globals,IDC_CONNECT_COMBO);
        adjustForConnType( hDlg, state, XP_TRUE );
        ceControlsFromAddrRec( hDlg, state );

        ceDlgSetup( &state->dlgHdr, hDlg, DLG_STATE_TRAPBACK );

        ceDlgComboShowHide( &state->dlgHdr, IDC_CONNECT_COMBO ); 

        result = TRUE;
    } else {
        state = (CeConnDlgState*)GetWindowLongPtr( hDlg, GWL_USERDATA );
        if ( !!state ) {
            XP_U16 id = LOWORD(wParam);

            if ( ceDoDlgHandle( &state->dlgHdr, message, wParam, lParam) ) {
                result = TRUE;

            } else if ( WM_NOTIFY == message ) {
                if ( (id-1) == state->connComboId ) {
                    adjustForConnType( hDlg, state, XP_FALSE );
                }
            } else if ( WM_COMMAND == message ) {
                if ( id == state->connComboId ) {
                    if ( HIWORD(wParam) == CBN_SELCHANGE ) {
                        adjustForConnType( hDlg, state, XP_FALSE );
                        result = TRUE;
                    }
                } else {
                    switch ( id ) {
                    case IDOK:
                        ceControlsToAddrRec( hDlg, state );
                    case IDCANCEL:
                        EndDialog(hDlg, id);
                        state->userCancelled = id == IDCANCEL;
                        result = TRUE;
                    }
                }
            }
        }
    }

    return result;
} /* ConnsDlg */

XP_Bool
WrapConnsDlg( HWND hDlg, CEAppGlobals* globals, const CommsAddrRec* addrRecIn, 
              CommsAddrRec* addrRecOut, DeviceRole role, XP_Bool isNewGame )
{
    XP_Bool result;
    CeConnDlgState state;
    ConnDlgPair types[] = {
#ifdef XWFEATURE_RELAY
        { COMMS_CONN_RELAY,     L"Relay" },
#endif
#ifdef XWFEATURE_IP_DIRECT
        { COMMS_CONN_IP_DIRECT, L"Direct connection" },
#endif
#ifdef XWFEATURE_SMS
        { COMMS_CONN_SMS,       L"Texting" },
#endif
        { COMMS_CONN_NONE,      NULL }
    };

    XP_MEMSET( &state, 0, sizeof( state ) );

    state.dlgHdr.globals = globals;
    state.types = types;
    state.role = role;
    state.forShowOnly = !isNewGame;
    XP_MEMCPY( &state.addrRec, addrRecIn, sizeof(state.addrRec) );

    DialogBoxParam( globals->hInst, (LPCTSTR)IDD_CONNSSDLG, hDlg,
                    (DLGPROC)ConnsDlg, (long)&state );

    result = isNewGame && !state.userCancelled;

    if ( result ) {
        XP_MEMCPY( addrRecOut, &state.addrRec, sizeof(*addrRecOut) );
    }

    return result;
} /* WrapConnsDlg */

#endif
