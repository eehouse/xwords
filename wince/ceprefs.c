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

#include "ceprefs.h"
#include "cemain.h"
#include "ceutil.h"

/* Stuff the strings for phonies.  Why can't I put this in the resource?
 */
static void
stuffPhoniesList( HWND hDlg )
{
    XP_U16 i;
    wchar_t* strings[] = {
        L"Ignore",
        L"Warn",
        L"Disallow"
    };

    for ( i = 0; i < 3; ++i ) {
        SendDlgItemMessage( hDlg, PHONIES_COMBO, CB_ADDSTRING, 
                            0, (long)strings[i] );
    }
} /* stuffPhoniesList */

static void
turnOnOff( HWND hDlg, XP_U16* idList, XP_U16 idCount, 
           XP_Bool turnOn )
{
    XP_U16 i;
    for ( i = 0; i < idCount; ++i ) {
        ceShowOrHide( hDlg, *idList++, turnOn );
    }
} /* turnOff */

static void
setTimerCtls( HWND hDlg, XP_Bool checked )
{
    ceShowOrHide( hDlg, TIMER_EDIT, checked );

    SendDlgItemMessage( hDlg, TIMER_CHECK, BM_SETCHECK, 
                        checked? BST_CHECKED:BST_UNCHECKED, 0 );
} /* setTimerCtls */

static void
adjustForChoice( HWND hDlg, PrefsDlgState* state )
{
    XP_U16 goesWithGlobal[] = {IDC_CHECKCOLORPLAYED, IDC_LEFTYCHECK,
                               IDC_CHECKSHOWCURSOR, IDC_CHECKROBOTSCORES};
    XP_U16 goesWithLocal[] = {IDC_CHECKSMARTROBOT,IDC_CHECKNOHINTS,
                              TIMER_CHECK, TIMER_EDIT, PHONIES_LABEL,
                              PHONIES_COMBO, IDC_PICKTILES };
    XP_U16 resID;

    resID = state->globals->doGlobalPrefs? 
        IDC_RADIOGLOBAL:IDC_RADIOLOCAL;
    SendDlgItemMessage( hDlg, resID, BM_SETCHECK, BST_CHECKED, 0L );

    if ( state->globals->doGlobalPrefs ) {
        turnOnOff( hDlg, goesWithLocal, 
                   sizeof(goesWithLocal)/sizeof(goesWithLocal[0]),
                   XP_FALSE );
        turnOnOff( hDlg, goesWithGlobal, 
                sizeof(goesWithGlobal)/sizeof(goesWithGlobal[0]),
                XP_TRUE);
    } else {
        turnOnOff( hDlg, goesWithGlobal, 
                 sizeof(goesWithGlobal)/sizeof(goesWithGlobal[0]),
                 XP_FALSE );
        turnOnOff( hDlg, goesWithLocal, 
                   sizeof(goesWithLocal)/sizeof(goesWithLocal[0]),
                   XP_TRUE);
    }
} /* adjustForChoice */

/* Copy global state into a local copy that can be changed without
 * committing should user cancel.
 */
void
loadStateFromCurPrefs( const CEAppPrefs* appPrefs, const CurGameInfo* gi, 
                       PrefsPrefs* prefsPrefs )
{
    prefsPrefs->gp.hintsNotAllowed = gi->hintsNotAllowed;
    prefsPrefs->gp.robotSmartness = gi->robotSmartness;
    prefsPrefs->gp.timerEnabled = gi->timerEnabled;
    prefsPrefs->gp.gameSeconds = gi->gameSeconds;
    prefsPrefs->gp.phoniesAction = gi->phoniesAction;
#ifdef FEATURE_TRAY_EDIT
    prefsPrefs->gp.allowPickTiles = gi->allowPickTiles;
#endif
    prefsPrefs->showColors = appPrefs->showColors;

    XP_MEMCPY( &prefsPrefs->cp, &appPrefs->cp, sizeof(prefsPrefs->cp) );
} /* loadStateFromCurPrefs */

void
loadCurPrefsFromState( CEAppPrefs* appPrefs, CurGameInfo* gi, 
                       const PrefsPrefs* prefsPrefs )
{
    gi->hintsNotAllowed = prefsPrefs->gp.hintsNotAllowed;
    gi->robotSmartness = prefsPrefs->gp.robotSmartness;
    gi->timerEnabled = prefsPrefs->gp.timerEnabled;
    gi->gameSeconds = prefsPrefs->gp.gameSeconds;
    gi->phoniesAction = prefsPrefs->gp.phoniesAction;
#ifdef FEATURE_TRAY_EDIT
    gi->allowPickTiles = prefsPrefs->gp.allowPickTiles;
#endif
    appPrefs->showColors = prefsPrefs->showColors;

    XP_MEMCPY( &appPrefs->cp, &prefsPrefs->cp, sizeof(appPrefs->cp) );
} /* loadCurPrefsFromState */

/* Reflect local state into the controls user will see.
 */
