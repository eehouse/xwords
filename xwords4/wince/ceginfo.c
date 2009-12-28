/* -*-mode: C; fill-column: 77; c-basic-offset: 4; compile-command: "make TARGET_OS=wince DEBUG=TRUE"; -*- */
/* 
 * Copyright 2002-2009 by Eric House (xwords@eehouse.org).  All rights
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
#include "ceresstr.h"

#define NUM_COLS 4
#define MENUDICTS_INCR 16

typedef struct _GameInfoState {
    CeDlgHdr dlgHdr;
    NewGameCtx* newGameCtx;
    XP_UCHAR* newDictName;
    XP_U16 dictNameLen;

    XP_U16 capMenuDicts;
    XP_U16 nMenuDicts;
    wchar_t** menuDicts;
    XP_U16 nPlayersId;
    XP_U16 dictListId;
#ifndef XWFEATURE_STANDALONE_ONLY
    XP_U16 roleComboId;
    DeviceRole curRole;
#endif

    XP_Bool isNewGame;              /* newGame or GameInfo */
    XP_Bool popConnsDlg;
    XP_Bool userCancelled;          /* OUT param */

    /* For tracking when to move stuff up/down */
    XP_Bool juggleHidden;
    XP_Bool roleConfigHidden;
    XP_U16 juggleSpacing;
    XP_U16 configSpacing;

    GInfoResults results;
    CePrefsPrefs* prefsPrefs;

    /* Support for repositioning lower items based on num players */
    XP_U16* moveIds;
    XP_U16 nMoveIds;
    XP_U16 prevNPlayers;
    XP_U16 playersSpacing;
    
} GameInfoState;

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
    GameInfoState* state = (GameInfoState*)ctxt;
    /* Let's display only the short form, but save the whole path */
    wchar_t* wstr;
    XP_U16 len;
    XP_S16 loc;                 /* < 0 means skip it */

    loc = findInsertPoint( wPath, state->menuDicts, 
                           state->nMenuDicts );

    if ( loc >= 0 ) {
        /* make a copy of the long name */
        len = wcslen( wPath ) + 1;
        wstr = (wchar_t*)XP_MALLOC( state->dlgHdr.globals->mpool, 
                                    len * sizeof(wstr[0]) );

        XP_MEMCPY( wstr, wPath, len*sizeof(wstr[0]) );
        if ( !state->menuDicts ) {
            XP_ASSERT( state->nMenuDicts == 0 );
            XP_ASSERT( state->capMenuDicts == 0 );
            state->capMenuDicts = MENUDICTS_INCR;
            state->menuDicts
                = (wchar_t**)XP_MALLOC( state->dlgHdr.globals->mpool, 
                                        state->capMenuDicts 
                                        * sizeof(state->menuDicts[0]) );
        } else if ( state->nMenuDicts == state->capMenuDicts ) {
            state->capMenuDicts += MENUDICTS_INCR;
            state->menuDicts
                = (wchar_t**)XP_REALLOC( state->dlgHdr.globals->mpool, 
                                         state->menuDicts, 
                                         state->capMenuDicts 
                                         * sizeof(state->menuDicts[0]) );
        }

        if ( loc < state->nMenuDicts ) {
            XP_MEMMOVE( &state->menuDicts[loc+1], &state->menuDicts[loc],
                        (state->nMenuDicts - loc) 
                        * sizeof(state->menuDicts[0]) );
        }
        state->menuDicts[loc] = wstr;
        ++state->nMenuDicts;
    }

    return XP_FALSE;
} /* addDictToState */

