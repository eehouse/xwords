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
#ifndef XWFEATURE_STANDALONE_ONLY

#include "symssock.h"
#include "symutil.h"

void
CSendSocket::RunL()
{
    XP_LOGF( "CSendSocket::RunL called; iStatus=%d", iStatus.Int() );
/*     iSendTimer.Cancel(); */

    TBool statusGood = iStatus.Int() == KErrNone;
    switch ( iSSockState ) {
    case ELookingUp:
        iResolver.Close();      /* we probably won't need this again */
        if ( statusGood ) {
            iNameRecord = iNameEntry();
            XP_LOGF( "name resolved" );
            ConnectL( TInetAddr::Cast(iNameRecord.iAddr).Address() );
        } else {
            ResetState();
        }
        break;
    case EConnecting:
        if ( statusGood ) {
            iSSockState = EConnected;
            XP_LOGF( "connect successful" );
            if ( iSendBuf.Length() > 0 ) {
                DoActualSend();
            } else if ( iListenPending ) {
                Listen();
            }
        } else {
            ResetState();
        }
        break;
    case ESending:
        if ( statusGood ) {
            XP_LOGF( "send successful" );
        } else {
            /* Depending on error, might need to close socket, reconnect, etc.
               For now we'll just trust the upper layers to figure out they
               didn't get through, or user to resend. */
            XP_LOGF( "send failed with error %d", iStatus.Int() );
        }

        iSSockState = EConnected;
        iSendBuf.SetLength(0);

        if ( iListenPending ) {
            Listen();
        }
        break;

    case EListening:
        if ( statusGood ) {
            if ( iDataLen == 0 ) {
                /* Do nothing; we need to read again via Listen call */
                iDataLen = XP_NTOHS( *(XP_U16*)iInBufDesc->Ptr() );
                XP_LOGF( "Recv succeeded with length; now looking for %d byte"
                         "packet", iDataLen );
            } else {
                iDataLen = 0;
                XP_LOGF( "Got packet! Calling callback" );
                (*iCallback)( iInBufDesc, iClosure );
            }
            iSSockState = EConnected;
            Listen();           /* go back to listening */
        } else {
            XP_LOGF( "listen failed with error %d", iStatus.Int() );
            ResetState();
        }
    }
} /* RunL */

void
CSendSocket::ResetState()
{
    iSendBuf.SetLength(0);
    iSSockState = ENotConnected;
}

TBool
CSendSocket::Listen()
{
    XP_LOGF( "CSendSocket::Listen" );
    iListenPending = ETrue;

    if ( IsActive() ) {
        /* since iListenPending is set, we'll eventually get to listening once
           all the RunL()s get called.  Do nothing. */
    } else {
        if ( iSSockState == ENotConnected ) {
            ConnectL();
        } else if ( iSSockState == EConnected ) {
            delete iInBufDesc;

            TInt seekLen = iDataLen == 0? 2: iDataLen;
            iInBufDesc = new TPtr8( iInBuf, seekLen );
            XP_LOGF( "calling iSendSocket.Recv; looking for %d bytes", 
                     iInBufDesc->MaxSize() );
            iSendSocket.Recv( *iInBufDesc, 0, iStatus );

            SetActive();
            iSSockState = EListening;
            iListenPending = EFalse;
        } else {
            XP_LOGF( "iSSockState=%d", iSSockState );
            XP_ASSERT( 0 );
        }
    }
    return ETrue;
} /* Listen */

TBool
CSendSocket::CancelListen()
{
    TBool result = iSSockState == EListening;
    if ( result ) {
        XP_ASSERT( IsActive() );
        Cancel();
    }
    return result;
} /* CancelListen */

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
    } else if ( iSSockState == EListening ) {
        iSendSocket.CancelRecv();        
    }
} /* DoCancel */

