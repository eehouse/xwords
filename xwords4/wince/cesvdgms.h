/* -*- fill-column: 77; c-basic-offset: 4; compile-command: "make TARGET_OS=wince DEBUG=TRUE" -*- */
/* 
 * Copyright 2008 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#ifndef _CESVDGMS_H_
#define _CESVDGMS_H_

#include "xptypes.h"
#include "cemain.h"

typedef enum {
    CE_SVGAME_CANCEL
    ,CE_SVGAME_RENAME
    ,CE_SVGAME_OPEN
} SavedGamesResult;

SavedGamesResult ceSavedGamesDlg( CEAppGlobals* globals, 
                                  const XP_UCHAR* curPath,
                                  wchar_t* buf, XP_U16 buflen );
XP_Bool ceConfirmUniqueName( CEAppGlobals* globals, HWND hWnd, XP_U16 strId, 
                             wchar_t* buf, XP_U16 buflen );
#endif
