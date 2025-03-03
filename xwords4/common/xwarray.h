/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/*
 * Copyright 2025 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#ifndef _XWARRAY_H_
#define _XWARRAY_H_

#ifdef CPLUS
extern "C" {
#endif

#include "mempool.h"
    // #include "comtypes.h"
#include "xptypes.h"

typedef struct XWArray XWArray;

XWArray* arr_make(MPFORMAL_NOCOMMA);
void arr_destroy( XWArray* array );

/* Set the sort order. Will result in a resort if there's data. Null is
   allowed, but then an insert is just an append. */
typedef int (*ArCompProc)(const void* dl1, const void* dl2);
void arr_setSort( XWArray* array, ArCompProc proc );

void arr_insert( XWArray* array, void* node );
void* arr_getNth( XWArray* array, XP_U32 nn );
void arr_remove( XWArray* array, void* node );

XP_U32 arr_length( const XWArray* array );

typedef ForEachAct (*ArMapProc)( void* elem, void* closure );
void arr_map( XWArray* array, ArMapProc mapProc, void* closure );

typedef void (*ArDisposeProc)( void* elem, void* closure );
void arr_removeAll( XWArray* array, ArDisposeProc dispProc, void* closure );

#ifdef CPLUS
}
#endif

#endif