static void
addDictsToMenu( GameInfoState* state )
{
    wchar_t* shortname;
    wchar_t shortPath[CE_MAX_PATH_LEN+1];
    XP_U16 i, nMenuDicts = state->nMenuDicts;
    XP_S16 sel = 0;
    CEAppGlobals* globals = state->dlgHdr.globals;

    /* insert the short names in the menu */
    for ( i = 0; i < nMenuDicts; ++i ) {
        wchar_t* wPath = state->menuDicts[i];
        shortname = wbname( shortPath, sizeof(shortPath), wPath );
        SendDlgItemMessage( state->dlgHdr.hDlg, state->dictListId, 
                            ADDSTRING(globals), 0, (long)shortname );

        if ( state->newDictName[0] != 0 && sel == 0 ) {
            XP_UCHAR buf[CE_MAX_PATH_LEN+1];
            WideCharToMultiByte( CP_ACP, 0, wPath, -1, buf, sizeof(buf),
                                 NULL, NULL );
            if ( 0 == XP_STRCMP( buf, state->newDictName ) ) {
                sel = i;
            }
        }
    }

    SendDlgItemMessage( state->dlgHdr.hDlg, state->dictListId,
                        SETCURSEL(globals), sel, 0L );
} /* addDictsToMenu */

static void
cleanupGameInfoState( GameInfoState* state )
{
    if ( !!state->menuDicts ) {
        XP_U16 nMenuDicts = state->nMenuDicts;
        XP_U16 i;
        for ( i = 0; i < nMenuDicts; ++i ) {
            XP_FREE( state->dlgHdr.globals->mpool, state->menuDicts[i] );
        }
        XP_FREE( state->dlgHdr.globals->mpool, state->menuDicts );
        state->menuDicts = NULL;
    }

    if ( !!state->moveIds ) {
        XP_FREE( state->dlgHdr.globals->mpool, state->moveIds );
        state->moveIds = NULL;
    }
} /* cleanupGameInfoState */

static void
loadFromGameInfo( GameInfoState* state )
{
    XP_U16 ii;
    CEAppGlobals* globals = state->dlgHdr.globals;
    CurGameInfo* gi = &globals->gameInfo;

#ifndef XWFEATURE_STANDALONE_ONLY
    XP_U16 role_ids[] = { IDS_ROLE_STANDALONE, IDS_ROLE_HOST, IDS_ROLE_GUEST };
    for ( ii = 0; ii < VSIZE(role_ids); ++ii ) {
        const wchar_t* wstr = ceGetResStringL( globals, role_ids[ii] );
        SendDlgItemMessage( state->dlgHdr.hDlg, state->roleComboId, 
                            ADDSTRING(globals), 0, (long)wstr );
    }
#endif

    for ( ii = 0; ii < MAX_NUM_PLAYERS; ++ii ) {
        wchar_t widebuf[8];
        /* put a string in the moronic combobox */
        swprintf( widebuf, L"%d", ii + 1 );
        SendDlgItemMessage( state->dlgHdr.hDlg, state->nPlayersId,
                            ADDSTRING(globals), 0, (long)widebuf );
    }

    newg_load( state->newGameCtx, gi );

#ifndef STUBBED_DICT
    if ( !!gi->dictName ) { 
        XP_SNPRINTF( state->newDictName, state->dictNameLen, "%s", 
                     gi->dictName );
    }
    if ( state->isNewGame ) {
        (void)ceLocateNDicts( globals, CE_MAXDICTS, addDictToState, state );
    } else {
        wchar_t wPath[CE_MAX_PATH_LEN+1];
        XP_ASSERT( gi->dictName[0] != '\0' );
        MultiByteToWideChar( CP_ACP, MB_PRECOMPOSED, gi->dictName, -1,
                             wPath, VSIZE(wPath) );
        (void)addDictToState( wPath, 0, state );
    }
    addDictsToMenu( state );
#endif

    if ( !state->isNewGame ) {
        ceEnOrDisable( state->dlgHdr.hDlg, state->dictListId, XP_FALSE );
    }
} /* loadFromGameInfo */

