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

#include <eikedwin.h>
#include <eikmfne.h> 
#if defined SERIES_60
# include "xwords_60.rsg"
#elif defined SERIES_80
# include <eikchlst.h>
# include "xwords_80.rsg"
#endif
#include <eiktxlbm.h>

#include "symgmdlg.h"
#include "symutil.h"
#include "symgamed.h"
#include "xwords.hrh"
#include "xwappview.h"


CXSavedGamesDlg::CXSavedGamesDlg( MPFORMAL CXWordsAppView* aOwner, 
                                  CXWGamesMgr* aGameMgr, 
                                  const TGameName* aCurName, 
                                  TGameName* result )
    : iOwner(aOwner), iGameMgr(aGameMgr), iCurName(aCurName), iResultP(result)
{
    MPASSIGN( this->mpool, mpool );
}

void
CXSavedGamesDlg::PreLayoutDynInitL()
{
    ResetNames( -1, iCurName );
}

TBool
CXSavedGamesDlg::OkToExitL( TInt aKeyCode )
{
#if defined SERIES_60
    return ETrue;
#elif defined SERIES_80
    TBool canReturn = EFalse;
    CEikTextListBox* box;
    const CListBoxView::CSelectionIndexArray* indices;
    box = static_cast<CEikTextListBox*>(Control(ESelGameChoice));
    TInt index = box->CurrentItemIndex();
    TDesC16 selName = (*iGameMgr->GetNames())[index];

    XP_LOGF( "CXSavedGamesDlg::OkToExitL(%d) called", aKeyCode );

    switch( aKeyCode ) {
    case XW_SGAMES_OPEN_COMMAND:
        indices = box->SelectionIndexes();
        if ( indices->Count() > 1 ) {
            XP_LOGF( "too many selected" );
        } else {
            /* Don't use indices: invalid when multi-select isn't on? */
            // TInt index = indices->At(0);
            XP_LOGF( "the %dth is selected:", index );
            *iResultP = (*iGameMgr->GetNames())[index];
            XP_LOGDESC16( iResultP );
            canReturn = ETrue;
        }
        break;

    case XW_SGAMES_RENAME_COMMAND:
        if ( 0 == selName.Compare(*iCurName) ) {
            iOwner->UserErrorFromID( R_ALERT_NO_RENAME_OPEN_GAME );
        } else {
            TGameName newName;
            if ( EditSelName( static_cast<TGameName*>(&selName), &newName ) ) {
                ResetNames( -1, &newName );
                box->DrawDeferred();
            }
        }
        break;

    case XW_SGAMES_DELETE_COMMAND: {
        XP_LOGF( "delete" );
        if ( 0 == selName.Compare(*iCurName) ) {
            iOwner->UserErrorFromID( R_ALERT_NO_DELETE_OPEN_GAME );
        } else if ( iOwner->UserQuery( (UtilQueryID)SYM_QUERY_CONFIRM_DELGAME, 
                                       NULL ) ) {
            if ( iGameMgr->DeleteSelected( index ) ) {
                ResetNames( index, NULL );
                box->DrawDeferred();
            }
        }
    }
        break;
    case EEikBidCancel:
        canReturn = ETrue;
    }

    return canReturn;
#endif
} /* OkToExitL */

void
CXSavedGamesDlg::ResetNames( TInt aPrefIndex, 
                             const TGameName* aSelName )
{
    /* PENDING aPrefIndex is a hint what to select next  */
#if defined SERIES_60
#elif defined SERIES_80
    CDesC16Array* names = iGameMgr->GetNames();
    TInt index = 0;             /* make compiler happy */
    const TGameName* seekName = NULL;

    if ( aPrefIndex >= 0 ) {
        index = aPrefIndex;
    } else if ( aSelName != NULL ) {
        seekName = aSelName;
    } else {
        seekName = iCurName;
    }
   
    if ( seekName != NULL && ( 0 != names->Find( *seekName, index ) ) ) {
        XP_LOGF( "Unable to find" );
        XP_LOGDESC16( seekName );
        XP_ASSERT( 0 );
        index = 0;              /* safe default if not found */
    }

    CEikTextListBox* box;
    box = static_cast<CEikTextListBox*>(Control(ESelGameChoice));

    box->Model()->SetItemTextArray( names );
    box->Model()->SetOwnershipType( ELbmDoesNotOwnItemArray );
    box->HandleItemAdditionL();

    box->SetCurrentItemIndex( index );
#endif
} /* ResetNames */

TBool
CXSavedGamesDlg::EditSelName( const TGameName* aSelName, TGameName* aNewName )
{
    aNewName->Copy( *aSelName );
    TBool renamed = EFalse;

    if ( CNameEditDlg::EditName( aNewName ) ) {
        if ( iGameMgr->Exists( aNewName ) ) {
            iOwner->UserErrorFromID( R_ALERT_RENAME_TARGET_EXISTS );
        } else if ( !iGameMgr->IsLegalName( aNewName ) ) {
            iOwner->UserErrorFromID( R_ALERT_RENAME_TARGET_BADNAME );
        } else {
            iGameMgr->Rename( aSelName, aNewName );
            renamed = ETrue;
        }
    }
    return renamed;
}

/* static */ TBool
CXSavedGamesDlg::DoGamesPicker( MPFORMAL CXWordsAppView* aOwner, 
                                CXWGamesMgr* aGameMgr, 
                                const TGameName* aCurName, TGameName* result )
{
    CXSavedGamesDlg* me = new CXSavedGamesDlg( MPPARM(mpool) aOwner,
                                               aGameMgr, aCurName, result );
    User::LeaveIfNull( me );

    return me->ExecuteLD( R_XWORDS_SAVEDGAMES_DLG );
} // DoGamesPicker
