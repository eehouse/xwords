/* -*- fill-column: 77; c-basic-offset: 4; compile-command: "make TARGET_OS=wince DEBUG=TRUE" -*- */
/* 
 * Copyright 2004-2008 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#ifdef ALLOW_CHOOSE_FONTS

#include <windowsx.h>
#include "stdafx.h" 
#include <commdlg.h>

#include "ceclrsel.h" 
#include "ceutil.h" 
#include "cedebug.h"
#include "debhacks.h"
#include "cefonts.h"

#define MIN_FONT_SHOWN 6
#define MAX_FONT_SHOWN 36

typedef struct _FontsDlgState {
    CeDlgHdr dlgHdr;
    HDC tmpDC;
    RECT textRect;
    XP_U16 fontsComboId;
    XP_U16 fontSizeId;
    XP_Bool inited;
} FontsDlgState;

#ifndef _WIN32_WCE
# define HAS_ENUMFONTFAMILIESEX
#endif

#ifdef HAS_ENUMFONTFAMILIESEX
# define ENUMFONTFAMILIES(a,b,c,d,e) EnumFontFamiliesEx((a),(b),(d),(e),0)
#else
# define ENUMFONTFAMILIES(a,b,c,d,e) EnumFontFamilies((a),(c),(d),(e))
#endif


/* int CALLBACK EnumFontFamProc( */
/*   ENUMLOGFONT *lpelf,    // logical-font data */
/*   NEWTEXTMETRIC *lpntm,  // physical-font data */
/*   DWORD FontType,        // type of font */
/*   LPARAM lParam          // application-defined data */
/* ); */

static int
fontProc2( ENUMLOGFONTEX* lpelfe,
           NEWTEXTMETRIC/*EX*/* XP_UNUSED(lpntme),
          DWORD FontType, LPARAM lParam)
{
/*     if ( !lstrcmp( L"Western", lpelfe->elfScript )  */
/*          && ((FontType & TRUETYPE_FONTTYPE) != 0 ) ) { */

        FontsDlgState* state = (FontsDlgState*)lParam;
        CEAppGlobals* globals = state->dlgHdr.globals;

        if ( 0 > SendDlgItemMessage( state->dlgHdr.hDlg,
                                     state->fontsComboId, 
                                     FINDSTRINGEXACT(globals), -1, 
                                     (LPARAM)lpelfe->elfLogFont.lfFaceName ) ) {
            SendDlgItemMessage( state->dlgHdr.hDlg, state->fontsComboId, 
                                ADDSTRING(globals), 0, 
                                (LPARAM)lpelfe->elfLogFont.lfFaceName );
        }
/*     } */
    return 1;
}

static int
fontProc( ENUMLOGFONTEX* lpelfe,               // logical-font data
          NEWTEXTMETRICEX* XP_UNUSED(lpntme),  // physical-font data
          DWORD XP_UNUSED(FontType),           // type of font
          LPARAM lParam)                       // application-defined data
{
    FontsDlgState* state = (FontsDlgState*)lParam;
    CEAppGlobals* globals = state->dlgHdr.globals;
#ifdef HAS_ENUMFONTFAMILIESEX
    LOGFONT fontInfo;

    XP_MEMSET( &fontInfo, 0, sizeof(fontInfo) );
    fontInfo.lfCharSet = DEFAULT_CHARSET;
    wcscpy( fontInfo.lfFaceName, lpelfe->elfLogFont.lfFaceName );
#endif

    ENUMFONTFAMILIES( state->tmpDC, 
                      &fontInfo,
                      lpelfe->elfLogFont.lfFaceName,
                      fontProc2, (LPARAM)state );
    return 1;
}

static void
ceLoadFontsInfo( FontsDlgState* state )
{
    LOGFONT fontInfo;
    XP_U16 ii;
    HWND hDlg = state->dlgHdr.hDlg;
    CEAppGlobals* globals = state->dlgHdr.globals;

    XP_MEMSET( &fontInfo, 0, sizeof(fontInfo) );
    fontInfo.lfCharSet = DEFAULT_CHARSET;

    state->tmpDC = CreateCompatibleDC( NULL );

    XP_LOGF( "%s: calling EnumFontFamilies", __func__ );
    ENUMFONTFAMILIES( state->tmpDC, 
                      &fontInfo,
                      NULL,
                      fontProc,
                      (LPARAM)state );

    DeleteDC( state->tmpDC );
    state->tmpDC = NULL;
    SendDlgItemMessage( hDlg, state->fontsComboId, SETCURSEL(globals), 0, 0L );

    /* Stuff the size list */
    for ( ii = MIN_FONT_SHOWN; ii <= MAX_FONT_SHOWN; ++ii ) {
        wchar_t widebuf[4];
        swprintf( widebuf, L"%d", ii );
        SendDlgItemMessage( hDlg, state->fontSizeId, 
                            ADDSTRING(globals), 0, (LPARAM)widebuf );
    }
    SendDlgItemMessage( hDlg, state->fontSizeId, SETCURSEL(globals), 0, 0L );
}

