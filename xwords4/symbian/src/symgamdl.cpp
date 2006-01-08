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

#include <eikedwin.h>
#include <eikmfne.h> 
#if defined SERIES_60
# include "xwords_60.rsg"
#elif defined SERIES_80
# include <eikchlst.h>
# include "xwords_80.rsg"
#endif

#include "symgamdl.h"
#include "symutil.h"
#include "xwords.hrh"

/***************************************************************************
 * TGameInfoBuf
 ***************************************************************************/
TGameInfoBuf::TGameInfoBuf( const CurGameInfo* aGi, 
#ifndef XWFEATURE_STANDALONE_ONLY
                            const CommsAddrRec* aCommsAddr,
#endif
                            CDesC16ArrayFlat* aDictList )
    : iDictList(aDictList), 
     iDictIndex(0)
{
    TInt i;

    for ( i = 0; i < MAX_NUM_PLAYERS; ++i ) {
        iIsRobot[i] = aGi->players[i].isRobot;
        iIsLocal[i] = aGi->players[i].isLocal;
        if ( aGi->players[i].name != NULL ) {
            XP_LOGF( "name[%d] = %s", i, aGi->players[i].name );
            TBuf8<32> tmp( aGi->players[i].name );
            iPlayerNames[i].Copy( tmp );
        } else {
            iPlayerNames[i].Copy( _L("") );
        }
    }
    iNPlayers = aGi->nPlayers;

    if ( aGi->dictName != NULL ) {
        TBuf8<32> tmp( aGi->dictName );
        TBuf16<32> dictName;
        dictName.Copy( tmp );
        (void)iDictList->Find( dictName, iDictIndex ); /*iDictIndex ref passed*/
    }

#ifndef XWFEATURE_STANDALONE_ONLY
    iServerRole = aGi->serverRole;
    iCommsAddr = *aCommsAddr;
#endif
} /* TGameInfoBuf::TGameInfoBuf */

void
TGameInfoBuf::CopyToL( MPFORMAL CurGameInfo* aGi
#ifndef XWFEATURE_STANDALONE_ONLY
                       , CommsAddrRec* aCommsAddr
#endif
 )
{
    TInt i;
    for ( i = 0; i < MAX_NUM_PLAYERS; ++i ) {
        aGi->players[i].isRobot = iIsRobot[i];
        aGi->players[i].isLocal = iIsLocal[i];

        symReplaceStrIfDiff( MPPARM(mpool) &aGi->players[i].name, 
                             iPlayerNames[i] );
    }

    aGi->nPlayers = SC( XP_U8, iNPlayers );

    TPtrC16 dictName = (*iDictList)[iDictIndex];
    symReplaceStrIfDiff( MPPARM(mpool) &aGi->dictName, dictName );

#ifndef XWFEATURE_STANDALONE_ONLY
    aGi->serverRole = iServerRole;
    *aCommsAddr = iCommsAddr;
#endif
}

/***************************************************************************
 * CXWGameInfoDlg
 ***************************************************************************/
CXWGameInfoDlg::CXWGameInfoDlg( MPFORMAL TGameInfoBuf* aGib, TBool aNewGame )
    : iIsNewGame(aNewGame), iGib(aGib)
{
/*     XP_LOGF( "CXWGameInfoDlg::CXWGameInfoDlg" ); */
    MPASSIGN( this->mpool, mpool );
}

CXWGameInfoDlg::~CXWGameInfoDlg()
{
/*     XP_LOGF( "CXWGameInfoDlg::~CXWGameInfoDlg" ); */
}

