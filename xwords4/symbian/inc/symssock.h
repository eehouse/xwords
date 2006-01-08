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

#ifndef _SYMSSOCK_H_
#define _SYMSSOCK_H_

#ifndef XWFEATURE_STANDALONE_ONLY

#include <es_sock.h>
#include <in_sock.h>
#include "comms.h"

const TInt KMaxMsgLen = 512;

class CSendSocket : public CActive {

 public:

    typedef void (*ReadNotifyCallback)( const TDesC8* aBuf,
                                        void *aClosure );
    static CSendSocket* NewL( ReadNotifyCallback aCallback, void* aClosure );

    ~CSendSocket();

    TBool SendL( const XP_U8* aBuf, XP_U16 aLen, const CommsAddrRec* aAddr );
    TBool Listen();

    void ConnectL( const CommsAddrRec* aAddr );
    void Disconnect();
    
 protected:
    void RunL();                /* from CActive */
    void DoCancel();            /* from CActive */

 private:
    CSendSocket( ReadNotifyCallback aCallback, void* aClosure );
    void ConstructL();
    void DoActualSend();

    void ConnectL();
    void ConnectL( TUint32 aIpAddr );

    TBool CancelListen();
    void ResetState();

    enum TSSockState { ENotConnected
                       ,ELookingUp
                       ,EConnecting
                       ,EConnected
                       ,ESending
                       ,EListening
    };

    TSSockState       iSSockState;
    RSocket           iSendSocket;
    RSocketServ       iSocketServer;
    RHostResolver     iResolver;
    TInetAddr         iAddress;
    TBuf8<KMaxMsgLen> iSendBuf;
    TInt              iDataLen;       /* How big should next packet be */
    TUint8            iInBuf[KMaxMsgLen];
    TPtr8*            iInBufDesc;     /* points to above buffer */
    CommsAddrRec      iCurAddr;
    TNameEntry        iNameEntry;
    TNameRecord       iNameRecord;
    TBool             iAddrSet;
    TBool             iListenPending;

    ReadNotifyCallback iCallback;
    void* iClosure;
};

#endif
#endif
