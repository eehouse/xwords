/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */

/* Copyright 1999-2001 by Eric House (xwords@eehouse.org).  All rights reserved.
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

typedef struct CEBitmapInfo {
    XP_U8* bits;
    XP_U16 nCols;
    XP_U16 nRows;
} CEBitmapInfo;

DictionaryCtxt* ce_dictionary_make(CEAppGlobals* globals, XP_UCHAR* name);
DictionaryCtxt* ce_dictionary_make_empty( CEAppGlobals* globals );

XP_Bool ce_pickDictFile( CEAppGlobals* globals, XP_UCHAR* buf, XP_U16 len );

/* ceLocateNDicts: Allocate and store in bufs ptrs to up to nSought paths to
 * dict files.  Return the number actually found.  Caller is responsible for
 * making sure bufs contains nSought slots.
 */
XP_U16 ceLocateNDicts( MPFORMAL XP_UCHAR** bufs, XP_U16 nSought );


XP_UCHAR* bname( XP_UCHAR* in );
#endif
