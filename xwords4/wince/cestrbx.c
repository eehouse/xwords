/* -*- fill-column: 77; c-basic-offset: 4; compile-command: "make TARGET_OS=wince DEBUG=TRUE" -*- */
/* 
 * Copyright 2002-2009 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#include <windowsx.h>

#include "cestrbx.h"
#include "cemain.h"
#include "ceutil.h"
#include "cedebug.h"

typedef struct StrBoxState {
    CeDlgHdr dlgHdr;
    const wchar_t* title;
#ifndef _WIN32_WCE
    HFONT font;
#endif
    XWStreamCtxt* stream;
    XP_U16 result;
    XP_Bool isQuery;
    XP_Bool textIsSet;
} StrBoxState;

static void
stuffTextInField( HWND hDlg, StrBoxState* state )
{
    XP_U16 nBytes = stream_getSize(state->stream);
    XP_U16 len, crlen;
    XP_UCHAR* sbuf;
    wchar_t* wbuf;

    sbuf = XP_MALLOC( state->dlgHdr.globals->mpool, nBytes + 1 );
    stream_getBytes( state->stream, sbuf, nBytes );

    crlen = strlen(XP_CR);
    if ( 0 == strncmp( XP_CR, &sbuf[nBytes-crlen], crlen ) ) {
        nBytes -= crlen;
    }
    sbuf[nBytes] = '\0';

    len = MultiByteToWideChar( CP_UTF8, 0, sbuf, nBytes, NULL, 0 );
    wbuf = XP_MALLOC( state->dlgHdr.globals->mpool, (len+1) * sizeof(*wbuf) );
    MultiByteToWideChar( CP_UTF8, 0, sbuf, nBytes, wbuf, len );
    XP_FREE( state->dlgHdr.globals->mpool, sbuf );
    wbuf[len] = 0;

    SetDlgItemText( hDlg, ID_EDITTEXT, wbuf );
    XP_FREE( state->dlgHdr.globals->mpool, wbuf );
} /* stuffTextInField */

LRESULT CALLBACK
StrBox(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    LRESULT handled = FALSE;
    StrBoxState* state;
    XP_U16 id;

    if ( message == WM_INITDIALOG ) {
        SetWindowLongPtr( hDlg, GWL_USERDATA, (long)lParam );
        state = (StrBoxState*)lParam;

        if  ( !!state->title ) {
            SendMessage( hDlg, WM_SETTEXT, 0, (long)state->title );
        }
#ifndef _WIN32_WCE
        SendDlgItemMessage( hDlg, ID_EDITTEXT, WM_SETFONT,
                            (WPARAM)state->font, 0L );
#endif
        if ( !state->isQuery ) {
            ceShowOrHide( hDlg, IDCANCEL, XP_FALSE );
        }

        ceDlgSetup( &state->dlgHdr, hDlg, 
                    state->isQuery? DLG_STATE_NONE : DLG_STATE_OKONLY );

        handled = TRUE;
    } else {
        state = (StrBoxState*)GetWindowLongPtr( hDlg, GWL_USERDATA );

        if ( !!state ) {
            if ( ceDoDlgHandle( &state->dlgHdr, message, wParam, lParam) ) {
                handled = TRUE;
            } else {
                switch (message) {

                case WM_COMMAND:

                    /* If I add the text above in the WM_INITDIALOG section it
                       shows up selected though selStart and selEnd are 0. */
                    if ( !state->textIsSet ) {
                        state->textIsSet = XP_TRUE;
                        stuffTextInField( hDlg, state );
                    }

                    id = LOWORD(wParam);
                    switch( id ) {

                    case IDOK:
                    case IDCANCEL:
                        state->result = id;
                        EndDialog(hDlg, id);
                        handled = TRUE;
                    }
                    break;
                }
            }
        }
    }
    return handled;
} /* StrBox */

XP_Bool
WrapStrBox( CEAppGlobals* globals, const wchar_t* title,
            XWStreamCtxt* stream, XP_U16 buttons )
{
    StrBoxState state;

    XP_MEMSET( &state, 0, sizeof(state) );

    state.title = title;
    state.stream = stream;
    state.isQuery = (buttons & ~MB_ICONMASK) != MB_OK;
    state.dlgHdr.globals = globals;

#ifndef _WIN32_WCE
    LOGFONT fontInfo;
    XP_MEMSET( &fontInfo, 0, sizeof(fontInfo) );
    fontInfo.lfHeight = 14;
    fontInfo.lfQuality = PROOF_QUALITY;
    wcscpy( fontInfo.lfFaceName, IS_SMARTPHONE(globals)? 
            L"Segoe Condensed" : L"Tahoma" );
    state.font = CreateFontIndirect( &fontInfo );
#endif

    assertOnTop( globals->hWnd );
    DialogBoxParam( globals->locInst, (LPCTSTR)IDD_STRBOX, globals->hWnd, 
                    (DLGPROC)StrBox, (long)&state );

#ifndef _WIN32_WCE
    DeleteObject( state.font );
#endif

    return state.result == IDOK;
}
