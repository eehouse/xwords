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

#ifndef _SYMRSOCK_H_
#define _SYMRSOCK_H_

#include <es_sock.h>
#include <in_sock.h>
#include "comms.h"

const TInt KMaxRcvMsgLen = 512;

class CReadSocket : public CActive {

 public:
    typedef void (*ReadNotifyCallback)( const TDesC8* aBuf,
                                        void *closure );

    static CReadSocket* NewL( ReadNotifyCallback aGotData,
                              void* aCallbackClosure );
    ~CReadSocket();

    void SetListenPort( TInt aPort );
    void Start();
    void Stop();

 protected:
    void RunL();                /* from CActive */
    void DoCancel();            /* from CActive */

 private:
    CReadSocket( ReadNotifyCallback aGotData, void* aCallbackClosure );
    void ConstructL();
    void RequestRecv();
    void Bind();

    enum TReadState {
        ENotReading
        ,EReading
    };

    TReadState iReadState;
    ReadNotifyCallback iGotData;
    void* iCallbackClosure;
    RSocket iListenSocket;
    TInt iPort;
    TBuf8<KMaxRcvMsgLen> iInBuf;
    TSockXfrLength iReadLength;
    RSocketServ iSocketServer;
    TInetAddr iRecvAddr;
};

#endif

#endif /* XWFEATURE_STANDALONE_ONLY */
