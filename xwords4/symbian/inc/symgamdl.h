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

#ifndef _SYMGAMDL_H_
#define _SYMGAMDL_H_

extern "C" {
#include "comtypes.h"
#include "xwstream.h"
#include "mempool.h"
#include "game.h"
#include "comms.h"
}

#include <e32base.h>
#include <eikdialg.h>

class TGameInfoBuf
{
 public:
    TGameInfoBuf::TGameInfoBuf( const CurGameInfo* aGi,
#ifndef XWFEATURE_STANDALONE_ONLY
                                const CommsAddrRec* aCommsAddr,
#endif
                                CDesC16ArrayFlat* aDictList );
    CDesC16ArrayFlat* GetDictList() { return iDictList; }
    void CopyToL( MPFORMAL CurGameInfo* aGi
#ifndef XWFEATURE_STANDALONE_ONLY
                  , CommsAddrRec* aCommsAddr 
#endif
                  );

    TBool iIsRobot[MAX_NUM_PLAYERS];
    TBool iIsLocal[MAX_NUM_PLAYERS];

    TBuf16<32> iPlayerNames[MAX_NUM_PLAYERS];

    CDesC16ArrayFlat* iDictList; /* owned externally! */
    TInt iDictIndex;
    TInt iNPlayers;

#ifndef XWFEATURE_STANDALONE_ONLY
    Connectedness iServerRole;
    CommsAddrRec iCommsAddr;
#endif
};

class CXWGameInfoDlg : public CEikDialog  /* CEikForm instead? */
{
 public:
    static TBool DoGameInfoDlgL( MPFORMAL TGameInfoBuf* aGib, 
                                 TBool aNewGame );

    ~CXWGameInfoDlg();

 private:
    CXWGameInfoDlg( MPFORMAL TGameInfoBuf* aGib, TBool aNewGame );

 private:
    void PreLayoutDynInitL();
    void HandleControlStateChangeL( TInt aControlId );
    TBool OkToExitL( TInt aKeyCode );

    void LoadPlayerInfo( TInt aWhich );
    void SavePlayerInfo( TInt aWhich );
    void SetPlayerShown( TInt aPlayer );
    void HideAndShow();

    CDesC16ArrayFlat* MakeNumListL( TInt aFirst, TInt aLast );

    TBool iIsNewGame;
    TGameInfoBuf* iGib;

    TInt iCurPlayerShown;

    MPSLOT
};

#endif
