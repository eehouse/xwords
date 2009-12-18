/* -*- compile-command: "make -j3 TARGET_OS=wince DEBUG=TRUE"; -*- */
/* 
 * Copyright 2009 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#include "ceresstr.h"
#include "ceutil.h"
#include "cedebug.h"
#include "strutils.h"

static XP_U16 getDLLVersion( HINSTANCE hinst );


HINSTANCE
ceLoadResFile( const XP_UCHAR* file )
{
    HINSTANCE hinst = NULL;
    wchar_t widebuf[256];
    (void)MultiByteToWideChar( CP_ACP, 0, file, -1, widebuf, VSIZE(widebuf) );
    hinst = LoadLibrary( widebuf );
    
    if ( CUR_DLL_VERSION != getDLLVersion( hinst ) ) {
        FreeLibrary( hinst );
        hinst = NULL;
    }

    return hinst;
} /* ceLoadResFile */

void
ceCloseResFile( HINSTANCE inst )
{
    XP_ASSERT( !!inst );
    FreeLibrary( inst );
}

#ifdef LOADSTRING_BROKEN
typedef struct _ResStrEntry {
    union {
        XP_UCHAR nstr[1];
        wchar_t wstr[1];
    } u;
} ResStrEntry;

typedef struct _ResStrStorage {
    ResStrEntry* entries[CE_LAST_RES_ID - CE_FIRST_RES_ID + 1];
#ifdef DEBUG
    XP_U16 nUsed;
#endif
} ResStrStorage;

static const ResStrEntry*
getEntry( CEAppGlobals* globals, XP_U16 resID, XP_Bool isWide )
{
    ResStrStorage* storage = (ResStrStorage*)globals->resStrStorage;
    ResStrEntry* entry;
    XP_U16 index;

    XP_ASSERT( resID >= CE_FIRST_RES_ID && resID <= CE_LAST_RES_ID );
    index = CE_LAST_RES_ID - resID;
    XP_ASSERT( index < VSIZE(storage->entries) );

    if ( !storage ) {
        XP_ASSERT( !globals->exiting );
        storage = XP_MALLOC( globals->mpool, sizeof( *storage ) );
        XP_MEMSET( storage, 0, sizeof(*storage) );
        globals->resStrStorage = storage;
    }

    entry = storage->entries[index];
    if ( !entry ) {
        wchar_t wbuf[265];
        XP_U16 len;
        LoadString( globals->locInst, resID, wbuf, VSIZE(wbuf) );
        if ( isWide ) {
            len = wcslen( wbuf );
            entry = (ResStrEntry*)XP_MALLOC( globals->mpool, 
                                             (len*sizeof(wchar_t))
                                             + sizeof(*entry) );
            wcscpy( entry->u.wstr, wbuf );
        } else {
            XP_UCHAR nbuf[265];
            (void)WideCharToMultiByte( CP_UTF8, 0, wbuf, -1,
                                       nbuf, VSIZE(nbuf), NULL, NULL );
            len = XP_STRLEN( nbuf );
            entry = (ResStrEntry*)XP_MALLOC( globals->mpool, 
                                             len + 1 + sizeof(*entry) );
            XP_STRNCPY( entry->u.nstr, nbuf, len + 1 );
        }

        storage->entries[index] = entry;
#ifdef DEBUG
        ++storage->nUsed;
#endif
    }

    return entry;
} /* getEntry */
#endif

const XP_UCHAR* 
ceGetResString( CEAppGlobals* globals, XP_U16 resID )
{
#ifdef LOADSTRING_BROKEN
    const ResStrEntry* entry = getEntry( globals, resID, XP_FALSE );   
    return entry->u.nstr;
#else
    /* Docs say that you can call LoadString with 0 as the length and it'll
       return a read-only ptr to the text within the resource, but I'm getting
       a ptr to wide chars back the resource text being multibyte.  I swear
       I've seen it work, though, so might be a res file formatting thing or a
       param to the res compiler.  Need to investigate.  Until I do, the above
       caches local multibyte copies of the resources so the API can stay the
       same. */
    const XP_UCHAR* str = NULL;
    LoadString( globals->locInst, resID, (LPSTR)&str, 0 );
    return str;
#endif
}

