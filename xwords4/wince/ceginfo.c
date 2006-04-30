/* -*-mode: C; fill-column: 77; c-basic-offset: 4; -*- */
/* 
 * Copyright 2002-2006 by Eric House (xwords@eehouse.org).  All rights
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

#include "ceginfo.h"
#include "cemain.h"
#include "ceutil.h"
#include "cedict.h"
#include "cecondlg.h"
#include "strutils.h"

#define NUM_COLS 4
#define MENUDICTS_INCR 16

#if 0
static XP_U16
ceCountLocalIn( HWND hDlg, XP_U16 nPlayers )
{
    XP_U16 nLocal = 0;
    XP_U16 i;

    for ( i = 0; i < nPlayers; ++i ) {
        if ( !ceGetChecked( hDlg, REMOTE_CHECK1 + (i * NUM_COLS) ) ) {
            ++nLocal;
        }
    }

    return nLocal;
} /* ceCountLocalIn */
#endif

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
addDictToState( const wchar_t* wPath, XP_U16 index, void* ctxt )
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
    wchar_t widebuf[32];
#ifndef XWFEATURE_STANDALONE_ONLY
    wchar_t* roles[] = { L"Standalone", L"Host", L"Guest" };
#endif
    XP_UCHAR* str;

    giState->curServerHilite = gi->serverRole;

    for ( i = 0; i < MAX_NUM_PLAYERS; ++i ) {
        LocalPlayer* lp = &gi->players[i];
        XP_U16 resID;
        XP_U16 idToCheck;

        /* set the robot checkbox */
        resID = ROBOT_CHECK1 + (NUM_COLS*i);
        idToCheck = lp->isRobot? resID : 0;
        CheckRadioButton( hDlg, resID, resID, idToCheck );

#ifndef XWFEATURE_STANDALONE_ONLY
        /* set the remote checkbox */
        resID = REMOTE_CHECK1 + (NUM_COLS*i);
        idToCheck = lp->isLocal? 0 : resID;
        CheckRadioButton( hDlg, resID, resID, idToCheck );
#endif

        /* set the player name */
        if ( lp->name != NULL ) {
            resID = NAME_EDIT1 + (NUM_COLS*i);
            ceSetDlgItemText( hDlg, resID, lp->name );
        }

        /* set the password, if any */

        /* put a string in the moronic combobox */
        swprintf( widebuf, L"%d", i + 1 );

        SendDlgItemMessage( hDlg, IDC_NPLAYERSCOMBO, CB_ADDSTRING, 0, 
                            (long)widebuf );
    }

    /* set the player num box */
    SendDlgItemMessage( hDlg, IDC_NPLAYERSCOMBO, CB_SETCURSEL, 
                        gi->nPlayers-1, 0L );

#ifndef XWFEATURE_STANDALONE_ONLY
    for ( i = 0; i < (sizeof(roles)/sizeof(roles[0])); ++i ) {
        SendDlgItemMessage( hDlg, IDC_ROLECOMBO, CB_ADDSTRING, 0, 
                            (long)roles[i] );
    }
    SendDlgItemMessage( hDlg, IDC_ROLECOMBO, CB_SETCURSEL, 
                        giState->curServerHilite, 0L );
#endif

#ifndef STUBBED_DICT
    if ( !!gi->dictName ) { 
        XP_LOGF( "%s: copying %s to giState->newDictName",
                 __FUNCTION__, gi->dictName );
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
                             wPath, sizeof(wPath)/sizeof(wPath[0]) );
        (void)addDictToState( wPath, 0, giState );
    }
    addDictsToMenu( giState );
#endif

    if ( !giState->isNewGame ) {
        XP_U16 disableIDs[] = { IDC_NPLAYERSCOMBO, 
                                IDC_ROLECOMBO,
                                IDC_DICTCOMBO};
        XP_U16 i;
        for( i = 0; i < sizeof(disableIDs)/sizeof(disableIDs[0]); ++i ) {
            ceEnOrDisable( hDlg, disableIDs[i], XP_FALSE );
        }
    }
} /* loadFromGameInfo */

static void
drawRow( HWND hDlg, XP_U16 rowN, XP_Bool showLine, XP_Bool isServer )
{
    XP_U16 offset = NUM_COLS * rowN;

    ceShowOrHide( hDlg, (XP_U16)(REMOTE_CHECK1 + offset), 
                  showLine && isServer );

    /* if it's a server and remote is checked, we show nothing more */
    if ( isServer && ceGetChecked( hDlg, (XP_U16)(REMOTE_CHECK1 + offset) ) ) {
        showLine = XP_FALSE;
    }

    ceShowOrHide( hDlg, (XP_U16)(NAME_EDIT1 + offset), showLine );

    ceShowOrHide( hDlg, (XP_U16)(ROBOT_CHECK1 + offset), showLine );

    showLine = showLine && !ceGetChecked( hDlg, 
                                          (XP_U16)(ROBOT_CHECK1 + offset) );
    ceShowOrHide( hDlg, (XP_U16)(PASS_EDIT1 + offset), showLine );
} /* drawRow */

