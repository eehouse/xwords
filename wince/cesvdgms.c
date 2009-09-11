/* -*- fill-column: 77; compile-command: "make TARGET_OS=wince DEBUG=TRUE" -*- */
/* 
 * Copyright 2004-2008 by Eric House (xwords@eehouse.org).  All rights
 * reserved.
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
#include "stdafx.h" 
#include <commdlg.h>
#include <wchar.h>
#ifdef _win32_wce
#include <aygshell.h>
#endif

#include "cemain.h" 
#include "cesvdgms.h" 
#include "ceutil.h" 
#include "ceresstr.h"
#include "cedebug.h" 
#include "debhacks.h"

typedef struct CeSaveGameNameState {
    CeDlgHdr dlgHdr;
    wchar_t* buf; 
    XP_U16 buflen;
    XP_U16 lableTextId;
    XP_Bool cancelled;
    XP_Bool inited;
} CeSaveGameNameState;

static XP_Bool
ceFileExists( CEAppGlobals* globals, const wchar_t* name )
{
    wchar_t buf[CE_MAX_PATH_LEN];
    DWORD attributes;
    XP_U16 len;

    len = ceGetPath( globals, DEFAULT_DIR_PATH_L, buf, VSIZE(buf) );
    _snwprintf( &buf[len], VSIZE(buf)-len, L"%s.xwg", name );

    attributes = GetFileAttributes( buf );
    return attributes != 0xFFFFFFFF;
}

static void
makeUniqueName( CEAppGlobals* globals, wchar_t* buf, XP_U16 bufLen )
{
    XP_U16 ii;
    const wchar_t* fmt = ceGetResStringL( globals, IDS_UNTITLED_FORMAT );
    for ( ii = 1; ii < 100; ++ii ) {
#ifdef DEBUG
        int len = 
#endif
            _snwprintf( buf, bufLen, fmt, ii );
        XP_ASSERT( len < bufLen );
        if ( !ceFileExists( globals, buf ) ) {
            break;
        }
    }
    /* If we fall out of the loop, the user will be asked to confirm delete
       of Untitled99 or somesuch.  That's ok.... */
} /* makeUniqueName */

static LRESULT CALLBACK
SaveNameDlg( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    CeSaveGameNameState* state;
    XP_U16 wid;
    BOOL result = FALSE;

    if ( message == WM_INITDIALOG ) {
        SetWindowLongPtr( hDlg, GWL_USERDATA, lParam );

        state = (CeSaveGameNameState*)lParam;
        state->cancelled = XP_TRUE;
        state->inited = XP_FALSE;

        wchar_t buf[128];
        LoadString( state->dlgHdr.globals->locInst, state->lableTextId, 
                    buf, VSIZE(buf) );
        (void)SetDlgItemText( hDlg, IDC_SVGN_SELLAB, buf );

        ceDlgSetup( &state->dlgHdr, hDlg, DLG_STATE_TRAPBACK );

        result = TRUE;
    } else {
        state = (CeSaveGameNameState*)GetWindowLongPtr( hDlg, GWL_USERDATA );
        if ( !!state ) {
            CEAppGlobals* globals = state->dlgHdr.globals;
            if ( !state->inited ) {
                state->inited = XP_TRUE;
                (void)SetDlgItemText( hDlg, IDC_SVGN_EDIT, state->buf );
            }

            if ( ceDoDlgHandle( &state->dlgHdr, message, wParam, lParam) ) {
                result = TRUE;
            } else {

                switch (message) {
                case WM_COMMAND:
                    wid = LOWORD(wParam);
                    switch( wid ) {
                    case IDOK: {
                        wchar_t buf[128];
                        XP_U16 len;
                        (void)GetDlgItemText( hDlg, IDC_SVGN_EDIT, buf, 
                                              VSIZE(buf) );
                        if ( ceFileExists( globals, buf ) ) {
                            wchar_t widebuf[128];
                            _snwprintf( widebuf, VSIZE(widebuf), 
                                        ceGetResStringL( globals,
                                                         IDS_FILEEXISTSFMT_L ),
                                        buf );
                            result = MessageBox( hDlg, widebuf,
                                                 ceGetResStringL(globals,
                                                                 IDS_FYI_L ),
                                                 MB_OK | MB_ICONHAND );
                            (void)SetDlgItemText( hDlg, IDC_SVGN_EDIT, 
                                                  state->buf );
                            break;
                        }
                        len = ceGetPath( globals, DEFAULT_DIR_PATH_L, 
                                         state->buf, state->buflen );
                        _snwprintf( &state->buf[len], state->buflen - len,
                                    L"%s.xwg", buf );
                        XP_LOGW( __func__, state->buf );
                        /* fallthru */
                        state->cancelled = XP_FALSE;
                    }
                    case IDCANCEL:
                        EndDialog(hDlg, wid);
                        result = TRUE;
                        break;
                    }
                }
            }
        }
    }
    return result;
} /* SaveNameDlg */

