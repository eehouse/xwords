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

#define MAX_CLIENTS 3

typedef struct BtaddrSockMap {
    bdaddr_t btaddr;
    int sock;
} BtaddrSockMap;

typedef struct LinBtStuff {
    CommonGlobals* globals;

    union {
        struct {
            BtaddrSockMap socks[MAX_CLIENTS];
            int listener;               /* socket */
            XP_U16 nSocks;
            XP_Bool threadDie;
        } master;
    } u;

    XP_Bool amMaster;
} LinBtStuff;

static void
lbt_addSock( LinBtStuff* btStuff, const bdaddr_t* btaddr, int sock )
{
    XP_U16 i;

    XP_ASSERT( btStuff->amMaster );
    XP_ASSERT( btStuff->u.master.nSocks < MAX_CLIENTS - 1 );

    for ( i = 0; i < MAX_CLIENTS; ++i ) {
        BtaddrSockMap* mp = &btStuff->u.master.socks[i];
        if ( mp->sock == -1 ) {
            XP_MEMCPY( &mp->btaddr, btaddr, sizeof(mp->btaddr) );
            mp->sock = sock;
            ++btStuff->u.master.nSocks;
            break;
        }
    }
    XP_ASSERT( i < MAX_CLIENTS );
} /* lbt_addSock */

static void
lbt_removeSock( LinBtStuff* btStuff, int sock )
{
    XP_U16 i;

    XP_ASSERT( btStuff->amMaster );
    XP_ASSERT( btStuff->u.master.nSocks > 0 );

    for ( i = 0; i < MAX_CLIENTS; ++i ) {
        BtaddrSockMap* mp = &btStuff->u.master.socks[i];
        if ( mp->sock == sock ) {
            mp->sock = -1;
            --btStuff->u.master.nSocks;
            break;
        }
    }
    XP_ASSERT( i < MAX_CLIENTS );
} /* lbt_removeSock */

static LinBtStuff*
lbt_make( MPFORMAL XP_Bool amMaster )
{
    LinBtStuff* btStuff = (LinBtStuff*)XP_MALLOC( mpool, sizeof(*btStuff) );
    XP_MEMSET( btStuff, 0, sizeof(*btStuff) );

    btStuff->amMaster = amMaster;

    if ( amMaster ) {
        XP_U16 i;
        for ( i = 0; i < MAX_CLIENTS; ++i ) {
            btStuff->u.master.socks[i].sock = -1;
        }
    }

    return btStuff;
} /* lbt_make */

static void
lbt_connectSocket( LinBtStuff* btStuff, const CommsAddrRec* addrP )
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
        saddr.l2_psm = htobs( XW_PSM );	/* need to get this psm via uuid lookup */
        XP_MEMCPY( &saddr.l2_bdaddr, &addrP->u.bt.btAddr, 
                   sizeof(saddr.l2_bdaddr) );
        // connect to server
        if ( 0 == connect( sock, (struct sockaddr *)&saddr, sizeof(saddr) ) ) {
            CommonGlobals* globals = btStuff->globals;
            (*globals->socketChanged)( globals->socketChangedClosure, 
                                       -1, sock );
        } else {
            XP_LOGF( "%s: connect->%s", __FUNCTION__, strerror(errno) );
            close( sock );
        }
    }
} /* lbt_connectSocket */

static XP_Bool
lbt_accept( int listener, void* ctxt )
{
    CommonGlobals* globals = (CommonGlobals*)ctxt;
    LinBtStuff* btStuff = globals->btStuff;
    int sock = -1;
    struct sockaddr_l2 inaddr;
    socklen_t slen;
    XP_Bool success;

    LOG_FUNC();

    XP_LOGF( "%s: calling accept", __FUNCTION__ );
    slen = sizeof( inaddr );
    XP_ASSERT( listener == btStuff->u.master.listener );
    sock = accept( listener, (struct sockaddr *)&inaddr, &slen );
    XP_LOGF( "%s: accept returned; sock = %d", __FUNCTION__, sock );
    
    success = sock >= 0;
    if ( success ) {
        lbt_addSock( btStuff, &inaddr.l2_bdaddr, sock );
        (*globals->socketChanged)( globals->socketChangedClosure, 
                                   -1, sock );
    } else {
        XP_LOGF( "%s: accept->%s", __FUNCTION__, strerror(errno) );
    }
    return success;
} /* lbt_accept */

