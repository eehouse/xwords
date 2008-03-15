/* -*- fill-column: 77; c-basic-offset: 4; compile-command: "make TARGET_OS=wince DEBUG=TRUE" -*- */
/* 
 * Copyright 2002-2008 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#include "stdafx.h" 
#include <commctrl.h>

#include "ceutil.h"
#include "cedefines.h"
#include "cedebug.h"

#define BUF_SIZE 128
#define VPADDING 4
#define HPADDING_L 2
#define HPADDING_R 3

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

    gotLen = (XP_U16)SendDlgItemMessage( hDlg, id, WM_GETTEXT, len, 
                                         (long)wbuf );
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
        XP_LOGF( "MoveWindow=>%ld", GetLastError() );
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
ceIsVisible( HWND XP_UNUSED_CE(hwnd) )
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

#ifdef _WIN32_WCE
static XP_Bool
ceIsFullScreen( CEAppGlobals* globals, HWND hWnd )
{
    XP_S16 screenHt;
    XP_U16 winHt;
    RECT rect;

    GetClientRect( hWnd, &rect );
    winHt = rect.bottom - rect.top; /* top should always be 0 */

    screenHt = GetSystemMetrics( SM_CYSCREEN );
    XP_ASSERT( screenHt >= winHt );

    screenHt -= winHt;
    
    if ( !!globals->hwndCB ) {
        RECT rect;
        GetWindowRect( globals->hwndCB, &rect );
        screenHt -= rect.bottom - rect.top;
    }

    XP_ASSERT( screenHt >= 0 );
    return screenHt == 0;
} /* ceIsFullScreen */

void
ceSizeIfFullscreen( CEAppGlobals* globals, HWND hWnd )
{
    if ( globals->appPrefs.fullScreen != ceIsFullScreen(globals, hWnd) ) {
        RECT rect;
        XP_U16 cbHeight = 0;
        if ( !!globals->hwndCB && hWnd == globals->hWnd ) {
            GetWindowRect( globals->hwndCB, &rect );
            cbHeight = rect.bottom - rect.top;
        }

        /* I'm leaving the SIP/cmdbar in place until I can figure out how to
           get menu events with it hidden -- and also the UI for making sure
           users don't get stuck in fullscreen mode not knowing how to reach
           menus to get out.  Later, add SHFS_SHOWSIPBUTTON and
           SHFS_HIDESIPBUTTON to the sets shown and hidden below.*/
        if ( globals->appPrefs.fullScreen ) {
            SHFullScreen( hWnd, SHFS_HIDETASKBAR | SHFS_HIDESTARTICON );

            SetRect( &rect, 0, 0, GetSystemMetrics(SM_CXSCREEN),
                     GetSystemMetrics(SM_CYSCREEN) );

        } else {
            SHFullScreen( hWnd, SHFS_SHOWTASKBAR | SHFS_SHOWSTARTICON );
            SystemParametersInfo( SPI_GETWORKAREA, 0, &rect, FALSE );
            if ( IS_SMARTPHONE(globals) ) {
                cbHeight = 0;
            }
        }

        rect.bottom -= cbHeight;
        MoveWindow( hWnd, rect.left, rect.top, rect.right - rect.left, 
                    rect.bottom - rect.top, TRUE );
    }
} /* ceSizeIfFullscreen */

static XP_Bool
mkFullscreenWithSoftkeys( CEAppGlobals* globals, HWND hDlg )
{
    XP_Bool success = XP_FALSE;
    SHMENUBARINFO mbi;
    XP_Bool fullScreen = XP_TRUE; /* probably want this TRUE for
                                      small-screened smartphones only. */
    if ( fullScreen ) {
        ceSizeIfFullscreen( globals, hDlg );
    }

    XP_MEMSET( &mbi, 0, sizeof(mbi) );
    mbi.cbSize = sizeof(mbi);
    mbi.hwndParent = hDlg;
    mbi.nToolBarId = IDM_OKCANCEL_MENUBAR;
    mbi.hInstRes = globals->hInst;
    success = SHCreateMenuBar( &mbi );
    if ( !success ) {
        XP_LOGF( "SHCreateMenuBar failed: %ld", GetLastError() );
    }
    return success;
} /* mkFullscreenWithSoftkeys */
#endif

