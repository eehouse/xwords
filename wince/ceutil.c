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

#include "ceutil.h"

#define BUF_SIZE 128

void
ceSetDlgItemText( HWND hDlg, XP_U16 id, XP_UCHAR* buf )
{
    wchar_t widebuf[BUF_SIZE];
    XP_U16 len;

    len = (XP_U16)XP_STRLEN( buf );
    ++len;
    if ( len <= BUF_SIZE ) {
        MultiByteToWideChar( CP_ACP, MB_PRECOMPOSED, buf, len,
                             widebuf, len );
        SendDlgItemMessage( hDlg, id, WM_SETTEXT, 0, (long)widebuf );
    }
} /* ceSetDlgItemText */

void
ceGetDlgItemText( HWND hDlg, XP_U16 id, XP_UCHAR* buf, XP_U16* bLen )
{
    XP_U16 len = *bLen;
    XP_U16 gotLen;
    wchar_t wbuf[BUF_SIZE];

    XP_ASSERT( len <= BUF_SIZE );

    gotLen = (XP_U16)SendDlgItemMessage( hDlg, id, WM_GETTEXT, len, (long)wbuf );
    if ( gotLen > 0 ) {
        gotLen = WideCharToMultiByte( CP_ACP, 0, wbuf, gotLen,
                                      buf, len, NULL, NULL );
        XP_ASSERT( gotLen < len );
        *bLen = gotLen;
        buf[gotLen] = '\0';
    } else {
        buf[0] = '\0';
        *bLen = 0;
    }
} /* ceGetDlgItemText */

/* This is stolen from some sample code somewhere
 */
void
positionDlg( HWND hDlg )
{
    RECT rt, rt1;
    int DlgWidth, DlgHeight;	// dialog width and height in pixel units
    int NewPosX, NewPosY;
    
    if ( GetWindowRect( hDlg, &rt1 ) ) {
        GetClientRect(GetParent(hDlg), &rt);
        DlgWidth	= rt1.right - rt1.left;
        DlgHeight	= rt1.bottom - rt1.top ;
        NewPosX		= (rt.right - rt.left - DlgWidth)/2;
        NewPosY		= rt.bottom - DlgHeight;
				
        // if dlg is larger than the physical screen 
        if (NewPosX < 0) {
            NewPosX = 0;
        }
        if (NewPosY < 0) {
            NewPosY = 0;
        }
        SetWindowPos(hDlg, 0, NewPosX, NewPosY,
                     0, 0, SWP_NOZORDER | SWP_NOSIZE);
    }

} /* positionDlg */

void
ce_selectAndShow( HWND hDlg, XP_U16 resID, XP_U16 index )
{
    SendDlgItemMessage( hDlg, resID, LB_SETCURSEL, index, 0 );
    SendDlgItemMessage( hDlg, resID, LB_SETANCHORINDEX, index, 0 );
} /* ce_selectAndShow */

void
ceShowOrHide( HWND hDlg, XP_U16 resID, XP_Bool visible )
{
    HWND itemH = GetDlgItem( hDlg, resID );
    if ( !!itemH ) {
        ShowWindow( itemH, visible? SW_SHOW: SW_HIDE );
    }
    /*     SendDlgItemMessage( hDlg, resID, WM_ENABLE, visible, visible ); */
    /*     SendDlgItemMessage( hDlg, resID, WM_ACTIVATE, visible, visible ); */
} /* ceShowOrHide */

void
ceEnOrDisable( HWND hDlg, XP_U16 resID, XP_Bool enable )
{
    HWND itemH = GetDlgItem( hDlg, resID );
    if ( !!itemH ) {
        EnableWindow( itemH, enable );
/*         SendDlgItemMessage( hDlg, resID, WM_ACTIVATE, enable, 0L ); */
    }
} /* ceShowOrHide */

void
ceSetChecked( HWND hDlg, XP_U16 resID, XP_Bool check )
{
    SendDlgItemMessage( hDlg, resID, BM_SETCHECK, 
                        check? BST_CHECKED:BST_UNCHECKED, 0L );
} /* ceSetBoolCheck */

XP_Bool
ceGetChecked( HWND hDlg, XP_U16 resID )
{
    XP_U16 checked;
    checked = (XP_U16)SendDlgItemMessage( hDlg, resID, BM_GETCHECK, 0, 0L );
    return checked == BST_CHECKED;
} /* ceGetChecked */
