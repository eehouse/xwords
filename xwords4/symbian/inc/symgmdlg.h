/* -*-mode: C; fill-column: 78; c-basic-offset: 4;-*- */ 
/* 
 * Copyright 2005 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#ifndef _SYMGMDLG_H_
#define _SYMGMDLG_H_

extern "C" {
#include "comtypes.h"
#include "mempool.h"
#include "util.h"
}

#include <e32base.h>
#include <eikdialg.h>

#include "symgmmgr.h"

class CXWordsAppView;

enum {
    SYM_QUERY_CONFIRM_DELGAME = QUERY_LAST_COMMON
};

class CXSavedGamesDlg : public CEikDialog
{
 public:
    static TBool DoGamesPicker( MPFORMAL CXWordsAppView* aOwner,
                                CXWGamesMgr* aGameMgr, 
                                const TGameName* aCurName, TGameName* result );

 private:
    CXSavedGamesDlg( MPFORMAL CXWordsAppView* aOwner, CXWGamesMgr* aGameMgr, 
                     const TGameName* aCurName, TGameName* result );

    TBool OkToExitL( TInt aKeyCode );
    void PreLayoutDynInitL();

    void ResetNames( TInt aPrefIndex, const TGameName* aSelName );
    TBool EditSelName( const TGameName* aSelName, TGameName* aNewName );

    CXWordsAppView* iOwner;/* uses: don't own this!!! */
    CXWGamesMgr* iGameMgr; /* I don't own this */
    const TGameName* iCurName;
    TGameName* iResultP;        /* ditto */
    MPSLOT
};


#endif
