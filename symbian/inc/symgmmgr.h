/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 2005 by Eric House (xwords@eehouse.org).
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


#ifndef _SYMGMMGR_H_
#define _SYMGMMGR_H_

extern "C" {
#include "comtypes.h"
#include "memstream.h"
#include "mempool.h"
}

#include <e32base.h>
#include <eikdialg.h>
#include <eiktxlbm.h>
#include <eiklbv.h>

typedef TBuf16<32> TGameName;

/* This class tracks games, each of which is (currently) saved in a
 * file in a hidden directory.
 */
class CXWGamesMgr : public CBase
{
 private:
    CXWGamesMgr( MPFORMAL CCoeEnv* aCoeEnv, TFileName* aBasePath );

    void BuildListL();
    TBool DeleteFileFor( TPtrC16* aName );
    void  GameNameToPath( TFileName* path, const TDesC16* name );

 public:
    static CXWGamesMgr* NewL( MPFORMAL CCoeEnv* aCoeEnv, TFileName* aBasePath );

    TInt GetNGames() const;
    /* Will be used by dialog */
    CDesC16Array* GetNames() const;

    /* Come up with some unique name */
    void MakeDefaultName( TGameName* aName );

    void StoreGameL( const TGameName* aName, XWStreamCtxt* stream );
    void LoadGameL( const TGameName* aName, XWStreamCtxt* stream );

    TBool DeleteSelected( TInt aIndex );
    TBool Exists( TGameName* aName );
    TBool IsLegalName( const TGameName* aName );
    void Rename( const TDesC16* aCurName, const TDesC16* aNewName );

 private:

    CDesC16ArrayFlat* iNamesList;
    TFileName iDir;
    CCoeEnv* iCoeEnv;
    TInt iGameCount;
    MPSLOT
};

#endif
