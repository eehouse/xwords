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

#include "stdafx.h" 

#include "xwstream.h"
#include "cemain.h"

LRESULT CALLBACK StrBox(HWND hDlg, UINT message, WPARAM wParam, 
			LPARAM lParam);


typedef struct StrBoxInit {
    CEAppGlobals* globals;
    wchar_t* title;
    XWStreamCtxt* stream;
    XP_U16 result;
    XP_Bool isQuery;
} StrBoxInit;