static void
ceDrawWithFont( FontsDlgState* state, HDC hdc )
{
    HWND hDlg = state->dlgHdr.hDlg;
    CEAppGlobals* globals = state->dlgHdr.globals;
    XP_S16 selFont = SendDlgItemMessage( hDlg, state->fontsComboId,
                                         GETCURSEL(globals), 0, 0L);
    XP_S16 selSize = SendDlgItemMessage( hDlg, state->fontSizeId,
                                         GETCURSEL(globals), 0, 0L);
    if ( selFont >= 0 && selSize >= 0 ) {
        LOGFONT fontInfo;
        wchar_t fontName[33];
        HFONT oldFont, newFont;
        RECT rect;
        wchar_t buf[16];
        wchar_t* lines[] = {
            buf
            ,L"ABCDEFGHIJKL"
            ,L"MNOPQRSTUV"
            ,L"WXYZ0123456789"
        };
        XP_U16 ii;

        (void)SendDlgItemMessage( hDlg, state->fontsComboId, 
                                  GETLBTEXT(globals), selFont,
                                  (LPARAM)fontName );

        XP_MEMSET( &fontInfo, 0, sizeof(fontInfo) );
        wcscpy( fontInfo.lfFaceName, fontName );
        fontInfo.lfHeight = selSize + MIN_FONT_SHOWN;
        swprintf( buf, L"Size: %d", fontInfo.lfHeight );

        newFont = CreateFontIndirect( &fontInfo );
        oldFont = SelectObject( hdc, newFont );

        rect = state->textRect;
        for ( ii = 0; ii < sizeof(lines)/sizeof(lines[0]); ++ii ) {
            DrawText( hdc, lines[ii], -1, &rect,
                      DT_SINGLELINE | DT_TOP | DT_LEFT );
            rect.top += MAX_FONT_SHOWN - 4;
        }

        SelectObject( hdc, oldFont );
    }
}

LRESULT CALLBACK
FontsDlgProc( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    FontsDlgState* state;
    BOOL result = FALSE;

    if ( message == WM_INITDIALOG ) {
        SetWindowLongPtr( hDlg, GWL_USERDATA, lParam );

        state = (FontsDlgState*)lParam;
        state->inited = XP_FALSE;

        state->fontsComboId = LB_IF_PPC(state->dlgHdr.globals,FONTS_COMBO);
        state->fontSizeId = LB_IF_PPC(state->dlgHdr.globals,FONTSIZE_COMBO);

        XP_LOGF( "calling ceDlgSetup" );
        ceDlgSetup( &state->dlgHdr, hDlg, DLG_STATE_NONE );
        XP_LOGF( "ceDlgSetup done" );

        result = TRUE;
    } else {
        state = (FontsDlgState*)GetWindowLongPtr( hDlg, GWL_USERDATA );
        if ( !!state ) {
            if ( !state->inited ) {
                state->inited = XP_TRUE;
                ceLoadFontsInfo( state );
            }

            if ( ceDoDlgHandle( &state->dlgHdr, message, wParam, lParam) ) {
                result = TRUE;
            } else if ( WM_NOTIFY == message ) {
                InvalidateRect( hDlg, &state->textRect, TRUE );
            } else if ( message == WM_COMMAND ) {
                XP_U16 wid = LOWORD(wParam);
                if ( CBN_SELCHANGE == HIWORD(wParam) ) {
                    if ( state->fontsComboId == wid ) {
                        InvalidateRect( hDlg, &state->textRect, TRUE );
                    } else if ( state->fontSizeId == wid ) {
                        InvalidateRect( hDlg, &state->textRect, TRUE );
                    }
                } else if ( BN_CLICKED == HIWORD(wParam) ) {
                    switch( wid ) {
                    case IDOK:
                    case IDCANCEL:
                        EndDialog( hDlg, LOWORD(wParam) );
                        result = TRUE;
                        break;
                    }
                }
            } else if ( message == WM_PAINT ) {
                PAINTSTRUCT ps; 
                HDC hdc = BeginPaint( hDlg, &ps); 
                ceDrawWithFont( state, hdc );
                EndPaint( hDlg, &ps ); 
                result = FALSE;
            }
        }
    }
    return result;
}

void
ceShowFonts( HWND hDlg, CEAppGlobals* globals )
{
    FontsDlgState state;
    XP_MEMSET( &state, 0, sizeof(state) );
    state.dlgHdr.globals = globals;

    state.textRect.left = 5;
    state.textRect.top = 60;
    state.textRect.right = 300;
    state.textRect.bottom = state.textRect.top + (4*MAX_FONT_SHOWN);

    (void)DialogBoxParam( globals->locInst, (LPCTSTR)IDD_FONTSSDLG, hDlg,
                          (DLGPROC)FontsDlgProc, (long)&state );
}

#endif
