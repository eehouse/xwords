/* -*-mode: C; fill-column: 77; c-basic-offset: 4; -*- */
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

#ifndef _CEGINFO_H_
#define _CEGINFO_H_

#include "stdafx.h" 
#include "cemain.h"
#include "ceprefs.h"
#include "cedict.h"
#include "ceutil.h"
#include "nwgamest.h"


typedef struct _GInfoResults {
    XP_Bool prefsChanged;
    XP_Bool colorsChanged;
    XP_Bool langChanged;
    XP_Bool addrChanged;
} GInfoResults;

typedef enum {
    GI_INFO_ONLY
    ,GI_NEW_GAME
    ,GI_GOTO_CONNS
} GIShow;

XP_Bool WrapGameInfoDialog( CEAppGlobals* globals, GIShow showWhat,
                            CePrefsPrefs* prefsPrefs,
                            XP_UCHAR* dictName, XP_U16 dictNameLen,
                            GInfoResults* results );

#endif
