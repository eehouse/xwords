/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 2005 by Eric House (fixin@peak.org).  (based on sample
 * app helloworldbasic "Copyright (c) 2002, Nokia. All rights
 * reserved.")
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

#include "xwords.hrh"
#include "symblnk.h"

#include "xwords.rsg"

CXWBlankSelDlg::CXWBlankSelDlg( const XP_UCHAR4* aTexts, 
                                TInt aNTiles, TInt* aResultP )
    :iTexts(aTexts), 
     iNTiles(aNTiles), 
     iResultP(aResultP), 
     iFacesList(NULL)
{
    // nothing else
}

void
CXWBlankSelDlg::PreLayoutDynInitL()
{
    // stuff the array
    CEikChoiceList* list;
    CDesC16ArrayFlat* facesList = new (ELeave)CDesC16ArrayFlat( iNTiles );

    TInt i;
    for ( i = 0; i < iNTiles; ++i ) {
        TBuf8<4> buf8( iTexts[i] );
        TBuf16<4> buf16;
        buf16.Copy( buf8 );
        facesList->AppendL( buf16 );
    }

    list = static_cast<CEikChoiceList*>(Control(ESelBlankChoice));
    list->SetArrayExternalOwnership( EFalse );
    list->SetArrayL( facesList );
}

TBool
CXWBlankSelDlg::OkToExitL( TInt aKeyCode )
{
    CEikChoiceList* list = static_cast<CEikChoiceList*>
        (Control(ESelBlankChoice));
    *iResultP = list->CurrentItem();
    return ETrue;
} // OkToExitL

/* static */
void 
CXWBlankSelDlg::UsePickTileDialogL( const XP_UCHAR4* texts, TInt aNTiles,
                                    TInt* resultP )
{
    CXWBlankSelDlg* dlg = new (ELeave) CXWBlankSelDlg( texts, aNTiles,
                                                       resultP );
    (void)dlg->ExecuteLD( R_XWORDS_BLANK_PICKER );
} // UsePickTileDialogL
