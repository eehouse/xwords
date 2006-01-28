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

#ifndef XWFEATURE_STANDALONE_ONLY

#include "cecondlg.h"
#include "ceutil.h"
#include "debhacks.h"

static void
ceControlsToAddrRec( HWND hDlg, CeConnDlgState* cState )
{
    XP_U16 len;

    len = sizeof(cState->addrRec.u.ip_relay.hostName);
    ceGetDlgItemText( hDlg, RELAYNAME_EDIT, 
                      cState->addrRec.u.ip_relay.hostName, &len );
    cState->addrRec.u.ip_relay.port = 
        (XP_U16)ceGetDlgItemNum( hDlg, RELAYPORT_EDIT );
    len = sizeof(cState->addrRec.u.ip_relay.cookie);
    ceGetDlgItemText( hDlg, COOKIE_EDIT, cState->addrRec.u.ip_relay.cookie, 
                      &len );
                      
}

static void
ceControlsFromAddrRec( HWND hDlg, const CeConnDlgState* cState )
{
    XP_UCHAR* str;

    switch( cState->addrRec.conType ) {
    case COMMS_CONN_RELAY:
        str = L"WiFi/Cellular data";
        break;
    default:
        XP_LOGF( "conType is %d", cState->addrRec.conType );
        XP_ASSERT( 0 );
        str = L"bad conType";
        break;
    }
    SendDlgItemMessage( hDlg, IDC_CONNECTCOMBO, CB_ADDSTRING, 0, str );
    SendDlgItemMessage( hDlg, IDC_CONNECTCOMBO, CB_SETCURSEL, 0, 0L );

    ceSetDlgItemText( hDlg, RELAYNAME_EDIT, cState->addrRec.u.ip_relay.hostName );
    ceSetDlgItemNum( hDlg, RELAYPORT_EDIT, cState->addrRec.u.ip_relay.port );
    ceSetDlgItemText( hDlg, COOKIE_EDIT, cState->addrRec.u.ip_relay.cookie );
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

        ceControlsFromAddrRec( hDlg, cState );
        result = TRUE;
    } else {
        cState = (CeConnDlgState*)GetWindowLong( hDlg, GWL_USERDATA );
        if ( !!cState ) {
            globals = cState->globals;

            if ( message == WM_COMMAND ) {
                XP_U16 id = LOWORD(wParam);

                switch( id ) {
                case IDOK:
                    ceControlsToAddrRec( hDlg, cState );
                case IDCANCEL:
                    EndDialog(hDlg, id);
                    cState->userCancelled = id == IDCANCEL;
                    result = TRUE;
                }
            }
        }
    }

    return result;
} /* ConnsDlg */

XP_Bool
WrapConnsDlg( HWND hDlg, CEAppGlobals* globals, const CommsAddrRec* addrRec, 
              CeConnDlgState* state )
{
    XP_Bool result;
    XP_MEMSET( state, 0, sizeof( *state ) );

    XP_LOGF( "WrapConnsDlg" );

    state->globals = globals;

    XP_MEMCPY( &state->addrRec, addrRec, sizeof(state->addrRec) );

    DH(DialogBoxParam)( globals->hInst, (LPCTSTR)IDD_CONNSSDLG, hDlg,
                        (DLGPROC)ConnsDlg, (long)state );

    result = !state->userCancelled;
    return result;
} /* WrapConnsDlg */

#endif
