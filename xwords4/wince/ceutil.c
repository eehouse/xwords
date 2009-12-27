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

#include "stdafx.h" 
#include <commctrl.h>
#include <shlobj.h>

#include "ceutil.h"
#include "cedefines.h"
#include "cedebug.h"
#include "debhacks.h"
#include "ceresstr.h"

#define BUF_SIZE 128
#define VPADDING 4
#define HPADDING_L 2
#define HPADDING_R 3

static XP_Bool ceDoDlgScroll( CeDlgHdr* dlgHdr, WPARAM wParam );
static void ceDoDlgFocusScroll( CeDlgHdr* dlgHdr, HWND nextCtrl );
static void adjustScrollPos( HWND hDlg, XP_S16 vertChange );

void
ceSetDlgItemText( HWND hDlg, XP_U16 id, const XP_UCHAR* str )
{
    wchar_t widebuf[BUF_SIZE];
    XP_U16 len, wlen;

    XP_ASSERT( str != NULL );

    len = (XP_U16)XP_STRLEN( str );

    if ( len >= VSIZE(widebuf) ) {
        len = VSIZE(widebuf) - 1;
    }

    wlen = MultiByteToWideChar( CP_UTF8, 0, str, len, widebuf, len );
    widebuf[wlen] = 0;
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
void
ceGetItemRect( HWND hDlg, XP_U16 resID, RECT* rect )
{
    HWND itemH;
    POINT pt = { .x = 0, .y = 0 };
    ScreenToClient( hDlg, &pt );

    itemH = GetDlgItem( hDlg, resID );
    GetWindowRect( itemH, rect );
    (void)OffsetRect( rect, pt.x, pt.y );
} /* ceGetItemRect */

void
ceMoveItem( HWND hDlg, XP_U16 resID, XP_S16 byX, XP_S16 byY )
{
    RECT rect;
    HWND itemH = GetDlgItem( hDlg, resID );
    ceGetItemRect( hDlg, resID, &rect );
    if ( !MoveWindow( itemH, rect.left + byX, rect.top + byY,
                      rect.right - rect.left, rect.bottom - rect.top,
                      TRUE ) ) {
        XP_LOGF( "MoveWindow=>%ld", GetLastError() );
    }
} /* ceMoveItem */

XP_U16
ceDistanceBetween( HWND hDlg, XP_U16 resID1, XP_U16 resID2 )
{
    RECT rect;
    ceGetItemRect( hDlg, resID1, &rect );
    XP_U16 top = rect.top;
    ceGetItemRect( hDlg, resID2, &rect );
    XP_ASSERT( rect.top > top );
    return rect.top - top;
}

#if 0
/* This has not been tested with ceMoveItem... */
void
ceCenterCtl( HWND hDlg, XP_U16 resID )
{
    RECT buttonR, dlgR;
    XP_U16 buttonWidth;
    XP_S16 byX;
    
    GetClientRect( hDlg, &dlgR );
    XP_ASSERT( dlgR.left == 0 && dlgR.top == 0 );

    ceGetItemRect( hDlg, resID, &buttonR );

    buttonWidth = buttonR.right - buttonR.left;
    byX = buttonR.left - ((dlgR.right - buttonWidth) / 2);

    ceMoveItem( hDlg, resID, byX, 0 );
} /* ceCenterCtl */
#endif

/* XP_Bool */
/* ceIsLandscape( CEAppGlobals* globals ) */
/* { */
/*     XP_U16 width, height; */
/*     XP_Bool landscape; */

/*     XP_ASSERT( !!globals ); */
/*     XP_ASSERT( !!globals->hWnd ); */

/*     if ( 0 ) { */
/* #if defined DEBUG && !defined _WIN32_WCE */
/*     } else if ( globals->dbWidth != 0 ) { */
/*         width = globals->dbWidth; */
/*         height = globals->dbHeight; */
/* #endif */
/*     } else { */
/*         RECT rect; */
/*         GetClientRect( globals->hWnd, &rect ); */
/*         width = (XP_U16)(rect.right - rect.left); */
/*         height = (XP_U16)(rect.bottom - rect.top); */
/*     } */

/*     landscape = (height - CE_SCORE_HEIGHT)  */
/*         < (width - CE_MIN_SCORE_WIDTH); */
/*     return landscape; */
/* } /\* ceIsLandscape *\/ */

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

static void
ceSize( CEAppGlobals* globals, HWND hWnd, XP_Bool fullScreen )
{
    RECT rect;
    XP_U16 cbHeight = 0;
    if ( !!globals->hwndCB ) {
        GetWindowRect( globals->hwndCB, &rect );
        cbHeight = rect.bottom - rect.top;
    }

    /* I'm leaving the SIP/cmdbar in place until I can figure out how to
       get menu events with it hidden -- and also the UI for making sure
       users don't get stuck in fullscreen mode not knowing how to reach
       menus to get out.  Later, add SHFS_SHOWSIPBUTTON and
       SHFS_HIDESIPBUTTON to the sets shown and hidden below.*/
    if ( fullScreen ) {
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
} /* ceSize */

XP_Bool
ceSizeIfFullscreen( CEAppGlobals* globals, HWND hWnd )
{
    XP_Bool resized = globals->appPrefs.fullScreen
        != ceIsFullScreen(globals, hWnd);
    if ( resized ) {
        ceSize( globals, hWnd, globals->appPrefs.fullScreen );
    }
    return resized;
}

static XP_Bool
mkFullscreenWithSoftkeys( CEAppGlobals* globals, HWND hDlg, XP_U16 curHt,
                          DlgStateTask doWhat )
{
    XP_Bool success = XP_FALSE;

    SHMENUBARINFO mbi;
    XP_MEMSET( &mbi, 0, sizeof(mbi) );
    mbi.cbSize = sizeof(mbi);
    mbi.hwndParent = hDlg;
    if ( 0 != (doWhat & DLG_STATE_DONEONLY) ) {
        mbi.nToolBarId = IDM_DONE_MENUBAR;
    } else if ( 0 != (doWhat & DLG_STATE_OKONLY) ) {
        mbi.nToolBarId = IDM_OK_MENUBAR;
    } else {
        mbi.nToolBarId = IDM_OKCANCEL_MENUBAR;
    }
    mbi.hInstRes = globals->locInst;
    success = SHCreateMenuBar( &mbi );
    if ( !success ) {
        XP_LOGF( "SHCreateMenuBar failed: %ld", GetLastError() );
    } else {

        if ( IS_SMARTPHONE(globals) ) {
            SHINITDLGINFO info;
            XP_MEMSET( &info, 0, sizeof(info) );
            info.dwMask = SHIDIM_FLAGS;
            info.dwFlags = SHIDIF_SIZEDLGFULLSCREEN;
            info.hDlg = hDlg;
            success = SHInitDialog( &info );
            if ( !success ) {
                XP_LOGF( "SHInitDialog failed: %ld", GetLastError() );
            }
        } else {
            XP_U16 screenHt = GetSystemMetrics(SM_CYFULLSCREEN);
            RECT rect;
            GetWindowRect( mbi.hwndMB, &rect );
            screenHt -= (rect.bottom - rect.top);
            if ( screenHt < curHt ) {
                ceSize( globals, hDlg, XP_TRUE );
            }
        }
    }

    return success;
} /* mkFullscreenWithSoftkeys */
#endif

static void
ceCenterBy( HWND hDlg, XP_U16 byHowMuch )
{
    HWND child;

    for ( child = GetWindow( hDlg, GW_CHILD );
          !!child;
          child = GetWindow( child, GW_HWNDNEXT ) ) {
        XP_S16 resID = GetDlgCtrlID( child );
        ceMoveItem( hDlg, resID, byHowMuch, 0 );
    }
} /* ceCenterBy */

#define MAX_DLG_ROWS 20
typedef struct _DlgRow {
    XP_U16 top, bottom;
    XP_U16 nIds;
    XP_U16 ids[12];
} DlgRow;

typedef struct _DlgRows {
    DlgRow rows[MAX_DLG_ROWS];
    XP_U16 nRows;
} DlgRows;

static void
insertId( HWND hDlg, DlgRows* rows, XP_U16 id )
{
    RECT rect;
    XP_U16 ii;
    DlgRow* row;
    ceGetItemRect( hDlg, id, &rect );

    // Find an entry it fits in, or add a new one.

    for ( ii = 0; ii < MAX_DLG_ROWS; ++ii ) {
        row = &rows->rows[ii];
        if ( row->bottom == 0 ) {
            row->top = rect.top;
            row->bottom = rect.bottom;
            ++rows->nRows;
            break;
        } else if ( rect.top >= row->bottom ) {
            /* continue */
        } else if ( rect.bottom <= row->top ) {
            /* continue */
        } else {
            if ( row->top < rect.top ) {
                row->top = rect.top;
            }
            if ( row->bottom > rect.bottom ) {
                row->bottom = rect.bottom;
            }
            break;
        }
    }

    XP_ASSERT( ii < VSIZE(rows->rows) );

    row->ids[row->nIds++] = id;
    XP_ASSERT( row->nIds < VSIZE(row->ids) );
}

static void
buildIdList( CeDlgHdr* dlgHdr )
{
    HWND child;
    XP_U16 ii, jj;
    DlgRows rows;
    XP_U16 nResIDsUsed;

    XP_MEMSET( &rows, 0, sizeof(rows) );

    for ( child = GetWindow( dlgHdr->hDlg, GW_CHILD ), ii = 0;
          !!child;
          child = GetWindow( child, GW_HWNDNEXT ) ) {
        XP_S16 resID = GetDlgCtrlID( child );
        if ( resID > 0 ) {
            insertId( dlgHdr->hDlg, &rows, resID );
        }
    }

    /* might need to sort first */
    nResIDsUsed = 0;
    for ( ii = 0; ii < rows.nRows; ++ii ) {
        DlgRow* row = &rows.rows[ii];
        for ( jj = 0; jj < row->nIds; ++jj ) {
            XP_U16 id = row->ids[jj];
            dlgHdr->resIDs[nResIDsUsed++] = id;
            XP_ASSERT( nResIDsUsed < dlgHdr->nResIDs );
        }

        dlgHdr->resIDs[nResIDsUsed++] = 0;
        XP_ASSERT( nResIDsUsed <= dlgHdr->nResIDs );
    }
    dlgHdr->nResIDsUsed = nResIDsUsed;
}

#define TITLE_HT 20            /* Need to get this from the OS */
void
ceDlgSetup( CeDlgHdr* dlgHdr, HWND hDlg, DlgStateTask doWhat )
{
    RECT rect;
    XP_U16 fullHeight, origWidth;
    CEAppGlobals* globals = dlgHdr->globals;

    dlgHdr->hDlg = hDlg;

    XP_ASSERT( !!globals );
    XP_ASSERT( !!hDlg );
    /* at most one of these two should be set */
    XP_ASSERT( (doWhat & (DLG_STATE_OKONLY|DLG_STATE_DONEONLY)) 
               != (DLG_STATE_OKONLY|DLG_STATE_DONEONLY));

    GetClientRect( hDlg, &rect );
    XP_ASSERT( rect.top == 0 );
    fullHeight = rect.bottom;         /* This is before we've resized it */
    origWidth = rect.right;

#ifdef _WIN32_WCE
    (void)mkFullscreenWithSoftkeys( globals, hDlg, fullHeight, doWhat);
#elif defined DEBUG
    /* Force it to be small so we can test scrolling etc. */
    if ( globals->dbWidth > 0 && globals->dbHeight > 0) {
        MoveWindow( hDlg, 0, 0, globals->dbWidth, globals->dbHeight, TRUE );
        rect.bottom = globals->dbHeight;
    }
#endif

    /* Measure again post-resize */
    GetClientRect( hDlg, &rect );
    if ( rect.right > origWidth ) {
        ceCenterBy( hDlg, (rect.right - origWidth) / 2 );
    }

    /* Set up the scrollbar if we're on PPC */
    if ( !IS_SMARTPHONE(globals) ) {
        SCROLLINFO sinfo;
     
        XP_MEMSET( &sinfo, 0, sizeof(sinfo) );
        sinfo.cbSize = sizeof(sinfo);

        sinfo.fMask = SIF_RANGE | SIF_POS | SIF_PAGE;
        if ( rect.bottom < fullHeight ) {
            sinfo.nMax = fullHeight;
            dlgHdr->nPage = sinfo.nPage = rect.bottom - 1;
        }
        
        (void)SetScrollInfo( hDlg, SB_VERT, &sinfo, FALSE );
    }

    if ( !!dlgHdr->resIDs ) {
        buildIdList( dlgHdr );
    }

    dlgHdr->doWhat = doWhat;

#ifdef _WIN32_WCE
    /* Need to trap this for all dialogs, even if they don't have edit
       controls.  The need goes away if the main window stops trapping it,
       but I don't understand why: trapping here is still required. */
    if ( IS_SMARTPHONE(globals) ) {
        trapBackspaceKey( hDlg );
    }
#endif
} /* ceDlgSetup */

void
ceDlgComboShowHide( CeDlgHdr* dlgHdr, XP_U16 baseId )
{
    HWND hDlg = dlgHdr->hDlg;

    if ( IS_SMARTPHONE(dlgHdr->globals) ) {
        ceShowOrHide( hDlg, baseId+2, XP_FALSE );
    } else {
        ceShowOrHide( hDlg, baseId, XP_FALSE );
        ceShowOrHide( hDlg, baseId+1, XP_FALSE );
    } 
}

#ifdef OVERRIDE_BACKKEY
static XP_Bool
editHasFocus( void )
{
    HWND focus = GetFocus();
    wchar_t buf[32];
    XP_Bool isEdit = !!focus
        && ( 0 != GetClassName( focus, buf, VSIZE(buf) ) )
        && !wcscmp( L"Edit", buf );
    return isEdit;
} /* editHasFocus */
#endif


static void
scrollForMove( CeDlgHdr* dlgHdr, XP_U16 newY ) 
{
    if ( dlgHdr->penDown ) {
        XP_S16 vertChange = dlgHdr->prevY - newY;
        dlgHdr->prevY = newY;
        adjustScrollPos( dlgHdr->hDlg, vertChange );
    }
}

XP_Bool
ceDoDlgHandle( CeDlgHdr* dlgHdr, UINT message, WPARAM wParam, LPARAM lParam )
{
    XP_Bool handled = XP_FALSE;
    XP_U16 hiword = HIWORD(lParam);
    XP_U16 hwwp;

    switch( message ) {
#ifdef OVERRIDE_BACKKEY
    case WM_HOTKEY:
        if ( VK_TBACK == hiword ) {
            if ( editHasFocus() ) {
                SHSendBackToFocusWindow( message, wParam, lParam );
            } else if ( 0 != (BACK_KEY_UP_MAYBE & LOWORD(lParam) ) ) {
                WPARAM cmd = (0 != (dlgHdr->doWhat
                                    & (DLG_STATE_DONEONLY|DLG_STATE_OKONLY))) ?
                    IDOK : IDCANCEL;
                SendMessage( dlgHdr->hDlg, WM_COMMAND, cmd, 0L );
            }
            handled = TRUE;
        }
        break;
#endif
    case WM_VSCROLL:
        handled = ceDoDlgScroll( dlgHdr, wParam );
        break;

    case WM_LBUTTONDOWN:
        dlgHdr->penDown = XP_TRUE;
        dlgHdr->prevY = hiword;
        handled = XP_TRUE;
        break;
    case WM_MOUSEMOVE:
        scrollForMove( dlgHdr, hiword );
        handled = XP_TRUE;
        break;
    case WM_LBUTTONUP:
        dlgHdr->penDown = XP_FALSE;
        handled = XP_TRUE;
        break;

    case WM_COMMAND: 
        hwwp = HIWORD(wParam);
        if ( BN_SETFOCUS == hwwp || EN_SETFOCUS == hwwp ) {
            ceDoDlgFocusScroll( dlgHdr, (HWND)lParam );
            /* don't set handled: dialog must handle EN_SETFOCUS! */
        } else if ( BN_KILLFOCUS == hwwp/* || EN_KILLFOCUS == hiword*/ ) {
            /* dialogs shouldn't have to handle these */
            handled = TRUE;
        }
        break;
    }
    return handled;
} /* ceDoDlgHandle */

void
ceDlgMoveBelow( CeDlgHdr* dlgHdr, XP_U16 resID, XP_S16 distance )
{
    XP_U16 ii;
    XP_ASSERT( !!dlgHdr->resIDs );
    XP_U16 nResIDsUsed = dlgHdr->nResIDsUsed;

    for ( ii = 0; ii < nResIDsUsed; ++ii ) {
        if ( dlgHdr->resIDs[ii] == resID ) {
            break;
        }
    }

    if ( ii < nResIDsUsed ) { /* found it? */
        while ( dlgHdr->resIDs[ii] != 0 ) { /* skip to end of row  */
            ++ii;
            XP_ASSERT( ii < nResIDsUsed ); /* found it? */
        }
        ++ii;                                  /* skip past the 0 */

        for ( ; ii < nResIDsUsed; ++ii ) {
            XP_U16 id = dlgHdr->resIDs[ii];
            if ( 0 != id ) {
                ceMoveItem( dlgHdr->hDlg, id, 0, distance );
            }
        }

#ifdef _WIN32_WCE
        if ( IS_SMARTPHONE(dlgHdr->globals) ) {
            SendMessage( dlgHdr->hDlg, DM_RESETSCROLL, 
                         (WPARAM)FALSE, (LPARAM)TRUE );
        }
#endif
    } else {
        XP_LOGF( "%s: resID %d not found", __func__, resID );
    }

}

static void
setScrollPos( HWND hDlg, XP_S16 newPos )
{
    SCROLLINFO sinfo;
    XP_S16 vertChange;

    XP_MEMSET( &sinfo, 0, sizeof(sinfo) );
    sinfo.cbSize = sizeof(sinfo);
    sinfo.fMask = SIF_POS;
    GetScrollInfo( hDlg, SB_VERT, &sinfo );

    if ( sinfo.nPos != newPos ) {
        XP_U16 oldPos = sinfo.nPos;
        sinfo.nPos = newPos;
        SetScrollInfo( hDlg, SB_VERT, &sinfo, XP_TRUE );

        GetScrollInfo( hDlg, SB_VERT, &sinfo );
        vertChange = oldPos - sinfo.nPos;
        if ( 0 != vertChange ) {
            RECT updateR;
            ScrollWindowEx( hDlg, 0, vertChange, NULL, NULL, NULL,
                            &updateR, SW_SCROLLCHILDREN|SW_ERASE);
            InvalidateRect( hDlg, &updateR, TRUE );
            (void)UpdateWindow( hDlg );
        }
    }
} /* setScrollPos */

static void
adjustScrollPos( HWND hDlg, XP_S16 vertChange )
{
    if ( vertChange != 0 ) {
        SCROLLINFO sinfo;

        XP_MEMSET( &sinfo, 0, sizeof(sinfo) );
        sinfo.cbSize = sizeof(sinfo);
        sinfo.fMask = SIF_POS;
        GetScrollInfo( hDlg, SB_VERT, &sinfo );

        setScrollPos( hDlg, sinfo.nPos + vertChange );
    }
} /* adjustScrollPos */

static XP_Bool
ceDoDlgScroll( CeDlgHdr* dlgHdr, WPARAM wParam )
{
    XP_Bool handled = !IS_SMARTPHONE(dlgHdr->globals);
    if ( handled ) {
        XP_S16 vertChange = 0;
    
        switch ( LOWORD(wParam) ) {

        case SB_LINEUP: // Scrolls one line up 
            vertChange = -1;
            break;
        case SB_PAGEUP: // 
            vertChange = -dlgHdr->nPage;
            break;

        case SB_LINEDOWN: // Scrolls one line down 
            vertChange = 1;
            break;
        case SB_PAGEDOWN: // Scrolls one page down 
            vertChange = dlgHdr->nPage;
            break;

        case SB_THUMBTRACK:     /* still dragging; don't redraw */
        case SB_THUMBPOSITION:
            setScrollPos( dlgHdr->hDlg, HIWORD(wParam) );
            break;
        }

        if ( 0 != vertChange ) {
            adjustScrollPos( dlgHdr->hDlg, vertChange );
        }
    }
    return handled;
} /* ceDoDlgScroll */


/*     wParam */
/*         If lParam is TRUE, this parameter identifies the control that
           receives the focus. If lParam is FALSE, this parameter indicates
           whether the next or previous control with the WS_TABSTOP style
           receives the focus. If wParam is zero, the next control receives
           the focus; otherwise, the previous control with the WS_TABSTOP
           style receives the focus.  */
/*     lParam */
/*         The low-order word indicates how the system uses wParam. If the
           low-order word is TRUE, wParam is a handle associated with the
           control that receives the focus; otherwise, wParam is a flag that
           indicates whether the next or previous control with the WS_TABSTOP
           style receives the focus.  */

static void
ceDoDlgFocusScroll( CeDlgHdr* dlgHdr, HWND nextCtrl )
{
    /* Scroll the current focus owner into view.
     *
     * There's nothing passed in to tell us who it is, so look it up.
     *
     * What's in view?  First, a window has a scroll position, nPos, that
     * tells how many pixels are scrolled out of view above the window.  Then
     * a control has an offset within the containing rect (which shifts as
     * it's scrolled.)  Finally, all rects are relative to the screen, so we
     * need to get the containing rect to figure out what the control's
     * position is.  
     *
     * The first question, which can be answered without reference to
     * scrolling, is "Are we in view?"  If we're not, then we need to look at
     * scrolling to see how to fix it.
     */

    if ( !IS_SMARTPHONE(dlgHdr->globals) && !!nextCtrl ) {
        HWND hDlg = dlgHdr->hDlg;
        RECT rect;
        XP_U16 dlgHeight, ctrlHeight, dlgTop;
        XP_S16 ctrlPos;

        GetClientRect( hDlg, &rect );
        dlgHeight = rect.bottom - rect.top;

        GetWindowRect( hDlg, &rect );
        dlgTop = rect.top;

        GetWindowRect( nextCtrl, &rect );
        ctrlPos = rect.top - dlgTop - TITLE_HT;
        ctrlHeight = rect.bottom - rect.top;

        if ( ctrlPos < 0 ) {
            adjustScrollPos( hDlg, ctrlPos );
        } else if ( (ctrlPos + ctrlHeight) > dlgHeight ) {
            setScrollPos( hDlg, ctrlPos - ctrlHeight );
        }
    }
} /* ceDoDlgFocusScroll */

static XP_Bool
ceFindMenu( HMENU menu, XP_U16 id, 
#ifndef _WIN32_WCE
            HMENU* foundMenu, XP_U16* foundPos,
#endif
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
            found = ceFindMenu( minfo.hSubMenu, id, 
#ifndef _WIN32_WCE
                                foundMenu, foundPos,
#endif
                                foundBuf, bufLen );
        } else if ( MFT_SEPARATOR == minfo.fType ) {
            continue;
        } else if ( minfo.wID == id ) {
            found = XP_TRUE;
#ifndef _WIN32_WCE
            *foundPos = pos;
            *foundMenu = menu;
#endif
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

static HMENU
ceGetMenu( const CEAppGlobals* globals )
{
#ifdef _WIN32_WCE
    TBBUTTONINFO info;
    XP_MEMSET( &info, 0, sizeof(info) );
    info.cbSize = sizeof(info);
    info.dwMask = TBIF_LPARAM;
    SendMessage( globals->hwndCB, TB_GETBUTTONINFO, IDM_MENU, 
                 (LPARAM)&info );
    return (HMENU)info.lParam;
#else
    return GetMenu( globals->hWnd );
#endif
}

void
ceSetLeftSoftkey( CEAppGlobals* globals, XP_U16 newId )
{
    if ( newId != globals->softKeyId ) {
        wchar_t menuTxt[32];    /* text of newId menu */
        HMENU menu = ceGetMenu( globals );
#ifdef _WIN32_WCE
        TBBUTTONINFO info;
#else
        HMENU prevMenu;
        XP_U16 prevPos;
#endif
        XP_U16 oldId = globals->softKeyId;
        if ( 0 == oldId ) {
            oldId = ID_INITIAL_SOFTID;
        }

        /* Look up the text... */
        if ( ceFindMenu( menu, newId, 
#ifndef _WIN32_WCE
                          &prevMenu, &prevPos,
#endif
                          menuTxt, VSIZE(menuTxt) ) ) {
            globals->softKeyId = newId;
#ifndef _WIN32_WCE
            globals->softKeyMenu = prevMenu;
#endif
        } else {
            XP_LOGF( "%s: ceFindMenu failed", __func__ );
        }

        /* Make it the button */
#ifdef _WIN32_WCE
        XP_MEMSET( &info, 0, sizeof(info) );
        info.cbSize = sizeof(info);
        info.dwMask = TBIF_TEXT | TBIF_COMMAND;
        info.idCommand = newId;
        info.pszText = menuTxt;
        SendMessage( globals->hwndCB, TB_SETBUTTONINFO, oldId, (LPARAM)&info );
#else
        setW32DummyMenu( globals, menu, newId, menuTxt );
#endif
    }
    ceCheckMenus( globals );  /* in case left key was or should be checked */
} /* ceSetLeftSoftkey */

static void
checkOneItem( const CEAppGlobals* globals, XP_U16 id, XP_Bool check )
{
    UINT uCheck = check ? MF_CHECKED : MF_UNCHECKED;
    HMENU menu = ceGetMenu( globals );

    (void)CheckMenuItem( menu, id, uCheck );
#ifndef _WIN32_WCE
    if ( id == globals->softKeyId ) {
        (void)CheckMenuItem( globals->softKeyMenu, id, uCheck );
    }
#endif
}

void
ceCheckMenus( const CEAppGlobals* globals )
{
    const BoardCtxt* board = globals->game.board;

    checkOneItem( globals, ID_MOVE_VALUES, board_get_showValues( board ));
    checkOneItem( globals, ID_MOVE_FLIP, board_get_flipped( board ) );
    checkOneItem( globals, ID_FILE_FULLSCREEN, globals->appPrefs.fullScreen );
    checkOneItem( globals, ID_MOVE_HIDETRAY,
                  TRAY_REVEALED != board_getTrayVisState( board ) );
} /* ceCheckMenus */

#ifdef OVERRIDE_BACKKEY
void
trapBackspaceKey( HWND hDlg )
{
    /* Override back key so we can pass it to edit controls */
    SendMessage( SHFindMenuBar(hDlg), SHCMBM_OVERRIDEKEY, VK_TBACK, 
                 MAKELPARAM (SHMBOF_NODEFAULT | SHMBOF_NOTIFY, 
                             SHMBOF_NODEFAULT | SHMBOF_NOTIFY));
    /* To undo the above
    SendMessage( SHFindMenuBar(hDlg), SHCMBM_OVERRIDEKEY, VK_TBACK, 
                 MAKELPARAM( SHMBOF_NODEFAULT | SHMBOF_NOTIFY, 
                             0 ) );
    */
}
#endif

/* Bugs in mingw32ce headers force defining _WIN32_IE, which causes
 * SHGetSpecialFolderPath to be defined as SHGetSpecialFolderPathW which
 * is not on Wince.  Once I turn off _WIN32_IE this can go away. */
#ifdef  _WIN32_IE
# ifdef SHGetSpecialFolderPath
#  undef SHGetSpecialFolderPath
# endif
BOOL SHGetSpecialFolderPath( HWND hwndOwner,
                             LPTSTR lpszPath,
                             int nFolder,
                             BOOL fCreate );
#endif

static void
lookupSpecialDir( wchar_t* bufW, XP_U16 indx )
{
    bufW[0] = 0;
#ifdef _WIN32_WCE
    SHGetSpecialFolderPath( NULL, bufW, 
                            (indx == MY_DOCS_CACHE)? 
                            CSIDL_PERSONAL : CSIDL_PROGRAM_FILES,
                            TRUE );
    if ( 0 == bufW[0] ) {
        XP_WARNF( "SHGetSpecialFolderPath failed" );
        wcscpy( bufW, L"\\My Documents" );
    }
#else
    wcscat( bufW, L"." );
#endif
    if ( indx == PROGFILES_CACHE ) {
        wcscat( bufW, L"\\" LCROSSWORDS_DIR_NODBG );
    } else {
        wcscat( bufW, L"\\" LCROSSWORDS_DIR L"\\" );
    }
}

XP_U16
ceGetPath( CEAppGlobals* globals, CePathType typ, 
           void* bufOut, XP_U16 bufLen )
{
    XP_U16 len;
    wchar_t bufW[CE_MAX_PATH_LEN];
    XP_U16 cacheIndx = typ == PROGFILES_PATH ? PROGFILES_CACHE : MY_DOCS_CACHE;
    wchar_t* specialDir = globals->specialDirs[cacheIndx];
    XP_Bool asAscii = XP_FALSE;

    if ( !specialDir ) {
        wchar_t buf[128];
        XP_U16 len;
        lookupSpecialDir( buf, cacheIndx );
        len = 1 + wcslen( buf );
        specialDir = XP_MALLOC( globals->mpool, len * sizeof(specialDir[0]) );
        wcscpy( specialDir, buf );
        globals->specialDirs[cacheIndx] = specialDir;
    }

    wcscpy( bufW, specialDir );

    switch( typ ) {
    case PREFS_FILE_PATH_L:
        wcscat( bufW, L"xwprefs" );
        break;
    case DEFAULT_DIR_PATH_L:
        /* nothing to do */
        break;
    case DEFAULT_GAME_PATH:
        asAscii = XP_TRUE;
        wcscat( bufW, L"_newgame" );
        break;

    case PROGFILES_PATH:
        /* nothing to do */
        break;
    }

    len = wcslen( bufW );
    if ( asAscii ) {
        (void)WideCharToMultiByte( CP_ACP, 0, bufW, len + 1,
                                   (char*)bufOut, bufLen, NULL, NULL );
    } else {
        wcscpy( (wchar_t*)bufOut, bufW );
    }
    return len;
} /* ceGetPath */

int
ceMessageBoxChar( CEAppGlobals* globals, const XP_UCHAR* str, 
                  const wchar_t* title, XP_U16 buttons, SkipAlertBits bit )
{
    int result = IDOK;
    XP_Bool callIt;

    if ( SAB_NONE == bit ) {
        callIt = XP_TRUE;
    } else {
        callIt = 0 == (bit & globals->skipAlrtBits);
    }

    if ( callIt ) {
        HWND parent;
        /* Get the length required, then alloc and go.  This is technically
           correct, but everywhere else I assume a 2:1 ratio for wchar_t:char. */
        XP_U16 clen = 1 + strlen(str);
        XP_U32 wlen = 1 + MultiByteToWideChar( CP_UTF8, 0, str, clen, NULL, 0 );
        wchar_t widebuf[wlen];
    
        MultiByteToWideChar( CP_UTF8, 0, str, clen, widebuf, wlen );

        parent = GetForegroundWindow();

        globals->skipAlrtBits |= bit;
        result = MessageBox( parent, widebuf, title, buttons );
        globals->skipAlrtBits &= ~bit;
    }

    return result;
} /* ceMessageBoxChar */

XP_Bool
ceGetExeDir( wchar_t* buf, XP_U16 bufLen )
{
    /* I wanted to use SHGetKnownFolderPath to search in \\Program
       Files\\Crosswords, but perhaps it's better to search in the directory
       in which the app is located.  If I get CAB files working for
       Smartphone, then that directory will be \\Program Files\\Crosswords.
       But if users have to install files using the File Explorer it'll be
       easier for them if all that's required is that the app and dictionaries
       be in the same place.  GetModuleFileName() supports both.
    */

    DWORD nChars = GetModuleFileName( NULL, buf, bufLen );
    XP_Bool success = nChars < bufLen;
    if ( success ) {
        wchar_t* lastSlash = wcsrchr( buf, '\\' );
        if ( !!lastSlash ) {
            *lastSlash = 0;
        }
    }

/*     SHGetSpecialFolderPath(NULL,NULL,0,FALSE); */

    return success;
} /* ceGetExeDir */
