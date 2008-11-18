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
#include "cedebug.h"
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
        wstr = (wchar_t*)XP_MALLOC( giState->dlgHdr.globals->mpool, 
                                    len * sizeof(wstr[0]) );

        XP_MEMCPY( wstr, wPath, len*sizeof(wstr[0]) );
        if ( !giState->menuDicts ) {
            XP_ASSERT( giState->nMenuDicts == 0 );
            XP_ASSERT( giState->capMenuDicts == 0 );
            giState->capMenuDicts = MENUDICTS_INCR;
            giState->menuDicts
                = (wchar_t**)XP_MALLOC( giState->dlgHdr.globals->mpool, 
                                        giState->capMenuDicts 
                                        * sizeof(giState->menuDicts[0]) );
        } else if ( giState->nMenuDicts == giState->capMenuDicts ) {
            giState->capMenuDicts += MENUDICTS_INCR;
            giState->menuDicts
                = (wchar_t**)XP_REALLOC( giState->dlgHdr.globals->mpool, 
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
    CEAppGlobals* globals = giState->dlgHdr.globals;

    /* insert the short names in the menu */
    for ( i = 0; i < nMenuDicts; ++i ) {
        wchar_t* wPath = giState->menuDicts[i];
        shortname = wbname( shortPath, sizeof(shortPath), wPath );
        SendDlgItemMessage( giState->dlgHdr.hDlg, giState->dictListId, 
                            ADDSTRING(globals), 0, (long)shortname );

        if ( giState->newDictName[0] != 0 && sel == 0 ) {
            XP_UCHAR buf[CE_MAX_PATH_LEN+1];
            WideCharToMultiByte( CP_ACP, 0, wPath, -1, buf, sizeof(buf),
                                 NULL, NULL );
            if ( 0 == XP_STRCMP( buf, giState->newDictName ) ) {
                sel = i;
            }
        }
    }

    SendDlgItemMessage( giState->dlgHdr.hDlg, giState->dictListId,
                        SETCURSEL(globals), sel, 0L );
} /* addDictsToMenu */

static void
cleanupGameInfoState( GameInfoState* giState )
{
    if ( !!giState->menuDicts ) {
        XP_U16 nMenuDicts = giState->nMenuDicts;
        XP_U16 i;
        for ( i = 0; i < nMenuDicts; ++i ) {
            XP_FREE( giState->dlgHdr.globals->mpool, giState->menuDicts[i] );
        }
        XP_FREE( giState->dlgHdr.globals->mpool, giState->menuDicts );
        giState->menuDicts = NULL;
    }

    if ( !!giState->moveIds ) {
        XP_FREE( giState->dlgHdr.globals->mpool, giState->moveIds );
        giState->moveIds = NULL;
    }
} /* cleanupGameInfoState */

static void
loadFromGameInfo( GameInfoState* giState )
{
    XP_U16 i;
    CEAppGlobals* globals = giState->dlgHdr.globals;
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
        SendDlgItemMessage( giState->dlgHdr.hDlg, giState->nPlayersId,
                            ADDSTRING(globals), 0, 
                            (long)widebuf );
    }

    newg_load( giState->newGameCtx, gi );

#ifndef STUBBED_DICT
    if ( !!gi->dictName ) { 
        XP_MEMCPY( giState->newDictName, gi->dictName,
                   (XP_U16)XP_STRLEN(gi->dictName)+1 );
    }
    if ( giState->isNewGame ) {
        (void)ceLocateNDicts( globals, CE_MAXDICTS, addDictToState, giState );
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
        ceEnOrDisable( giState->dlgHdr.hDlg, giState->dictListId, XP_FALSE );
    }
} /* loadFromGameInfo */

