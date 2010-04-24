/* -*- fill-column: 77; c-basic-offset: 4; compile-command: "make TARGET_OS=wince DEBUG=TRUE" -*- */
/* 
 * Copyright 2002-2009 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#include <stdio.h>

#include "ceprefs.h"
#include "cemain.h"
#include "ceclrsel.h"
#include "ceutil.h"
#include "debhacks.h"
#include "cedebug.h"
#include "cefonts.h"
#include "ceresstr.h"
#include "strutils.h"

typedef struct _CePrefsDlgState {
    CeDlgHdr dlgHdr;
    CePrefsPrefs prefsPrefs;

    XP_UCHAR langFileName[MAX_PATH];

    XP_U16 phonComboId;

    XP_Bool userCancelled;
    //XP_Bool doGlobalPrefs;      /* state of the radio */
    XP_Bool isNewGame;
    XP_Bool colorsChanged;
    XP_Bool langChanged;
} CePrefsDlgState;

/* Stuff the strings for phonies.  Why can't I put this in the resource?
 */
static void
stuffPhoniesList( CePrefsDlgState* state )
{
    HWND hDlg = state->dlgHdr.hDlg;
    CEAppGlobals* globals = state->dlgHdr.globals;
    XP_U16 ii;
    XP_U16 resIDs[] = { IDS_IGNORE_L,IDS_WARN_L, IDS_DISALLOW_L };

    for ( ii = 0; ii < VSIZE(resIDs); ++ii ) {
        const wchar_t* str = ceGetResStringL( globals, resIDs[ii] );
        SendDlgItemMessage( hDlg, state->phonComboId, 
                            ADDSTRING(globals), ii, (long)str );
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
adjustForChoice( CePrefsDlgState* state )
{
    HWND hDlg = state->dlgHdr.hDlg;
    XP_U16 goesWithGlobal[] = {IDC_CHECKCOLORPLAYED, IDC_LEFTYCHECK,
                               IDC_CHECKSHOWCURSOR, IDC_CHECKROBOTSCORES,
                               IDC_SKIPCONFIRM, IDC_HIDETILEVALUES, 
                               IDC_PREFCOLORS, IDC_PREFLOCALE
#ifdef ALLOW_CHOOSE_FONTS
                               ,IDC_PREFFONTS
#endif

 };
    XP_U16 goesWithLocal[] = {IDC_CHECKSMARTROBOT, IDC_CHECKHINTSOK,
                              TIMER_CHECK, TIMER_EDIT, PHONIES_LABEL,
                              PHONIES_COMBO, IDC_PHONIESUPDOWN, 
                              PHONIES_COMBO_PPC,
                              IDC_PICKTILES
#ifdef XWFEATURE_SEARCHLIMIT
                              ,IDC_CHECKHINTSLIMITS
#endif
    };
    XP_U16 resID;
    XP_Bool doGlobalPrefs = state->dlgHdr.globals->doGlobalPrefs;

    resID = doGlobalPrefs? IDC_RADIOGLOBAL:IDC_RADIOLOCAL;
    SendDlgItemMessage( hDlg, resID, BM_SETCHECK, BST_CHECKED, 0L );
    resID = doGlobalPrefs? IDC_RADIOLOCAL:IDC_RADIOGLOBAL;
    SendDlgItemMessage( hDlg, resID, BM_SETCHECK, BST_UNCHECKED, 0L );

    if ( doGlobalPrefs ) {
        turnOnOff( hDlg, goesWithLocal, VSIZE(goesWithLocal), XP_FALSE );
        turnOnOff( hDlg, goesWithGlobal, VSIZE(goesWithGlobal), XP_TRUE);
    } else {
        turnOnOff( hDlg, goesWithGlobal, VSIZE(goesWithGlobal), XP_FALSE );
        turnOnOff( hDlg, goesWithLocal, VSIZE(goesWithLocal), XP_TRUE);
    }

    if ( !doGlobalPrefs ) {
        setTimerCtls( hDlg, ceGetChecked( hDlg, TIMER_CHECK ) );
#ifdef XWFEATURE_SEARCHLIMIT
        ceShowOrHide( hDlg, IDC_CHECKHINTSLIMITS, 
                      ceGetChecked( hDlg, IDC_CHECKHINTSOK) );
#endif
        ceDlgComboShowHide( &state->dlgHdr, PHONIES_COMBO );
    }

#ifdef _WIN32_WCE
    if ( IS_SMARTPHONE(state->dlgHdr.globals) ) {
        SendMessage( hDlg, DM_RESETSCROLL, (WPARAM)FALSE, (LPARAM)TRUE );
    }
#endif
} /* adjustForChoice */

/* Copy global state into a local copy that can be changed without
 * committing should user cancel.
 */
void
loadStateFromCurPrefs( CEAppGlobals* XP_UNUSED_STANDALONE(globals), 
                       const CEAppPrefs* appPrefs, 
                       const CurGameInfo* gi, CePrefsPrefs* prefsPrefs )
{
    prefsPrefs->gp.hintsNotAllowed = gi->hintsNotAllowed;
    prefsPrefs->gp.robotSmartness = gi->robotSmartness;
    prefsPrefs->gp.timerEnabled = gi->timerEnabled;
    prefsPrefs->gp.gameSeconds = gi->gameSeconds;
    prefsPrefs->gp.phoniesAction = gi->phoniesAction;
#ifdef FEATURE_TRAY_EDIT
    prefsPrefs->gp.allowPickTiles = gi->allowPickTiles;
#endif
#ifdef XWFEATURE_SEARCHLIMIT
    prefsPrefs->gp.allowHintRect = gi->allowHintRect;
#endif

    XP_MEMCPY( &prefsPrefs->cp, &appPrefs->cp, sizeof(prefsPrefs->cp) );
    XP_MEMCPY( &prefsPrefs->colors, &appPrefs->colors,
               sizeof(prefsPrefs->colors) );

#ifndef XWFEATURE_STANDALONE_ONLY
    if ( globals->game.comms != NULL ) {
        comms_getAddr( globals->game.comms, &prefsPrefs->addrRec );
    } else {
        comms_getInitialAddr( &prefsPrefs->addrRec
#ifdef XWFEATURE_RELAY
                              ,RELAY_NAME_DEFAULT, RELAY_PORT_DEFAULT
#endif
                              );
    }
#endif
} /* loadStateFromCurPrefs */

void
loadCurPrefsFromState( CEAppGlobals* XP_UNUSED_STANDALONE(globals), 
                       CEAppPrefs* appPrefs, 
                       CurGameInfo* gi, const CePrefsPrefs* prefsPrefs )
{
    gi->hintsNotAllowed = prefsPrefs->gp.hintsNotAllowed;
    gi->robotSmartness = prefsPrefs->gp.robotSmartness;
    gi->timerEnabled = prefsPrefs->gp.timerEnabled;
    gi->gameSeconds = prefsPrefs->gp.gameSeconds;
    gi->phoniesAction = prefsPrefs->gp.phoniesAction;
#ifdef FEATURE_TRAY_EDIT
    gi->allowPickTiles = prefsPrefs->gp.allowPickTiles;
#endif
#ifdef XWFEATURE_SEARCHLIMIT
    gi->allowHintRect = prefsPrefs->gp.allowHintRect;
#endif

    XP_MEMCPY( &appPrefs->cp, &prefsPrefs->cp, sizeof(appPrefs->cp) );
    XP_MEMCPY( &appPrefs->colors, &prefsPrefs->colors,
               sizeof(prefsPrefs->colors) );

#ifndef XWFEATURE_STANDALONE_ONLY
    /* I don't think this'll work... */
    if ( globals->game.comms != NULL ) {
        comms_setAddr( globals->game.comms, &prefsPrefs->addrRec );
    } else {
        XP_LOGF( "no comms to set addr on!!!" );
    }
#endif
} /* loadCurPrefsFromState */

/* Reflect local state into the controls user will see.
 */
static void
loadControlsFromState( CePrefsDlgState* pState )
{
    HWND hDlg = pState->dlgHdr.hDlg;
    CEAppGlobals* globals = pState->dlgHdr.globals;
    CePrefsPrefs* prefsPrefs = &pState->prefsPrefs;

    ceSetChecked( hDlg, IDC_CHECKCOLORPLAYED, prefsPrefs->showColors );
    ceSetChecked( hDlg, IDC_CHECKSMARTROBOT, 
                  prefsPrefs->gp.robotSmartness > 0 );
    ceSetChecked( hDlg, IDC_CHECKHINTSOK, !prefsPrefs->gp.hintsNotAllowed );

    ceSetChecked( hDlg, IDC_CHECKSHOWCURSOR, prefsPrefs->cp.showBoardArrow );
    ceSetChecked( hDlg, IDC_CHECKROBOTSCORES, prefsPrefs->cp.showRobotScores );
    ceSetChecked( hDlg, IDC_SKIPCONFIRM, prefsPrefs->cp.skipCommitConfirm );
    ceSetChecked( hDlg, IDC_HIDETILEVALUES, prefsPrefs->cp.hideTileValues );

#ifdef FEATURE_TRAY_EDIT
    ceSetChecked( hDlg, IDC_PICKTILES, prefsPrefs->gp.allowPickTiles );
#endif
#ifdef XWFEATURE_SEARCHLIMIT
    if ( !IS_SMARTPHONE(globals) ) {
        ceSetChecked( hDlg, IDC_CHECKHINTSLIMITS,  prefsPrefs->gp.allowHintRect );
    }
#endif
    /* timer */
    ceSetDlgItemNum( hDlg, TIMER_EDIT, prefsPrefs->gp.gameSeconds / 60 );

    SendDlgItemMessage( hDlg, pState->phonComboId, SETCURSEL(globals), 
                        prefsPrefs->gp.phoniesAction, 0L );

    if ( !pState->isNewGame ) {
        XP_U16 unavail[] = { TIMER_CHECK, TIMER_EDIT, IDC_CHECKHINTSOK
#ifdef FEATURE_TRAY_EDIT
                             ,IDC_PICKTILES
#endif
        };
        XP_U16 i;
        for ( i = 0; i < VSIZE(unavail); ++i ) {
            ceEnOrDisable( hDlg, unavail[i], XP_FALSE );
        }
    }
} /* loadControlsFromState */

/* Save the new choices into state so caller can do what it wants with
 * the values.
 */
static void
ceControlsToPrefs( CePrefsDlgState* state )
{
    XP_S16 selIndex;
    CePrefsPrefs* prefsPrefs = &state->prefsPrefs;
    HWND hDlg = state->dlgHdr.hDlg;
    CEAppGlobals* globals = state->dlgHdr.globals;

    prefsPrefs->showColors = ceGetChecked( hDlg, IDC_CHECKCOLORPLAYED );
    prefsPrefs->gp.robotSmartness
        = ceGetChecked( hDlg, IDC_CHECKSMARTROBOT ) ? 1 : 0;
    prefsPrefs->gp.hintsNotAllowed = !ceGetChecked( hDlg, IDC_CHECKHINTSOK );

    selIndex = (XP_U16)SendDlgItemMessage( hDlg, state->phonComboId,
                                           GETCURSEL(globals), 
                                           0, 0 );
    if ( selIndex != LB_ERR ) {
        prefsPrefs->gp.phoniesAction = (XWPhoniesChoice)selIndex;
    }

    prefsPrefs->cp.showBoardArrow = ceGetChecked( hDlg, IDC_CHECKSHOWCURSOR );
    prefsPrefs->cp.showRobotScores = ceGetChecked( hDlg, IDC_CHECKROBOTSCORES );
    prefsPrefs->cp.skipCommitConfirm = ceGetChecked( hDlg, IDC_SKIPCONFIRM );
    prefsPrefs->cp.hideTileValues = ceGetChecked( hDlg, IDC_HIDETILEVALUES );
    prefsPrefs->gp.timerEnabled = ceGetChecked( hDlg, TIMER_CHECK );

    if ( prefsPrefs->gp.timerEnabled ) {
        XP_U16 minutes;

        minutes = ceGetDlgItemNum( hDlg, TIMER_EDIT );

        prefsPrefs->gp.gameSeconds = minutes * 60;
    }
#ifdef FEATURE_TRAY_EDIT
    prefsPrefs->gp.allowPickTiles = ceGetChecked( hDlg, IDC_PICKTILES );
#endif
#ifdef XWFEATURE_SEARCHLIMIT
    if ( !IS_SMARTPHONE(globals) ) {
        prefsPrefs->gp.allowHintRect = ceGetChecked( hDlg, IDC_CHECKHINTSLIMITS );
    }
#endif
} /* ceControlsToPrefs */

LRESULT CALLBACK
PrefsDlg(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    XP_U16 id;
    CePrefsDlgState* pState;

    if ( message == WM_INITDIALOG ) {
        SetWindowLongPtr( hDlg, GWL_USERDATA, lParam );
        pState = (CePrefsDlgState*)lParam;
        
        pState->phonComboId = LB_IF_PPC(pState->dlgHdr.globals,PHONIES_COMBO);

        ceDlgSetup( &pState->dlgHdr, hDlg, DLG_STATE_TRAPBACK );
        ceDlgComboShowHide( &pState->dlgHdr, PHONIES_COMBO ); 

        stuffPhoniesList( pState );

        loadControlsFromState( pState );

        adjustForChoice( pState );
        return TRUE;

    } else {
        pState = (CePrefsDlgState*)GetWindowLongPtr( hDlg, GWL_USERDATA );
        if ( !!pState ) {
            if ( !ceDoDlgHandle( &pState->dlgHdr, message, wParam, lParam ) ) {
                CEAppGlobals* globals = pState->dlgHdr.globals;
                XP_Bool timerOn;

                switch (message) {
                case WM_COMMAND:
                    id = LOWORD(wParam);
                    switch( id ) {

                    case IDC_RADIOGLOBAL:
                    case IDC_RADIOLOCAL:
                        globals->doGlobalPrefs = id == IDC_RADIOGLOBAL;
                        adjustForChoice( pState );
                        break;

                    case TIMER_CHECK:
                        timerOn = SendDlgItemMessage( hDlg, TIMER_CHECK, 
                                                      BM_GETCHECK, 0, 0 );
                        setTimerCtls( hDlg, timerOn );
                        break;
                    case IDC_PREFCOLORS:
                        pState->colorsChanged = 
                            ceDoColorsEdit( hDlg, globals, 
                                            pState->prefsPrefs.colors );
                        break;
                    case IDC_PREFLOCALE: {
                        XP_UCHAR newFile[MAX_PATH];
                        if ( ceChooseResFile( hDlg, globals, 
                                              pState->langFileName,
                                              newFile, VSIZE(newFile)) ) {
                            pState->langChanged = XP_TRUE;
                            XP_STRNCPY( pState->langFileName, newFile, 
                                        VSIZE(pState->langFileName) );
                        }
                    }
                        break;
#ifdef ALLOW_CHOOSE_FONTS
                    case IDC_PREFFONTS:
                        ceShowFonts( hDlg, globals );
                        break;
#endif

#ifdef XWFEATURE_SEARCHLIMIT
                    case IDC_CHECKHINTSOK:
                        timerOn = SendDlgItemMessage( hDlg, IDC_CHECKHINTSOK, 
                                                      BM_GETCHECK, 0, 0 );
                        ceShowOrHide( hDlg, IDC_CHECKHINTSLIMITS, timerOn );
                        break;
                    case IDC_CHECKHINTSLIMITS:
                        if ( IS_SMARTPHONE(globals) ) {
                            ceMessageBoxChar( globals, 
                                              ceGetResString( globals,
                                                              IDS_NEED_TOUCH ),
                                              ceGetResStringL( globals,
                                                               IDS_FYI_L ),
                                              MB_OK | MB_ICONHAND, SAB_NONE );
                            ceSetChecked( hDlg, IDC_CHECKHINTSLIMITS, XP_FALSE );
                        }
                        break;
#endif

                    case IDOK:
                        ceControlsToPrefs( pState );
                    case IDCANCEL:
                        EndDialog( hDlg, id );
                        pState->userCancelled = id == IDCANCEL;
                        return TRUE;
                    }
                }
            }        
        }
    }

    return FALSE;
} /* PrefsDlg */

/* Using state in prefsPrefs, and initing and then storing dialog state in
   state, put up the dialog and return whether it was cancelled.
 */
XP_Bool
WrapPrefsDialog( HWND hDlg, CEAppGlobals* globals, CePrefsPrefs* prefsPrefs,
                 XP_Bool isNewGame, XP_Bool* colorsChanged,  XP_Bool* langChanged )
{
    CePrefsDlgState state;
    XP_Bool result;

    XP_MEMSET( &state, 0, sizeof(state) );

    state.dlgHdr.globals = globals;
    state.isNewGame = isNewGame;
    if ( !!globals->langFileName ) {
        XP_STRNCPY( state.langFileName, globals->langFileName, 
                    VSIZE(state.langFileName) );
    }
    XP_MEMCPY( &state.prefsPrefs, prefsPrefs, sizeof( state.prefsPrefs ) );

    DialogBoxParam( globals->locInst, (LPCTSTR)IDD_OPTIONSDLG, hDlg,
                    (DLGPROC)PrefsDlg, (long)&state );
    
    result = !state.userCancelled;

    if ( result ) {
        XP_MEMCPY( prefsPrefs, &state.prefsPrefs, sizeof( *prefsPrefs ) );
        *colorsChanged = state.colorsChanged;
        *langChanged = state.langChanged;

        replaceStringIfDifferent( globals->mpool, &globals->langFileName, 
                                  state.langFileName );
    }

    return result;
} /* WrapPrefsDialog */
