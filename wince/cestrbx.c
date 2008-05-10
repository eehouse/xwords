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
stuffTextInField( HWND hDlg, StrBoxInit* init )
{
    XP_U16 nBytes = stream_getSize(init->stream);
    XP_U16 len, crlen;
    XP_UCHAR* sbuf;
    wchar_t* wbuf;
#ifdef MEM_DEBUG
    CEAppGlobals* globals = init->globals;
#endif

    sbuf = XP_MALLOC( globals->mpool, nBytes + 1 );
    stream_getBytes( init->stream, sbuf, nBytes );

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
    CEAppGlobals* globals = NULL;
    StrBoxInit* init;
    XP_U16 id;

    if ( message == WM_INITDIALOG ) {
        SetWindowLong( hDlg, GWL_USERDATA, (long)lParam );
        init = (StrBoxInit*)lParam;

        globals = init->globals;

        if  ( !!init->title ) {
            SendMessage( hDlg, WM_SETTEXT, 0, (long)init->title );
        }

        if ( !init->isQuery ) {
            ceShowOrHide( hDlg, IDCANCEL, XP_FALSE );
            /* also want to expand the text box to the bottom */
            if ( !ceIsLandscape( globals ) ) {
                ceCenterCtl( hDlg, IDOK );
            }
        }

        ceDlgSetup( globals, hDlg );

        handled = TRUE;
    } else {
        init = (StrBoxInit*)GetWindowLong( hDlg, GWL_USERDATA );

        if ( !!init ) {

            switch (message) {

            case WM_VSCROLL:
                ceDoDlgScroll( globals, hDlg, wParam );
                break;

            case WM_COMMAND:                

                /* If I add the text above in the WM_INITDIALOG section it
                   shows up selected though selStart and selEnd are 0. */
                if ( !init->textIsSet ) { 	 
                    init->textIsSet = XP_TRUE; 	 
                    stuffTextInField( hDlg, init ); 	 
                } 	 

                id = LOWORD(wParam);
                switch( id ) {

                case IDOK:
                case IDCANCEL:
                    init->result = id;
                    EndDialog(hDlg, id);
                    handled = TRUE;
                }
                break;
            }
        }
    }
    return handled;
} /* StrBox */
