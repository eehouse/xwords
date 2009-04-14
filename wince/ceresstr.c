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

HINSTANCE
ceLoadResFile( const XP_UCHAR* file )
{
    HINSTANCE hinst = NULL;
    wchar_t widebuf[128];
    XP_U16 len = MultiByteToWideChar( CP_ACP, 0, file, -1, widebuf, VSIZE(widebuf) );
    widebuf[len] = 0;
    hinst = LoadLibrary( widebuf );
    XP_LOGF( "strsInst: %p", hinst );
    return hinst;
}

void
ceCloseResFile( HINSTANCE inst )
{
    XP_ASSERT( !!inst );
    FreeLibrary( inst );
}

#ifdef LOADSTRING_BROKEN
typedef struct _ResStrEntry {
    union {
        XP_UCHAR nstr[1];
        wchar_t wstr[1];
    } u;
} ResStrEntry;

typedef struct _ResStrStorage {
    ResStrEntry* entries[CE_LAST_RES_ID - CE_FIRST_RES_ID + 1];
} ResStrStorage;

static const ResStrEntry*
getEntry( CEAppGlobals* globals, XP_U16 resID, XP_Bool isWide )
{
    ResStrStorage* storage = (ResStrStorage*)globals->resStrStorage;
    ResStrEntry* entry;
    XP_U16 index;

    XP_ASSERT( resID >= CE_FIRST_RES_ID && resID <= CE_LAST_RES_ID );
    index = CE_LAST_RES_ID - resID;
    XP_ASSERT( index < VSIZE(storage->entries) );

    if ( !storage ) {
        storage = XP_MALLOC( globals->mpool, sizeof( *storage ) );
        XP_MEMSET( storage, 0, sizeof(*storage) );
        globals->resStrStorage = storage;
    }

    entry = storage->entries[index];
    if ( !entry ) {
        wchar_t wbuf[265];
        XP_U16 len;
        LoadString( globals->locInst, resID, wbuf, VSIZE(wbuf) );
        if ( isWide ) {
            len = wcslen( wbuf );
            entry = (ResStrEntry*)XP_MALLOC( globals->mpool, 
                                             (len*sizeof(wchar_t))
                                             + sizeof(*entry) );
            wcscpy( entry->u.wstr, wbuf );
        } else {
            XP_UCHAR nbuf[265];
            (void)WideCharToMultiByte( CP_ACP, 0, wbuf, -1,
                                       nbuf, VSIZE(nbuf), NULL, NULL );
            len = XP_STRLEN( nbuf );
            entry = (ResStrEntry*)XP_MALLOC( globals->mpool, 
                                             len + sizeof(*entry) );
            XP_STRNCPY( entry->u.nstr, nbuf, len + 1 );
        }

        storage->entries[index] = entry;

        XP_LOGF( "%s: created entry for %d", __func__, resID );
    }

    return entry;
} /* getEntry */
#endif

const XP_UCHAR* 
ceGetResString( CEAppGlobals* globals, XP_U16 resID )
{
#ifdef LOADSTRING_BROKEN
    const ResStrEntry* entry = getEntry( globals, resID, XP_FALSE );   
    return entry->u.nstr;
#else
    /* Docs say that you can call LoadString with 0 as the length and it'll
       return a read-only ptr to the text within the resource, but I'm getting
       a ptr to wide chars back the resource text being multibyte.  I swear
       I've seen it work, though, so might be a res file formatting thing or a
       param to the res compiler.  Need to investigate.  Until I do, the above
       caches local multibyte copies of the resources so the API can stay the
       same. */
    const XP_UCHAR* str = NULL;
    LoadString( globals->locInst, resID, (LPSTR)&str, 0 );
    return str;
#endif
}

const wchar_t*
ceGetResStringL( CEAppGlobals* globals, XP_U16 resID )
{
#ifdef LOADSTRING_BROKEN
    const ResStrEntry* entry = getEntry( globals, resID, XP_TRUE );   
    return entry->u.wstr;
#else
    /* Docs say that you can call LoadString with 0 as the length and it'll
       return a read-only ptr to the text within the resource, but I'm getting
       a ptr to wide chars back the resource text being multibyte.  I swear
       I've seen it work, though, so might be a res file formatting thing or a
       param to the res compiler.  Need to investigate.  Until I do, the above
       caches local multibyte copies of the resources so the API can stay the
       same. */
    const XP_UCHAR* str = NULL;
    LoadString( globals->locInst, resID, (LPSTR)&str, 0 );
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
        for ( ii = 0; ii < VSIZE(storage->entries); ++ii ) {
            ResStrEntry* entry = storage->entries[ii];
            if ( !!entry ) {
                XP_FREE( globals->mpool, entry );
            }
        }

        XP_FREE( globals->mpool, storage );
        globals->resStrStorage = NULL;
    }
}
#endif
