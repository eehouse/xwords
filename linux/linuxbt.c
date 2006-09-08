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

/* #define XW_USE_THREADS */
#ifdef XW_USE_THREADS
# include <pthread.h>
#endif

#include "linuxbt.h"
#include "comms.h"


#define MAX_CLIENTS 3

typedef struct BtaddrSockMap {
    bdaddr_t btaddr;
    int sock;
} BtaddrSockMap;

typedef struct LinBtStuff {
    CommonGlobals* globals;
    int listener;               /* socket */

    union {
        struct {
#ifdef XW_USE_THREADS
            pthread_t acceptThread;
#endif
            BtaddrSockMap socks[MAX_CLIENTS];
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
} /* lbt_addSock */

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
} /* lbt_connectSocket */

static void
waitForOne( LinBtStuff* btStuff )
{
    int sock;
    struct sockaddr_l2 inaddr;
    socklen_t slen;

    // accept one connection
    XP_LOGF( "%s: blocking on accept", __FUNCTION__ );
    slen = sizeof( inaddr );
    sock = accept( btStuff->listener, (struct sockaddr *)&inaddr, &slen );
    XP_LOGF( "%s: accept returned; sock = %d", __FUNCTION__, sock );

    {
        char buf[18];
        (void)ba2str( &inaddr.l2_bdaddr, buf );
        XP_LOGF( "got connection from %s", buf );
    }

    if ( sock >= 0 ) {
        CommonGlobals* globals = btStuff->globals;
        lbt_addSock( btStuff, &inaddr.l2_bdaddr, sock );
        (*globals->socketChanged)( globals->socketChangedClosure,
                                   -1, sock );
    } else {
        XP_LOGF( "%s: accept failed with %s", __FUNCTION__,
                 strerror(errno) );
    }
} /* waitForOne */

#ifdef XW_USE_THREADS
static void*
lbt_acceptThreadProc( void* arg )
{
    CommonGlobals* globals = (CommonGlobals*)arg;
    LinBtStuff* btStuff = globals->u.bt.btStuff;

    while ( !btStuff->u.master.threadDie ) {
        waitForOne( btStuff );
    }

    LOG_RETURN_VOID();
    return NULL;
} /* acceptThreadProc */
#endif

static void
lbt_waitConnection( CommonGlobals* globals )
{
    LinBtStuff* btStuff = globals->u.bt.btStuff;
    struct sockaddr_l2 saddr;

    btStuff->listener = socket( AF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP );

    XP_MEMSET( &saddr, 0, sizeof(saddr) );
    saddr.l2_family = AF_BLUETOOTH;
    saddr.l2_bdaddr = *BDADDR_ANY;
    saddr.l2_psm = htobs( XW_PSM );
    bind( btStuff->listener, (struct sockaddr *)&saddr, sizeof(saddr) );

    listen( btStuff->listener, MAX_CLIENTS );

#ifdef XW_USE_THREADS
    int pthread_err;
    btStuff->u.master.threadDie = XP_FALSE;
    pthread_err = pthread_create( &btStuff->u.master.acceptThread, 
                                  NULL, lbt_acceptThreadProc, globals );
    XP_ASSERT( 0 == pthread_err );
    pthread_detach( btStuff->u.master.acceptThread );
#else
    waitForOne( btStuff );    
#endif
}

void
linux_bt_open( CommonGlobals* globals, XP_Bool amMaster )
{
    LinBtStuff* btStuff = globals->u.bt.btStuff;
    if ( !btStuff ) {
        btStuff = globals->u.bt.btStuff
            = lbt_make( MPPARM(globals->params->util->mpool) amMaster );
        btStuff->globals = globals;
        globals->u.bt.btStuff = btStuff;

        if ( amMaster ) {
            lbt_waitConnection( globals );
        }
    }
} /* linux_bt_open */

void
linux_bt_close( CommonGlobals* globals )
{
    LinBtStuff* btStuff = globals->u.bt.btStuff;

    if ( !!btStuff ) {
        if ( btStuff->amMaster ) {
#ifdef XW_USE_THREADS
            int ret;
            btStuff->u.master.threadDie = XP_TRUE;
            ret = pthread_join( btStuff->u.master.acceptThread, NULL );
            if ( 0 != ret ) {
                XP_LOGF( "pthread_join=>%s", strerror(errno) );
            }
#endif
            close( btStuff->listener );
        }
        XP_FREE( globals->params->util->mpool, btStuff );
        globals->u.bt.btStuff = NULL;
    }
} /* linux_bt_close */

XP_S16
linux_bt_send( const XP_U8* buf, XP_U16 buflen, 
               const CommsAddrRec* addrP, 
               CommonGlobals* globals )
{
    XP_S16 nSent = -1;
    LinBtStuff* btStuff = globals->u.bt.btStuff;
    if ( !!btStuff ) {
        CommsAddrRec addr;

        LOG_FUNC();

        XP_ASSERT( !!btStuff );
        if ( !addrP ) {
            comms_getAddr( globals->game.comms, &addr );
            addrP = &addr;
        }

        if ( globals->socket < 0 ) {
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

        LOG_RETURNF( "%d", nSent );
    }
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
    LinBtStuff* btStuff = globals->u.bt.btStuff;
    lbt_removeSock( btStuff, sock );
}

#endif /* XWFEATURE_BLUETOOTH */

