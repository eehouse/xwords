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

#ifdef FEATURE_TRAY_EDIT
static void
showCurTray( HWND hDlg, BlankDialogState* bState )
{
    if ( bState->pi->why == PICK_FOR_CHEAT ) {
        PickInfo* pi = bState->pi;
        XP_U16 lenSoFar = 0;
        XP_U16 i;
        XP_UCHAR labelBuf[48];
        wchar_t widebuf[48];
        XP_UCHAR* name;

        name = bState->globals->gameInfo.players[bState->playerNum].name;

        lenSoFar += XP_SNPRINTF( labelBuf + lenSoFar, 
                                 sizeof(labelBuf) - lenSoFar,
                                 "%d of %d for %s" XP_CR "Cur", 
                                 pi->thisPick, pi->nTotal, name );

        for ( i = 0; i < pi->nCurTiles; ++i ) {
            lenSoFar += XP_SNPRINTF( labelBuf+lenSoFar, 
                                     sizeof(labelBuf)-lenSoFar, "%s%s",
                                     i==0?": ":", ", pi->curTiles[i] );
        }

        (void)MultiByteToWideChar( CP_ACP, MB_PRECOMPOSED, 
                                   labelBuf, lenSoFar + 1,
                                   widebuf, 
                                   (sizeof(widebuf)/sizeof(widebuf[0]))
                                   + sizeof(widebuf[0]) );

        SetDlgItemText( hDlg,IDC_PICKMSG, widebuf );
    }
} /* showCurTray */
#endif

LRESULT CALLBACK
BlankDlg(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    BlankDialogState* bState;
    XP_U16 id;
    XP_UCHAR ch;

    if ( message == WM_INITDIALOG ) {
        SetWindowLong( hDlg, GWL_USERDATA, lParam );
        bState = (BlankDialogState*)lParam;

        positionDlg( hDlg );

#ifdef FEATURE_TRAY_EDIT
        if ( bState->pi->why == PICK_FOR_CHEAT ) {
            showCurTray( hDlg, bState );
            ceShowOrHide( hDlg, IDC_BPICK, XP_FALSE );
        } else {
            XP_ASSERT( bState->pi->why == PICK_FOR_BLANK );
            ceShowOrHide( hDlg, IDC_PICKALL, XP_FALSE );
            ceShowOrHide( hDlg, IDC_CPICK, XP_FALSE );
        }
#endif

        loadLettersList( hDlg, bState );
    } else {
        XP_UCHAR4* texts;
        XP_U16 i;

        bState = (BlankDialogState*)GetWindowLong( hDlg, GWL_USERDATA );

        switch (message) {
        case WM_KEYDOWN:           /* key down.  Select a list item? */
            XP_LOGF( "got WM_KEYDOWN" );
            break;
#if 0                           /* this isn't working */
        case WM_CHAR:           /* key down.  Select a list item? */
            ch = (XP_UCHAR)wParam;
            if ( ch >= 'a' && ch <= 'z' ) {
                ch += 'A' - 'a';
            }

            XP_LOGF( "BlankDlg: got char: %c", ch );
            
            texts = bState->texts;
            for ( i = bState->nTiles - 1; i >= 0 ; --i ) {
                if ( ch == texts[i][0] ) {
                    ce_selectAndShow( hDlg, BLANKFACE_LIST, i );
                    break;
                }
            }

            break;
#endif
        case WM_COMMAND:
            id = LOWORD(wParam);
            if ( id == IDC_PICKALL ) {
                bState->result = -1;
            } else if ( id == IDOK ) {
                bState->result = 
                    (XP_S16)SendDlgItemMessage( hDlg, BLANKFACE_LIST, 
                                                LB_GETCURSEL, 0, 0 );
            } else {
                break;
            }
            EndDialog( hDlg, id );
            return TRUE;
        }

    }

    return FALSE;
} /* BlankDlg */
