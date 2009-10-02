/* -*-mode: C; fill-column: 77; c-basic-offset: 4; -*- */
/* 
 * Copyright 2002-2007 by Eric House (xwords@eehouse.org).  All rights reserved.
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
#include "ceutil.h"

typedef struct CeGamePrefs {
    XP_U16 gameSeconds;
    XP_Bool hintsNotAllowed;
    XP_U8 robotSmartness;
    XP_Bool timerEnabled;
#ifdef FEATURE_TRAY_EDIT
    XP_Bool allowPickTiles;
#endif
#ifdef XWFEATURE_SEARCHLIMIT
    XP_Bool allowHintRect;
#endif
    
    XWPhoniesChoice phoniesAction;
    /* phonies something */
} CeGamePrefs;

typedef struct CePrefsPrefs {
    /* per-game */
    CeGamePrefs gp;

#ifndef XWFEATURE_STANDALONE_ONLY
    CommsAddrRec addrRec;
#endif

    /* global */
    CommonPrefs cp;
    XP_Bool showColors;
    
    COLORREF colors[CE_NUM_EDITABLE_COLORS];
} CePrefsPrefs;

XP_Bool WrapPrefsDialog( HWND hDlg, CEAppGlobals* globals, 
                         CePrefsPrefs* prefsPrefs, XP_Bool isNewGame,
                         XP_Bool* colorsChanged, XP_Bool* langChanged );
void loadStateFromCurPrefs( CEAppGlobals* globals, const CEAppPrefs* appPrefs, 
                            const CurGameInfo* gi, CePrefsPrefs* prefsPrefs );
void loadCurPrefsFromState( CEAppGlobals* globals, CEAppPrefs* appPrefs, 
                            CurGameInfo* gi, const CePrefsPrefs* prefsPrefs );

LRESULT CALLBACK PrefsDlg(HWND, UINT, WPARAM, LPARAM);

#endif