CSendSocket::CSendSocket( ReadNotifyCallback aCallback, 
                          void* aClosure )
    : CActive(EPriorityStandard)
     ,iSSockState(ENotConnected)
     ,iAddrSet( EFalse )
     ,iCallback(aCallback)
     ,iClosure(aClosure)
     ,iListenPending(EFalse)
     ,iInBufDesc( NULL )
     ,iDataLen( 0 )
{
}

CSendSocket::~CSendSocket()
{
    Cancel();
    iSocketServer.Close();
}

void
CSendSocket::ConstructL()
{
	CActiveScheduler::Add( this );
    XP_LOGF( "calling iSocketServer.Connect()" );
    TInt err = iSocketServer.Connect( 2 );
    XP_LOGF( "Connect=>%d", err );
	User::LeaveIfError( err );
}

/*static*/ CSendSocket*
CSendSocket::NewL( ReadNotifyCallback aCallback, void* aClosure )
{
    CSendSocket* me = new CSendSocket( aCallback, aClosure );
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
                                 KSockStream, KProtocolInetTcp );
    XP_LOGF( "iSocket.Open => %d", err );
    User::LeaveIfError( err );

    // Set up address information
    iAddress.SetPort( iCurAddr.u.ip_relay.port );
    iAddress.SetAddress( aIpAddr );

    // Initiate socket connection
    XP_LOGF( "calling iSendSocket.Connect" );
    iSendSocket.Connect( iAddress, iStatus );

    SetActive();
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

        if ( iCurAddr.u.ip_relay.hostName && iCurAddr.u.ip_relay.hostName[0] ) {

            XP_LOGF( "connecting to %s", iCurAddr.u.ip_relay.hostName );

            TBuf16<MAX_HOSTNAME_LEN> tbuf;
            tbuf.Copy( TBuf8<MAX_HOSTNAME_LEN>(iCurAddr.u.ip_relay.hostName) );
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
        } else {
            /* PENDING FIX THIS!!!! */
            XP_LOGF( "Can't connect: no relay name" );
        }
    }
} /* ConnectL */

void
CSendSocket::ConnectL( const CommsAddrRec* aAddr )
{
    /* If we're connected and the address is the same, do nothing.  Otherwise
       disconnect, change the address, and reconnect. */

    TBool sameAddr = iAddrSet && 
        (0 == XP_MEMCMP( (void*)&iCurAddr, (void*)aAddr, sizeof(aAddr) ) );

    if ( sameAddr && iSSockState >= EConnected ) {
        /* do nothing */
    } else {
        Disconnect();

        iCurAddr = *aAddr;
        iAddrSet = ETrue;

        ConnectL();    
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

    XP_ASSERT( iSSockState == EListening || !IsActive() );
    if ( iSSockState == ESending ) {
        success = EFalse;
    } else if ( aLen > KMaxMsgLen ) {
        success = EFalse;
    } else if ( iSendBuf.Length() != 0 ) {
        XP_LOGF( "old buffer not sent yet" );
        success = EFalse;
    } else {
        /* TCP-based protocol requires 16-bits of length, in network
           byte-order, followed by data. */
        XP_U16 netLen = XP_HTONS( aLen );
        iSendBuf.Append( (TUint8*)&netLen, sizeof(netLen) );
        iSendBuf.Append( aBuf, aLen );

        if ( iSSockState == ENotConnected ) {
            ConnectL( aAddr );
        } else if ( iSSockState == EConnected || iSSockState == EListening ) {
            DoActualSend();
        } else {
            XP_ASSERT( 0 );     /* not sure why we'd be here */
        }
        success = ETrue;
    }

    return success;
} /* SendL */

void
CSendSocket::DoActualSend()
{
    XP_LOGF( "CSendSocket::DoActualSend called" );

    if ( CancelListen() ) {
        iListenPending = ETrue;
    }

    iSendSocket.Write( iSendBuf, iStatus ); // Initiate actual write
    SetActive();
    
    // Request timeout
/*     iSendTimer.After( iWriteTimeout ); */
    iSSockState = ESending;
}

#endif
