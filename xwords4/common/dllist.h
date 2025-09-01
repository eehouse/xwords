/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/*
 * Copyright 2024 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#ifndef __DLLIST_H__
#define __DLLIST_H__

#ifdef CPLUS
extern "C" {
#endif

#include "comtypes.h"
#include "xptypes.h"
#include "comtypes.h"

typedef struct DLHead {
    struct DLHead* _next;
    struct DLHead* _prev;
} DLHead;

typedef int (*DLCompProc)(const DLHead* dl1, const DLHead* dl2);

DLHead* dll_insert( DLHead* list, DLHead* node, DLCompProc proc );
DLHead* dll_append( DLHead* list, DLHead* node );
DLHead* dll_remove( DLHead* list, DLHead* node );
XP_U16 dll_length( const DLHead* list );
DLHead* dll_sort( DLHead* list, DLCompProc proc );

typedef ForEachAct (*DLMapProc)(const DLHead* elem, void* closure);
typedef void (*DLDisposeProc)(DLHead* elem, void* closure);
DLHead* dll_map( DLHead* list, DLMapProc mapProc, DLDisposeProc dispProc,
                 void* closure );
void dll_removeAll( DLHead* list, DLDisposeProc dispProc, void* closure );

#ifdef CPLUS
}
#endif

#endif
