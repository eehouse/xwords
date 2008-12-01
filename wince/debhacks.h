/* -*- fill-column: 77; c-basic-offset: 4; -*- */
/* 
 * Copyright 2006 by Eric House (xwords@eehouse.org).  All rights reserved.
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

/* This file should eventually go away as changes move up into the linux dev
 * tools
 */

#ifndef _DEBHACKS_H_
#define _DEBHACKS_H_

#if defined USE_RAW_MINGW

#ifdef USE_DEB_HACKS

#define DH(func) debhack_##func

typedef struct DH(WIN32_FIND_DATA) {
	DWORD dwFileAttributes;
	FILETIME dh_ftCreationTime;
	FILETIME dh_ftLastAccessTime;
	FILETIME dh_ftLastWriteTime;
	DWORD dh_nFileSizeHigh;
	DWORD dh_nFileSizeLow;
	DWORD dh_dwReserved1;
	WCHAR cFileName[MAX_PATH];
} DH(WIN32_FIND_DATA);

DWORD DH(GetCurrentThreadId)(void);
BOOL DH(SetEvent)(HANDLE);
BOOL DH(ResetEvent)(HANDLE);

#else

#define DH(func) func

#endif

/* these are apparently defined in aygshell.h.  I got the values by googling
   for 'define SHFS_SHOWTASKBAR' etc.  They should eventually move into the
   mingw project's headers .*/

#define SHFS_SHOWTASKBAR            0x0001
#define SHFS_HIDETASKBAR            0x0002
#define SHFS_SHOWSIPBUTTON          0x0004
#define SHFS_HIDESIPBUTTON          0x0008
#define SHFS_SHOWSTARTICON          0x0010
#define SHFS_HIDESTARTICON          0x0020

/* got this somewhere else via google */
#define SHCMBF_HMENU 0x0010

#endif /* USE_RAW_MINGW */

#ifndef IME_ESC_SET_MODE
# define IME_ESC_SET_MODE            0x0800
#endif
#ifndef IM_SPELL
# define IM_SPELL 0
#endif

#if 0
 /* http://forums.microsoft.com/MSDN/ShowPost.aspx?PostID=1591512&SiteID=1 */
#define IM_SPELL 0
#define IME_ESC_SET_MODE            0x0800
/* http://forums.microsoft.com/MSDN/ShowPost.aspx?PostID=1476620&SiteID=1 */
#define EIM_SPELL IM_SPELL
/* http://wolfpack.twu.net/docs/gtkwin32/gdkprivate-win32.h */
#define WM_IME_REQUEST 0x0288

/* http://forums.microsoft.com/MSDN/ShowPost.aspx?PostID=1476620&SiteID=1 */
#define EM_SETINPUTMODE 0x00DE 

/* #define IMR_ISIMEAWARE              0 */
/* #define IMEAF_AWARE                 0 */
/* #define IME_ESC_RETAIN_MODE_ICON    0 */
#endif

#endif
