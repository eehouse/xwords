/* -*-mode: C; fill-column: 77; c-basic-offset: 4; -*- */
/* 
 * Copyright 1999 - 2001 by Eric House (xwords@eehouse.org).  All rights reserved.
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
#ifndef _DICTLIST_H_
#define _DICTLIST_H_

#include "palmmain.h"
#include "palmdict.h"

enum { DL_STORAGE, DL_VFS };

typedef struct DictListEntry {
    XP_UCHAR* path;
    XP_UCHAR* baseName;                 /* points into, or ==, path */
    DictionaryCtxt* dict;    /* cache so can refcount */
    XP_UCHAR location;                  /* Storage RAM or VFS */
    union {
        struct {
            UInt16 cardNo;
            LocalID dbID;
        } dmData;
        struct {
            UInt16 volNum;
        } vfsData;
    } u;
} DictListEntry;

PalmDictList* DictListMake( MPFORMAL_NOCOMMA );
void DictListFree( MPFORMAL PalmDictList* dl );
XP_U16 DictListCount( PalmDictList* dl );

XP_Bool getDictWithName( const PalmDictList* dl, const unsigned char* name, 
                         DictListEntry** dle );
void cacheDictForName( PalmDictList* dl, const XP_UCHAR* dictName, 
                       DictionaryCtxt* ctxt );
void removeFromDictCache( PalmDictList* dl, XP_UCHAR* dictName );

XP_Bool getNthDict( const PalmDictList* dl, short n, DictListEntry** dle );

#endif