const wchar_t*
ceGetResStringL( CEAppGlobals* globals, XP_U16 resID )
{
#ifdef LOADSTRING_BROKEN
    const ResStrEntry* entry = getEntry( globals, resID, XP_TRUE );   
    return entry->u.wstr;
#else
    /* Docs say that you can call LoadString with 0 as the length and it'll
       return a read-only ptr to the text within the resource, but I'm getting
       a ptr to wide chars back the resource text being multibyte.  I swear
       I've seen it work, though, so might be a res file formatting thing or a
       param to the res compiler.  Need to investigate.  Until I do, the above
       caches local multibyte copies of the resources so the API can stay the
       same. */
    const XP_UCHAR* str = NULL;
    LoadString( globals->locInst, resID, (LPSTR)&str, 0 );
    return str;
#endif
}

#ifdef LOADSTRING_BROKEN
void
ceFreeResStrings( CEAppGlobals* globals )
{
#ifdef DEBUG
    XP_U16 nUsed = 0;
#endif
    ResStrStorage* storage = (ResStrStorage*)globals->resStrStorage;
    if ( !!storage ) {
        XP_U16 ii;
        for ( ii = 0; ii < VSIZE(storage->entries); ++ii ) {
            ResStrEntry* entry = storage->entries[ii];
            if ( !!entry ) {
                XP_FREE( globals->mpool, entry );
#ifdef DEBUG
                ++nUsed;
#endif
            }
        }

        XP_ASSERT( nUsed == storage->nUsed );
        XP_FREE( globals->mpool, storage );
        globals->resStrStorage = NULL;
    }
#ifdef DEBUG
    XP_LOGF( "%s: %d of %d strings loaded and used", __func__, nUsed,
             VSIZE(storage->entries) );
#endif
}
#endif

typedef struct _DllSelState {
    CeDlgHdr dlgHdr;
    wchar_t wbuf[MAX_PATH];
    const wchar_t* curFile;

    wchar_t* names[8];
    wchar_t* files[8];
    XP_U16 nItems;
    XP_U16 initialSel;

    XP_U16 dllListID;
    XP_Bool inited;
    XP_Bool cancelled;
} DllSelState;

static void
copyWideStr( CEAppGlobals* XP_UNUSED_DBG(globals), const wchar_t* str, 
             wchar_t** loc )
{
    XP_U16 len = 1 + wcslen( str );
    *loc = XP_MALLOC( globals->mpool, len * sizeof(**loc) );
    wcscpy( *loc, str );
 }

static XP_U16
getDLLVersion( HINSTANCE hinst )
{
    XP_U16 version = 0;         /* illegal value */
    HRSRC rsrcH = FindResource( hinst, MAKEINTRESOURCE(ID_DLLVERS_RES),
                                TEXT("DLLV") );
    if ( !!rsrcH ) {
        HGLOBAL globH = LoadResource( hinst, rsrcH );
        version = *(XP_U16*)globH;
        DeleteObject( globH );
    }
    return version;
}

/* Iterate through .dll files listing the name of any that has one.  Pair with
 * file from which it came since that's what we'll return.
 */
static void
listDlls( DllSelState* state )
{
    HANDLE fileH;
    HWND hDlg = state->dlgHdr.hDlg;
    WIN32_FIND_DATA data;
    CEAppGlobals* globals = state->dlgHdr.globals;
    XP_U16 nItems = 0;
    XP_S16 selIndex = 0;        /* default to built-in */
    wchar_t name[64];

    LoadString( globals->hInst, IDS_LANGUAGE_NAME, name, VSIZE(name) );
    copyWideStr( globals, name, &state->names[nItems++] );
    (void)SendDlgItemMessage( hDlg, state->dllListID, ADDSTRING(globals),
                              0, (LPARAM)name );

    wchar_t path[MAX_PATH];
    ceGetExeDir( path, VSIZE(path) );
    wcscat( path, L"\\xwords4*.dll" );

    XP_MEMSET( &data, 0, sizeof(data) );
    fileH = FindFirstFile( path, &data );
    while ( fileH != INVALID_HANDLE_VALUE ) {

        HINSTANCE hinst = LoadLibrary( data.cFileName );
        if ( !!hinst ) {
            if ( CUR_DLL_VERSION != getDLLVersion( hinst ) ) {
                /* do nothing; wrong version (or just not our .dll) */
            } else if ( LoadString( hinst, IDS_LANGUAGE_NAME, 
                             name, VSIZE(name) ) ) {
                (void)SendDlgItemMessage( hDlg, state->dllListID, 
                                          ADDSTRING(globals),
                                          0, (LPARAM)name );
                copyWideStr( globals, name, &state->names[nItems] );
                copyWideStr( globals, data.cFileName, &state->files[nItems] );

                if ( !!state->curFile ) {
                    if ( !wcscmp( data.cFileName, state->curFile ) ) {
                        selIndex = nItems;
                    }
                }

                ++nItems;
            } else {
                XP_LOGF( "IDS_LANGUAGE_NAME not found in %ls",
                         data.cFileName );
            }
            FreeLibrary( hinst );
        } else {
            logLastError("LoadLibrary");
            XP_LOGF( "Unable to open" );
        }

        if ( nItems >= VSIZE(state->names) ) {
            break;
        } else if ( !FindNextFile( fileH, &data ) ) {
            XP_ASSERT( GetLastError() == ERROR_NO_MORE_FILES );
            break;
        }
    }
    SendDlgItemMessage( hDlg, state->dllListID, SETCURSEL(globals), 
                        selIndex, 0L );

    state->nItems = nItems;
    state->initialSel = selIndex;
} /* listDlls */

