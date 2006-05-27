/* -*- fill-column: 77; c-basic-offset: 4; compile-command: "make TARGET_OS=wince DEBUG=TRUE" -*- */
/* 
 * Copyright 2002-2004 by Eric House (xwords@eehouse.org).  All rights reserved.
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
#include "cedefines.h"

#define BUF_SIZE 128
#define VPADDING 4
#define HPADDING 2

void
ceSetDlgItemText( HWND hDlg, XP_U16 id, const XP_UCHAR* buf )
{
    wchar_t widebuf[BUF_SIZE];
    XP_U16 len;

    XP_ASSERT( buf != NULL );

    len = (XP_U16)XP_STRLEN( buf );

    if ( len >= BUF_SIZE ) {
        len = BUF_SIZE - 1;
    }

    MultiByteToWideChar( CP_ACP, MB_PRECOMPOSED, buf, len, widebuf, len );
    widebuf[len] = 0;
    SendDlgItemMessage( hDlg, id, WM_SETTEXT, 0, (long)widebuf );
} /* ceSetDlgItemText */

void
ceSetDlgItemFileName( HWND hDlg, XP_U16 id, XP_UCHAR* str )
{
    XP_UCHAR* stripstart;
    XP_UCHAR buf[BUF_SIZE];
    XP_U16 len = XP_STRLEN(str);

    if ( len >= BUF_SIZE ) {
        len = BUF_SIZE - 1;
    }

    XP_MEMCPY( buf, str, len );
    buf[len] = '\0';

    stripstart = strrchr( (const char*)buf, '.' );
    if ( !!stripstart ) {
        *stripstart = '\0';
    }

    ceSetDlgItemText( hDlg, id, buf );
} /* ceSetDlgItemFileName */

void
ceGetDlgItemText( HWND hDlg, XP_U16 id, XP_UCHAR* buf, XP_U16* bLen )
{
    XP_U16 len = *bLen;
    XP_U16 gotLen;
    wchar_t wbuf[BUF_SIZE];

    XP_ASSERT( len <= BUF_SIZE );

    gotLen = (XP_U16)SendDlgItemMessage( hDlg, id, WM_GETTEXT, len, (long)wbuf );
    if ( gotLen > 0 ) {
        XP_ASSERT( gotLen < len );
        if ( gotLen >= len ) {
            gotLen = len - 1;
        }
        gotLen = WideCharToMultiByte( CP_ACP, 0, wbuf, gotLen,
                                      buf, len, NULL, NULL );
        *bLen = gotLen;
        buf[gotLen] = '\0';
    } else {
        buf[0] = '\0';
        *bLen = 0;
    }
} /* ceGetDlgItemText */

void
ceSetDlgItemNum( HWND hDlg, XP_U16 id, XP_S32 num )
{
    XP_UCHAR buf[20];
    XP_SNPRINTF( buf, sizeof(buf), "%ld", num );
    ceSetDlgItemText( hDlg, id, buf );
} /* ceSetDlgItemNum */

XP_S32
ceGetDlgItemNum( HWND hDlg, XP_U16 id )
{
    XP_S32 result = 0;
    XP_UCHAR buf[24];
    XP_U16 len = sizeof(buf);
    ceGetDlgItemText( hDlg, id, buf, &len );

    result = atoi( buf );
    return result;
} /* ceGetDlgItemNum */

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
} /* ceShowOrHide */

