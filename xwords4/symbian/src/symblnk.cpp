/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 2005 by Eric House (xwords@eehouse.org).  (based on sample
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
#if defined SERIES_60
# include <aknlistquerycontrol.h>
# include "xwords_60.rsg"
#elif defined SERIES_80
# include <eikchlst.h>
# include "xwords_80.rsg"
#endif

#include "xwords.hrh"
#include "symblnk.h"


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
#if defined SERIES_80
    CEikChoiceList* list;
#elif defined SERIES_60
    CAknListQueryControl* list;
#endif
    CDesC16ArrayFlat* facesList = new (ELeave)CDesC16ArrayFlat( iNTiles );

    TInt i;
    for ( i = 0; i < iNTiles; ++i ) {
        TBuf16<4> buf16;
        buf16.Copy( TPtrC8(iTexts[i]) );
        facesList->AppendL( buf16 );
    }

#if defined SERIES_80
    list = static_cast<CEikChoiceList*>(Control(ESelBlankChoice));
    list->SetArrayExternalOwnership( EFalse );
    list->SetArrayL( facesList );
#elif defined SERIES_60
    list = static_cast<CAknListQueryControl*>(Control(ESelBlankChoice));
#endif
}

TBool
CXWBlankSelDlg::OkToExitL( TInt /*aKeyCode*/ )
{
#if defined SERIES_80
    CEikChoiceList* list = static_cast<CEikChoiceList*>
        (Control(ESelBlankChoice));
    *iResultP = list->CurrentItem();
#endif
    return ETrue;
} // OkToExitL

/* static */ void 
CXWBlankSelDlg::UsePickTileDialogL( const XP_UCHAR4* texts, TInt aNTiles,
                                    TInt* resultP )
{
    CXWBlankSelDlg* dlg = new (ELeave) CXWBlankSelDlg( texts, aNTiles,
                                                       resultP );
    (void)dlg->ExecuteLD( R_XWORDS_BLANK_PICKER );
} // UsePickTileDialogL
