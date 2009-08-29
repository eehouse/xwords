/* -*- compile-command: ""; -*- */
/* 
 * Copyright 2009 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#ifndef _CERESSTR_H_
#define _CERESSTR_H_

#include "cemain.h"

HINSTANCE ceLoadResFile( const XP_UCHAR* file );
void ceCloseResFile( HINSTANCE inst );
XP_Bool ceChooseResFile( HWND hwnd, CEAppGlobals* globals, 
                         const XP_UCHAR* curFileName,
                         XP_UCHAR* buf, XP_U16 bufLen );

const XP_UCHAR* ceGetResString( CEAppGlobals* globals, XP_U16 resID );
const wchar_t* ceGetResStringL( CEAppGlobals* globals, XP_U16 resID );

# ifdef LOADSTRING_BROKEN
void ceFreeResStrings( CEAppGlobals* globals );
# else
# define ceFreeResStrings(g)
# endif

#endif
