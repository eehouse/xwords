/* -*-mode: C; fill-column: 77; c-basic-offset: 4; -*- */
/* 
 * Copyright 2002 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#ifndef _CEGINFO_H_
#define _CEGINFO_H_

#include "stdafx.h" 
#include "cemain.h"
#include "ceprefs.h"
#include "cedict.h"

typedef struct GameInfoState {
    CEAppGlobals* globals;
    HWND hDlg;
    XP_UCHAR newDictName[CE_MAX_PATH_LEN+1];

    XP_U16 capMenuDicts;
    XP_U16 nMenuDicts;
    wchar_t** menuDicts;

    XP_Bool isNewGame;              /* newGame or GameInfo */
    XP_Bool userCancelled;          /* OUT param */

    XP_Bool prefsChanged;
    XP_Bool colorsChanged;
    XP_Bool addrChanged;
    XP_Bool curSelSet;
    Connectedness curServerHilite;
    CePrefsPrefs prefsPrefs;
} GameInfoState;


LRESULT CALLBACK GameInfo(HWND, UINT, WPARAM, LPARAM);

#endif
