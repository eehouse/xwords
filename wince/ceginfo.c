/* -*-mode: C; fill-column: 77; c-basic-offset: 4; compile-command: "make TARGET_OS=wince DEBUG=TRUE"; -*- */
/* 
 * Copyright 2002-2008 by Eric House (xwords@eehouse.org).  All rights
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

#include <stdio.h>   /* swprintf */
#include "ceginfo.h"
#include "cemain.h"
#include "ceutil.h"
#include "cedict.h"
#include "cecondlg.h"
#include "strutils.h"

#define NUM_COLS 4
#define MENUDICTS_INCR 16

static XP_S16
findInsertPoint( const wchar_t* wPath, wchar_t** menuDicts, 
                 XP_U16 nMenuDicts )
{ 
    XP_S16 loc = 0;             /* simple case: nothing here */

    if ( nMenuDicts > 0 ) {
        wchar_t thisShortBuf[CE_MAX_PATH_LEN+1];
        wchar_t* thisShortName = wbname( thisShortBuf, sizeof(thisShortBuf), 
                                         wPath );

    /* If the short path doesn't already exist, find where it belongs.  This
       is wasteful if we're doing this a lot since the short path isn't
       cached. */
        for ( /* loc = 0*/; loc < nMenuDicts; ++loc ) {
            wchar_t oneShortBuf[CE_MAX_PATH_LEN+1];
            wchar_t* oneShortName = wbname( oneShortBuf, sizeof(oneShortBuf), 
                                            menuDicts[loc] );
            int diff = _wcsicmp( thisShortName, oneShortName );
            if ( diff > 0 ) {
                continue;
            } else if ( diff == 0 ) {
                loc = -1;
            }
            break;
        }
    }

    return loc;
} /* findInsertPoint */

static XP_Bool
addDictToState( const wchar_t* wPath, XP_U16 XP_UNUSED(index), void* ctxt )
{
    GameInfoState* giState = (GameInfoState*)ctxt;
    /* Let's display only the short form, but save the whole path */
    wchar_t* wstr;
    XP_U16 len;
    XP_S16 loc;                 /* < 0 means skip it */

    loc = findInsertPoint( wPath, giState->menuDicts, 
                           giState->nMenuDicts );

    if ( loc >= 0 ) {
        /* make a copy of the long name */
        len = wcslen( wPath ) + 1;
        wstr = (wchar_t*)XP_MALLOC( giState->globals->mpool, 
                                    len * sizeof(wstr[0]) );

        XP_MEMCPY( wstr, wPath, len*sizeof(wstr[0]) );
        if ( !giState->menuDicts ) {
            XP_ASSERT( giState->nMenuDicts == 0 );
            XP_ASSERT( giState->capMenuDicts == 0 );
            giState->capMenuDicts = MENUDICTS_INCR;
            giState->menuDicts
                = (wchar_t**)XP_MALLOC( giState->globals->mpool, 
                                        giState->capMenuDicts 
                                        * sizeof(giState->menuDicts[0]) );
        } else if ( giState->nMenuDicts == giState->capMenuDicts ) {
            giState->capMenuDicts += MENUDICTS_INCR;
            giState->menuDicts
                = (wchar_t**)XP_REALLOC( giState->globals->mpool, 
                                         giState->menuDicts, 
                                         giState->capMenuDicts 
                                         * sizeof(giState->menuDicts[0]) );
        }

        if ( loc < giState->nMenuDicts ) {
            XP_MEMMOVE( &giState->menuDicts[loc+1], &giState->menuDicts[loc],
                        (giState->nMenuDicts - loc) 
                        * sizeof(giState->menuDicts[0]) );
        }
        giState->menuDicts[loc] = wstr;
        ++giState->nMenuDicts;
    }

    return XP_FALSE;
} /* addDictToState */

static void
addDictsToMenu( GameInfoState* giState )
{
    wchar_t* shortname;
    wchar_t shortPath[CE_MAX_PATH_LEN+1];
    XP_U16 i, nMenuDicts = giState->nMenuDicts;
    XP_S16 sel = 0;

    /* insert the short names in the menu */
    for ( i = 0; i < nMenuDicts; ++i ) {
        wchar_t* wPath = giState->menuDicts[i];
        shortname = wbname( shortPath, sizeof(shortPath), wPath );
        SendDlgItemMessage( giState->hDlg, IDC_DICTCOMBO, CB_ADDSTRING, 0, 
                            (long)shortname );

        if ( giState->newDictName[0] != 0 && sel == 0 ) {
            XP_UCHAR buf[CE_MAX_PATH_LEN+1];
            WideCharToMultiByte( CP_ACP, 0, wPath, -1, buf, sizeof(buf),
                                 NULL, NULL );
            if ( 0 == XP_STRCMP( buf, giState->newDictName ) ) {
                sel = i;
            }
        }
    }

    SendDlgItemMessage( giState->hDlg, IDC_DICTCOMBO, CB_SETCURSEL, sel, 0L );
} /* addDictsToMenu */

