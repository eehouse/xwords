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

#ifndef _FRANKDLIST_H_
#define _FRANKDLIST_H_

#include <ebm_object.h>

extern "C" {
#include "comtypes.h"
}

#include "mempool.h"
#include "frankdict.h"

#define MAX_DICTS 16 

class FrankDictList {

 public:

    FrankDictList( MPFORMAL_NOCOMMA );
    ~FrankDictList();

    XP_U16 GetDictCount() { return fNDicts; }

    XP_UCHAR* GetNthName( XP_U16 n ) {
	XP_ASSERT( n < fNDicts ); 
	return fDictNames[n];
    }

    FileLoc GetNthLoc( XP_U16 n ) {
	XP_ASSERT( n < fNDicts ); 
	return fDictLocs[n];
    }

    XP_S16 IndexForName( XP_UCHAR* name );

 private:
    void populateList();
    XP_S16 dictListInsert( ebo_enumerator_t* eboep, FileLoc loc );

    XP_U16 fNDicts;
    XP_UCHAR* fDictNames[MAX_DICTS];
    FileLoc fDictLocs[MAX_DICTS];

    MPSLOT
};
#endif
