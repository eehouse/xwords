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
#include "symutil.h"
#include "xwords.hrh"

/***************************************************************************
 * TGameInfoBuf
 ***************************************************************************/
TGameInfoBuf::TGameInfoBuf( const CurGameInfo* aGi, 
                            CDesC16ArrayFlat* aDictList )
    : iDictList(aDictList)
{
    TInt i;

    for ( i = 0; i < MAX_NUM_PLAYERS; ++i ) {
        iIsRobot[i] = aGi->players[i].isRobot;

        TBuf8<32> tmp( aGi->players[i].name );
        iPlayerNames[i].Copy( tmp );
    }
    iNPlayers = aGi->nPlayers;

    if ( aGi->dictName != NULL ) {
        TBuf8<32> tmp( aGi->dictName );
        TBuf16<32> dictName;
        dictName.Copy( tmp );
        TInt found = iDictList->Find( dictName, 
                                      iDictIndex ); /* iDictIndex ref passed */
        XP_ASSERT( found == 0 );
    }
} /* TGameInfoBuf::TGameInfoBuf */

void
TGameInfoBuf::CopyToL( MPFORMAL CurGameInfo* aGi )
{
    TInt i;
    for ( i = 0; i < MAX_NUM_PLAYERS; ++i ) {
        aGi->players[i].isRobot = iIsRobot[i];

        symReplaceStrIfDiff( MPPARM(mpool) &aGi->players[i].name, 
                             iPlayerNames[i] );
    }

    aGi->nPlayers = iNPlayers;

    TPtrC16 dictName = (*iDictList)[iDictIndex];
    symReplaceStrIfDiff( MPPARM(mpool) &aGi->dictName, dictName );
}

/***************************************************************************
 * CXWGameInfoDlg
 ***************************************************************************/
CXWGameInfoDlg::CXWGameInfoDlg( MPFORMAL TGameInfoBuf* aGib, TBool aNewGame )
    : iGib(aGib), iIsNewGame(aNewGame)
{
    MPASSIGN( this->mpool, mpool );
}

CXWGameInfoDlg::~CXWGameInfoDlg()
{
}

void 
CXWGameInfoDlg::PreLayoutDynInitL()
{
    TInt i;
    CEikChoiceList* list;

    /* This likely belongs in its own method */
    const TInt deps[] = { EConnectionRole
    };
    for ( i = 0; i < sizeof(deps)/sizeof(deps[0]); ++i ) {
        HandleControlStateChangeL( deps[i] );
    }

    list = static_cast<CEikChoiceList*>(Control(ENPlayersList));
    list->SetCurrentItem( iGib->iNPlayers - 1 );

    list = static_cast<CEikChoiceList*>(Control(ENPlayersWhichList));
    list->SetArrayExternalOwnership( EFalse );
    list->SetArrayL( MakeNumListL( 1, iGib->iNPlayers ) );

    list = static_cast<CEikChoiceList*>(Control(ESelDictChoice));
    list->SetArrayL( iGib->iDictList );
    list->SetArrayExternalOwnership( ETrue );
    list->SetCurrentItem( iGib->iDictIndex );

    iCurPlayerShown = 0;
    LoadPlayerInfo( iCurPlayerShown );

} /* PreLayoutDynInitL */

