/* -*- compile-command: "make -j3 TARGET_OS=wince DEBUG=TRUE RELAY_NAME_DEFAULT=localhost" -*- */
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

#include "ceresstr.h"

#ifdef LOADSTRING_BROKEN
typedef struct _ResStrEntry {
    XP_U16 resID;
    XP_UCHAR str[1];
} ResStrEntry;

typedef struct _ResStrStorage {
    ResStrEntry* entries[CE_LAST_RES_ID - CE_FIRST_RES_ID + 1];
} ResStrStorage;
#endif


const XP_UCHAR* 
ceGetResString( CEAppGlobals* globals, XP_U16 resID )
{
    HINSTANCE hinst = globals->strsInst;
    if ( !hinst ) {
        hinst = globals->hInst;
    }

#ifdef LOADSTRING_BROKEN
    const XP_UCHAR* str = NULL;
    ResStrEntry* entry = NULL;
    XP_U16 ii;

    XP_ASSERT( resID >= CE_FIRST_RES_ID && resID <= CE_LAST_RES_ID );

    ResStrStorage* storage = (ResStrStorage*)globals->resStrStorage;
    if ( !storage ) {
        storage = XP_MALLOC( globals->mpool, sizeof( *storage ) );
        XP_MEMSET( storage, 0, sizeof(*storage) );
        globals->resStrStorage = storage;
    }

    for ( ii = 0; ; ++ii ) {
        XP_ASSERT( ii < VSIZE(storage->entries) );
        entry = storage->entries[ii];
        if ( !entry ) {
            break;
        } else if ( entry->resID == resID ) {
            str = entry->str;
            XP_LOGF( "%s: found entry for %d", __func__, resID );
            break;
        }
    }

    if ( !str ) {
        wchar_t wbuf[265];
        XP_UCHAR nbuf[265];
        XP_U16 len;
        LoadString( hinst, resID, wbuf, VSIZE(wbuf) );
        (void)WideCharToMultiByte( CP_ACP, 0, wbuf, -1,
                                   nbuf, VSIZE(nbuf), NULL, NULL );
        len = XP_STRLEN( nbuf );
        entry = (ResStrEntry*)XP_MALLOC( globals->mpool, len + sizeof(*entry) );
        entry->resID = resID;
        XP_STRNCPY( entry->str, nbuf, len + 1 );
        str = entry->str;
        XP_ASSERT( !storage->entries[ii] );
        XP_ASSERT( ii == 0 || !!storage->entries[ii-1] );
        storage->entries[ii] = entry;

        XP_LOGF( "%s: created entry for %d", __func__, resID );
    }

    return str;

#else
    /* Docs say that you can call LoadString with 0 as the length and it'll
       return a read-only ptr to the text within the resource, but I'm getting
       a ptr to wide chars back the resource text being multibyte.  I swear
       I've seen it work, though, so might be a res file formatting thing or a
       param to the res compiler.  Need to investigate.  Until I do, the above
       caches local multibyte copies of the resources so the API can stay the
       same. */
    const XP_UCHAR* str = NULL;
    LoadString( hinst, resID, (LPSTR)&str, 0 );
    return str;
#endif
}

#ifdef LOADSTRING_BROKEN
void
ceFreeResStrings( CEAppGlobals* globals )
{
    ResStrStorage* storage = (ResStrStorage*)globals->resStrStorage;
    if ( !!storage ) {
        XP_U16 ii;
        for ( ii = 0; ; ++ii ) {
            ResStrEntry* entry = storage->entries[ii];
            if ( !entry ) {
                break;
            }
            XP_FREE( globals->mpool, entry );
        }

        XP_FREE( globals->mpool, storage );
        globals->resStrStorage = NULL;
    }
}
#endif