static void
loadControlsFromState( HWND hDlg, PrefsDlgState* pState )
{
    PrefsPrefs* prefsPrefs = &pState->prefsPrefs;
    XP_UCHAR numBuf[10];

    ceSetChecked( hDlg, IDC_CHECKCOLORPLAYED, prefsPrefs->showColors );
    ceSetChecked( hDlg, IDC_CHECKSMARTROBOT, 
                    prefsPrefs->gp.robotSmartness > 0 );
    ceSetChecked( hDlg, IDC_CHECKNOHINTS, prefsPrefs->gp.hintsNotAllowed );

    ceSetChecked( hDlg, IDC_CHECKSHOWCURSOR, prefsPrefs->cp.showBoardArrow );
    ceSetChecked( hDlg, IDC_CHECKROBOTSCORES, prefsPrefs->cp.showRobotScores );

#ifdef FEATURE_TRAY_EDIT
    ceSetChecked( hDlg, IDC_PICKTILES, prefsPrefs->gp.allowPickTiles );
#endif
    /* timer */
    sprintf( numBuf, "%d", prefsPrefs->gp.gameSeconds / 60 );
    ceSetDlgItemText( hDlg, TIMER_EDIT, numBuf );
    setTimerCtls( hDlg, prefsPrefs->gp.timerEnabled );

    SendDlgItemMessage( hDlg, PHONIES_COMBO, CB_SETCURSEL, 
                        prefsPrefs->gp.phoniesAction, 0L );

    if ( !pState->isNewGame ) {
        SendDlgItemMessage( hDlg, IDC_CHECKNOHINTS, WM_ENABLE, FALSE, 0L );
#ifdef FEATURE_TRAY_EDIT
        SendDlgItemMessage( hDlg, IDC_PICKTILES, WM_ENABLE, FALSE, 0L );
#endif
    }
} /* loadControlsFromState */

/* Save the new choices into state so caller can do what it wants with
 * the values.
 */
static void
ceControlsToPrefs( HWND hDlg, PrefsPrefs* prefsPrefs )
{
    XP_S16 selIndex;

    prefsPrefs->showColors = ceGetChecked( hDlg, IDC_CHECKCOLORPLAYED );
    prefsPrefs->gp.robotSmartness
        = ceGetChecked( hDlg, IDC_CHECKSMARTROBOT ) ? 1 : 0;
    prefsPrefs->gp.hintsNotAllowed = ceGetChecked( hDlg, IDC_CHECKNOHINTS );

    selIndex = (XP_U16)SendDlgItemMessage( hDlg, PHONIES_COMBO, CB_GETCURSEL, 
                                           0, 0 );
    if ( selIndex != LB_ERR ) {
        prefsPrefs->gp.phoniesAction = (XWPhoniesChoice)selIndex;
    }

    prefsPrefs->cp.showBoardArrow = ceGetChecked( hDlg, IDC_CHECKSHOWCURSOR );
    prefsPrefs->cp.showRobotScores = ceGetChecked( hDlg, IDC_CHECKROBOTSCORES );
#ifdef FEATURE_TRAY_EDIT
    prefsPrefs->gp.allowPickTiles = ceGetChecked( hDlg, IDC_PICKTILES );
#endif
} /* ceControlsToPrefs */

LRESULT CALLBACK
PrefsDlg(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    CEAppGlobals* globals;
    XP_U16 id;
    PrefsDlgState* pState;

    if ( message == WM_INITDIALOG ) {
        SetWindowLong( hDlg, GWL_USERDATA, lParam );
        pState = (PrefsDlgState*)lParam;
        globals = pState->globals;

        stuffPhoniesList( hDlg );

        positionDlg( hDlg );

        loadControlsFromState( hDlg, pState );
        adjustForChoice( hDlg, pState );

        return TRUE;

    } else {
        XP_Bool timerOn;
        pState = (PrefsDlgState*)GetWindowLong( hDlg, GWL_USERDATA );
        globals = pState->globals;

        switch (message) {
        case WM_COMMAND:
            id = LOWORD(wParam);
            switch( id ) {

            case IDC_RADIOGLOBAL:
            case IDC_RADIOLOCAL:
                pState->globals->doGlobalPrefs = id == IDC_RADIOGLOBAL;
                adjustForChoice( hDlg, pState );
                break;

            case TIMER_CHECK:
                timerOn = SendDlgItemMessage( hDlg, TIMER_CHECK, BM_GETCHECK,
                                              0, 0 );
                setTimerCtls( hDlg, timerOn );
                break;

            case IDOK:
                ceControlsToPrefs( hDlg, &pState->prefsPrefs );
            case IDCANCEL:
                EndDialog(hDlg, id);
                pState->userCancelled = id == IDCANCEL;
                return TRUE;
            }
        }        

    }

    return FALSE;
} /* PrefsDlg */

/* Using state in prefsPrefs, and initing and then storing dialog state in
   state, put up the dialog and return whether it was cancelled.
 */
XP_Bool
WrapPrefsDialog( HWND hDlg, CEAppGlobals* globals, PrefsDlgState* state, 
                 PrefsPrefs* prefsPrefs, XP_Bool isNewGame )
{
    XP_Bool result;
    XP_MEMSET( state, 0, sizeof(*state) );

    state->globals = globals;
    state->isNewGame = isNewGame;
    XP_MEMCPY( &state->prefsPrefs, prefsPrefs, sizeof( state->prefsPrefs ) );

    DialogBoxParam( globals->hInst, (LPCTSTR)IDD_OPTIONSDLG, hDlg,
                    (DLGPROC)PrefsDlg, (long)state );
    
    result = !state->userCancelled;

    if ( result ) {
        XP_MEMCPY( prefsPrefs, &state->prefsPrefs, sizeof( *prefsPrefs ) );
    }

    return result;
} /* WrapPrefsDialog */