void
CXWGameInfoDlg::HandleControlStateChangeL( TInt aControlId )
{
    XP_LOGF( "HandleControlStateChangeL got %d", aControlId );
    CEikChoiceList* list;
    CEikChoiceList* whichList;
    TInt item;
    TBool show;

    switch ( aControlId ) {
    case EConnectionRole:
        /* Hide EConnectionType if it's standalone */
        list = static_cast<CEikChoiceList*>(Control(EConnectionRole));

        MakeLineVisible( EConnectionType, list->CurrentItem() != 0 );
        break;

    case ENPlayersList:
        /* The ENPlayersWhichList must match the number of players available,
           and we need to display a different player if we're currently
           showing one who no longer exists. */
        list = static_cast<CEikChoiceList*>
            (Control( ENPlayersList ));
        item = list->CurrentItem();

        whichList = static_cast<CEikChoiceList*>
            (Control( ENPlayersWhichList ));
        whichList->SetArrayL( MakeNumListL( 1, item + 1 ) );

        if ( item < iCurPlayerShown ) {
            /* HandleControlStateChangeL seems not to get called for this
               change. But the DrawDeferred()s below make it ok */
            whichList->SetCurrentItem( item );
            SetPlayerShown( item );
        } else {
            whichList->SetCurrentItem( iCurPlayerShown );
        }
        whichList->DrawDeferred();

        break;

    case ENPlayersWhichList:
        list = static_cast<CEikChoiceList*>
            (Control( ENPlayersWhichList ));
        SetPlayerShown( list->CurrentItem() );
        break;

    case EPlayerLocationChoice:
        list = static_cast<CEikChoiceList*>
            (Control(EPlayerLocationChoice ));
        show = list->CurrentItem() == 0;
        
        MakeLineVisible( EPlayerName, show );
        MakeLineVisible( EPlayerSpeciesChoice, show );
        MakeLineVisible( EDecryptPassword, show );
        break;

    default:
        break;
    }
}

TBool 
CXWGameInfoDlg::OkToExitL( TInt aKeyCode )
{
    CEikChoiceList* list;

    /* Dictionary */
    list = static_cast<CEikChoiceList*>(Control(ESelDictChoice));
    iGib->iDictIndex = list->CurrentItem();

    /* Player data (all but displayed already saved) */
    SavePlayerInfo( iCurPlayerShown );

    /* number of players */
    list = static_cast<CEikChoiceList*>(Control(ENPlayersList));
    iGib->iNPlayers = list->CurrentItem() + 1;



    return ETrue;
}

void
CXWGameInfoDlg::LoadPlayerInfo( TInt aWhich )
{
    /* location */
#ifndef XWFEATURE_STANDALONE_ONLY
#endif

    /* name */
    XP_LOGF( "setting name" );
    CEikEdwin* nameEditor = static_cast<CEikEdwin*>(Control(EPlayerName));
    nameEditor->SetTextL( &iGib->iPlayerNames[aWhich] );
    nameEditor->DrawDeferred();

    XP_LOGF( "done setting name" );

    /* species */
    CEikChoiceList* list = static_cast<CEikChoiceList*>
        (Control(EPlayerSpeciesChoice ));
    TInt oldVal = list->CurrentItem();
    TInt newVal = iGib->iIsRobot[aWhich]? 1:0;
    if ( oldVal != newVal ) {
        list->SetCurrentItem( newVal );
        list->DrawDeferred();
    }

    /* password */
}

void
CXWGameInfoDlg::SavePlayerInfo( TInt aWhich )
{

    /* name */
    CEikEdwin* nameEditor = static_cast<CEikEdwin*>(Control(EPlayerName));
    nameEditor->GetText( iGib->iPlayerNames[aWhich] );
    
    /* species */
    CEikChoiceList* list = static_cast<CEikChoiceList*>
        (Control(EPlayerSpeciesChoice ));
    iGib->iIsRobot[aWhich] = (list->CurrentItem() == 1);
}

void
CXWGameInfoDlg::SetPlayerShown( TInt aPlayer )
{
    if ( aPlayer != iCurPlayerShown ) {
        SavePlayerInfo( iCurPlayerShown );
        LoadPlayerInfo( aPlayer );
        iCurPlayerShown = aPlayer;
    }
}

CDesC16ArrayFlat*
CXWGameInfoDlg::MakeNumListL( TInt aFirst, TInt aLast )
{
    XP_ASSERT( aFirst <= aLast );

    CDesC16ArrayFlat* list = new (ELeave)CDesC16ArrayFlat(aLast - aFirst + 1);
    TInt i;        
    char str[2] = { 0, 0 };
    for ( i = aFirst; i <= aLast; ++i ) {
        str[0] = '0' + i;
        TBuf8<4> buf8( (unsigned char*)str );
        TBuf16<4> buf16;
        buf16.Copy( buf8 );
        list->AppendL( buf16 );
    }

    return list;
} /* MakeNumListL */