static void
cleanupGameInfoState( GameInfoState* giState )
{
    if ( !!giState->menuDicts ) {
        XP_U16 nMenuDicts = giState->nMenuDicts;
        XP_U16 i;
        for ( i = 0; i < nMenuDicts; ++i ) {
            XP_FREE( giState->globals->mpool, giState->menuDicts[i] );
        }
        XP_FREE( giState->globals->mpool, giState->menuDicts );
        giState->menuDicts = NULL;
    }
} /* cleanupGameInfoState */

static void
loadFromGameInfo( HWND hDlg, CEAppGlobals* globals, GameInfoState* giState )
{
    XP_U16 i;
    CurGameInfo* gi = &globals->gameInfo;

#if defined XWFEATURE_RELAY || defined XWFEATURE_BLUETOOTH
    wchar_t* roles[] = { L"Standalone", L"Host", L"Guest" };
    for ( i = 0; i < VSIZE(roles); ++i ) {
        SendDlgItemMessage( hDlg, IDC_ROLECOMBO, CB_ADDSTRING, 0, 
                            (long)roles[i] );
    }
#endif

    for ( i = 0; i < MAX_NUM_PLAYERS; ++i ) {
        wchar_t widebuf[8];
        /* put a string in the moronic combobox */
        swprintf( widebuf, L"%d", i + 1 );
        SendDlgItemMessage( hDlg, IDC_NPLAYERSCOMBO, CB_ADDSTRING, 0, 
                            (long)widebuf );
    }

    newg_load( giState->newGameCtx, gi );

#ifndef STUBBED_DICT
    if ( !!gi->dictName ) { 
        XP_LOGF( "%s: copying %s to giState->newDictName",
                 __func__, gi->dictName );
        XP_MEMCPY( giState->newDictName, gi->dictName,
                   (XP_U16)XP_STRLEN(gi->dictName)+1 );
    }
    if ( giState->isNewGame ) {
        (void)ceLocateNDicts( MPPARM(globals->mpool) globals->hInst, 
                              CE_MAXDICTS, addDictToState, giState );
    } else {
        wchar_t wPath[CE_MAX_PATH_LEN+1];
        XP_ASSERT( gi->dictName[0] != '\0' );
        MultiByteToWideChar( CP_ACP, MB_PRECOMPOSED, gi->dictName, -1,
                             wPath, VSIZE(wPath) );
        (void)addDictToState( wPath, 0, giState );
    }
    addDictsToMenu( giState );
#endif

    if ( !giState->isNewGame ) {
        ceEnOrDisable( hDlg, IDC_DICTCOMBO, XP_FALSE );
    }
} /* loadFromGameInfo */

static XP_Bool
stateToGameInfo( HWND hDlg, CEAppGlobals* globals, GameInfoState* giState )
{
    CurGameInfo* gi = &globals->gameInfo;
    XP_Bool timerOn;
    XP_Bool success = newg_store( giState->newGameCtx, gi, XP_TRUE );

    if ( success ) {

        /* dictionary */ {
            int sel;
            sel = SendDlgItemMessage( hDlg, IDC_DICTCOMBO, CB_GETCURSEL, 
                                      0, 0L );
            if ( sel >= 0 ) {
                WideCharToMultiByte( CP_ACP, 0, giState->menuDicts[sel], -1,
                                     giState->newDictName, 
                                     sizeof(giState->newDictName), NULL, NULL );
            }
            replaceStringIfDifferent( globals->mpool, &gi->dictName,
                                      giState->newDictName );
        }

        /* timer */
        timerOn = ceGetChecked( hDlg, TIMER_CHECK );
        gi->timerEnabled = timerOn;
        if ( timerOn ) {
            XP_UCHAR numBuf[10];
            XP_U16 len = sizeof(numBuf);
            ceGetDlgItemText( hDlg, TIMER_EDIT, numBuf, &len );
            if ( len > 0 ) {
                XP_U16 num = atoi( numBuf );
                gi->gameSeconds = num * 60;
            }
        }
    
        /* preferences */
        if ( giState->prefsChanged ) {
            loadCurPrefsFromState( globals, &globals->appPrefs, gi, 
                                   &giState->prefsPrefs );
        } 
    }

    LOG_RETURNF( "%d", (int)success );
    return success;
} /* stateToGameInfo */