/* Make sure that there are enough non-remote players to draw the number
 * we've been asked to draw.  At this point I'm not changing the actual
 * values in the widgets.  Is that ok? PENDING
 */
static void
countAndSetRemote( HWND hDlg, XP_U16 nPlayers, XP_Bool counterWins, 
                   XP_Bool* isRemote )
{
    XP_U16 i;
    XP_U16 nLocal = 0;

    for ( i = 0; i < MAX_NUM_PLAYERS; ++i ) {
        XP_Bool remote = 
            ceGetChecked( hDlg, (XP_U16)(REMOTE_CHECK1 + (i*MAX_COLS)) );
        isRemote[i] = remote;
        if ( !remote ) {
            ++nLocal;
        }
    }
    
    if ( counterWins ) {
        XP_U16 nToChange = nPlayers - nLocal;
        for ( i = 0; nToChange > 0 && i < MAX_NUM_PLAYERS; ++i ) {
            if ( isRemote[i] ) {
                isRemote[i] = XP_FALSE;
                --nToChange;
            }
        }
    }
} /* countAndSetRemote */

/* ceAdjustVisibility
 *
 * Called after any change to an interdependent widget, goes through and puts
 * all in sync, by show/hiding and by changing values of things like the
 * remote checkboxes and the player count combo.
 *
 * Param counterWins governs the syncing.  If true, it means that if there's
 * a conflict between the number of lines visible and the counter, the number
 * visible must be adjusted.  If false, then the counter must be adjusted to
 * match the number of lines.
 *
 * The number changes as the ROLE changes.  In particular, if we switch from
 * SERVER to CLIENT, and there are players set to remote, then the count will
 * change since we've changed what players should be counted (assuming not
 * all are local).
 */
static XP_Bool
ceAdjustVisibility( HWND hDlg, GameInfoState* giState, XP_Bool counterWins )
{
    XP_Bool result;
    Connectedness serverRole = (Connectedness)
        SendDlgItemMessage( hDlg, IDC_ROLECOMBO, CB_GETCURSEL, 0, 0L );
    XP_U16 nToDraw = MAX_NUM_PLAYERS;
    XP_U16 nDrawn = 0;
    XP_U16 row;
    XP_Bool isRemote[MAX_NUM_PLAYERS];
    XP_U16 counterValue = 1 + (XP_U16)SendDlgItemMessage( hDlg, 
                                                          IDC_NPLAYERSCOMBO,
                                                          CB_GETCURSEL, 0, 0L );

    counterWins = XP_TRUE;      /* test */

    countAndSetRemote( hDlg, counterValue, counterWins, isRemote );

    if ( counterWins ) {
        nToDraw = counterValue;
        XP_DEBUGF( "drawing %d rows", nToDraw );
    } else {

    }

    ceShowOrHide( hDlg, IDC_REMOTE_LABEL, serverRole == SERVER_ISSERVER );
    if ( serverRole == SERVER_ISCLIENT ) {
        ceShowOrHide( hDlg, IDC_TOTAL_LABEL, XP_FALSE );
        ceShowOrHide( hDlg, IDC_LOCALP_LABEL, XP_TRUE );
    } else {
        ceShowOrHide( hDlg, IDC_LOCALP_LABEL, XP_FALSE );
        ceShowOrHide( hDlg, IDC_TOTAL_LABEL, XP_TRUE );
    }

    for ( row = 0; row < MAX_NUM_PLAYERS; ++row ) {

        XP_Bool drawIt;
        /* for each line, if we're a client and it's remote, skip it.  If
           it's not already visible, don't change it */

        XP_ASSERT( row < MAX_NUM_PLAYERS );
        
        if ( serverRole == SERVER_ISCLIENT ) {
            drawIt = !isRemote[row];
        } else if ( serverRole == SERVER_STANDALONE ) {
            drawIt = XP_TRUE;
        } else {
            drawIt = XP_TRUE;
        }

        if ( drawIt ) {
            drawIt = nDrawn < nToDraw;
        }
        drawRow( hDlg, row, drawIt, 
                 serverRole == SERVER_ISSERVER );

        if ( drawIt ) {
            ++nDrawn;
        }
    }

    /* Change the counter if it's not assumed to be right */
    if ( !counterWins ) {
        (void)SendDlgItemMessage( hDlg, IDC_NPLAYERSCOMBO,
                                  CB_SETCURSEL, nDrawn - 1, 0L );
        result = XP_TRUE;
    } else {
        XP_ASSERT( nDrawn <= nToDraw );
        result = nDrawn == nToDraw;
    }

    return result;
} /* ceAdjustVisibility */

