/* -*- fill-column: 77; c-basic-offset: 4; compile-command: "make TARGET_OS=wince DEBUG=TRUE" -*- */
/* 
 * Copyright 2002-2006 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#include "cestrbx.h"
#include "cemain.h"
#include "ceutil.h"

static void
stuffTextInField( HWND hDlg, StrBoxState* state )
{
    XP_U16 nBytes = stream_getSize(state->stream);
    XP_U16 len, crlen;
    XP_UCHAR* sbuf;
    wchar_t* wbuf;
#ifdef MEM_DEBUG
    CEAppGlobals* globals = state->dlgHdr.globals;
#endif

    sbuf = XP_MALLOC( globals->mpool, nBytes + 1 );
    stream_getBytes( state->stream, sbuf, nBytes );

    crlen = strlen(XP_CR);
    if ( 0 == strncmp( XP_CR, &sbuf[nBytes-crlen], crlen ) ) {
        nBytes -= crlen;
    }
    sbuf[nBytes] = '\0';

    len = MultiByteToWideChar( CP_ACP, MB_PRECOMPOSED, sbuf, nBytes,
                               NULL, 0 );
    wbuf = XP_MALLOC( globals->mpool, (len+1) * sizeof(*wbuf) );
    MultiByteToWideChar( CP_ACP, MB_PRECOMPOSED, sbuf, nBytes,
                         wbuf, len );
    XP_FREE( globals->mpool, sbuf );
    wbuf[len] = 0;

    SetDlgItemText( hDlg, ID_EDITTEXT, wbuf );
    XP_FREE( globals->mpool, wbuf );
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

        if ( !state->isQuery ) {
            ceShowOrHide( hDlg, IDCANCEL, XP_FALSE );
            /* also want to expand the text box to the bottom */
            /* ceIsLandscape() is going away.... */
/*             if ( !ceIsLandscape( state->dlgHdr.globals ) ) { */
/*                 ceCenterCtl( hDlg, IDOK ); */
/*             } */
        }

        ceDlgSetup( &state->dlgHdr, hDlg, DLG_STATE_NONE );

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