static void
handlePrefsButton( HWND hDlg, CEAppGlobals* globals, GameInfoState* giState )
{
    CePrefsDlgState state;

    /* need to keep my stuff in a temporary place and to read back out of it
       if launched a second time before the user's cancelled or not out of
       the calling dlg.*/

    if ( WrapPrefsDialog( hDlg, globals, &state, &giState->prefsPrefs,
                          giState->isNewGame ) ) {
        giState->prefsChanged = XP_TRUE;
        giState->colorsChanged = state.colorsChanged;
        /* nothing to do until user finally does confirm the parent dialog */
    }
} /* handlePrefsButton */

#if defined XWFEATURE_RELAY || defined XWFEATURE_BLUETOOTH
static void
handleConnOptionsButton( HWND hDlg, CEAppGlobals* globals,
                         DeviceRole role, GameInfoState* giState )
{
    CeConnDlgState state;

    if ( WrapConnsDlg( hDlg, globals, &giState->prefsPrefs.addrRec, 
                       role, &state ) ) {
        XP_MEMCPY( &giState->prefsPrefs.addrRec, &state.addrRec,
                   sizeof(giState->prefsPrefs.addrRec) );
        giState->addrChanged = XP_TRUE;
    }
}
#endif

static XP_U16 
resIDForCol( XP_U16 player, NewGameColumn col )
{
    XP_U16 resID = 0;
    switch ( col ) {
#ifndef XWFEATURE_STANDALONE_ONLY
    case NG_COL_REMOTE:
        resID = REMOTE_CHECK1;
        break;
#endif
    case NG_COL_ROBOT:
        resID = ROBOT_CHECK1;
        break;
    case NG_COL_NAME:
        resID = NAME_EDIT1;
        break;
    case NG_COL_PASSWD:
        resID = PASS_EDIT1;
        break;
    }
    XP_ASSERT( resID != 0 );
    return resID + ( player * NUM_COLS );
} /* resIDForCol */

static XP_U16 
resIDForAttr( NewGameAttr attr )
{
    XP_U16 resID = 0;
    switch( attr ) {
    case NG_ATTR_NPLAYERS:
        resID = IDC_NPLAYERSCOMBO;
        break;
#if defined XWFEATURE_RELAY || defined XWFEATURE_BLUETOOTH
    case NG_ATTR_ROLE:
        resID = IDC_ROLECOMBO;
        break;
    case NG_ATTR_REMHEADER:
        resID = IDC_REMOTE_LABEL;
        break;
#endif
    case NG_ATTR_NPLAYHEADER:
        resID = IDC_TOTAL_LABEL;
        break;
    case NG_ATTR_CANJUGGLE:
        resID = GIJUGGLE_BUTTON;
        break;
    default:
        break;
    }
    XP_ASSERT( resID != 0 );
    return resID;
} /* resIDForAttr */

static void
doForNWEnable( HWND hDlg, XP_U16 resID, XP_TriEnable enable )
{
    XP_Bool makeVisible = enable != TRI_ENAB_HIDDEN;
    ceShowOrHide( hDlg, resID, makeVisible );
    if ( makeVisible ) {
        ceEnOrDisable( hDlg, resID, enable == TRI_ENAB_ENABLED );
    }
} /* doForNWEnable */

static void
ceEnableColProc( void* closure, XP_U16 player, NewGameColumn col, 
                 XP_TriEnable enable )
{
    GameInfoState* giState = (GameInfoState*)closure;
    XP_U16 resID = resIDForCol( player, col );
    doForNWEnable( giState->hDlg, resID, enable );
}

static void
ceEnableAttrProc( void* closure, NewGameAttr attr, XP_TriEnable enable )
{
    GameInfoState* giState = (GameInfoState*)closure;
    XP_U16 resID = resIDForAttr( attr );
    doForNWEnable( giState->hDlg, resID, enable );
} /* ceEnableAttrProc */

