/* -*-mode: C; fill-column: 77; c-basic-offset: 4; -*- */
/* 
 * Copyright 2002-2004 by Eric House (fixin@peak.org).  All rights reserved.
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
stuffTextInField( HWND hDlg, CEAppGlobals* globals, XWStreamCtxt* stream )
{
    XP_U16 nBytes = stream_getSize(stream);
    XP_U16 len;
    XP_UCHAR* sbuf;
    wchar_t* wbuf;

    sbuf = XP_MALLOC( globals->mpool, nBytes );
    stream_getBytes( stream, sbuf, nBytes );

    len = MultiByteToWideChar( CP_ACP, MB_PRECOMPOSED, sbuf, nBytes,
                               NULL, 0 );
    wbuf = XP_MALLOC( globals->mpool, (len+1) * sizeof(*wbuf) );
    XP_MEMSET( wbuf, 0, (len+1) * sizeof(*wbuf) );
    MultiByteToWideChar( CP_ACP, MB_PRECOMPOSED, sbuf, nBytes,
                         wbuf, len );
    XP_FREE( globals->mpool, sbuf );

    SetDlgItemText( hDlg, ID_EDITTEXT, wbuf );
    XP_FREE( globals->mpool, wbuf );

    /* This isn't working to stop the highlighting of text */
    SendDlgItemMessage( hDlg, ID_EDITTEXT, EM_SETSEL, -1, 0L );
    XP_LOGF( "called SendDlgItemMessage with -1" );
} /* stuffTextInField */

LRESULT CALLBACK
StrBox(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
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
/*             ceCenterCtl( hDlg, IDOK ); */
        }

        stuffTextInField( hDlg, globals, init->stream );
	
        return TRUE;
    } else {
        init = (StrBoxInit*)GetWindowLong( hDlg, GWL_USERDATA );

        switch (message) {

        case WM_COMMAND:
            id = LOWORD(wParam);
            switch( id ) {

            case IDOK:
            case IDCANCEL:
                init->result = id;
                EndDialog(hDlg, id);
                return TRUE;
            }
            break;
        }
    }
    return FALSE;
} /* StrBox */