void
ceStackButtonsRight( CEAppGlobals* globals, HWND hDlg )
{
    XP_Bool justRemove = XP_FALSE;
    XP_ASSERT( !!globals );

#ifdef _WIN32_WCE
    if ( mkFullscreenWithSoftkeys( globals, hDlg ) ) {
        justRemove = XP_TRUE;
    }
#endif

    if ( justRemove || ceIsLandscape( globals ) ) {
        XP_U16 resIDs[] = { IDOK, IDCANCEL };
        RECT wrect, crect;
        XP_U16 left, top;
        XP_U16 butWidth, butHeight;
        XP_U16 barHt, i, nButtons, spacing;
        XP_U16 newWidth, mainWidth;

        /* First, figure height and width to use */
        butHeight = 0;
        butWidth = 0;
        nButtons = 0;
        for ( i = 0; i < VSIZE(resIDs); ++i ) {
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

        GetWindowRect( globals->hWnd, &wrect );
        mainWidth = wrect.right - wrect.left;

        /* Make sure we're not proposing to make the dialog wider than the
           screen */
        GetWindowRect( hDlg, &wrect );
        newWidth = wrect.right - wrect.left +
            butWidth + HPADDING_L + HPADDING_R;

        if ( justRemove || (newWidth <= mainWidth) ) {

            GetClientRect( hDlg, &crect ); 
            barHt = wrect.bottom - wrect.top - crect.bottom;

            spacing = crect.bottom - (nButtons * (butHeight + (VPADDING*2)));
            spacing /= nButtons + 1;

            top = spacing - (butHeight / 2) + VPADDING;
            left = crect.right + HPADDING_L;

            for ( i = 0; i < VSIZE(resIDs); ++i ) {
                HWND itemH = GetDlgItem( hDlg, resIDs[i] );
                if ( ceIsVisible( itemH ) ) { 
                    (void)MoveWindow( itemH, left, top, butWidth, butHeight, 
                                      TRUE );
                    top += butHeight + spacing + (VPADDING * 2);
                }
            }

            if ( justRemove ) {
                MoveWindow( hDlg, wrect.left, wrect.top,
                            wrect.right - wrect.left, 
                            wrect.bottom - wrect.top - butHeight - 2, 
                            FALSE );
            } else {
                butWidth += HPADDING_L + HPADDING_R;
                MoveWindow( hDlg, wrect.left - (butWidth/2), wrect.top,
                            newWidth, wrect.bottom - wrect.top - butHeight - 2, 
                            FALSE );
            }
        }
    }
} /* ceStackButtonsRight */

static XP_Bool
ceFindMenu( HMENU menu, XP_U16 id, HMENU* foundMenu, XP_U16* foundPos,
            wchar_t* foundBuf, XP_U16 bufLen )
{
    XP_Bool found = XP_FALSE;
    XP_U16 pos;
    MENUITEMINFO minfo;

    XP_MEMSET( &minfo, 0, sizeof(minfo) );
    minfo.cbSize = sizeof(minfo);

    for ( pos = 0; !found; ++pos ) {
        /* Set these each time through loop.  GetMenuItemInfo can change
           some of 'em. */
        minfo.fMask = MIIM_SUBMENU | MFT_STRING | MIIM_ID | MIIM_TYPE;
        minfo.dwTypeData = foundBuf;
        minfo.fType = MFT_STRING;
        minfo.cch = bufLen;

        if ( !GetMenuItemInfo( menu, pos, TRUE, &minfo ) ) {
            break;              /* pos is too big */
        } else if ( NULL != minfo.hSubMenu ) {
            found = ceFindMenu( minfo.hSubMenu, id, foundMenu, foundPos,
                                foundBuf, bufLen );
        } else if ( MFT_SEPARATOR == minfo.fType ) {
            continue;
        } else if ( minfo.wID == id ) {
            found = XP_TRUE;
            *foundPos = pos;
            *foundMenu = menu;
        }
    }
    return found;
} /* ceFindMenu */

#ifndef _WIN32_WCE
static void
setW32DummyMenu( CEAppGlobals* globals, HMENU menu, XP_U16 id, wchar_t* oldNm )
{
    XP_LOGW( __func__, oldNm );
    if ( globals->dummyMenu == NULL ) {
        HMENU tmenu;
        XP_U16 tpos;
        wchar_t ignore[32];
        if ( ceFindMenu( menu, W32_DUMMY_ID, &tmenu, &tpos, ignore, 
                         VSIZE(ignore) ) ) {
            globals->dummyMenu = tmenu;
            globals->dummyPos = tpos;
        }
    }

    if ( globals->dummyMenu != NULL ) {
        MENUITEMINFO minfo;
        XP_MEMSET( &minfo, 0, sizeof(minfo) );
        minfo.cbSize = sizeof(minfo);
        minfo.fMask = MFT_STRING | MIIM_TYPE | MIIM_ID;
        minfo.fType = MFT_STRING;
        minfo.dwTypeData = oldNm;
        minfo.cch = wcslen( oldNm );
        minfo.wID = id;

        if ( !SetMenuItemInfo( globals->dummyMenu, globals->dummyPos, 
                               TRUE, &minfo ) ) {
            XP_LOGF( "SetMenuItemInfo failed" );
        }
    }
}
#endif

void
ceSetLeftSoftkey( CEAppGlobals* globals, XP_U16 newId )
{
    if ( newId != globals->softkey.oldId ) {
        HMENU menu;
        HMENU prevMenu;
        XP_U16 prevPos;
        XP_U16 oldId = globals->softkey.oldId;
        if ( 0 == oldId ) {
            oldId = ID_INITIAL_SOFTID;
        }

#ifdef _WIN32_WCE
        TBBUTTONINFO info;
        XP_MEMSET( &info, 0, sizeof(info) );
        info.cbSize = sizeof(info);
#endif

#ifdef _WIN32_WCE
        info.dwMask = TBIF_LPARAM;
        SendMessage( globals->hwndCB, TB_GETBUTTONINFO, IDM_MENU, 
                     (LPARAM)&info );
        menu = (HMENU)info.lParam;  /* Use to remove item being installed in
                                       left button */
#else
        menu = GetMenu( globals->hWnd );
#endif

        /* First put any existing menu item back in the main menu! */
        if ( globals->softkey.oldMenu != 0 ) {
            if ( ! InsertMenu( globals->softkey.oldMenu, 
                               globals->softkey.oldPos, MF_BYPOSITION, 
                               globals->softkey.oldId,
                               globals->softkey.oldName ) ) {
                XP_LOGF( "%s: InsertMenu failed", __func__ );
            }
        }

        /* Then find, remember and remove the new */
        if ( ceFindMenu( menu, newId, &prevMenu, &prevPos,
                         globals->softkey.oldName,
                         VSIZE(globals->softkey.oldName) ) ) {
            if ( !DeleteMenu( prevMenu, prevPos, MF_BYPOSITION ) ) {
                XP_LOGF( "%s: DeleteMenu failed", __func__ );
            }
            globals->softkey.oldMenu = prevMenu;
            globals->softkey.oldPos = prevPos;
            globals->softkey.oldId = newId;
        } else {
            XP_LOGF( "%s: ceFindMenu failed", __func__ );
        }

        /* Make it the button */
#ifdef _WIN32_WCE
        info.dwMask = TBIF_TEXT | TBIF_COMMAND;
        info.idCommand = newId;
        info.pszText = globals->softkey.oldName;
        SendMessage( globals->hwndCB, TB_SETBUTTONINFO, oldId, (LPARAM)&info );
#else
        setW32DummyMenu( globals, menu, newId, globals->softkey.oldName );
#endif
    }
} /* ceSetLeftSoftkey */
