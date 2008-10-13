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
#ifdef _win32_wce
#include <aygshell.h>
#endif

#include "cemain.h" 
#include "cesvdgms.h" 
#include "ceutil.h" 
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
ceFileExists( const wchar_t* name )
{
    wchar_t buf[128];
    DWORD attributes;
    swprintf( buf, DEFAULT_DIR_NAME L"\\%s.xwg", name );

    attributes = GetFileAttributes( buf );
    return attributes != 0xFFFFFFFF;
}

static void
makeUniqueName( wchar_t* buf, XP_U16 XP_UNUSED_DBG(bufLen) )
{
    XP_U16 ii;
    for ( ii = 1; ii < 100; ++ii ) {
#ifdef DEBUG
        int len = 
#endif
            swprintf( buf, L"Untitled%d", ii );
        XP_ASSERT( len < bufLen );
        if ( !ceFileExists( buf ) ) {
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
        LoadString( state->dlgHdr.globals->hInst, state->lableTextId, 
                    buf, VSIZE(buf) );
        (void)SetDlgItemText( hDlg, IDC_SVGN_SELLAB, buf );

        ceDlgSetup( &state->dlgHdr, hDlg, DLG_STATE_TRAPBACK );

        result = TRUE;
    } else {
        state = (CeSaveGameNameState*)GetWindowLongPtr( hDlg, GWL_USERDATA );
        if ( !!state ) {
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
                        (void)GetDlgItemText( hDlg, IDC_SVGN_EDIT, buf, 
                                              VSIZE(buf) );
                        if ( ceFileExists( buf ) ) {
                            messageBoxChar( state->dlgHdr.globals, 
                                            "File exists", L"Oops!", MB_OK );
                            break;
                        }
                        swprintf( state->buf, DEFAULT_DIR_NAME L"\\%s.xwg",
                                  buf );
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
ceConfirmUniqueName( CEAppGlobals* globals, XP_U16 strId, wchar_t* buf, 
                     XP_U16 buflen )
{
    CeSaveGameNameState state;

    LOG_FUNC();

    makeUniqueName( buf, buflen );

    XP_MEMSET( &state, 0, sizeof(state) );
    state.dlgHdr.globals = globals;
    state.buf = buf;
    state.buflen = buflen;
    state.lableTextId = strId;
    (void)DialogBoxParam( globals->hInst, (LPCTSTR)IDD_SAVENAMEDLG, 
                          globals->hWnd, 
                          (DLGPROC)SaveNameDlg, (long)&state );
    XP_LOGW( __func__, buf );
    return !state.cancelled;
} /* ceConfirmUniqueName */

typedef struct CeSavedGamesState {
    CeDlgHdr dlgHdr;
    wchar_t* buf; 
    XP_U16 buflen;
    XP_S16 sel;
    XP_U16 openGameIndex;
    wchar_t curName[128];
    XP_U16 nItems;

    XP_U16 gameListId;
    XP_Bool opened;
    XP_Bool inited;
    XP_Bool relaunch;
} CeSavedGamesState;

/* Probably belongs as a utility */
static void
getCBText( CeSavedGamesState* state, XP_U16 id, XP_U16 sel, wchar_t* buf, 
           XP_U16* lenp )
{
    HWND hDlg = state->dlgHdr.hDlg;
    CEAppGlobals* globals = state->dlgHdr.globals;
    XP_U16 len;

    len = SendDlgItemMessage( hDlg, id, GETLBTEXTLEN(globals), sel, 0L );

    if ( len < *lenp ) {
        (void)SendDlgItemMessage( hDlg, id, GETLBTEXT(globals), sel,
                                  (LPARAM)buf );
    } else {
        XP_ASSERT( 0 );
    }
    *lenp = len;
} /* getCBText */

static void
getFullSelPath( CeSavedGamesState* state, wchar_t* buf, XP_U16 buflen )
{
    XP_U16 len;
    lstrcpy( buf, DEFAULT_DIR_NAME L"\\" );
    len = lstrlen( buf );
    buflen -= len;
    getCBText( state, state->gameListId, state->sel, &buf[len], &buflen );
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
    ceEnOrDisable( hDlg, IDC_SVGM_CHANGE, haveItem && !curSelected );
}

static void
initSavedGamesData( CeSavedGamesState* state )
{
    HANDLE fileH;
    HWND hDlg = state->dlgHdr.hDlg;
    CEAppGlobals* globals = state->dlgHdr.globals;
    WIN32_FIND_DATA data;
    wchar_t path[256];
    XP_S16 curSel = -1;
    XP_U16 ii;
    XP_U16 nItems = 0;

    XP_MEMSET( &data, 0, sizeof(data) );
    lstrcpy( path, DEFAULT_DIR_NAME L"\\" );
    lstrcat( path, L"*.xwg" );

    fileH = FindFirstFile( path, &data );
    for ( ii = 0; fileH != INVALID_HANDLE_VALUE; ++ii ) {
        XP_U16 len = wcslen( data.cFileName );
        XP_U16 item;
        XP_Bool isCurGame = 0 == wcscmp( state->curName, data.cFileName );

        XP_ASSERT( data.cFileName[len-4] == '.');
        data.cFileName[len-4] = 0;

        /* Insert in sorted order.  This should be fast enough for reasonable
           numbers of saved games. */
        for ( item = 0; item < nItems; ++item ) {
            wchar_t buf[256];
            (void)SendDlgItemMessage( hDlg, state->gameListId, 
                                      GETLBTEXT(globals), item,
                                      (LPARAM)buf );
            /* Does the current item belong above the one we're inserting? */
            if ( 0 <= wcscmp( buf, data.cFileName ) ) {
                break;
            }
        }
        (void)SendDlgItemMessage( hDlg, state->gameListId, 
                                  INSERTSTRING(globals),
                                  item, (LPARAM)data.cFileName );

        /* Remember which entry matches the currently opened game, and adjust
           if it's changed.  We may be incrementing an uninitialized curSel,
           but that's ok as isCurGame is guaranteed to be true exactly once
           through. */
        if ( isCurGame ) {
            curSel = item;
        } else if ( curSel >= item ) {
            ++curSel;           /* since we've moved it up */
        }

        ++nItems;

        if ( !FindNextFile( fileH, &data ) ) {
            XP_ASSERT( GetLastError() == ERROR_NO_MORE_FILES );
            break;
        }
    }

    state->nItems = nItems;
    state->openGameIndex = curSel;

    SendDlgItemMessage( hDlg, state->gameListId, 
                        SETCURSEL(globals), curSel, 0 );
    state->sel = curSel;

    setButtons( state );

    LOG_RETURN_VOID();
} /* initSavedGamesData */

static XP_Bool
renameSelected( CeSavedGamesState* state )
{
    wchar_t path[MAX_PATH];
    XP_Bool confirmed = ceConfirmUniqueName( state->dlgHdr.globals, IDS_RENAME, 
                                             path, VSIZE(path) );
    if ( confirmed ) {
        wchar_t curPath[MAX_PATH];
        getFullSelPath( state, curPath, VSIZE(curPath) );
        confirmed = MoveFile( curPath, path );
    }
    return confirmed;
} /* renameSelected */

static XP_Bool
duplicateSelected( CeSavedGamesState* state )
{
    wchar_t newPath[MAX_PATH];
    XP_Bool confirmed;

    confirmed = ceConfirmUniqueName( state->dlgHdr.globals, IDS_DUPENAME, 
                                     newPath, VSIZE(newPath) );
    if ( confirmed ) {
        wchar_t curPath[MAX_PATH];
        getFullSelPath( state, curPath, VSIZE(curPath) );
        confirmed = CopyFile( curPath, newPath, TRUE ); /* TRUE is what??? */
    }
    return confirmed;
} /* duplicateSelected */

static XP_Bool
deleteSelected( CeSavedGamesState* state )
{
    wchar_t buf[128];
    wchar_t path[128];
    XP_U16 len = VSIZE(buf);

    /* confirm first!!!! */
    XP_Bool confirmed = queryBoxChar( state->dlgHdr.globals, 
                                      "Are you certain you want to delete the "
                                      "selected game?  This action cannot be "
                                      "undone.");
    if ( confirmed ) {
        getCBText( state, state->gameListId,
                   state->sel, buf, &len );
        swprintf( path, DEFAULT_DIR_NAME L"\\%s.xwg", buf );
        confirmed = DeleteFile( path );
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
                        getCBText( state, state->gameListId, state->sel, 
                                   buf, &len );
                        swprintf( state->buf, DEFAULT_DIR_NAME L"\\%s.xwg", 
                                  buf );
                        XP_LOGW( "returning", state->buf );
                        state->opened = XP_TRUE;
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

XP_Bool
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
        wchar_t shortName[128];
        XP_U16 len;
        XP_LOGF( curPath );

        len = (XP_U16)XP_STRLEN( curPath );
        MultiByteToWideChar( CP_ACP, MB_PRECOMPOSED, curPath, len + 1, 
                             shortName, len + 1 );
        len = wcslen( DEFAULT_DIR_NAME L"\\" );
        lstrcpy( state.curName, shortName+len );
        XP_LOGW( "shortName", state.curName );
    }

    for ( ; ; ) {
        (void)DialogBoxParam( globals->hInst, (LPCTSTR)IDD_SAVEDGAMESDLG, 
                              globals->hWnd, 
                              (DLGPROC)SavedGamesDlg, (long)&state );

        if ( !state.relaunch ) {
            break;
        }
        state.relaunch = XP_FALSE;
    }
    XP_LOGW( __func__, buf );

    return state.opened;
} /* ceSavedGamesDlg  */
