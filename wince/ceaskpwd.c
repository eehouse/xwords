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
#include "ceresstr.h"
#include <wchar.h>

static void
nameToLabel( PasswdDialogState* pState, const XP_UCHAR* name, XP_U16 labelID )
{
    wchar_t wideName[64];
    wchar_t wBuf[128];
    const wchar_t* fmt;

    MultiByteToWideChar( CP_ACP, MB_PRECOMPOSED, name, -1, wideName,
                         VSIZE(wideName) );

    fmt = ceGetResStringL( pState->dlgHdr.globals, IDS_PASSWDFMT_L );
    swprintf( wBuf, fmt, wideName );

    SendDlgItemMessage( pState->dlgHdr.hDlg, labelID, WM_SETTEXT, 
                        0, (long)wBuf );
} /* nameToLabel */

LRESULT CALLBACK
PasswdDlg(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    PasswdDialogState* pState;
    XP_U16 id;

    if ( message == WM_INITDIALOG ) {
        SetWindowLongPtr( hDlg, GWL_USERDATA, lParam );
        pState = (PasswdDialogState*)lParam;

        ceDlgSetup( &pState->dlgHdr, hDlg, DLG_STATE_TRAPBACK );

        nameToLabel( pState, pState->name, IDC_PWDLABEL );

        return TRUE;
    } else {
        pState = (PasswdDialogState*)GetWindowLongPtr( hDlg, GWL_USERDATA );
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