void 
CXWGameInfoDlg::PreLayoutDynInitL()
{
/*     XP_LOGF( "CXWGameInfoDlg::PreLayoutDynInitL" ); */
#if defined SERIES_80

    /* This likely belongs in its own method */
#ifndef XWFEATURE_STANDALONE_ONLY
    const TInt deps[] = { 
        EConnectionRole
    };
    TInt i;
    for ( i = 0; i < sizeof(deps)/sizeof(deps[0]); ++i ) {
        HandleControlStateChangeL( deps[i] );
    }
#endif

    CEikChoiceList* list;
    list = static_cast<CEikChoiceList*>(Control(ENPlayersList));
    XP_ASSERT( list != NULL );
    list->SetCurrentItem( iGib->iNPlayers - 1 );

    list = static_cast<CEikChoiceList*>(Control(ENPlayersWhichList));
    XP_ASSERT( list != NULL );
    list->SetArrayExternalOwnership( EFalse );
    list->SetArrayL( MakeNumListL( 1, iGib->iNPlayers ) );

    list = static_cast<CEikChoiceList*>(Control(ESelDictChoice));
    XP_ASSERT( list != NULL );
    list->SetArrayL( iGib->iDictList );
    list->SetCurrentItem( iGib->iDictIndex );

    iCurPlayerShown = 0;
    LoadPlayerInfo( iCurPlayerShown );

#ifndef XWFEATURE_STANDALONE_ONLY
    list = static_cast<CEikChoiceList*>(Control(EConnectionRole));
    XP_ASSERT( list != NULL );
    TInt sel = (TInt)iGib->iServerRole;
    list->SetCurrentItem( sel );
    list->DrawDeferred();

    list = static_cast<CEikChoiceList*>(Control(EConnectionType));
    XP_ASSERT( list != NULL );
    sel = (TInt)(iGib->iCommsAddr.conType) - 2; /* first 2 unused */
    XP_ASSERT( sel >= 0 );
    list->SetCurrentItem( sel );
    list->DrawDeferred();

    CEikEdwin* edwin = static_cast<CEikEdwin*>(Control(ECookie));
    TBuf16<MAX_COOKIE_LEN> cookieBuf;
    cookieBuf.Copy( TBuf8<MAX_COOKIE_LEN>
                    (iGib->iCommsAddr.u.ip_relay.cookie) );
    edwin->SetTextL( &cookieBuf );
    edwin->DrawDeferred();
    
    edwin = static_cast<CEikEdwin*>(Control(ERelayName));
    TBuf16<MAX_HOSTNAME_LEN> nameBuf;
    nameBuf.Copy( TBuf8<MAX_HOSTNAME_LEN>
                  (iGib->iCommsAddr.u.ip_relay.hostName) );
    edwin->SetTextL( &nameBuf );
    edwin->DrawDeferred();
    
    TInt num = iGib->iCommsAddr.u.ip_relay.port;
    CEikNumberEditor* hostPort
        = static_cast<CEikNumberEditor*>(Control(ERelayPort));
    hostPort->SetNumber( num );
    hostPort->DrawDeferred();

    HideAndShow();
#endif
#endif
} /* PreLayoutDynInitL */

void
CXWGameInfoDlg::HideAndShow()
{
/*     XP_LOGF( "HideAndShow" ); */
#if defined SERIES_80
    /* if it's standalone, hide all else.  Then if it's not IP, hide all
       below. */

#ifndef XWFEATURE_STANDALONE_ONLY
    CEikChoiceList* list;
    TBool showConnect;
    list = static_cast<CEikChoiceList*>(Control(EConnectionRole));
    XP_ASSERT( list != NULL );
    showConnect = list->CurrentItem() != 0;

    TBool showIP = showConnect;
    if ( showIP ) {
        list = static_cast<CEikChoiceList*>(Control(EConnectionType));
        XP_ASSERT( list != NULL );
        showIP = list->CurrentItem() == 0;
    }

    MakeLineVisible( EConnectionType, showConnect );
    MakeLineVisible( ECookie, showIP );
    MakeLineVisible( ERelayName, showIP );
    MakeLineVisible( ERelayPort, showIP );
#endif
#endif
} /* HideAndShow */

