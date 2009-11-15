/* -*- fill-column: 77; c-basic-offset: 4; compile-command: "make TARGET_OS=wince DEBUG=TRUE" -*- */
/* 
 * Copyright 2008 by Eric House (xwords@eehouse.org).   All rights reserved.
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

#include <stdio.h>
#include "cedebug.h"

#ifdef DEBUG

#define CASE_STR(c)  case c: str = #c; break

const char* 
messageToStr( UINT message )
{
    const char* str;
    switch( message ) {
        CASE_STR( WM_NCACTIVATE );
        CASE_STR( WM_QUERYNEWPALETTE );
#ifdef _WIN32_WCE
        CASE_STR( WM_IME_NOTIFY );
        CASE_STR( WM_IME_SETCONTEXT );
#endif
        CASE_STR( WM_WINDOWPOSCHANGED );
        CASE_STR( WM_MOVE );
        CASE_STR( WM_SIZE );
        CASE_STR( WM_ACTIVATE );
        CASE_STR( WM_SETTINGCHANGE );
        CASE_STR( WM_VSCROLL );
        CASE_STR( WM_COMMAND );
        CASE_STR( WM_PAINT );
        CASE_STR( WM_LBUTTONDOWN );
        CASE_STR( WM_MOUSEMOVE );
        CASE_STR( WM_LBUTTONUP );
        CASE_STR( WM_KEYDOWN );
        CASE_STR( WM_KEYUP );
        CASE_STR( WM_CHAR );
        CASE_STR( WM_TIMER );
        CASE_STR( WM_DESTROY );
        CASE_STR( XWWM_TIME_RQST );
        CASE_STR( XWWM_HOSTNAME_ARRIVED );
        CASE_STR( XWWM_SOCKET_EVT );
        CASE_STR( WM_DRAWITEM );
        CASE_STR( WM_NEXTDLGCTL );
        CASE_STR( WM_CTLCOLORSTATIC );
        CASE_STR( WM_CTLCOLORBTN );
        CASE_STR( WM_SETFONT );
        CASE_STR( WM_INITDIALOG );
        CASE_STR( WM_SHOWWINDOW );
        CASE_STR( WM_WINDOWPOSCHANGING );
        CASE_STR( WM_SETFOCUS );
        CASE_STR( WM_NCPAINT );
        CASE_STR( WM_ERASEBKGND );
        CASE_STR( WM_NCCALCSIZE );
        CASE_STR( WM_SETTEXT );
        CASE_STR( WM_CTLCOLORDLG );
        CASE_STR( WM_MOUSEACTIVATE );
        CASE_STR( WM_SETCURSOR );
        CASE_STR( WM_CTLCOLORLISTBOX );
        CASE_STR( WM_CTLCOLOREDIT );
        CASE_STR( WM_NCDESTROY );
        CASE_STR( WM_NOTIFY );
        CASE_STR( WM_NCHITTEST );
        CASE_STR( WM_HSCROLL );
        CASE_STR( WM_STYLECHANGED );
        CASE_STR( WM_NOTIFYFORMAT );
        CASE_STR( WM_KILLFOCUS );
        CASE_STR( WM_CTLCOLORSCROLLBAR );
        CASE_STR( WM_NCMOUSEMOVE );
        CASE_STR( SBM_SETSCROLLINFO );
        CASE_STR( WM_HOTKEY );
        CASE_STR( WM_CLOSE );
        CASE_STR( WM_ACTIVATEAPP );
        CASE_STR( WM_ENTERMENULOOP );
        CASE_STR( WM_EXITMENULOOP );
        CASE_STR( WM_INITMENUPOPUP );
        CASE_STR( WM_CANCELMODE );
        CASE_STR( WM_ENTERIDLE );
        CASE_STR( WM_GETDLGCODE );
    default:
        str = "<unknown>";
    }
    return str;
} /* messageToStr */

void
XP_LOGW( const XP_UCHAR* prefix, const wchar_t* arg )
{
    XP_UCHAR buf[512];
    (void)WideCharToMultiByte( CP_UTF8, 0, arg, -1,
                               buf, sizeof(buf), NULL, NULL );
    XP_LOGF( "%s: %s", prefix, buf );
}

void
logRect( const char* comment, const RECT* rect )
{
    XP_LOGF( "%s: %s: left=%ld,top=%ld,right=%ld,bottom=%ld", __func__,
             comment, rect->left, rect->top, rect->right, rect->bottom );
}

#undef CASE_STR

void
logLastError( const char* comment )
{
    LPVOID lpMsgBuf;
    DWORD lastErr = GetLastError();
    XP_UCHAR msg[256];
    XP_U16 len;
    XP_U16 lenSoFar;

    snprintf( msg, sizeof(msg), "%s (err: %ld): ", comment, lastErr );
    lenSoFar = strlen( msg );

    FormatMessage( 
                  FORMAT_MESSAGE_ALLOCATE_BUFFER | 
                  FORMAT_MESSAGE_FROM_SYSTEM | 
                  FORMAT_MESSAGE_IGNORE_INSERTS,
                  NULL,
                  lastErr,
                  0, // Default language
                  (LPTSTR) &lpMsgBuf,
                  0,
                  NULL 
                  );

    len = wcslen( lpMsgBuf );
    if ( len >= sizeof(msg)-lenSoFar ) {
        len = sizeof(msg) - lenSoFar - 1;
    }
    WideCharToMultiByte( CP_ACP, 0, lpMsgBuf, len + 1,
                         msg + lenSoFar, len + 1, NULL, NULL );
    LocalFree( lpMsgBuf );

    XP_LOGF( "system error: %s", msg );
} /* logLastError */

#endif  /* DEBUG */
