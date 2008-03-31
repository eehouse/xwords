/* -*-mode: C; fill-column: 77; c-basic-offset: 4; -*- */
/* 
 * Copyright 2005 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#if defined XWFEATURE_RELAY || defined XWFEATURE_BLUETOOTH

#include "cecondlg.h"
#include "ceutil.h"
#include "debhacks.h"

static void
ceControlsToAddrRec( HWND hDlg, CeConnDlgState* cState )
{
    XP_U16 len;

    if ( cState->addrRec.conType == COMMS_CONN_RELAY ) {
#ifdef XWFEATURE_RELAY
        len = sizeof(cState->addrRec.u.ip_relay.hostName);
        ceGetDlgItemText( hDlg, RELAYNAME_EDIT, 
                          cState->addrRec.u.ip_relay.hostName, &len );
        cState->addrRec.u.ip_relay.port = 
            (XP_U16)ceGetDlgItemNum( hDlg, RELAYPORT_EDIT );
        len = sizeof(cState->addrRec.u.ip_relay.cookie);
        ceGetDlgItemText( hDlg, COOKIE_EDIT, cState->addrRec.u.ip_relay.cookie, 
                          &len );
#endif
    } else if ( cState->addrRec.conType == COMMS_CONN_BT ) {
#ifdef XWFEATURE_BLUETOOTH
        if ( cState->role == SERVER_ISCLIENT ) {
            len = sizeof(cState->addrRec.u.bt.hostName);
            ceGetDlgItemText( hDlg, IDC_BLUET_ADDR_EDIT, 
                              cState->addrRec.u.bt.hostName, &len );
        }
#endif
    } else {
        XP_ASSERT(0);
    }
} /* ceControlsToAddrRec */

static void
adjustForConnType( HWND hDlg, const CeConnDlgState* cState )
{
    XP_U16 relayIds[] = { 
        IDC_COOKIE_LAB,
#ifdef XWFEATURE_RELAY
        COOKIE_EDIT,IDC_CRELAYHINT_LAB,IDC_CRELAYNAME_LAB,RELAYNAME_EDIT, 
        IDC_CRELAYPORT_LAB, RELAYPORT_EDIT,
#endif
        0 };
    XP_U16 btIds[] = { 
        IDC_BLUET_ADDR_LAB,
#ifdef XWFEATURE_BLUETOOTH
        IDC_BLUET_ADDR_EDIT, IDC_BLUET_ADDR_BROWSE,
#endif
        0 };
    XP_U16* allIDs[] = { relayIds, btIds };
    XP_U16* on = NULL;
    XP_U16 i;

    if ( cState->addrRec.conType == COMMS_CONN_RELAY ) {
        on = relayIds;
    } else if ( cState->addrRec.conType == COMMS_CONN_BT ) {
        on = 
#ifdef XWFEATURE_BLUETOOTH
            cState->role != SERVER_ISCLIENT ? NULL:
#endif
        btIds;             /* we want the "disabled" message */
    }

    for ( i = 0; i < VSIZE(allIDs); ++i ) {
        XP_U16* ids = allIDs[i];
        XP_Bool enable = ids == on;
        while ( *ids != 0 ) {
            ceShowOrHide( hDlg, *(ids++), enable );
        }
    }
} /* adjustForConnType */

static XP_U16
conTypeToIndex( CommsConnType conType )
{
    XP_U16 index = 0;
    switch( conType ) {
    case COMMS_CONN_RELAY:
        index = 1;
        break;
    case COMMS_CONN_BT:
        index = 0;
        break;
    default:
        XP_ASSERT(0);
    }
    return index;
}

static CommsConnType
indexToConType( XP_U16 index )
{
    CommsConnType conType = COMMS_CONN_NONE;
    switch( index ) {
    case 0:
        conType = COMMS_CONN_BT; break;
    case 1:
        conType = COMMS_CONN_RELAY; break;
    default:
        XP_ASSERT(0);
    }
    return conType;
}