static void 
ceGetColProc( void* closure, XP_U16 player, NewGameColumn col, 
              NgCpCallbk cpcb, const void* cpClosure )
{
    NGValue value;
    GameInfoState* giState = (GameInfoState*)closure;
    XP_U16 resID = resIDForCol( player, col );
    XP_UCHAR txt[128];
    XP_U16 len;

    switch ( col ) {
#ifndef XWFEATURE_STANDALONE_ONLY
    case NG_COL_REMOTE:
#endif
    case NG_COL_ROBOT:
        value.ng_bool = ceGetChecked( giState->hDlg, resID );
        break;
    case NG_COL_NAME:
    case NG_COL_PASSWD:
        len = sizeof(txt);
        ceGetDlgItemText( giState->hDlg, resID, txt, &len );
        value.ng_cp = &txt[0];
        break;
    default:
        XP_ASSERT(0);
    }

    (*cpcb)( value, cpClosure );
} /* ceGetColProc */

static void 
ceSetColProc( void* closure, XP_U16 player, NewGameColumn col, 
              const NGValue value )
{
    GameInfoState* giState = (GameInfoState*)closure;
    XP_U16 resID = resIDForCol( player, col );
    const XP_UCHAR* cp;

    switch( col ) {
    case NG_COL_PASSWD:
    case NG_COL_NAME:
        if ( NULL == value.ng_cp ) {
            cp = "";
        } else {
            cp = value.ng_cp;
        }
        ceSetDlgItemText( giState->hDlg, resID, cp );
        break;
#ifndef XWFEATURE_STANDALONE_ONLY
    case NG_COL_REMOTE:
#endif
    case NG_COL_ROBOT:
        ceSetChecked( giState->hDlg, resID, value.ng_bool );
        break;
    default:
        XP_ASSERT(0);
    }
}

static void 
ceSetAttrProc(void* closure, NewGameAttr attr, const NGValue value )
{
    GameInfoState* giState = (GameInfoState*)closure;
    XP_U16 resID = resIDForAttr( attr );

    LOG_FUNC();

    switch ( attr ) {
    case NG_ATTR_NPLAYERS:
        SendDlgItemMessage( giState->hDlg, resID, CB_SETCURSEL, 
                            value.ng_u16 - 1, 0L );
        break;
#ifndef XWFEATURE_STANDALONE_ONLY
    case NG_ATTR_ROLE:
        SendDlgItemMessage( giState->hDlg, resID, CB_SETCURSEL, 
                            value.ng_role, 0L );
        break;
#endif
    case NG_ATTR_NPLAYHEADER:
        ceSetDlgItemText( giState->hDlg, resID, value.ng_cp );
        break;
    default:
        break;
    }
} /* ceSetAttrProc */

static XP_U16
playerFromID( XP_U16 id, XP_U16 base )
{
    XP_U16 player;
    player = (id - base) / NUM_COLS;
/*     XP_LOGF( "%s: looks like row %d", __func__, player ); */
    return player;
}

static void
handleColChecked( GameInfoState* giState, XP_U16 id, XP_U16 base )
{
    NGValue value;
    XP_U16 player = playerFromID( id, base );

    value.ng_bool = ceGetChecked( giState->hDlg, id );

    newg_colChanged( giState->newGameCtx, player );
}

/* It's too much work at this point to get the icon button looking good,
 * e.g. properly greyed-out when disabled.  So I'm sticking with the "J".
 * Here's the code to start with if I get more ambitious.  Remember: the
 * GIJUGGLE_BUTTON needs to have the BS_OWNERDRAW attribute for this to work.
 */
#ifdef OWNERDRAW_JUGGLE
static void
ceDrawIconButton( CEAppGlobals* globals, DRAWITEMSTRUCT* dis )
{
    HBITMAP bmp = LoadBitmap( globals->hInst, 
                              MAKEINTRESOURCE(IDB_JUGGLEBUTTON) );
    if ( !!bmp ) {
        ceDrawBitmapInRect( dis->hDC, &dis->rcItem, bmp );
        DeleteObject( bmp );
    }
} /* ceDrawColorButton */
#endif

