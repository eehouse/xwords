/* -*-mode: C; fill-column: 77; c-basic-offset: 4; -*- */
/* 
 * Copyright 2004 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#ifdef XWFEATURE_SEARCHLIMIT

#include <stdio.h>
#include "cehntlim.h"

static void
initComboBox( HintLimitsState* state, XP_U16 id, XP_U16 startVal )
{
    HWND hDlg = state->dlgHdr.hDlg;
    XP_U16 ii;
    for ( ii = 0; ii < MAX_TRAY_TILES; ++ii ) {
        wchar_t str[4];
        swprintf( str, L"%d", ii+1 );

        SendDlgItemMessage( hDlg, id, INSERTSTRING(state->dlgHdr.globals), ii, (long)str );

        if ( (ii+1) == startVal ) {
            SendDlgItemMessage( hDlg, id, SETCURSEL(state->dlgHdr.globals), ii, 0L );
        }
    }
    
} /* initComboBox */

static XP_U16
getComboValue( HintLimitsState* state, XP_U16 id )
{
    HWND hDlg = state->dlgHdr.hDlg;
    LONG result;
    result = SendDlgItemMessage( hDlg, id, GETCURSEL(state->dlgHdr.globals), 0, 0L );
    if ( result == CB_ERR ) {
        result = 1;
    }
    return (XP_U16)result + 1;  /* number is 1-based but index 0-based */
} /* getComboValue */

LRESULT CALLBACK
HintLimitsDlg( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    HintLimitsState* hState;

    if ( message == WM_INITDIALOG ) {
        SetWindowLongPtr( hDlg, GWL_USERDATA, lParam );
        hState = (HintLimitsState*)lParam;

        ceDlgSetup( &hState->dlgHdr, hDlg, DLG_STATE_NONE );
        ceDlgComboShowHide( &hState->dlgHdr, HC_MIN_COMBO );
        ceDlgComboShowHide( &hState->dlgHdr, HC_MAX_COMBO );

        return TRUE;
    } else {
        hState = (HintLimitsState*)GetWindowLongPtr( hDlg, GWL_USERDATA );
        if ( !!hState ) {

            if ( !hState->inited ) {
                initComboBox( hState, 
                              LB_IF_PPC(hState->dlgHdr.globals, HC_MIN_COMBO), 
                              hState->min );
                initComboBox( hState, 
                              LB_IF_PPC(hState->dlgHdr.globals,HC_MAX_COMBO), 
                              hState->max );
                hState->inited = XP_TRUE;
            }

            if ( ceDoDlgHandle( &hState->dlgHdr, message, wParam, lParam) ) {
                return TRUE;
            }

            if ( (message == WM_COMMAND) && (BN_CLICKED == HIWORD(wParam) ) ) {
                XP_U16 id = LOWORD(wParam);
                switch( id ) {
                case IDOK: 
                    hState->min = getComboValue( hState, 
                                                 LB_IF_PPC(hState->dlgHdr.globals,HC_MIN_COMBO) );
                    hState->max = getComboValue( hState, 
                                                 LB_IF_PPC(hState->dlgHdr.globals,HC_MAX_COMBO) );
                case IDCANCEL:
                    hState->cancelled = id == IDCANCEL;
                    
                    EndDialog( hDlg, id );
                    return TRUE;
                }
            }
        }
    }

    return FALSE;
} /* HintLimitsDlg */

#endif /* XWFEATURE_SEARCHLIMIT */
