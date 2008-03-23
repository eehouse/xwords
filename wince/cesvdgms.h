/* -*-mode: C; fill-column: 77; c-basic-offset: 4; -*- */
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

#ifdef _WIN32_WCE
# define DEFAULT_DIR_NAME L"\\My Documents\\Crosswords"
# define PREFSFILENAME L"\\My Documents\\Crosswords\\xwprefs"
# define UNSAVEDGAMEFILENAME "\\My Documents\\Crosswords\\_newgame"
#else
# define DEFAULT_DIR_NAME L"\\tmp"
# define PREFSFILENAME L"\\tmp\\xwprefs"
# define UNSAVEDGAMEFILENAME "\\tmp\\_newgame"
#endif


#ifdef _WIN32_WCE
# define DEFAULT_DIR_NAME L"\\My Documents\\Crosswords"
/* # define PREFSFILENAME L"\\My Documents\\Crosswords\\xwprefs" */
/* # define UNSAVEDGAMEFILENAME "\\My Documents\\Crosswords\\_newgame" */
#else
/* # define DEFAULT_DIR_NAME L"." */
# define DEFAULT_DIR_NAME L"\\tmp"
#endif

XP_Bool ceSavedGamesDlg( CEAppGlobals* globals, const XP_UCHAR* curPath,
                         wchar_t* buf, XP_U16 buflen );
XP_Bool ceConfirmUniqueName( CEAppGlobals* globals, wchar_t* buf, 
                             XP_U16 buflen );
#endif