static XP_Bool
stateToGameInfo( GameInfoState* giState )
{
    CEAppGlobals* globals = giState->dlgHdr.globals;
    CurGameInfo* gi = &globals->gameInfo;
    HWND hDlg = giState->dlgHdr.hDlg;
    XP_Bool timerOn;
    XP_Bool success = newg_store( giState->newGameCtx, gi, XP_TRUE );

    if ( success ) {

        /* dictionary */ {
            int sel;
            sel = SendDlgItemMessage( hDlg, giState->dictListId, 
                                      GETCURSEL(globals), 0, 0L );
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

    return success;
} /* stateToGameInfo */

#ifndef DM_RESETSCROLL
//http://www.nah6.com/~itsme/cvs-xdadevtools/itsutils/src/its_windows_message_list.txt
# define DM_RESETSCROLL 0x0402
#endif

static void
raiseForHiddenPlayers( GameInfoState* giState, XP_U16 nPlayers )
{
    HWND hDlg = giState->dlgHdr.hDlg;
    XP_U16 ii;
    XP_S16 moveY;

    if ( nPlayers != giState->prevNPlayers ) {
        if ( !giState->moveIds ) {
            XP_S16 ids[32];
            HWND child;
            RECT rect;
            XP_U16 playersBottom;

            ceGetItemRect( hDlg, NAME_EDIT4, &rect );
            playersBottom = rect.bottom;
            ceGetItemRect( hDlg, NAME_EDIT3, &rect );
            giState->playersSpacing = playersBottom - rect.bottom;

            for ( child = GetWindow( hDlg, GW_CHILD ), ii = 0;
                  !!child;
                  child = GetWindow( child, GW_HWNDNEXT ) ) {
                XP_S16 resID = GetDlgCtrlID( child );
                if ( resID > 0 ) {
                    ceGetItemRect( hDlg, resID, &rect );
                    if ( rect.top > playersBottom ) {
                        XP_ASSERT( ii < VSIZE(ids)-1 );
                        ids[ii] = resID;
                        ++ii;
                    }
                }
            }
            giState->moveIds = XP_MALLOC( giState->dlgHdr.globals->mpool, 
                                          sizeof(giState->moveIds[0]) * ii );
            XP_MEMCPY( giState->moveIds, ids, 
                       sizeof(giState->moveIds[0]) * ii );
            giState->nMoveIds = ii;
        }

        moveY = giState->playersSpacing * (nPlayers - giState->prevNPlayers);
        for ( ii = 0; ii < giState->nMoveIds; ++ii ) {
            ceMoveItem( hDlg, giState->moveIds[ii], 0, moveY );
        }
        giState->prevNPlayers = nPlayers;

#ifdef _WIN32_WCE
        if ( IS_SMARTPHONE(giState->dlgHdr.globals) ) {
            SendMessage( hDlg, DM_RESETSCROLL, (WPARAM)FALSE, (LPARAM)TRUE );
        }
#endif
    }
}

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
resIDForAttr( GameInfoState* state, NewGameAttr attr )
{
    XP_U16 resID = 0;
    switch( attr ) {
    case NG_ATTR_NPLAYERS:
        resID = state->nPlayersId;
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
    doForNWEnable( giState->dlgHdr.hDlg, resID, enable );
}

static void
ceEnableAttrProc( void* closure, NewGameAttr attr, XP_TriEnable enable )
{
    GameInfoState* giState = (GameInfoState*)closure;
    XP_U16 resID = resIDForAttr( giState, attr );
    doForNWEnable( giState->dlgHdr.hDlg, resID, enable );
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
        value.ng_bool = ceGetChecked( giState->dlgHdr.hDlg, resID );
        break;
    case NG_COL_NAME:
    case NG_COL_PASSWD:
        len = sizeof(txt);
        ceGetDlgItemText( giState->dlgHdr.hDlg, resID, txt, &len );
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
        ceSetDlgItemText( giState->dlgHdr.hDlg, resID, cp );
        break;
#ifndef XWFEATURE_STANDALONE_ONLY
    case NG_COL_REMOTE:
#endif
    case NG_COL_ROBOT:
        ceSetChecked( giState->dlgHdr.hDlg, resID, value.ng_bool );
        break;
    default:
        XP_ASSERT(0);
    }
} /* ceSetColProc */

static void 
ceSetAttrProc(void* closure, NewGameAttr attr, const NGValue value )
{
    GameInfoState* giState = (GameInfoState*)closure;
    XP_U16 resID = resIDForAttr( giState, attr );
    CEAppGlobals* globals = giState->dlgHdr.globals;

    switch ( attr ) {
    case NG_ATTR_NPLAYERS:
        SendDlgItemMessage( giState->dlgHdr.hDlg, resID, 
                            SETCURSEL(globals), 
                            value.ng_u16 - 1, 0L );
        raiseForHiddenPlayers( giState, value.ng_u16 );
        break;
#ifndef XWFEATURE_STANDALONE_ONLY
    case NG_ATTR_ROLE:
        SendDlgItemMessage( giState->dlgHdr.hDlg, resID, SETCURSEL(globals), 
                            value.ng_role, 0L );
        break;
#endif
    case NG_ATTR_NPLAYHEADER:
        ceSetDlgItemText( giState->dlgHdr.hDlg, resID, value.ng_cp );
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
    return player;
}

static void
handleColChecked( GameInfoState* giState, XP_U16 id, XP_U16 base )
{
    NGValue value;
    XP_U16 player = playerFromID( id, base );

    value.ng_bool = ceGetChecked( giState->dlgHdr.hDlg, id );

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

static void
checkUpdateCombo( GameInfoState* giState, XP_U16 id )
{
    if ( (id == giState->nPlayersId)
         && giState->isNewGame ) {  /* ignore if in info mode */
        NGValue value;
        XP_U16 nPlayers =  1 + (XP_U16)
            SendDlgItemMessage( giState->dlgHdr.hDlg, id,
                                GETCURSEL(giState->dlgHdr.globals), 0, 0L);
        value.ng_u16 = nPlayers;
        XP_ASSERT( !!giState->newGameCtx );
        newg_attrChanged( giState->newGameCtx, 
                          NG_ATTR_NPLAYERS, value );

        raiseForHiddenPlayers( giState, nPlayers );
    }
} /* checkUpdateCombo */

LRESULT CALLBACK
GameInfo(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    CEAppGlobals* globals;
    XP_U16 id;
    GameInfoState* giState;
    LRESULT result = FALSE;

/*     XP_LOGF( "%s: %s(%d)", __func__, messageToStr( message ), message ); */

    if ( message == WM_INITDIALOG ) {
        SetWindowLongPtr( hDlg, GWL_USERDATA, lParam );
        giState = (GameInfoState*)lParam;
        globals = giState->dlgHdr.globals;

        giState->nPlayersId = LB_IF_PPC(globals,IDC_NPLAYERSCOMBO);
        giState->dictListId = LB_IF_PPC(globals,IDC_DICTLIST);
        giState->prevNPlayers = MAX_NUM_PLAYERS;

        ceDlgSetup( &giState->dlgHdr, hDlg, DLG_STATE_TRAPBACK );
        ceDlgComboShowHide( &giState->dlgHdr, IDC_NPLAYERSCOMBO ); 
        ceDlgComboShowHide( &giState->dlgHdr, IDC_DICTLIST );

        giState->newGameCtx = newg_make( MPPARM(globals->mpool)
                                         giState->isNewGame,
                                         &globals->util,
                                         ceEnableColProc, 
                                         ceEnableAttrProc, 
                                         ceGetColProc,
                                         ceSetColProc,
                                         ceSetAttrProc,
                                         giState );

        loadFromGameInfo( giState );
        loadStateFromCurPrefs( globals, &globals->appPrefs, &globals->gameInfo,
                               &giState->prefsPrefs );

        if ( giState->isNewGame ) {
            (void)SetWindowText( hDlg, L"New game" );
        }

        result = TRUE;

    } else {
        giState = (GameInfoState*)GetWindowLongPtr( hDlg, GWL_USERDATA );
        if ( !!giState ) {
            globals = giState->dlgHdr.globals;

            XP_ASSERT( hDlg == giState->dlgHdr.hDlg );
            result = ceDoDlgHandle( &giState->dlgHdr, message, wParam, lParam );
            if ( !result ) {
                switch (message) {

#ifdef OWNERDRAW_JUGGLE
                case WM_DRAWITEM:   /* for BS_OWNERDRAW style */
                    ceDrawIconButton( globals, (DRAWITEMSTRUCT*)lParam );
                    result = TRUE;
                    break;
#endif

                case WM_NOTIFY:
                    if ( !!giState->newGameCtx ) {
                        checkUpdateCombo( giState, LOWORD(wParam)-1 );
                    }
                    break;

                case WM_COMMAND:
                    result = TRUE;
                    id = LOWORD(wParam);
                    if ( id == giState->nPlayersId ) {
                        if ( HIWORD(wParam) == CBN_SELCHANGE ) {
                            checkUpdateCombo( giState, id );
                        }
                    } else {
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
                            /* Juggle vs switch.  On Win32, updates are
                               coalesced so you don't see anything on screen
                               if you change a field then change it back.  In
                               terms of messages, all we see here is a
                               WM_CTLCOLOREDIT for each field being changed.
                               If I post a custom event here, it comes in
                               *before* the WM_CTLCOLOREDIT events.  Short of
                               a timer, which starts a race with the user, I
                               see no way to get notified after the drawing's
                               done.  So for now, we switch rather than
                               juggle: call juggle until something actually
                               happens. */
                            while ( !newg_juggle( giState->newGameCtx ) ) {
                            }
                            break;

                        case OPTIONS_BUTTON:
                            handlePrefsButton( hDlg, globals, giState );
                            break;

                        case IDOK:
                            if ( !stateToGameInfo( giState ) ) {
                                break;
                            }
                        case IDCANCEL:
                            EndDialog(hDlg, id);
                            giState->userCancelled = id == IDCANCEL;
                            cleanupGameInfoState( giState );
                            newg_destroy( giState->newGameCtx );
                            giState->newGameCtx = NULL;
                        }
                        break;
                    default:
                        result = FALSE;
                    }
                }
            }
        }
    }

    return result;
} /* GameInfo */
