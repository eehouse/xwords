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
#ifndef XWFEATURE_STANDALONE_ONLY

#include "symssock.h"
#include "symutil.h"

void
CSendSocket::RunL()
{
    XP_LOGF( "CSendSocket::RunL called; iStatus=%d", iStatus.Int() );
/*     iSendTimer.Cancel(); */

    switch ( iSSockState ) {
    case ELookingUp:
        iResolver.Close();      /* we probably won't need this again */
        XP_ASSERT( iStatus.Int() == KErrNone );
        if ( iStatus == KErrNone ) {
            iNameRecord = iNameEntry();
            XP_LOGF( "name resolved: now:" );
            ConnectL( TInetAddr::Cast(iNameRecord.iAddr).Address() );
        }
        break;
    case EConnecting:
        XP_ASSERT( iStatus.Int() == KErrNone );
        if ( iStatus == KErrNone ) {
            iSSockState = EConnected;
            XP_LOGF( "connect successful" );
            if ( iSendBuf.Length() > 0 ) {
                DoActualSend();
            }
        }
        break;
    case ESending:
        XP_ASSERT( iStatus.Int() == KErrNone );
        if ( iStatus == KErrNone ) {
            iSSockState = EConnected;
            iSendBuf.SetLength(0);
            XP_LOGF( "send successful" );

            /* Send was successful.  Need to tell anybody?  Might want to
               update display somehow. */
        }
        break;
    }
} /* RunL */

void
CSendSocket::DoCancel()
{
    if ( iSSockState == ESending ) {
        iSendSocket.CancelWrite();
        iSSockState = EConnected;
    } else if ( iSSockState == EConnecting ) {
        iSendSocket.CancelConnect();
        iSSockState = ENotConnected;
    } else if ( iSSockState == ELookingUp ) {
        iResolver.Cancel();
        iResolver.Close();
    }
} /* DoCancel */

CSendSocket::CSendSocket()
    : CActive(EPriorityStandard)
     ,iSSockState(ENotConnected)
     ,iAddrSet( EFalse )
{
}

CSendSocket::~CSendSocket()
{
}

void
CSendSocket::ConstructL()
{
	CActiveScheduler::Add( this );
    XP_LOGF( "calling iSocketServer.Connect()" );
    TInt err = iSocketServer.Connect();
    XP_LOGF( "Connect=>%d", err );
	User::LeaveIfError( err );
}

/*static*/ CSendSocket*
CSendSocket::NewL()
{
    CSendSocket* me = new CSendSocket();
    CleanupStack::PushL( me );
    me->ConstructL();
    CleanupStack::Pop( me );
    return me;
}

void 
CSendSocket::ConnectL( TUint32 aIpAddr )
{
    XP_LOGF( "ConnectL( 0x%x )", aIpAddr );

    TInt err = iSendSocket.Open( iSocketServer, KAfInet, 
                            KSockDatagram, KProtocolInetUdp );
    XP_LOGF( "iSocket.Open => %d", err );
    User::LeaveIfError( err );

    // Set up address information
    iAddress.SetPort( iCurAddr.u.ip.port );
    iAddress.SetAddress( aIpAddr );

    // Initiate socket connection
    iSendSocket.Connect( iAddress, iStatus );
    iSSockState = EConnecting;
        
    // Start a timeout
    /* 	    iSendTimer.After( iConnectTimeout ); */

} /* ConnectL */

void 
CSendSocket::ConnectL()
{
    XP_LOGF( "CSendSocket::ConnectL" );
    if ( iSSockState == ENotConnected ) {
        TInetAddr ipAddr;

        XP_LOGF( "connecting to %s", iCurAddr.u.ip.hostName );

        TBuf16<MAX_HOSTNAME_LEN> tbuf;
        tbuf.Copy( TBuf8<MAX_HOSTNAME_LEN>(iCurAddr.u.ip.hostName) );
        TInt err = ipAddr.Input( tbuf );
        XP_LOGF( "ipAddr.Input => %d", err );

        if ( err != KErrNone ) {
            /* need to look it up */
            err = iResolver.Open( iSocketServer, KAfInet, KProtocolInetUdp );
            XP_LOGF( "iResolver.Open => %d", err );
            User::LeaveIfError( err );

	        iResolver.GetByName( tbuf, iNameEntry, iStatus );
            iSSockState = ELookingUp;

            SetActive();
        } else {
            ConnectL( ipAddr.Address() );
        }
    }
} /* ConnectL */

void
CSendSocket::Disconnect()
{
    XP_LOGF( "CSendSocket::Disconnect" );
    Cancel();
    if ( iSSockState >= EConnected ) {
        iSendSocket.Close();
    }
    iSSockState = ENotConnected;
} /* Disconnect */

TBool
CSendSocket::SendL( const XP_U8* aBuf, XP_U16 aLen, const CommsAddrRec* aAddr )
{
    XP_LOGF( "CSendSocket::SendL called" );
    TBool success;

    XP_ASSERT( !IsActive() );
    XP_LOGF( "here" );
    if ( iSSockState == ESending ) {
        success = EFalse;
    } else if ( aLen > KMaxMsgLen ) {
        success = EFalse;
    } else {
        XP_ASSERT( iSendBuf.Length() == 0 );
        iSendBuf.Copy( aBuf, aLen );
        XP_LOGF( "here 1" );

        if ( iAddrSet && (0 != XP_MEMCMP( (void*)&iCurAddr, (void*)aAddr, 
                                          sizeof(aAddr) )) ) {
            Disconnect();
        }
        XP_ASSERT( !iAddrSet );
        XP_LOGF( "here 2: aAddr = 0x%x", aAddr );
        iCurAddr = *aAddr;
        iAddrSet = ETrue;
        XP_LOGF( "here 3" );

        if ( iSSockState == ENotConnected ) {
            ConnectL();
        } else if ( iSSockState == EConnected ) {
            DoActualSend();
        }
        success = ETrue;
    }

    return success;
} /* SendL */

void
CSendSocket::DoActualSend()
{
    XP_LOGF( "CSendSocket::DoActualSend called" );
    iSendSocket.Write( iSendBuf, iStatus ); // Initiate actual write
    SetActive();
    
    // Request timeout
/*     iSendTimer.After( iWriteTimeout ); */
    iSSockState = ESending;
}

#endif
