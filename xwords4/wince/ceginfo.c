/* -*-mode: C; fill-column: 77; c-basic-offset: 4; -*- */
/* 
 * Copyright 2002 by Eric House (fixin@peak.org).  All rights reserved.
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
#include "strutils.h"

#define NUM_COLS 4

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

        /* set the player name */
        resID = NAME_EDIT1 + (NUM_COLS*i);
        ceSetDlgItemText( hDlg, resID, lp->name );

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
    for ( i = 0; i < 3; ++i ) {
        SendDlgItemMessage( hDlg, IDC_ROLECOMBO, CB_ADDSTRING, 0, 
                            (long)roles[i] );
    }
    SendDlgItemMessage( hDlg, IDC_ROLECOMBO, CB_SETCURSEL, 
                        gi->serverRole, 0L );
#endif

    /* set the dictionary name */
    if ( !!gi->dictName ) { 
        str = bname( gi->dictName );
        XP_MEMCPY( giState->newDictName, gi->dictName, 
                   (XP_U16)XP_STRLEN(gi->dictName)+1 );

    } else if ( !!(str = ceLocateNthDict( MPPARM(globals->mpool) 0 ) ) ) {
        XP_MEMCPY( giState->newDictName, str, (XP_U16)XP_STRLEN(str)+1 );
        XP_FREE( globals->mpool, str );
        str = bname( giState->newDictName );

    } else {
#ifdef STUBBED_DICT
        /* assumption is there's no dict on the device */
        XP_ASSERT( !ceLocateNthDict( MPPARM(globals->mpool) 0 ) );
        str = "(Stub dict)";
#else
        str = "--pick--";
#endif
    }

    ceSetDlgItemFileName( hDlg, IDC_DICTBUTTON, str );

    if ( !giState->isNewGame ) {
        XP_U16 disableIDs[] = { IDC_NPLAYERSCOMBO, 
                                IDC_DICTBUTTON};
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
        return XP_TRUE;
    } else {
        XP_ASSERT( nDrawn <= nToDraw );
        return nDrawn == nToDraw;
    }
    
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

    nPlayers = 1 + (XP_U16)SendDlgItemMessage( hDlg, IDC_NPLAYERSCOMBO, 
                                               CB_GETCURSEL, 0, 0 );
    gi->nPlayers = (XP_U8)nPlayers;
    XP_DEBUGF( "Set nPlayers to %d", nPlayers );

    for ( i = 0, offset = 0; i < nPlayers; ++i, offset += NUM_COLS ) {
        XP_U16 id;
        XP_Bool checked;
        LocalPlayer* lp = &gi->players[i];

        if ( curServerHilite == SERVER_ISSERVER ) {
            id = REMOTE_CHECK1 + offset;
            lp->isLocal = !ceGetChecked( hDlg, id );
        } else {
            lp->isLocal = XP_TRUE;
        }

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

    /* dictionary */
    replaceStringIfDifferent( MPPARM(globals->mpool) &gi->dictName,
                              giState->newDictName );

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
        loadCurPrefsFromState( &globals->appPrefs, gi, &giState->prefsPrefs );
    }

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
        globals = giState->globals;

        loadFromGameInfo( hDlg, globals, giState );
        loadStateFromCurPrefs( &globals->appPrefs, &globals->gameInfo, 
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
                        if ( giState->isNewGame ) {    /* ignore if in info mode */
                            XP_U16 role;
                            XP_U16 sel;
                            sel = (XP_U16)SendDlgItemMessage( hDlg, 
                                                              IDC_NPLAYERSCOMBO,
                                                              CB_GETCURSEL, 0, 0L);
                            ++sel;
                            role = (XP_U16)SendDlgItemMessage( hDlg, IDC_ROLECOMBO,
                                                               CB_GETCURSEL, 0, 0L);
                            ceAdjustVisibility( hDlg, giState, XP_TRUE );
                        }
                    }
                    break;

                case IDC_ROLECOMBO:
                    if ( HIWORD(wParam) == CBN_SELCHANGE ) {
                        if ( giState->isNewGame ) {    /* ignore if in info mode */
                            XP_U16 sel;
                            sel = (XP_U16)SendDlgItemMessage( hDlg, IDC_ROLECOMBO,
                                                              CB_GETCURSEL, 0, 0L);
                            giState->curServerHilite = (Connectedness)sel;
                            ceAdjustVisibility( hDlg, giState, XP_FALSE );
                        }
                    }
                    break;

#ifndef STUBBED_DICT
                case IDC_DICTBUTTON:
                    if ( giState->isNewGame ) { /* ignore if in info mode */
                        giState->newDictName[0] = 0;
                        if ( ce_pickDictFile( globals, giState->newDictName,
                                              sizeof(giState->newDictName) )) {
                            XP_UCHAR* basename = bname(giState->newDictName);
                            ceSetDlgItemFileName( hDlg, id, basename );
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
