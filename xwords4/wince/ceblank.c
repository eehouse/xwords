/* -*-mode: C; fill-column: 77; c-basic-offset: 4; -*- */
/* 
 * Copyright 2002 by Eric House (fixin@peak.org).  All rights reserved.
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

#include "ceblank.h"
#include "cemain.h"
#include "ceutil.h"

static void
loadLettersList( HWND hDlg, BlankDialogState* bState )
{
    XP_U16 i;
    XP_U16 nTiles = bState->nTiles;
    XP_UCHAR4* texts = bState->texts;
    
    for ( i = 0; i < nTiles; ++i ) {	
        XP_U16 len;
        wchar_t widebuf[4];

        len = MultiByteToWideChar( CP_ACP, MB_PRECOMPOSED, 
                                   texts[i], strlen(texts[i]),
                                   widebuf, 
                                   sizeof(widebuf)/sizeof(widebuf[0]) );
        widebuf[len] = 0;

        SendDlgItemMessage( hDlg, BLANKFACE_LIST, LB_ADDSTRING, 
                            0, (long)widebuf );
    }
    ce_selectAndShow( hDlg, BLANKFACE_LIST, 0 );
} /* loadLettersList */

LRESULT CALLBACK
BlankDlg(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    BlankDialogState* bState;
    XP_U16 id;

    if ( message == WM_INITDIALOG ) {
        SetWindowLong( hDlg, GWL_USERDATA, lParam );
        bState = (BlankDialogState*)lParam;

        positionDlg( hDlg );

        loadLettersList( hDlg, bState );
    } else {
        bState = (BlankDialogState*)GetWindowLong( hDlg, GWL_USERDATA );

        switch (message) {
        case WM_COMMAND:
            id = LOWORD(wParam);
            switch( id ) {
            case IDOK:
                bState->result = SendDlgItemMessage( hDlg, BLANKFACE_LIST, 
                                                     LB_GETCURSEL, 0, 0 );
                EndDialog( hDlg, id );
                return TRUE;
            }
        }

    }

    return FALSE;
} /* BlankDlg */
