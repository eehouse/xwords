/* -*- fill-column: 77; compile-command: "make -j2 TARGET_OS=wince DEBUG=TRUE" -*- */
/* 
 * Copyright 2002-2009 by Eric House (xwords@eehouse.org).  All rights
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

#include "ceblank.h"
#include "cemain.h"
#include "ceutil.h"
#include "debhacks.h"
#include "cedebug.h"

typedef struct BlankDialogState {
    CeDlgHdr dlgHdr;
    const PickInfo* pi;
    XP_U16 playerNum;
    const XP_UCHAR** texts;
    XP_U16 nTiles;
    XP_S16 result;
    XP_Bool canBackup;
} BlankDialogState; 

static void
loadLettersList( BlankDialogState* bState )
{
    XP_U16 ii;
    XP_U16 nTiles = bState->nTiles;
    HWND hDlg = bState->dlgHdr.hDlg;
    CEAppGlobals* globals = bState->dlgHdr.globals;
    const XP_UCHAR** texts = bState->texts;
    
    for ( ii = 0; ii < nTiles; ++ii ) {	
        XP_U16 len;
        wchar_t widebuf[4];

        len = MultiByteToWideChar( CP_UTF8, 0, texts[ii], strlen(texts[ii]),
                                   widebuf, VSIZE(widebuf) );
        widebuf[len] = 0;

        SendDlgItemMessage( hDlg, LB_IF_PPC(globals,BLANKFACE_LIST), 
                            ADDSTRING(globals), 0, (long)widebuf );
    }

    SendDlgItemMessage( hDlg, LB_IF_PPC(globals,BLANKFACE_LIST), 
                        SETCURSEL(globals), 0, 0 );
#ifdef _WIN32_WCE
    SendDlgItemMessage( hDlg, BLANKFACE_LIST_PPC, LB_SETANCHORINDEX, 0, 0 );
#endif
} /* loadLettersList */

#ifdef FEATURE_TRAY_EDIT
static void
showCurTray( HWND hDlg, BlankDialogState* bState )
{
    if ( bState->pi->why == PICK_FOR_CHEAT ) {
        const PickInfo* pi = bState->pi;
        XP_U16 lenSoFar = 0;
        XP_U16 i;
        XP_UCHAR labelBuf[48];
        wchar_t widebuf[48];
        XP_UCHAR* name;
        CEAppGlobals* globals = bState->dlgHdr.globals;

        name = globals->gameInfo.players[bState->playerNum].name;

        lenSoFar += XP_SNPRINTF( labelBuf + lenSoFar, 
                                 sizeof(labelBuf) - lenSoFar,
                                 "%d of %d for %s" XP_CR "Cur", 
                                 pi->thisPick + 1, pi->nTotal, name );

        for ( i = 0; i < pi->nCurTiles; ++i ) {
            lenSoFar += XP_SNPRINTF( labelBuf+lenSoFar, 
                                     sizeof(labelBuf)-lenSoFar, "%s%s",
                                     i==0?": ":", ", pi->curTiles[i] );
        }

        (void)MultiByteToWideChar( CP_UTF8, 0, labelBuf, lenSoFar + 1, widebuf, 
                                   VSIZE(widebuf) + sizeof(widebuf[0]) );

        SetDlgItemText( hDlg,IDC_PICKMSG, widebuf );
    }
} /* showCurTray */
#endif

static LRESULT CALLBACK
BlankDlg(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    BlankDialogState* bState;
    XP_U16 id;
    LRESULT result = FALSE;     /* default */

    if ( message == WM_INITDIALOG ) {
        SetWindowLongPtr( hDlg, GWL_USERDATA, lParam );
        bState = (BlankDialogState*)lParam;

#ifdef FEATURE_TRAY_EDIT
        if ( bState->pi->why == PICK_FOR_CHEAT ) {
            showCurTray( hDlg, bState );
            ceShowOrHide( hDlg, IDC_BPICK, XP_FALSE );
        } else {
            XP_ASSERT( bState->pi->why == PICK_FOR_BLANK );
            ceShowOrHide( hDlg, IDC_CPICK, XP_FALSE );
            ceShowOrHide( hDlg, IDC_PICKMSG, XP_FALSE );
        }
        bState->canBackup = (bState->pi->why == PICK_FOR_CHEAT)
            && (bState->pi->thisPick > 0);
        ceShowOrHide( hDlg, IDC_BACKUP, bState->canBackup );
#endif

        ceDlgSetup( &bState->dlgHdr, hDlg, DLG_STATE_TRAPBACK );
        ceDlgComboShowHide( &bState->dlgHdr, BLANKFACE_LIST ); 

        loadLettersList( bState );
        result = TRUE;
    } else {
        bState = (BlankDialogState*)GetWindowLongPtr( hDlg, GWL_USERDATA );
        if ( !!bState ) {

            if ( ceDoDlgHandle( &bState->dlgHdr, message, wParam, lParam) ) {
                result = TRUE;
                goto exit;
            }

            switch ( message ) {
            case WM_COMMAND:
                id = LOWORD(wParam);
                if ( 0 ) {
#ifdef FEATURE_TRAY_EDIT
                } else if ( id == IDCANCEL ) {
                    bState->result = PICKER_PICKALL;
                } else if ( id == IDC_BACKUP ) {
                    bState->result = PICKER_BACKUP;
#endif
                } else if ( id == IDOK ) {
                    CEAppGlobals* globals = bState->dlgHdr.globals;
                    bState->result = (XP_S16)
                        SendDlgItemMessage( hDlg, 
                                            LB_IF_PPC(globals,BLANKFACE_LIST), 
                                            GETCURSEL(globals), 0, 0 );
                } else {
                    break;
                }
                EndDialog( hDlg, id );
                result = TRUE;
            }
        }
    }

 exit:
    return result;
} /* BlankDlg */

XP_Bool
WrapBlankDlg( CEAppGlobals* globals, const PickInfo* pi,
              XP_U16 playerNum, const XP_UCHAR** texts, XP_U16 nTiles )
{
    BlankDialogState state;
    XP_MEMSET( &state, 0, sizeof(state) );

    state.dlgHdr.globals = globals;
    state.texts = texts;
    state.nTiles = nTiles;
    state.playerNum = playerNum;
    state.pi = pi;

    assertOnTop( globals->hWnd );
    DialogBoxParam( globals->locInst, (LPCTSTR)IDD_ASKBLANK, globals->hWnd, 
                    (DLGPROC)BlankDlg, (long)&state );
    return state.result;
}