static XP_Bool
stateToGameInfo( GameInfoState* state )
{
    CEAppGlobals* globals = state->dlgHdr.globals;
    CurGameInfo* gi = &globals->gameInfo;
    HWND hDlg = state->dlgHdr.hDlg;
    XP_Bool timerOn;
    XP_Bool success = newg_store( state->newGameCtx, gi, XP_TRUE );

    if ( success ) {

        /* dictionary */ {
            int sel;
            sel = SendDlgItemMessage( hDlg, state->dictListId, 
                                      GETCURSEL(globals), 0, 0L );
            if ( sel >= 0 ) {
                WideCharToMultiByte( CP_ACP, 0, state->menuDicts[sel], -1,
                                     state->newDictName, state->dictNameLen, 
                                     NULL, NULL );
            }
            replaceStringIfDifferent( globals->mpool, &gi->dictName,
                                      state->newDictName );
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
        if ( state->results.prefsChanged ) {
            loadCurPrefsFromState( globals, &globals->appPrefs, gi, 
                                   state->prefsPrefs );
        } 
    }

    return success;
} /* stateToGameInfo */

static void
raiseForJuggle( GameInfoState* state, XP_Bool nowHidden )
{
    if ( nowHidden != state->juggleHidden ) {
        ceDlgMoveBelow( &state->dlgHdr, GIJUGGLE_BUTTON, 
                        state->juggleSpacing * (nowHidden? -1 : 1) );
        state->juggleHidden = nowHidden;
    }
}

#ifndef XWFEATURE_STANDALONE_ONLY
static void
raiseForRoleChange( GameInfoState* state, DeviceRole role )
{
    XP_Bool configHidden = role == SERVER_STANDALONE;
    if ( configHidden != state->roleConfigHidden ) {
        ceDlgMoveBelow( &state->dlgHdr, state->roleComboId, 
                        state->configSpacing * (configHidden? -1 : 1) );
        state->roleConfigHidden = configHidden;
    }
}
#endif

static void
raiseForHiddenPlayers( GameInfoState* state, XP_U16 nPlayers )
{
    if ( nPlayers != state->prevNPlayers ) {
        ceDlgMoveBelow( &state->dlgHdr, NAME_EDIT4, 
                        state->playersSpacing
                        * (nPlayers - state->prevNPlayers) );
        state->prevNPlayers = nPlayers;
    }
} /* raiseForHiddenPlayers */

static void
handlePrefsButton( HWND hDlg, CEAppGlobals* globals, GameInfoState* state )
{
    XP_Bool colorsChanged, langChanged;
    if ( WrapPrefsDialog( hDlg, globals, state->prefsPrefs,
                          state->isNewGame, &colorsChanged, &langChanged ) ) {
        state->results.prefsChanged = XP_TRUE;
        state->results.colorsChanged = colorsChanged;
        state->results.langChanged = langChanged;
        /* nothing to do until user finally does confirm the parent dialog */
    }
} /* handlePrefsButton */

#ifndef XWFEATURE_STANDALONE_ONLY
static XP_Bool
callConnsDlg( GameInfoState* state )
{
    XP_Bool connsComplete = XP_FALSE;
    /* maybe flag when this isn't changed?  No.  Check on "Ok" as tagged elsewhere. */
    if ( WrapConnsDlg( state->dlgHdr.hDlg, state->dlgHdr.globals, 
                       &state->prefsPrefs->addrRec, 
                       &state->prefsPrefs->addrRec, state->curRole,
                       state->isNewGame,
                       &connsComplete ) ) {
        state->results.addrChanged = XP_TRUE;
    }
    return connsComplete;
}

static void
handleConnOptionsButton( GameInfoState* state )
{
    HWND hDlg = state->dlgHdr.hDlg;
    CEAppGlobals* globals = state->dlgHdr.globals;
    DeviceRole role;
    NGValue value;

    role = (DeviceRole)SendDlgItemMessage( hDlg, state->roleComboId,
                                           GETCURSEL(globals), 0, 0L);
    value.ng_role = state->curRole = role;
    if ( value.ng_role != globals->gameInfo.serverRole ) {
        state->results.addrChanged = XP_TRUE;
    }

    newg_attrChanged( state->newGameCtx, NG_ATTR_ROLE, value );
    raiseForRoleChange( state, role );
} /* handleConnOptionsButton */
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
#ifndef XWFEATURE_STANDALONE_ONLY
    case NG_ATTR_ROLE:
        resID = state->roleComboId;
        break;
    case NG_ATTR_CANCONFIG:
        resID = GIROLECONF_BUTTON;
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
    GameInfoState* state = (GameInfoState*)closure;
    XP_U16 resID = resIDForCol( player, col );
    doForNWEnable( state->dlgHdr.hDlg, resID, enable );
}

