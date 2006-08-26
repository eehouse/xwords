/* -*-mode: C; fill-column: 78; c-basic-offset: 4; compile-command: "make MEMDEBUG=TRUE";-*- */ 
/* 
 * Copyright 2006 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#ifdef XWFEATURE_BLUETOOTH

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/l2cap.h>


#include "linuxbt.h"
#include "comms.h"

typedef struct LinBtStuff {
    CommonGlobals* globals;
    XP_Bool amMaster;
} LinBtStuff;

static LinBtStuff*
linBtMake( MPFORMAL XP_Bool amMaster )
{
    LinBtStuff* btStuff = (LinBtStuff*)XP_MALLOC( mpool, sizeof(*btStuff) );
    XP_MEMSET( btStuff, 0, sizeof(*btStuff) );

    btStuff->amMaster = amMaster;

    return btStuff;
}

static void
btConnectSocket( LinBtStuff* btStuff, const CommsAddrRec* addrP )
{
    struct sockaddr_l2 saddr;
    int sock;

    XP_MEMSET( &saddr, 0, sizeof(saddr) );

    // allocate a socket
    sock = socket( AF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP );
    if ( sock < 0 ) {
        XP_LOGF( "%s: socket->%s", __FUNCTION__, strerror(errno) );
    } else {

        // set the connection parameters (who to connect to)
        saddr.l2_family = AF_BLUETOOTH;
        saddr.l2_psm = htobs( XW_PSM );
        XP_MEMCPY( &saddr.l2_bdaddr, &addrP->u.bt.btAddr, 
                   sizeof(saddr.l2_bdaddr) );
        // connect to server
        if ( 0 == connect( sock, (struct sockaddr *)&saddr, sizeof(saddr) ) ) {
            CommonGlobals* globals = btStuff->globals;
            (*globals->socketChanged)( globals->socketChangedClosure, 
                                       -1, sock );
        } else {
            XP_LOGF( "%s: connect->%s", __FUNCTION__, strerror(errno) );
        }
    }
}

void
linux_bt_open( CommonGlobals* globals, XP_Bool amMaster )
{
    LinBtStuff* btStuff = globals->u.bt.btStuff;
    if ( !btStuff ) {
        btStuff = globals->u.bt.btStuff
            = linBtMake( MPPARM(globals->params->util->mpool)
                         amMaster );
        btStuff->globals = globals;
        globals->u.bt.btStuff = btStuff;
        
    }
}

void
linux_bt_close( CommonGlobals* globals )
{
    LinBtStuff* btStuff = globals->u.bt.btStuff;
    if ( !!btStuff ) {
        XP_FREE( globals->params->util->mpool, btStuff );
        globals->u.bt.btStuff = NULL;
    }
}

XP_S16 linux_bt_send( const XP_U8* buf, XP_U16 buflen, 
                      const CommsAddrRec* addrP, 
                      CommonGlobals* globals )
{
    LinBtStuff* btStuff = globals->u.bt.btStuff;
    CommsAddrRec addr;
    XP_S16 nSent = -1;

    LOG_FUNC();

    XP_ASSERT( !!btStuff );
    if ( !addrP ) {
        comms_getAddr( globals->game.comms, &addr );
        addrP = &addr;
    }

    if ( globals->socket < 0 ) {
        btConnectSocket( btStuff, addrP );
    }

    if ( globals->socket >= 0 ) {
        nSent = write( globals->socket, buf, buflen );
        if ( nSent < 0 ) {
            XP_LOGF( "%s: send->%s", __FUNCTION__, strerror(errno) );
        } else if ( nSent < buflen ) {
            XP_LOGF( "%s: send only %d bytes of %d", __FUNCTION__, nSent, 
                     buflen );
        }
    } else {
        XP_LOGF( "%s: socket still not set", __FUNCTION__ );
    }

    LOG_RETURNF( "%d", nSent );
    return nSent;
}

XP_S16
linux_bt_receive( CommonGlobals* globals, XP_U8* buf, XP_U16 buflen )
{
    XP_S16 nRead = 0;
    int sock = globals->socket;
    LOG_FUNC();
    XP_ASSERT( sock >= 0 );

    nRead = read( sock, buf, buflen );
    if ( nRead < 0 ) {
        XP_LOGF( "%s: read->%s", __FUNCTION__, strerror(errno) );
    }

    LOG_RETURNF( "%d", nRead );
    return nRead;
}

#endif /* XWFEATURE_BLUETOOTH */