static void
ceControlsFromAddrRec( HWND hDlg, const CeConnDlgState* cState )
{
    XP_U16 i;
    wchar_t* strs[] = { 
        L"Bluetooth"
        , L"WiFi/Cellular data"
    };

    for ( i = 0; i < VSIZE(strs); ++i ) {
        SendDlgItemMessage( hDlg, IDC_CONNECTCOMBO, CB_ADDSTRING, 
                            0, (LPARAM)strs[i] );
    }

    SendDlgItemMessage( hDlg, IDC_CONNECTCOMBO, CB_SETCURSEL, 
                        conTypeToIndex(cState->addrRec.conType), 0L );

    if ( cState->addrRec.conType == COMMS_CONN_RELAY ) {
#ifdef XWFEATURE_RELAY
        ceSetDlgItemText( hDlg, RELAYNAME_EDIT, 
                          cState->addrRec.u.ip_relay.hostName );
        ceSetDlgItemNum( hDlg, RELAYPORT_EDIT, 
                         cState->addrRec.u.ip_relay.port );
        ceSetDlgItemText( hDlg, COOKIE_EDIT, 
                          cState->addrRec.u.ip_relay.cookie );
#endif
    } else if ( cState->addrRec.conType == COMMS_CONN_BT ) {
#ifdef XWFEATURE_BLUETOOTH
        if ( cState->role == SERVER_ISCLIENT ) {
            ceSetDlgItemText( hDlg, IDC_BLUET_ADDR_EDIT, 
                              cState->addrRec.u.bt.hostName );
        }
#endif
    } else {
        XP_ASSERT(0);
    }
}

static LRESULT CALLBACK
ConnsDlg( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    LRESULT result = FALSE;

    CeConnDlgState* cState;
    CEAppGlobals* globals;

    if ( message == WM_INITDIALOG ) {
        SetWindowLong( hDlg, GWL_USERDATA, lParam );
        cState = (CeConnDlgState*)lParam;
        globals = cState->globals;

        adjustForConnType( hDlg, cState );

        ceControlsFromAddrRec( hDlg, cState );

        ceDlgSetup( globals, hDlg );

        result = TRUE;
    } else {
        cState = (CeConnDlgState*)GetWindowLong( hDlg, GWL_USERDATA );
        if ( !!cState ) {
            globals = cState->globals;

            if ( message == WM_COMMAND ) {
                XP_U16 id = LOWORD(wParam);

                switch( id ) {

                case IDC_CONNECTCOMBO:
                    if ( HIWORD(wParam) == CBN_SELCHANGE ) {
                        XP_S16 sel;
                        sel = SendDlgItemMessage( hDlg, IDC_CONNECTCOMBO,
                                                  CB_GETCURSEL, 0, 0L );
                        cState->addrRec.conType = indexToConType( sel );
                        adjustForConnType( hDlg, cState );
                        result = TRUE;
                    }
                    break;

                case IDOK:
                    ceControlsToAddrRec( hDlg, cState );
                case IDCANCEL:
                    EndDialog(hDlg, id);
                    cState->userCancelled = id == IDCANCEL;
                    result = TRUE;
                }
            } else if ( message == WM_VSCROLL ) {
                if ( !IS_SMARTPHONE(globals) ) {
                    ceDoDlgScroll( hDlg, wParam );
                }
            }
        }
    }

    return result;
} /* ConnsDlg */

XP_Bool
WrapConnsDlg( HWND hDlg, CEAppGlobals* globals, const CommsAddrRec* addrRec, 
              DeviceRole role, CeConnDlgState* state )
{
    XP_Bool result;
    XP_MEMSET( state, 0, sizeof( *state ) );

    state->globals = globals;
    state->role = role;
    XP_MEMCPY( &state->addrRec, addrRec, sizeof(state->addrRec) );

    DialogBoxParam( globals->hInst, (LPCTSTR)IDD_CONNSSDLG, hDlg,
                    (DLGPROC)ConnsDlg, (long)state );

    result = !state->userCancelled;
    return result;
} /* WrapConnsDlg */

#endif