void
CXWGameInfoDlg::HandleControlStateChangeL( TInt aControlId )
{
/*     XP_LOGF( "HandleControlStateChangeL got %d", aControlId ); */
#if defined SERIES_80
    CEikChoiceList* list;
    CEikChoiceList* whichList;
    TInt item;
    TBool show;

    switch ( aControlId ) {
#ifndef XWFEATURE_STANDALONE_ONLY
    case EConnectionRole:
    case EConnectionType:
        HideAndShow();
        break;
#endif
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
        HandleControlStateChangeL( EPlayerLocationChoice );
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
#endif
}

TBool 
CXWGameInfoDlg::OkToExitL( TInt /*aKeyCode*/ )
{
#if defined SERIES_80
    CEikChoiceList* list;

    /* Dictionary */
    list = static_cast<CEikChoiceList*>(Control(ESelDictChoice));
    iGib->iDictIndex = list->CurrentItem();

    /* Player data (all but displayed already saved) */
    SavePlayerInfo( iCurPlayerShown );

    /* number of players */
    list = static_cast<CEikChoiceList*>(Control(ENPlayersList));
    iGib->iNPlayers = list->CurrentItem() + 1;
#endif

#ifndef XWFEATURE_STANDALONE_ONLY
    list = static_cast<CEikChoiceList*>(Control(EConnectionRole));
    TInt sel = list->CurrentItem();
    iGib->iServerRole = (Connectedness)sel;

    if ( iGib->iServerRole != SERVER_STANDALONE ) {

        list = static_cast<CEikChoiceList*>(Control(EConnectionType));
        iGib->iCommsAddr.conType = SC(CommsConnType, list->CurrentItem() + 2 );
    
        if ( iGib->iCommsAddr.conType == COMMS_CONN_RELAY ) {
            /* cookie */
            CEikEdwin* edwin = static_cast<CEikEdwin*>(Control(ECookie));
            TBuf16<MAX_COOKIE_LEN> cookieBuf;
            edwin->GetText( cookieBuf );
            TBuf8<MAX_COOKIE_LEN> buf8cookie;
            buf8cookie.Copy( cookieBuf );
            TInt len = buf8cookie.Length();
            XP_MEMCPY( iGib->iCommsAddr.u.ip_relay.cookie,
                       (void*)buf8cookie.Ptr(), len );
            iGib->iCommsAddr.u.ip_relay.cookie[len] = '\0';

            /* hostname */
            edwin = static_cast<CEikEdwin*>(Control(ERelayName));
            TBuf16<MAX_HOSTNAME_LEN> nameBuf;
            edwin->GetText( nameBuf );
            TBuf8<MAX_HOSTNAME_LEN> buf8;
            buf8.Copy( nameBuf );
            len = buf8.Length();
            XP_MEMCPY( iGib->iCommsAddr.u.ip_relay.hostName,
                       (void*)buf8.Ptr(), len );
            iGib->iCommsAddr.u.ip_relay.hostName[len] = '\0';
    
            /* port */
            CEikNumberEditor* hostPort
                = static_cast<CEikNumberEditor*>(Control(ERelayPort));
            iGib->iCommsAddr.u.ip_relay.port = SC( XP_U16, hostPort->Number() );
        }
    }
#endif
    return ETrue;
} /* OkToExitL */

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

#if defined SERIES_80
    /* species */
    CEikChoiceList* list = static_cast<CEikChoiceList*>
        (Control(EPlayerSpeciesChoice ));
    TInt oldVal = list->CurrentItem();
    TInt newVal = iGib->iIsRobot[aWhich]? 1:0;
    if ( oldVal != newVal ) {
        list->SetCurrentItem( newVal );
        list->DrawDeferred();
    }

    /* remoteness */
    list = static_cast<CEikChoiceList*>(Control(EPlayerLocationChoice ));
    oldVal = list->CurrentItem();
    newVal = iGib->iIsLocal[aWhich]? 0:1;
    if ( oldVal != newVal ) {
        list->SetCurrentItem( newVal );
        list->DrawDeferred();
    }
#endif

    /* password */
}

void
CXWGameInfoDlg::SavePlayerInfo( TInt aWhich )
{

    /* name */
    CEikEdwin* nameEditor = static_cast<CEikEdwin*>(Control(EPlayerName));
    nameEditor->GetText( iGib->iPlayerNames[aWhich] );
    
    /* species */
#if defined SERIES_80
    CEikChoiceList* list = static_cast<CEikChoiceList*>
        (Control(EPlayerSpeciesChoice ));
    iGib->iIsRobot[aWhich] = (list->CurrentItem() == 1);

    list = static_cast<CEikChoiceList*>(Control(EPlayerLocationChoice ));
    iGib->iIsLocal[aWhich] = (list->CurrentItem() == 0);
#endif
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
    for ( i = aFirst; i <= aLast; ++i ) {
        TBuf16<4> num;
        num.Num( i );
        list->AppendL( num );
    }

    return list;
} /* MakeNumListL */

/* static*/ TBool
CXWGameInfoDlg::DoGameInfoDlgL( MPFORMAL TGameInfoBuf* aGib, TBool aNewGame )
{
    XP_LOGF( "CXWGameInfoDlg::DoGameInfoDlgL called" );
    CXWGameInfoDlg* me = 
        new(ELeave)CXWGameInfoDlg( MPPARM(mpool) aGib, aNewGame );
    XP_LOGF( "calling ExecuteLD" );
    return me->ExecuteLD( R_XWORDS_NEWGAME_DLG );
}