static void
getStringAndReplace( CEAppGlobals* globals, HWND hDlg, XP_U16 id, 
                     XP_UCHAR** sloc ) 
{
    XP_UCHAR cbuf[33];
    XP_U16 len;

    len = sizeof(cbuf);
    ceGetDlgItemText( hDlg, id, cbuf, &len );

    replaceStringIfDifferent( MPPARM(globals->mpool) sloc, cbuf );
} /* getStringAndReplace */

static void
stateToGameInfo( HWND hDlg, CEAppGlobals* globals, GameInfoState* giState )
{
    XP_U16 i;
    CurGameInfo* gi = &globals->gameInfo;
    XP_U16 nPlayers;
    XP_Bool timerOn;
    XP_U16 offset;
    Connectedness curServerHilite
        = (Connectedness )SendDlgItemMessage( hDlg, IDC_ROLECOMBO, 
                                              CB_GETCURSEL, 0, 0L );
    XP_ASSERT( curServerHilite == giState->curServerHilite );
    gi->serverRole = curServerHilite;

    nPlayers = 1 + (XP_U16)SendDlgItemMessage( hDlg, IDC_NPLAYERSCOMBO, 
                                               CB_GETCURSEL, 0, 0 );
    gi->nPlayers = (XP_U8)nPlayers;
    XP_DEBUGF( "Set nPlayers to %d", nPlayers );

    for ( i = 0, offset = 0; i < nPlayers; ++i, offset += NUM_COLS ) {
        XP_U16 id;
        XP_Bool checked;
        LocalPlayer* lp = &gi->players[i];

#ifndef XWFEATURE_STANDALONE_ONLY
        if ( curServerHilite == SERVER_ISSERVER ) {
            id = REMOTE_CHECK1 + offset;
            lp->isLocal = !ceGetChecked( hDlg, id );
        } else {
            lp->isLocal = XP_TRUE;
        }
#endif

        /* robot */
        id = ROBOT_CHECK1 + offset;
        checked = ceGetChecked( hDlg, id );
        lp->isRobot = checked;

        /* password */
        id = PASS_EDIT1 + offset;
        getStringAndReplace( globals, hDlg, id, &lp->password );

        /* name */
        id = NAME_EDIT1 + offset;
        getStringAndReplace( globals, hDlg, id, &lp->name );
    }

    /* dictionary */ {
        int sel;
        XP_LOGF( "%s: sending CB_GETCURSEL", __FUNCTION__ );
        sel = SendDlgItemMessage( hDlg, IDC_DICTCOMBO, CB_GETCURSEL, 0, 0L );
        XP_LOGF( "%s: sel came back %d", __FUNCTION__, sel );
        if ( sel >= 0 ) {
            WideCharToMultiByte( CP_ACP, 0, giState->menuDicts[sel], -1,
                                 giState->newDictName, 
                                 sizeof(giState->newDictName), NULL, NULL );
            XP_LOGF( "%s: text is %s", __FUNCTION__, giState->newDictName );
        }
        replaceStringIfDifferent( MPPARM(globals->mpool) &gi->dictName,
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

    LOG_RETURN_VOID();
} /* stateToGameInfo */

static void
handleOptionsButton( HWND hDlg, CEAppGlobals* globals, GameInfoState* giState )
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
} /* handleOptionsButton */

#ifndef XWFEATURE_STANDALONE_ONLY
static void
handleConnOptionsButton( HWND hDlg, CEAppGlobals* globals, 
                         GameInfoState* giState )
{
    CeConnDlgState state;

    if ( WrapConnsDlg( hDlg, globals, &giState->prefsPrefs.addrRec, &state ) ) {
        XP_MEMCPY( &giState->prefsPrefs.addrRec, &state.addrRec, 
                   sizeof(giState->prefsPrefs.addrRec) );
        giState->addrChanged = XP_TRUE;
    }
}
#endif

/* playersFollowCounts:
 * Force the data on players into sync with the counts.  This is really only
 * an issue if a local/remote change has happened.  Meant to be called after
 * the count has been changed.
 *
 * If the current role is LOCAL, then count should match the number of
 * players marked local.  If necessary, flip players to LOCAL to match the
 * count.  In any case, activate or deactivate to match the count.
 */
#if 0
static void
playersFollowCounts( HWND hDlg, GameInfoState* giState )
{
    Connectedness curServerHilite
        = (Connectedness )SendDlgItemMessage( hDlg, IDC_ROLECOMBO, 
                                              CB_GETCURSEL, 0, 0L );
    XP_U16 nPlayers = (XP_U16)SendDlgItemMessage( hDlg, IDC_NPLAYERSCOMBO, 
                                                  CB_GETCURSEL, 0, 0L );

    if ( curServerHilite == SERVER_ISCLIENT ) {
        XP_U16 nLocal = countLocalIn( hDlg, nPlayers );

        while ( nLocal < nPlayers ) {
            XP_U16 i;
            for ( i = 0; i < nPlayers; ++i ) {
                
            }
        }


        XP_DEBUGF( "need to check" );
    }
} /* playersFollowCounts */
#endif

LRESULT CALLBACK
GameInfo(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    CEAppGlobals* globals;
    XP_U16 id;
    GameInfoState* giState;
#ifndef XWFEATURE_STANDALONE_ONLY
    XP_Bool on;
#endif

    if ( message == WM_INITDIALOG ) {

        SetWindowLong( hDlg, GWL_USERDATA, lParam );
        giState = (GameInfoState*)lParam;
        giState->hDlg = hDlg;
        globals = giState->globals;

        loadFromGameInfo( hDlg, globals, giState );
        loadStateFromCurPrefs( globals, &globals->appPrefs, &globals->gameInfo, 
                               &giState->prefsPrefs );

        ceAdjustVisibility( hDlg, giState, XP_FALSE );

        if ( giState->isNewGame ) {
            (void)SetWindowText( hDlg, L"New game" );
        }
        return TRUE;

    } else {
        giState = (GameInfoState*)GetWindowLong( hDlg, GWL_USERDATA );
        if ( !!giState ) {
            globals = giState->globals;

            switch (message) {
            case WM_COMMAND:
                id = LOWORD(wParam);
                switch( id ) {

                case ROBOT_CHECK1:
                case ROBOT_CHECK2:
                case ROBOT_CHECK3:
                case ROBOT_CHECK4:
                    ceAdjustVisibility( hDlg, giState, XP_TRUE );
                    break;

#ifndef XWFEATURE_STANDALONE_ONLY
                case REMOTE_CHECK1:
                case REMOTE_CHECK2:
                case REMOTE_CHECK3:
                case REMOTE_CHECK4:
                    XP_ASSERT( giState->curServerHilite == SERVER_ISSERVER );
                    on = ceGetChecked( hDlg, id );
                    ceAdjustVisibility( hDlg, giState, XP_FALSE );
                    break;
#endif

                case IDC_NPLAYERSCOMBO:
                    if ( HIWORD(wParam) == CBN_SELCHANGE ) {
                        if ( giState->isNewGame ) {   /* ignore if in info mode */
                            XP_U16 role;
                            XP_U16 sel;
                            sel = (XP_U16)SendDlgItemMessage( hDlg, 
                                                              IDC_NPLAYERSCOMBO,
                                                              CB_GETCURSEL, 
                                                              0, 0L);
                            ++sel;
                            role = (XP_U16)SendDlgItemMessage( hDlg, 
                                                               IDC_ROLECOMBO,
                                                               CB_GETCURSEL,
                                                               0, 0L);
                            ceAdjustVisibility( hDlg, giState, XP_TRUE );
                        }
                    }
                    break;

#ifndef XWFEATURE_STANDALONE_ONLY
                case IDC_ROLECOMBO:
                    if ( HIWORD(wParam) == CBN_SELCHANGE ) {
                        if ( giState->isNewGame ) {  /* ignore if in info mode */
                            XP_U16 sel;
                            sel = (XP_U16)SendDlgItemMessage( hDlg, IDC_ROLECOMBO,
                                                              CB_GETCURSEL, 0, 
                                                              0L);
                            giState->curServerHilite = (Connectedness)sel;
                            ceAdjustVisibility( hDlg, giState, XP_FALSE );

                            /* If we've switched to a state where we'll be
                               connecting */
                            if ( sel != SERVER_STANDALONE ) {
                                handleConnOptionsButton( hDlg, globals, giState );
                            }
                        }
                    }
                    break;
#endif
                case OPTIONS_BUTTON:
                    handleOptionsButton( hDlg, globals, giState );
                    break;

                case IDOK:
                    stateToGameInfo( hDlg, globals, giState );
                case IDCANCEL:
                    EndDialog(hDlg, id);
                    giState->userCancelled = id == IDCANCEL;
                    cleanupGameInfoState( giState );
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
