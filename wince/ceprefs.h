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

#ifndef _CEPREFS_H_
#define _CEPREFS_H_

#include "stdafx.h" 
#include "cemain.h"

typedef struct CeGamePrefs {
    XP_U16 gameSeconds;
    XP_Bool hintsNotAllowed;
    XP_U8 robotSmartness;
    XP_Bool timerEnabled;
    XWPhoniesChoice phoniesAction;
    /* phonies something */
} CeGamePrefs;

typedef struct PrefsPrefs {
    /* per-game */
    CeGamePrefs gp;

    /* global */
    XP_Bool showColors;
    CommonPrefs cp;
} PrefsPrefs;

typedef struct PrefsDlgState {
    CEAppGlobals* globals;
    XP_Bool userCancelled;
    //XP_Bool doGlobalPrefs;      /* state of the radio */
    XP_Bool isNewGame;
    PrefsPrefs prefsPrefs;
} PrefsDlgState;

XP_Bool WrapPrefsDialog( HWND hDlg, CEAppGlobals* globals, 
                         PrefsDlgState* state, PrefsPrefs* prefsPrefs, 
                         XP_Bool isNewGame );
void loadStateFromCurPrefs( const CEAppPrefs* appPrefs, const CurGameInfo* gi, 
                            PrefsPrefs* prefsPrefs );
void loadCurPrefsFromState( CEAppPrefs* appPrefs, CurGameInfo* gi, 
                            const PrefsPrefs* prefsPrefs );

LRESULT CALLBACK PrefsDlg(HWND, UINT, WPARAM, LPARAM);

#endif
