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

#include "symrsock.h"

/*static*/ CReadSocket* 
CReadSocket::NewL( ReadNotifyCallback aCallback, void* aCallbackClosure )
{
	CReadSocket* self = new (ELeave) CReadSocket( aCallback, aCallbackClosure );
	CleanupStack::PushL( self );
	self->ConstructL();
    CleanupStack::Pop( self );
	return self;
}

CReadSocket::CReadSocket( ReadNotifyCallback aGotData, void* aCallbackClosure )
    : iGotData(aGotData)
     , iCallbackClosure(aCallbackClosure)
     , iReadState(ENotReading)
     , iPort(0)
     , CActive(EPriorityStandard)
{
    iInBuf.SetLength(0);
}

CReadSocket::~CReadSocket()
{
    Cancel();
    iSocketServer.Close();
}

void
CReadSocket::ConstructL()
{
	CActiveScheduler::Add( this );
    TInt err = iSocketServer.Connect();
    XP_LOGF( "iSocketServer.Connect => %d", err );

    err = iListenSocket.Open( iSocketServer, KAfInet, 
                              KSockDatagram, KProtocolInetUdp );
    XP_LOGF( "iListenSocket.Open => %d", err );

    if ( iPort != 0 ) {
        Bind();
    }
}

void
CReadSocket::Bind()
{
    XP_ASSERT( iPort != 0 );
    TSockAddr iSockAddress( KProtocolInetUdp );
    iSockAddress.SetPort( iPort );

    TInt err = iListenSocket.Bind( iSockAddress );
    XP_LOGF( "iListenSocket.Bind => %d", err );
} /* Bind */

void
CReadSocket::SetListenPort( TInt aPort )
{
    XP_LOGF( "SetListenPort(%d)", aPort );
    if ( aPort != iPort ) {
        TBool wasActive = iReadState == EReading && IsActive();
        if ( wasActive ) {
            Cancel();               /* stop anything ongoing */
        }
        iPort = aPort;
        Bind();
        if ( wasActive ) {
            RequestRecv();
        }
    }
} /* SetListenPort */

void
CReadSocket::RequestRecv()
{
    XP_ASSERT( !IsActive() );

    XP_LOGF( "calling iListenSocket.RecvFrom" );
    iListenSocket.RecvFrom( iInBuf, iRecvAddr, 0, iStatus );

    iReadState = EReading;
    SetActive();
} /* RequestRead */

void
CReadSocket::RunL()
{
    XP_ASSERT( iStatus.Int() == KErrNone );
    XP_ASSERT( iReadState == EReading );
    if ( iStatus.Int() == KErrNone ) {
        XP_LOGF( "SUCCESS: packet received!" );
        (*iGotData)( &iInBuf, iCallbackClosure );
        iInBuf.SetLength(0);
    }
    iReadState = ENotReading;

    RequestRecv();              /* We just keep listening... */
} /* RunL */

void
CReadSocket::DoCancel()
{
    if ( iReadState == EReading ) {
        iListenSocket.CancelRecv();
    }
}

void
CReadSocket::Start()
{
    if ( !IsActive() ) {
        RequestRecv();
    }
}

void
CReadSocket::Stop()
{
    Cancel();
}

#endif