XP_Bool
ceConfirmUniqueName( CEAppGlobals* globals, HWND hWnd, XP_U16 strId, 
                     wchar_t* buf, XP_U16 buflen )
{
    CeSaveGameNameState state;

    makeUniqueName( globals, buf, buflen );

    XP_MEMSET( &state, 0, sizeof(state) );
    state.dlgHdr.globals = globals;
    state.buf = buf;
    state.buflen = buflen;
    state.lableTextId = strId;
    (void)DialogBoxParam( globals->locInst, (LPCTSTR)IDD_SAVENAMEDLG, 
                          hWnd, (DLGPROC)SaveNameDlg, (long)&state );

    return !state.cancelled;
} /* ceConfirmUniqueName */

typedef struct CeSavedGamesState {
    CeDlgHdr dlgHdr;
    wchar_t* buf; 
    XP_U16 buflen;
    XP_S16 sel;                     /* index of game name currently selected */
    XP_U16 openGameIndex;           /* index of game that's open */
    wchar_t openNameW[128];
    wchar_t newNameW[MAX_PATH];
    XP_U16 nItems;

    XP_U16 gameListId;
    XP_Bool inited;
    XP_Bool relaunch;
    SavedGamesResult result;
} CeSavedGamesState;

static void
ceBasename( wchar_t* buf, const wchar_t* path )
{
    const wchar_t* ptr = path + wcslen(path);
    const wchar_t* dot = NULL;

    for ( ; ; ) {
        if ( ptr == path ) {
            break;
        } else if ( *ptr == L'\\' ) {
            ++ptr;
            break;
        } else if ( !dot && *ptr == L'.' ) {
            dot = ptr;
        }
        --ptr;
    }
    lstrcpy( buf, ptr );
    if ( !!dot ) {
        buf[dot-ptr] = 0;           /* nuke extension */
    }
} /* ceBasename */

/* Probably belongs as a utility */
static void
getComboText( CeSavedGamesState* state, wchar_t* buf, XP_U16* lenp )
{
    HWND hDlg = state->dlgHdr.hDlg;
    CEAppGlobals* globals = state->dlgHdr.globals;
    XP_U16 id = state->gameListId;
    XP_U16 sel = state->sel;
    XP_U16 len;

    len = SendDlgItemMessage( hDlg, id, GETLBTEXTLEN(globals), sel, 0L );

    if ( len < *lenp ) {
        (void)SendDlgItemMessage( hDlg, id, GETLBTEXT(globals), sel,
                                  (LPARAM)buf );
    } else {
        XP_ASSERT( 0 );
    }
    *lenp = len;
} /* getComboText */

static void
getFullSelPath( CeSavedGamesState* state, wchar_t* buf, XP_U16 buflen )
{
    XP_U16 len = ceGetPath( state->dlgHdr.globals, 
                            DEFAULT_DIR_PATH_L, buf, buflen );
    buflen -= len;
    getComboText( state, &buf[len], &buflen );
    lstrcat( buf, L".xwg" );
}

static void
setButtons( CeSavedGamesState* state ) 
{
    XP_Bool curSelected = state->openGameIndex == state->sel;
    XP_Bool haveItem = state->nItems > 0;
    HWND hDlg = state->dlgHdr.hDlg;

    ceEnOrDisable( hDlg, IDC_SVGM_OPEN, haveItem && !curSelected );
    ceEnOrDisable( hDlg, IDC_SVGM_DUP, haveItem );
    ceEnOrDisable( hDlg, IDC_SVGM_DEL, haveItem && !curSelected );
    ceEnOrDisable( hDlg, IDC_SVGM_CHANGE, haveItem );
}

