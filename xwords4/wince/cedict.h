/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */

/* Copyright 1999-2006 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#ifndef _CEDICT_H_
#define _CEDICT_H_

#include "cemain.h"

#define CE_MAXDICTS 0x7FFF

typedef struct CEBitmapInfo {
    XP_U8* bits;
    XP_U16 nCols;
    XP_U16 nRows;
} CEBitmapInfo;

DictionaryCtxt* ce_dictionary_make(CEAppGlobals* globals, const char* name);
DictionaryCtxt* ce_dictionary_make_empty( CEAppGlobals* globals );

/* Callback: return true if done; false to continue */
typedef XP_Bool (*OnePathCB)( const wchar_t* wPath, XP_U16 index, void* ctxt );

/* ceLocateNDicts: Allocate and store in bufs ptrs to up to nSought paths to
 * dict files.  Return the number actually found.  Caller is responsible for
 * making sure bufs contains nSought slots.
 */
XP_U16 ceLocateNDicts( CEAppGlobals* globals, XP_U16 nSought, 
                       OnePathCB cb, void* ctxt );

/* return just the name, no extension, of dict, written to buf, pointed to by
   return value (which is into buf, but not necessarily the first char.) */
wchar_t* wbname( wchar_t* buf, XP_U16 buflen, const wchar_t* in );

const XP_UCHAR* bname( const XP_UCHAR* in );
#endif
