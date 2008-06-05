/* -*-mode: C; fill-column: 77; c-basic-offset: 4; -*- */
/* 
 * Copyright 2002 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#include "ceaskpwd.h"
#include "cemain.h"
#include "ceutil.h"
#include "debhacks.h"
#include <stdio.h>              /* swprintf */

static void
nameToLabel( HWND hDlg, const XP_UCHAR* name, XP_U16 labelID )
{
    wchar_t wideName[128];
    wchar_t wBuf[128];
    XP_U16 len;

    len = (XP_U16)XP_STRLEN( name );
    MultiByteToWideChar( CP_ACP, MB_PRECOMPOSED, name, len + 1,
                         wideName, len + 1 );

    swprintf( wBuf, L"Enter password for %s:", wideName );

    SendDlgItemMessage( hDlg, labelID, WM_SETTEXT, 0, (long)wBuf );
} /* nameToLabel */

LRESULT CALLBACK
PasswdDlg(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    PasswdDialogState* pState;
    XP_U16 id;

    if ( message == WM_INITDIALOG ) {
        SetWindowLong( hDlg, GWL_USERDATA, lParam );
        pState = (PasswdDialogState*)lParam;

        ceDlgSetup( &pState->dlgHdr, hDlg, DLG_STATE_TRAPBACK );

        nameToLabel( hDlg, pState->name, IDC_PWDLABEL );

        return TRUE;
    } else {
        pState = (PasswdDialogState*)GetWindowLong( hDlg, GWL_USERDATA );
        if ( !!pState ) {

            if ( ceDoDlgHandle( &pState->dlgHdr, message, wParam, lParam ) ) {
                return TRUE;
            }

            switch ( message ) {
            case WM_COMMAND:
                id = LOWORD(wParam);
                switch( id ) {
                case IDOK: 
                    ceGetDlgItemText( hDlg, PASS_EDIT, pState->buf, 
                                      pState->lenp );
                case IDCANCEL:
                    pState->userCancelled = id == IDCANCEL;
                    EndDialog( hDlg, id );

                    return TRUE;
                }
            }
        }
    }
    return FALSE;
} /* PasswdDlg */