static void
lbt_listenerSetup( CommonGlobals* globals )
{
    LinBtStuff* btStuff = globals->btStuff;
    struct sockaddr_l2 saddr;
    int listener;

    listener = socket( AF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP );
    btStuff->u.master.listener = listener;

    XP_MEMSET( &saddr, 0, sizeof(saddr) );
    saddr.l2_family = AF_BLUETOOTH;
    saddr.l2_bdaddr = *BDADDR_ANY;
    saddr.l2_psm = htobs( XW_PSM ); /* need to associate uuid with this before opening? */
    bind( listener, (struct sockaddr *)&saddr, sizeof(saddr) );

    listen( listener, MAX_CLIENTS );

    (*globals->addAcceptor)( listener, lbt_accept, globals );
} /* lbt_listenerSetup */

void
linux_bt_open( CommonGlobals* globals, XP_Bool amMaster )
{
    LinBtStuff* btStuff = globals->btStuff;
    if ( !btStuff ) {
        btStuff = globals->btStuff
            = lbt_make( MPPARM(globals->params->util->mpool) amMaster );
        btStuff->globals = globals;
        globals->btStuff = btStuff;

        if ( amMaster ) {
            lbt_listenerSetup( globals );
        } else {
            if ( globals->socket < 0 ) {
                CommsAddrRec addr;
                comms_getAddr( globals->game.comms, &addr );
                lbt_connectSocket( btStuff, &addr );
            }
        }
    }
} /* linux_bt_open */

void
linux_bt_close( CommonGlobals* globals )
{
    LinBtStuff* btStuff = globals->btStuff;

    if ( !!btStuff ) {
        if ( btStuff->amMaster ) {
            close( btStuff->u.master.listener );
        }
        XP_FREE( globals->params->util->mpool, btStuff );
        globals->btStuff = NULL;
    }
} /* linux_bt_close */

XP_S16
linux_bt_send( const XP_U8* buf, XP_U16 buflen, 
               const CommsAddrRec* addrP, 
               CommonGlobals* globals )
{
    XP_S16 nSent = -1;
    LinBtStuff* btStuff;

    XP_LOGF( "%s(len=%d)", __FUNCTION__, buflen );

    btStuff = globals->btStuff;
    if ( !!btStuff ) {
        CommsAddrRec addr;
        if ( !addrP ) {
            comms_getAddr( globals->game.comms, &addr );
            addrP = &addr;
        }

        if ( globals->socket < 0  && !btStuff->amMaster ) {
            lbt_connectSocket( btStuff, addrP );
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
    }
    LOG_RETURNF( "%d", nSent );
    return nSent;
} /* linux_bt_send */

XP_S16
linux_bt_receive( int sock, XP_U8* buf, XP_U16 buflen )
{
    XP_S16 nRead = 0;
    LOG_FUNC();
    XP_ASSERT( sock >= 0 );

    nRead = read( sock, buf, buflen );
    if ( nRead < 0 ) {
        XP_LOGF( "%s: read->%s", __FUNCTION__, strerror(errno) );
    }

    LOG_RETURNF( "%d", nRead );
    return nRead;
}

void
linux_bt_socketclosed( CommonGlobals* globals, int sock )
{
    LinBtStuff* btStuff = globals->btStuff;
    if ( btStuff->amMaster ) {
        lbt_removeSock( btStuff, sock );
    }
}

#endif /* XWFEATURE_BLUETOOTH */

