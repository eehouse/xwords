/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 2005 by Eric House (fixin@peak.org).
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
#include <eiktxlbm.h>

#include "symgmdlg.h"
#include "symutil.h"
#include "xwords.hrh"
#include "xwords.rsg"


CXSavedGamesDlg::CXSavedGamesDlg( MPFORMAL CXWGamesMgr* aGameMgr, 
                                  const TGameName* aCurName, TGameName* result )
    : iGameMgr(aGameMgr), iCurName(aCurName), iResultP(result)
{
    MPASSIGN( this->mpool, mpool );
}

void
CXSavedGamesDlg::PreLayoutDynInitL()
{
    ResetNames(-1);
}

TBool
CXSavedGamesDlg::OkToExitL( TInt aKeyCode )
{
    TBool canReturn = EFalse;
    TInt index;
    CEikTextListBox* box;
    const CListBoxView::CSelectionIndexArray* indices;
    box = static_cast<CEikTextListBox*>(Control(ESelGameChoice));

    XP_LOGF( "CXSavedGamesDlg::OkToExitL(%d) called", aKeyCode );

    switch( aKeyCode ) {
    case XW_SGAMES_OPEN_COMMAND:
        indices = box->SelectionIndexes();
        if ( indices->Count() > 1 ) {
            XP_LOGF( "too many selected" );
        } else {
            /* Don't use indices: invalid when multi-select isn't on? */
            // TInt index = indices->At(0);
            index = box->CurrentItemIndex();
            XP_LOGF( "the %dth is selected:", index );
            *iResultP = (*iGameMgr->GetNames())[index];
            XP_LOGDESC16( iResultP );
            canReturn = ETrue;
        }
        break;

    case XW_SGAMES_RENAME_COMMAND:
        EditSelName();
        break;

    case XW_SGAMES_DELETE_COMMAND: {
        XP_LOGF( "delete" );
        index = box->CurrentItemIndex();
        TDesC16* selName = &(*iGameMgr->GetNames())[index];
        if ( 0 == selName->Compare(*iCurName) ) {
            /* Warn user why can't delete */
        } else if ( ConfirmDelete() ) {
            if ( iGameMgr->DeleteSelected( index ) ) {
                ResetNames( index );
                box->DrawDeferred();
            }
        }
    }
        break;
    case EEikBidCancel:
        canReturn = ETrue;
    }

    return canReturn;
} /* OkToExitL */

void
CXSavedGamesDlg::ResetNames( TInt aPrefIndex )
{
    /* PENDING aPrefIndex is a hint what to select next  */

    CDesC16Array* names = iGameMgr->GetNames();
    TInt index;

    if ( 0 != names->Find( *iCurName, index ) ) {
        XP_LOGF( "Unable to find" );
        XP_LOGDESC16( iCurName );
        XP_ASSERT( 0 );
        index = 0;              /* safe default if not found */
    }

    CEikTextListBox* box;
    box = static_cast<CEikTextListBox*>(Control(ESelGameChoice));

    box->Model()->SetItemTextArray( names );
    box->Model()->SetOwnershipType( ELbmDoesNotOwnItemArray );
    box->HandleItemAdditionL();
    box->SetCurrentItemIndex( index );
} /* ResetNames */

TBool
CXSavedGamesDlg::ConfirmDelete()
{
    return ETrue;
} /* ConfirmDelete */

void
CXSavedGamesDlg::EditSelName()
{
}

/* static */ TBool
CXSavedGamesDlg::DoGamesPicker( MPFORMAL CXWGamesMgr* aGameMgr, 
                                const TGameName* aCurName, TGameName* result )
{
    CXSavedGamesDlg* me = new CXSavedGamesDlg( MPPARM(mpool)
                                               aGameMgr, aCurName, result );
    User::LeaveIfNull( me );

    return me->ExecuteLD( R_XWORDS_SAVEDGAMES_DLG );
} // DoGamesPicker
