// -*-mode: C; fill-column: 78; c-basic-offset: 4; -*-
/* 
 * Copyright 2001-2002 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#include "frankdlist.h"
#include "frankids.h"

#include "strutils.h"

FrankDictList::FrankDictList(MPFORMAL_NOCOMMA)
{
    MPASSIGN( this->mpool, mpool );
    fNDicts = 0;

    for ( XP_U16 i = 0; i < MAX_DICTS; ++i ) {
        fDictNames[i] = (XP_UCHAR*)NULL;
    }

    populateList();
} // FrankDictList::FrankDictList 

FrankDictList::~FrankDictList()
{
    for ( XP_U16 i = 0; i < fNDicts; ++i ) {
        XP_ASSERT( !!fDictNames[i] );
        XP_FREE( mpool, fDictNames[i] );
    }
} 

XP_S16 
FrankDictList::IndexForName( XP_UCHAR* name )
{
    XP_ASSERT( !!name );
    for ( XP_S16 i = 0; i < fNDicts; ++i ) {
        if ( 0 == XP_STRCMP( name, fDictNames[i] ) ) {
            return i;
        }
    }

    XP_ASSERT(0);
    return -1;
} // FrankDictList::IndexForName

XP_S16
FrankDictList::dictListInsert( ebo_enumerator_t* eboep, FileLoc loc )
{
    U16 flags;
    if ( strcmp( eboep->name.publisher, PUB_ERICHOUSE ) == 0 
         && strcmp( eboep->name.extension, EXT_XWORDSDICT ) == 0 
         && ( ((flags = GetDictFlags( eboep, loc )) == 0x0001 )
              || (flags == 0x0002) || (flags == 0x0003) ) ) {

        XP_UCHAR* newName = (XP_UCHAR*)eboep->name.name;
        XP_U16 nDicts = fNDicts;
        XP_S16 pred;            // predecessor

        // it's a keeper.  Insert in alphabetical order
        for ( pred = nDicts - 1; pred >= 0; --pred ) {
            XP_S16 cmps = XP_STRCMP( newName, fDictNames[pred] );

            // 0 means a duplicate, e.g one on MMC and another in
            // RAM. Drop the dup in favor of the RAM copy.
            if ( cmps == 0 ) { 
                return -1;
            }

            if ( cmps > 0 ) {
                break;          // found it
            }
        }

        /* Now move any above the new location up */
        XP_S16 newLoc = pred + 1;
        for ( XP_U16 j = nDicts; j > newLoc; --j ) {
            fDictNames[j] = fDictNames[j-1];
        }

        XP_ASSERT( newLoc >= 0 );
        fDictNames[newLoc] = copyString( MPPARM(mpool) newName );
        fDictLocs[newLoc] = loc;
        ++fNDicts;

        return newLoc;

    } else {
        return -1;
    }
}

void
FrankDictList::populateList()
{
    int result;
    ebo_enumerator_t eboe;

    for ( result = ebo_first_object( &eboe );
          result == EBO_OK;
          result = ebo_next_object( &eboe ) ) {
        dictListInsert( &eboe, IN_RAM );
    }

    for ( result = ebo_first_xobject( &eboe );
          result == EBO_OK;
          result = ebo_next_xobject( &eboe ) ) {
        dictListInsert( &eboe, ON_MMC );
    }
} // FrankDictList::populateList