static void 
unlistDlls( DllSelState* state )
{
    XP_U16 ii;
#ifdef DEBUG
    CEAppGlobals* globals = state->dlgHdr.globals;
#endif
    for ( ii = 0; ii < state->nItems; ++ii ) {
        XP_ASSERT( ii == 0 || !!state->files[ii] );
        if ( ii > 0 ) {
            XP_FREE( globals->mpool, state->files[ii] );
        }
        XP_FREE( globals->mpool, state->names[ii] );
    }
}

static XP_Bool
getSelText( DllSelState* state )
{
    XP_Bool gotIt = XP_FALSE;
    HWND hDlg = state->dlgHdr.hDlg;
    CEAppGlobals* globals = state->dlgHdr.globals;

    XP_S16 sel = SendDlgItemMessage( hDlg, state->dllListID, 
                                     GETCURSEL(globals), 0, 0 );

    if ( sel >= 0 && sel != state->initialSel ) {
        gotIt = XP_TRUE;
        if ( sel > 0 ) {
            wcscpy( state->wbuf, state->files[sel] );
        }
    }
    return gotIt;
} /* getSelText */

LRESULT CALLBACK
DllSelDlg( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    DllSelState* state;
    BOOL result = FALSE;

    if ( message == WM_INITDIALOG ) {
        SetWindowLongPtr( hDlg, GWL_USERDATA, lParam );

        state = (DllSelState*)lParam;
        state->cancelled = XP_TRUE;

        state->dllListID = LB_IF_PPC( state->dlgHdr.globals, LOCALES_COMBO );

        ceDlgSetup( &state->dlgHdr, hDlg, DLG_STATE_NONE );

        ceDlgComboShowHide( &state->dlgHdr, LOCALES_COMBO );

        result = TRUE;
    } else {
        state = (DllSelState*)GetWindowLongPtr( hDlg, GWL_USERDATA );
        if ( !!state ) {
            if ( !state->inited ) {
                state->inited = XP_TRUE;
                listDlls( state );
            }
        
            if ( ceDoDlgHandle( &state->dlgHdr, message, wParam, lParam) ) {
                result = TRUE;
            } else if ( (WM_COMMAND == message) 
                        && (BN_CLICKED == HIWORD(wParam)) ) {
                switch( LOWORD(wParam) ) {
                case IDOK:
                    state->cancelled = !getSelText( state );
                    /* fallthrough */
                case IDCANCEL:
                    unlistDlls( state );
                    EndDialog( hDlg, LOWORD(wParam) );
                    result = TRUE;
                    break;
                }
            }
        }
    }

    return result;
} /* DllSelDlg */

/* ceChooseResFile: List all the available .rc files and return if user
 * chooses one different from the one passed in.
 */
XP_Bool
ceChooseResFile( HWND hwnd, CEAppGlobals* globals, const XP_UCHAR* curFileName,
                 XP_UCHAR* buf, XP_U16 bufLen )
{
    DllSelState state;
    wchar_t wCurFile[MAX_PATH];

    XP_MEMSET( &state, 0, sizeof(state) );

    state.dlgHdr.globals = globals;

    if ( !!curFileName ) {
        (void)MultiByteToWideChar( CP_ACP, 0, curFileName, -1,
                                   wCurFile, VSIZE(wCurFile) );
        state.curFile = wCurFile;
    }

    (void)DialogBoxParam( globals->locInst, (LPCTSTR)IDD_LOCALESDLG, 
                          hwnd, (DLGPROC)DllSelDlg, (long)&state );

    if ( !state.cancelled ) {
        (void)WideCharToMultiByte( CP_ACP, 0, state.wbuf, -1,
                                   buf, bufLen, NULL, NULL );
    }

    LOG_RETURNF( "%s", buf );
    return !state.cancelled;
}