static void
initSavedGamesData( CeSavedGamesState* state )
{
    HANDLE fileH;
    HWND hDlg = state->dlgHdr.hDlg;
    CEAppGlobals* globals = state->dlgHdr.globals;
    WIN32_FIND_DATA data;
    wchar_t path[CE_MAX_PATH_LEN];
    XP_S16 sel;
    XP_U16 ii;
    XP_U16 nItems = 0;

    XP_MEMSET( &data, 0, sizeof(data) );
    ceGetPath( globals, DEFAULT_DIR_PATH_L, path, VSIZE(path) );
    lstrcat( path, L"*.xwg" );

    fileH = FindFirstFile( path, &data );
    for ( ii = 0; fileH != INVALID_HANDLE_VALUE; ++ii ) {
        XP_U16 len = wcslen( data.cFileName );

        XP_ASSERT( data.cFileName[len-4] == L'.');
        data.cFileName[len-4] = 0;

        (void)SendDlgItemMessage( hDlg, state->gameListId, 
                                  ADDSTRING(globals),
                                  0, (LPARAM)data.cFileName );
        ++nItems;

        if ( !FindNextFile( fileH, &data ) ) {
            XP_ASSERT( GetLastError() == ERROR_NO_MORE_FILES );
            break;
        }
    }
    state->nItems = nItems;

    /* Now locate the open game and game we should select (which may
       differ) */
    sel = SendDlgItemMessage( hDlg, state->gameListId, FINDSTRINGEXACT(globals),
                              -1, (LPARAM)state->openNameW );
    XP_ASSERT( sel >= 0 );      /* should always have this */
    state->openGameIndex = sel;

    sel = SendDlgItemMessage( hDlg,state->gameListId, FINDSTRINGEXACT(globals),
                              -1, (LPARAM)state->newNameW );
    if ( sel < 0 ) {
        sel = state->openGameIndex;
    }

    SendDlgItemMessage( hDlg, state->gameListId, SETCURSEL(globals), sel, 0 );
    state->sel = sel;

    setButtons( state );
} /* initSavedGamesData */

static XP_Bool
renameSelected( CeSavedGamesState* state )
{
    wchar_t newPath[MAX_PATH];
    XP_Bool confirmed = ceConfirmUniqueName( state->dlgHdr.globals, state->dlgHdr.hDlg,
                                             IDS_RENAME, newPath, VSIZE(newPath) );
    if ( confirmed ) {
        /* If we're renaming the current game, we have to exit and let
           calling code handle it.  If we're renaming any other game, we can
           do it here. */
        if ( state->openGameIndex == state->sel ) {
            _snwprintf( state->buf, state->buflen, L"%s", newPath );
            state->result = CE_SVGAME_RENAME;
        } else {
            wchar_t curPath[MAX_PATH];
            getFullSelPath( state, curPath, VSIZE(curPath) );
            confirmed = MoveFile( curPath, newPath );
        }
    }

    if ( confirmed ) {
        ceBasename( state->newNameW, newPath );
    } else {
        state->newNameW[0] = 0;
    }
    return confirmed;
} /* renameSelected */

static XP_Bool
duplicateSelected( CeSavedGamesState* state )
{
    wchar_t newPath[MAX_PATH];
    XP_Bool confirmed;

    confirmed = ceConfirmUniqueName( state->dlgHdr.globals, state->dlgHdr.hDlg,
                                     IDS_DUPENAME, newPath, VSIZE(newPath) );
    if ( confirmed ) {
        wchar_t curPath[MAX_PATH];
        getFullSelPath( state, curPath, VSIZE(curPath) );
        confirmed = CopyFile( curPath, newPath, TRUE ); /* TRUE is what??? */
    }

    if ( confirmed ) {
        ceBasename( state->newNameW, newPath );
    } else {
        state->newNameW[0] = 0;
    }

    return confirmed;
} /* duplicateSelected */

static XP_Bool
deleteSelected( CeSavedGamesState* state )
{
    CEAppGlobals* globals = state->dlgHdr.globals;
    /* confirm first!!!! */
    XP_Bool confirmed = queryBoxChar( globals, state->dlgHdr.hDlg,
                                      ceGetResString( globals, 
                                                      IDS_CONFIM_DELETE ) );
    if ( confirmed ) {
        wchar_t pathW[CE_MAX_PATH_LEN];
        XP_U16 len = ceGetPath( state->dlgHdr.globals, 
                                DEFAULT_DIR_PATH_L, pathW, VSIZE(pathW) );
        XP_U16 remLen = VSIZE(pathW) - len;
        getComboText( state, &pathW[len], &remLen );
        wcscat( pathW, L".xwg" );
        confirmed = DeleteFile( pathW );
        if ( confirmed ) {
            state->sel = -1;
        }
    }
    return confirmed;
} /* deleteSelected */

