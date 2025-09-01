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

#ifndef _LINDMGR_H_
#define _LINDMGR_H_

#include "comtypes.h"

typedef struct LinDictMgr LinDictMgr;

typedef void (*DictAddedProc)(void* closure, const XP_UCHAR* dictName);
typedef void (*DictRemovedProc)(void* closure, const XP_UCHAR* dictName);

LinDictMgr* ldm_init(DictAddedProc ap, DictRemovedProc rp, void* closure);
void ldm_destroy( LinDictMgr* ldm );
void ldm_addDir( LinDictMgr* ldm, const char* path );
XP_Bool ldm_pathFor( LinDictMgr* ldm, const char* dictName,
                     char buf[], XP_U16 bufLen );
GSList* ldm_listDicts( LinDictMgr* ldm );
#endif