void
ceEnOrDisable( HWND hDlg, XP_U16 resID, XP_Bool enable )
{
    HWND itemH = GetDlgItem( hDlg, resID );
    if ( !!itemH ) {
        EnableWindow( itemH, enable );
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

/* Return dlg-relative rect. 
 */
static void
GetItemRect( HWND hDlg, XP_U16 resID, RECT* rect )
{
    RECT dlgRect;
    HWND itemH = GetDlgItem( hDlg, resID );
    XP_U16 clientHt, winHt;

    GetClientRect( hDlg, &dlgRect );
    clientHt = dlgRect.bottom;
    GetWindowRect( hDlg, &dlgRect );
    winHt = dlgRect.bottom - dlgRect.top;
    GetWindowRect( itemH, rect );

    /* GetWindowRect includes the title bar, but functions like MoveWindow
       set relative to the client area below it.  So subtract out the
       difference between window ht and client rect ht -- the title bar --
       when returning the item's rect. */
    (void)OffsetRect( rect, -dlgRect.left, 
                      -(dlgRect.top + winHt - clientHt) );
} /* GetItemRect */

void
ceCenterCtl( HWND hDlg, XP_U16 resID )
{
    RECT buttonR, dlgR;
    HWND itemH = GetDlgItem( hDlg, resID );
    XP_U16 newX, buttonWidth;
    
    GetClientRect( hDlg, &dlgR );
    XP_ASSERT( dlgR.left == 0 && dlgR.top == 0 );

    GetItemRect( hDlg, resID, &buttonR );

    buttonWidth = buttonR.right - buttonR.left;
    newX = ( dlgR.right - buttonWidth ) / 2;

    if ( !MoveWindow( itemH, newX, buttonR.top,
                      buttonWidth, 
                      buttonR.bottom - buttonR.top, TRUE ) ) {
        XP_LOGF( "MoveWindow=>%d", GetLastError() );
    }
} /* ceCenterCtl */

XP_Bool
ceIsLandscape( CEAppGlobals* globals )
{
    XP_U16 width, height;
    XP_Bool landscape;

    XP_ASSERT( !!globals );
    XP_ASSERT( !!globals->hWnd );

    if ( 0 ) {
#if defined DEBUG && !defined _WIN32_WCE
    } else if ( globals->dbWidth != 0 ) {
        width = globals->dbWidth;
        height = globals->dbHeight;
#endif
    } else {
        RECT rect;
        GetClientRect( globals->hWnd, &rect );
        width = (XP_U16)(rect.right - rect.left);
        height = (XP_U16)(rect.bottom - rect.top);
    }

    landscape = (height - CE_SCORE_HEIGHT) 
        < (width - CE_MIN_SCORE_WIDTH);
    return landscape;
} /* ceIsLandscape */

/* Can't figure out how to do this on CE.  IsWindowVisible doesn't work, and
   GetWindowInfo isn't even there. */
static XP_Bool
ceIsVisible( HWND hwnd )
{
#ifdef _WIN32_WCE              /* GetWindowInfo isn't on CE */
    return XP_TRUE;
#else
    XP_Bool visible = XP_FALSE;
    WINDOWINFO wi;
    wi.cbSize = sizeof(wi);

    if ( !!hwnd && GetWindowInfo( hwnd, &wi ) ) {
        visible = (wi.dwStyle & WS_VISIBLE) != 0;
    }

    return visible;
#endif
} /* ceIsVisible */

void
ceStackButtonsRight( CEAppGlobals* globals, HWND hDlg )
{
    XP_ASSERT( !!globals );

    if ( ceIsLandscape( globals ) ) {
        XP_U16 resIDs[] = { IDOK, IDCANCEL };
        RECT wrect, crect;
        XP_U16 left, top;
        XP_U16 butWidth, butHeight;
        XP_U16 barHt, i, nButtons, spacing;
        XP_U16 newWidth;

        /* First, figure height and width to use */
        butHeight = 0;
        butWidth = 0;
        nButtons = 0;
        for ( i = 0; i < sizeof(resIDs)/sizeof(resIDs[0]); ++i ) {
            HWND itemH = GetDlgItem( hDlg, resIDs[i] );
            if ( ceIsVisible( itemH ) ) {
                RECT buttonRect;
                GetClientRect( itemH, &buttonRect );

                if ( butWidth < buttonRect.right ) {
                    butWidth = buttonRect.right;
                }
                if ( butHeight < buttonRect.bottom ) {
                    butHeight = buttonRect.bottom;
                }
                ++nButtons;
            }
        }

        /* Make sure we're not proposing to make the dialog wider than the
           screen */
        GetClientRect( hDlg, &crect );
        newWidth = crect.right - crect.left + butWidth + (HPADDING*2);
        GetWindowRect( globals->hWnd, &wrect );
        if ( newWidth <= wrect.right - wrect.left ) {

            GetWindowRect( hDlg, &wrect ); 
            barHt = wrect.bottom - wrect.top - crect.bottom;

            spacing = crect.bottom - (nButtons * (butHeight + (VPADDING*2)));
            spacing /= nButtons + 1;

            top = spacing - (butHeight / 2) + VPADDING;
            left = crect.right + HPADDING;

            for ( i = 0; i < sizeof(resIDs)/sizeof(resIDs[0]); ++i ) {
                HWND itemH = GetDlgItem( hDlg, resIDs[i] );
                if ( ceIsVisible( itemH ) ) { 
                    (void)MoveWindow( itemH, left, top, butWidth, butHeight, 
                                      TRUE );
                    top += butHeight + spacing + (VPADDING * 2);
                }
            }

            butWidth += HPADDING*2;
            MoveWindow( hDlg, wrect.left - (butWidth/2), wrect.top,
                        newWidth, wrect.bottom - wrect.top - butHeight - 2, 
                        FALSE );
        }
    }
} /* ceStackButtonsRight */

#ifdef DEBUG
void
XP_LOGW( const XP_UCHAR* prefix, const wchar_t* arg )
{
    XP_UCHAR buf[512];
    (void)WideCharToMultiByte( CP_ACP, 0, arg, -1,
                               buf, sizeof(buf), NULL, NULL );
    XP_LOGF( "%s: %s", prefix, buf );
}
#endif