static XP_Bool
tryGameChanged( CeSavedGamesState* state )
{
    XP_S16 sel = SendDlgItemMessage( state->dlgHdr.hDlg, state->gameListId,
                                     GETCURSEL(state->dlgHdr.globals), 0, 0L);
    XP_Bool changing = sel >= 0 && state->sel != sel;
    if ( changing ) {
        state->sel = sel;
        setButtons( state );
    }
    return changing;
} /* tryGameChanged */

static LRESULT CALLBACK
SavedGamesDlg( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    CeSavedGamesState* state;
    BOOL result = FALSE;

    if ( message == WM_INITDIALOG ) {
        SetWindowLongPtr( hDlg, GWL_USERDATA, lParam );

        state = (CeSavedGamesState*)lParam;
        state->inited = XP_FALSE;
        state->gameListId = LB_IF_PPC(state->dlgHdr.globals,IDC_SVGM_GAMELIST);

        ceDlgSetup( &state->dlgHdr, hDlg, DLG_STATE_DONEONLY );
        ceDlgComboShowHide( &state->dlgHdr, IDC_SVGM_GAMELIST );

        result = TRUE;
    } else {
        state = (CeSavedGamesState*)GetWindowLongPtr( hDlg, GWL_USERDATA );
        if ( !!state ) {

            if ( !state->inited ) {
                state->inited = XP_TRUE;
                initSavedGamesData( state );
            }

            if ( ceDoDlgHandle( &state->dlgHdr, message, wParam, lParam) ) {
                result = TRUE;
            } else if ( WM_NOTIFY == message ) {
                result = tryGameChanged( state );
            } else if ( message == WM_COMMAND ) {
                XP_U16 wid = LOWORD(wParam);

                if ( CBN_SELCHANGE == HIWORD(wParam) ) {
                    if (state->gameListId == wid ) {
                        result = tryGameChanged( state );
                    }
                } else if ( BN_CLICKED == HIWORD(wParam) ) {
                    switch( wid ) {
                    case IDC_SVGM_DUP:
                        state->relaunch = duplicateSelected( state );
                        break;
                    case IDC_SVGM_CHANGE:
                        state->relaunch = renameSelected( state );
                        break;
                    case IDC_SVGM_DEL:
                        state->relaunch = deleteSelected( state );
                        break;

                    case IDC_SVGM_OPEN: {
                        wchar_t buf[128];
                        XP_U16 len = VSIZE(buf);
                        getComboText( state, buf, &len );
                        len = ceGetPath( state->dlgHdr.globals, 
                                         DEFAULT_DIR_PATH_L, state->buf,
                                         state->buflen );
                        _snwprintf( &state->buf[len], state->buflen - len,
                                    L"%s.xwg", buf );
                        XP_LOGW( "returning", state->buf );
                        state->result = CE_SVGAME_OPEN;
                    }
                        /* fallthrough */
                    case IDOK:  /* Done button */
                        EndDialog(hDlg, wid);
                        result = TRUE;
                        break;
                    }
                    
                    if ( state->relaunch ) {
                        EndDialog( hDlg, wid );
                        result = TRUE;
                    }
                }
            }
        }
    }

    return result;
} /* SavedGamesDlg */

SavedGamesResult
ceSavedGamesDlg( CEAppGlobals* globals, const XP_UCHAR* curPath,
                 wchar_t* buf, XP_U16 buflen )
{
    CeSavedGamesState state;

    LOG_FUNC();

    XP_MEMSET( &state, 0, sizeof(state) ); /* sets cancelled */
    state.dlgHdr.globals = globals;
    state.buf = buf;
    state.buflen = buflen;
    state.sel = -1;

    if ( !!curPath ) {
        wchar_t widebuf[MAX_PATH];
        XP_U16 len;
        len = (XP_U16)XP_STRLEN( curPath );
        MultiByteToWideChar( CP_ACP, MB_PRECOMPOSED, curPath, len + 1, 
                             widebuf, len + 1 );
        ceBasename( state.openNameW, widebuf );
    }

    for ( ; ; ) {
        state.relaunch = XP_FALSE;
        state.result = CE_SVGAME_CANCEL;

        assertOnTop( globals->hWnd );
        (void)DialogBoxParam( globals->locInst, (LPCTSTR)IDD_SAVEDGAMESDLG, 
                              globals->hWnd, 
                              (DLGPROC)SavedGamesDlg, (long)&state );

        if ( !state.relaunch || (state.result == CE_SVGAME_RENAME) ) {
            break;
        }
    }
    XP_LOGW( __func__, buf );

    return state.result;
} /* ceSavedGamesDlg  */
