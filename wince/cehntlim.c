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
#include "ceutil.h"

static void
initComboBox( HWND hDlg, XP_U16 id, XP_U16 startVal )
{
    XP_U16 i;
    for ( i = 1; i <= MAX_TRAY_TILES; ++i ) {
        wchar_t str[4];
        swprintf( str, L"%d", i );

        SendDlgItemMessage( hDlg, id, CB_ADDSTRING, 0, (long)str );

        if ( i == startVal ) {
            SendDlgItemMessage( hDlg, id, CB_SETCURSEL, i-1, 0L );
        }
    }
    
} /* initComboBox */

static XP_U16
getComboValue( HWND hDlg, XP_U16 id )
{
    LONG result;
    result = SendDlgItemMessage( hDlg, id, CB_GETCURSEL, 0, 0L );
    if ( result == CB_ERR ) {
        result = 1;
    }
    return (XP_U16)result + 1;  /* number is 1-based but index 0-based */
} /* getComboValue */

LRESULT CALLBACK
HintLimitsDlg( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    HintLimitsState* hState;
    CEAppGlobals* globals;
    XP_U16 id;

    if ( message == WM_INITDIALOG ) {
        SetWindowLong( hDlg, GWL_USERDATA, lParam );
        hState = (HintLimitsState*)lParam;
        globals = hState->globals;

        ceDlgSetup( globals, hDlg );

        return TRUE;
    } else {
        hState = (HintLimitsState*)GetWindowLong( hDlg, GWL_USERDATA );
        if ( !!hState ) {

            if ( !hState->inited ) {
                initComboBox( hDlg, HC_MIN_COMBO, hState->min );
                initComboBox( hDlg, HC_MAX_COMBO, hState->max );
                hState->inited = XP_TRUE;
            }

            switch ( message ) {
            case WM_COMMAND:
                id = LOWORD(wParam);
                switch( id ) {
                case IDOK: 
                    hState->min = getComboValue( hDlg, HC_MIN_COMBO );
                    hState->max = getComboValue( hDlg, HC_MAX_COMBO );
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