LRESULT CALLBACK
GameInfo(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    CEAppGlobals* globals;
    XP_U16 id;
    GameInfoState* giState;

    if ( message == WM_INITDIALOG ) {
        SetWindowLong( hDlg, GWL_USERDATA, lParam );
        giState = (GameInfoState*)lParam;
        giState->hDlg = hDlg;
        globals = giState->globals;

        ceStackButtonsRight( globals, hDlg );

        giState->newGameCtx = newg_make( MPPARM(globals->mpool)
                                         giState->isNewGame,
                                         &globals->util,
                                         ceEnableColProc, 
                                         ceEnableAttrProc, 
                                         ceGetColProc,
                                         ceSetColProc,
                                         ceSetAttrProc,
                                         giState );

        loadFromGameInfo( hDlg, globals, giState );
        loadStateFromCurPrefs( globals, &globals->appPrefs, &globals->gameInfo,
                               &giState->prefsPrefs );

        if ( giState->isNewGame ) {
            (void)SetWindowText( hDlg, L"New game" );
        }
        return TRUE;

    } else {
        giState = (GameInfoState*)GetWindowLong( hDlg, GWL_USERDATA );
        if ( !!giState ) {
            globals = giState->globals;

            switch (message) {

#ifdef OWNERDRAW_JUGGLE
            case WM_DRAWITEM:   /* for BS_OWNERDRAW style */
                ceDrawIconButton( globals, (DRAWITEMSTRUCT*)lParam );
                return TRUE;
#endif

            case WM_COMMAND:
                id = LOWORD(wParam);
                switch( id ) {

                case ROBOT_CHECK1:
                case ROBOT_CHECK2:
                case ROBOT_CHECK3:
                case ROBOT_CHECK4:
                    handleColChecked( giState, id, ROBOT_CHECK1 );
                    break;

#ifndef XWFEATURE_STANDALONE_ONLY
                case REMOTE_CHECK1:
                case REMOTE_CHECK2:
                case REMOTE_CHECK3:
                case REMOTE_CHECK4:
                    handleColChecked( giState, id, REMOTE_CHECK1 );
                    break;
#endif

                case IDC_NPLAYERSCOMBO:
                    if ( HIWORD(wParam) == CBN_SELCHANGE ) {
                        if ( giState->isNewGame ) {   /* ignore if in info
                                                         mode */
                            NGValue value;
                            value.ng_u16 = 1 + (XP_U16)
                                SendDlgItemMessage( hDlg, 
                                                    IDC_NPLAYERSCOMBO,
                                                    CB_GETCURSEL, 0, 0L);
                            newg_attrChanged( giState->newGameCtx, 
                                              NG_ATTR_NPLAYERS, value );
                        }
                    }
                    break;

#if defined XWFEATURE_RELAY || defined XWFEATURE_BLUETOOTH
                case IDC_ROLECOMBO:
                    if ( HIWORD(wParam) == CBN_SELCHANGE ) {
                        if ( giState->isNewGame ) {  /* ignore if in info
                                                        mode */
                            NGValue value;
                            value.ng_role = 
                                (DeviceRole)SendDlgItemMessage( hDlg, 
                                                            IDC_ROLECOMBO,
                                                            CB_GETCURSEL, 0, 
                                                            0L);
                            newg_attrChanged( giState->newGameCtx, 
                                              NG_ATTR_ROLE, value );
                            /* If we've switched to a state where we'll be
                               connecting */
                            if ( value.ng_role != SERVER_STANDALONE ) {
                                handleConnOptionsButton( hDlg, globals, 
                                                         value.ng_role, 
                                                         giState );
                            }
                        }
                    }
                    break;
#endif
                case GIJUGGLE_BUTTON:
                    XP_ASSERT( giState->isNewGame );
                    /* Juggle vs switch.  On Win32, updates are coalesced so
                       you don't see anything on screen if you change a field
                       then change it back.  In terms of messages, all we see
                       here is a WM_CTLCOLOREDIT for each field being
                       changed.  If I post a custom event here, it comes in
                       *before* the WM_CTLCOLOREDIT events.  Short of a
                       timer, which starts a race with the user, I see no way
                       to get notified after the drawing's done.  So for now,
                       we switch rather than juggle: call juggle until
                       something actually happens. */
                    while ( !newg_juggle( giState->newGameCtx ) ) {
                    }
                    break;

                case OPTIONS_BUTTON:
                    handlePrefsButton( hDlg, globals, giState );
                    break;

                case IDOK:
                    if ( !stateToGameInfo( hDlg, globals, giState ) ) {
                        break;
                    }
                case IDCANCEL:
                    EndDialog(hDlg, id);
                    giState->userCancelled = id == IDCANCEL;
                    cleanupGameInfoState( giState );
                    newg_destroy( giState->newGameCtx );
                    return TRUE;
                }
                break;
                /*     case WM_CLOSE: */
                /* 	EndDialog(hDlg, id); */
                /* 	return TRUE; */
                /*     default: */
                /* 	return DefWindowProc(hDlg, message, wParam, lParam); */
            }
        }
    }
    return FALSE;
} /* GameInfo */
