/* -*-mode: C; fill-column: 78; c-basic-offset: 4;-*- */ 
/* 
 * Copyright 2005 by Eric House (fixin@peak.org).  All rights reserved.
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

#include <eikedwin.h>
#include <eikmfne.h> 
#include <eikchlst.h>

#include "symgamdl.h"
#include "xwords.hrh"

CXWGameInfoDlg::CXWGameInfoDlg( MPFORMAL_NOCOMMA )
{
    MPASSIGN( this->mpool, mpool );
}

CXWGameInfoDlg::~CXWGameInfoDlg()
{
}

void 
CXWGameInfoDlg::PreLayoutDynInitL()
{
    /* This likely belongs in its own method */
    const TInt deps[] = { EConnectionRole
                          ,EPlayerLocationChoice1
                          ,EPlayerLocationChoice2
                          ,EPlayerLocationChoice3
                          ,EPlayerLocationChoice4
    };
    for ( TInt i = 0; i < sizeof(deps)/sizeof(deps[0]); ++i ) {
        HandleControlStateChangeL( deps[i] );
    }
} /* PreLayoutDynInitL */

void
CXWGameInfoDlg::HandleControlStateChangeL( TInt aControlId )
{
    XP_LOGF( "HandleControlStateChangeL got %d", aControlId );
    CEikChoiceList* list;
    TInt index;

    switch ( aControlId ) {
    case EConnectionRole:
        /* Hide EConnectionType if it's standalone */
        list = static_cast<CEikChoiceList*>(Control(EConnectionRole));

        MakeLineVisible( EConnectionType, list->CurrentItem() != 0 );
        break;
    case EPlayerLocationChoice1:
    case EPlayerLocationChoice2:
    case EPlayerLocationChoice3:
    case EPlayerLocationChoice4: {
        index = aControlId - EPlayerLocationChoice1;
        list = static_cast<CEikChoiceList*>
            (Control(EPlayerLocationChoice1 + index ));
        TBool show = list->CurrentItem() == 0;
        
        MakeLineVisible( EPlayerName1 + index, show );
        MakeLineVisible( EPlayerSpeciesChoice1 + index, show );
        MakeLineVisible( EDecryptPassword1 + index, show );
    }
        break;
    default:
        break;
    }
}

TBool 
CXWGameInfoDlg::OkToExitL( TInt aKeyCode )
{
    return ETrue;
}