static void
ceEnableAttrProc( void* closure, NewGameAttr attr, XP_TriEnable enable )
{
    GameInfoState* state = (GameInfoState*)closure;
    XP_U16 resID = resIDForAttr( state, attr );
    doForNWEnable( state->dlgHdr.hDlg, resID, enable );
    if ( resID == GIJUGGLE_BUTTON ) {
        raiseForJuggle( state, enable == TRI_ENAB_HIDDEN );
    }
} /* ceEnableAttrProc */

static void 
ceGetColProc( void* closure, XP_U16 player, NewGameColumn col, 
              NgCpCallbk cpcb, const void* cpClosure )
{
    NGValue value;
    GameInfoState* state = (GameInfoState*)closure;
    XP_U16 resID = resIDForCol( player, col );
    XP_UCHAR txt[128];
    XP_U16 len;

    switch ( col ) {
#ifndef XWFEATURE_STANDALONE_ONLY
    case NG_COL_REMOTE:
#endif
    case NG_COL_ROBOT:
        value.ng_bool = ceGetChecked( state->dlgHdr.hDlg, resID );
        break;
    case NG_COL_NAME:
    case NG_COL_PASSWD:
        len = sizeof(txt);
        ceGetDlgItemText( state->dlgHdr.hDlg, resID, txt, &len );
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
    GameInfoState* state = (GameInfoState*)closure;
    XP_U16 resID = resIDForCol( player, col );
    const XP_UCHAR* cp;
    XP_UCHAR buf[32];

    switch( col ) {
    case NG_COL_PASSWD:
    case NG_COL_NAME:
        if ( NULL != value.ng_cp ) {
            cp = value.ng_cp;
        } else if ( col == NG_COL_NAME ) {
            const XP_UCHAR* str = ceGetResString( state->dlgHdr.globals, 
                                                  IDS_PLAYER_FORMAT );
            snprintf( buf, sizeof(buf), str, player + 1 );
            cp = buf;
        } else {
            cp = "";
        }
        ceSetDlgItemText( state->dlgHdr.hDlg, resID, cp );
        break;
#ifndef XWFEATURE_STANDALONE_ONLY
    case NG_COL_REMOTE:
#endif
    case NG_COL_ROBOT:
        ceSetChecked( state->dlgHdr.hDlg, resID, value.ng_bool );
        break;
    default:
        XP_ASSERT(0);
    }
} /* ceSetColProc */

static void 
ceSetAttrProc(void* closure, NewGameAttr attr, const NGValue value )
{
    GameInfoState* state = (GameInfoState*)closure;
    XP_U16 resID = resIDForAttr( state, attr );
    CEAppGlobals* globals = state->dlgHdr.globals;

    switch ( attr ) {
    case NG_ATTR_NPLAYERS:
        SendDlgItemMessage( state->dlgHdr.hDlg, resID, 
                            SETCURSEL(globals), 
                            value.ng_u16 - 1, 0L );
        raiseForHiddenPlayers( state, value.ng_u16 );
        break;
#ifndef XWFEATURE_STANDALONE_ONLY
    case NG_ATTR_ROLE:
        SendDlgItemMessage( state->dlgHdr.hDlg, resID, SETCURSEL(globals), 
                            value.ng_role, 0L );
        state->curRole = value.ng_role;
        raiseForRoleChange( state, value.ng_role );
        break;
#endif
    case NG_ATTR_NPLAYHEADER:
        ceSetDlgItemText( state->dlgHdr.hDlg, resID, value.ng_cp );
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
handleColChecked( GameInfoState* state, XP_U16 id, XP_U16 base )
{
    NGValue value;
    XP_U16 player = playerFromID( id, base );

    value.ng_bool = ceGetChecked( state->dlgHdr.hDlg, id );

    newg_colChanged( state->newGameCtx, player );
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
checkUpdateCombo( GameInfoState* state, XP_U16 id )
{
    HWND hDlg = state->dlgHdr.hDlg;

    if ( id == state->nPlayersId ) {
        if ( state->isNewGame ) {  /* ignore if in info mode */
            XP_S16 sel;
            XP_U16 nPlayers;
            NGValue value;

            sel = SendDlgItemMessage( hDlg, id, 
                                      GETCURSEL(state->dlgHdr.globals), 0, 0L);
            nPlayers =  1 + sel;
            value.ng_u16 = nPlayers;
            XP_ASSERT( !!state->newGameCtx );
            newg_attrChanged( state->newGameCtx, 
                              NG_ATTR_NPLAYERS, value );

            raiseForHiddenPlayers( state, nPlayers );
         }
    } else if ( id == state->roleComboId ) {
        XP_ASSERT( SERVER_STANDALONE == 0 );
        handleConnOptionsButton( state );
    }
} /* checkUpdateCombo */

static LRESULT CALLBACK
GameInfo( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
    CEAppGlobals* globals;
    XP_U16 id;
    GameInfoState* state;
    LRESULT result = FALSE;

/*     XP_LOGF( "%s: %s(%d)", __func__, messageToStr( message ), message ); */

    if ( message == WM_INITDIALOG ) {
        SetWindowLongPtr( hDlg, GWL_USERDATA, lParam );
        state = (GameInfoState*)lParam;
        globals = state->dlgHdr.globals;

        state->nPlayersId = LB_IF_PPC(globals,IDC_NPLAYERSCOMBO);
#ifndef XWFEATURE_STANDALONE_ONLY
        state->roleComboId = LB_IF_PPC(globals, IDC_ROLECOMBO);
#endif
        state->dictListId = LB_IF_PPC(globals,IDC_DICTLIST);
        state->prevNPlayers = MAX_NUM_PLAYERS;

        ceDlgSetup( &state->dlgHdr, hDlg, DLG_STATE_TRAPBACK );
        state->playersSpacing = ceDistanceBetween( hDlg, NAME_EDIT3, NAME_EDIT4 );
        state->juggleSpacing = ceDistanceBetween( state->dlgHdr.hDlg,
                                                  GIJUGGLE_BUTTON, 
                                                  IDC_DICTLABEL );

#ifndef XWFEATURE_STANDALONE_ONLY
        state->configSpacing = ceDistanceBetween( state->dlgHdr.hDlg,
                                                  GIROLECONF_BUTTON, 
                                                  IDC_TOTAL_LABEL );

        ceDlgComboShowHide( &state->dlgHdr, IDC_ROLECOMBO );
#endif
        ceDlgComboShowHide( &state->dlgHdr, IDC_NPLAYERSCOMBO ); 
        ceDlgComboShowHide( &state->dlgHdr, IDC_DICTLIST );

        state->newGameCtx = newg_make( MPPARM(globals->mpool)
                                         state->isNewGame,
                                         &globals->util,
                                         ceEnableColProc, 
                                         ceEnableAttrProc, 
                                         ceGetColProc,
                                         ceSetColProc,
                                         ceSetAttrProc,
                                         state );

        loadFromGameInfo( state );
        loadStateFromCurPrefs( globals, &globals->appPrefs, &globals->gameInfo,
                               state->prefsPrefs );

        if ( state->isNewGame ) {
            (void)SetWindowText( hDlg, ceGetResStringL( globals, 
                                                        IDS_NEW_GAME ) );
        }

        result = TRUE;

    } else {
        state = (GameInfoState*)GetWindowLongPtr( hDlg, GWL_USERDATA );
        if ( !!state ) {
            globals = state->dlgHdr.globals;

            XP_ASSERT( hDlg == state->dlgHdr.hDlg );
            result = ceDoDlgHandle( &state->dlgHdr, message, wParam, lParam );
            if ( !result ) {
                switch (message) {

#ifdef OWNERDRAW_JUGGLE
                case WM_DRAWITEM:   /* for BS_OWNERDRAW style */
                    ceDrawIconButton( globals, (DRAWITEMSTRUCT*)lParam );
                    result = TRUE;
                    break;
#endif

                case WM_PAINT:
                    if ( state->popConnsDlg ) {
                        state->popConnsDlg = XP_FALSE;
                        callConnsDlg( state );
                    }
                    break;

                case WM_NOTIFY:
                    if ( !!state->newGameCtx ) {
                        checkUpdateCombo( state, LOWORD(wParam)-1 );
                    }
                    break;

                case WM_COMMAND:
                    result = TRUE;
                    id = LOWORD(wParam);
                    if ( id == state->nPlayersId
#ifndef XWFEATURE_STANDALONE_ONLY
                         || id == state->roleComboId
#endif
                         ) {
                        if ( HIWORD(wParam) == CBN_SELCHANGE ) {
                            checkUpdateCombo( state, id );
                        }
                    } else {
                        switch( id ) {
                        case ROBOT_CHECK1:
                        case ROBOT_CHECK2:
                        case ROBOT_CHECK3:
                        case ROBOT_CHECK4:
                            handleColChecked( state, id, ROBOT_CHECK1 );
                            break;

#ifndef XWFEATURE_STANDALONE_ONLY
                        case REMOTE_CHECK1:
                        case REMOTE_CHECK2:
                        case REMOTE_CHECK3:
                        case REMOTE_CHECK4:
                            handleColChecked( state, id, REMOTE_CHECK1 );
                            break;

                        case IDC_ROLECOMBO:
                        case IDC_ROLECOMBO_PPC:
                            if ( HIWORD(wParam) == CBN_SELCHANGE ) {
                                /* If we've switched to a state where we'll be
                                   connecting */
                                handleConnOptionsButton( state );
                            }
                            break;
                        case GIROLECONF_BUTTON:
                            (void)callConnsDlg( state );
                            break;
#endif
                        case GIJUGGLE_BUTTON:
                            XP_ASSERT( state->isNewGame );
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
                            while ( !newg_juggle( state->newGameCtx ) ) {
                            }
                            break;

                        case OPTIONS_BUTTON:
                            handlePrefsButton( hDlg, globals, state );
                            break;

                        case IDOK: {
                            if ( state->curRole != SERVER_STANDALONE
                                 && !comms_checkComplete( 
                                     &state->prefsPrefs->addrRec )
                                 && !callConnsDlg( state ) ) {
                                break;
                            } else if ( !stateToGameInfo( state ) ) {
                                break;
                            }
                        }
                            /* FALLTHRU */
                        case IDCANCEL:
                            EndDialog(hDlg, id);
                            state->userCancelled = id == IDCANCEL;
                            cleanupGameInfoState( state );
                            newg_destroy( state->newGameCtx );
                            state->newGameCtx = NULL;
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

XP_Bool
WrapGameInfoDialog( CEAppGlobals* globals, GIShow showWhat,
                    CePrefsPrefs* prefsPrefs,
                    XP_UCHAR* dictName, XP_U16 dictNameLen,
                    GInfoResults* results )
{
    GameInfoState state;
    XP_U16 resIDs[48];
    
    XP_MEMSET( &state, 0, sizeof(state) );
    state.dlgHdr.globals = globals;
    state.dlgHdr.resIDs = resIDs;
    state.dlgHdr.nResIDs = VSIZE(resIDs);
    state.isNewGame = showWhat != GI_INFO_ONLY;
    state.popConnsDlg = showWhat == GI_GOTO_CONNS;
    state.prefsPrefs = prefsPrefs;
    state.newDictName = dictName;
    state.dictNameLen = dictNameLen;

    assertOnTop( globals->hWnd );
    DialogBoxParam( globals->locInst, (LPCTSTR)IDD_GAMEINFO, globals->hWnd,
                    (DLGPROC)GameInfo, (long)&state );

    if ( !state.userCancelled ) {
        XP_MEMCPY( results, &state.results, sizeof(*results) );
    }

    return !state.userCancelled;
} /* WrapGameInfoDialog */
