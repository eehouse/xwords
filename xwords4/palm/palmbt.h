/* -*-mode: C; fill-column: 77; c-basic-offset: 4; -*- */
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

#ifndef _PALMBT_H_
#define _PALMBT_H_

#ifdef XWFEATURE_BLUETOOTH

#include "comms.h"
#include "palmmain.h"

#define PALM_BT_NAME_LEN 48

/* Needed: feedback to main so status can be posted on the board.  Events we
 * might care about:
 *
 * - BT disconnected -- not available
 * - Server up and socket available
 * - client trying to connect (includes having ACL conn)
 * - client has a connection (L2CAP socket open)
 * - received data
 * - sending data
 * - done sending data
 */

/**
 * palm_bt_appendWaitTicks
 * reduce waitTicks if have work to do
 */


void palm_bt_amendWaitTicks( PalmAppGlobals* globals, Int32* result );

XP_Bool palm_bt_init( PalmAppGlobals* globals, XP_Bool* userCancelled );
void palm_bt_reset( PalmAppGlobals* globals );
void palm_bt_close( PalmAppGlobals* globals );

typedef enum {
    BTCBEVT_CONFIRM, BTCBEVT_CONN, BTCBEVT_DATA
} BtCbEvt;
typedef struct BtCbEvtInfo {
    BtCbEvt evt;
    union {
        struct {
            const char* hostName;
            XP_Bool confirmed;
        } confirm;
        struct {
            const void* data;
            const CommsAddrRec* fromAddr;
            XP_U8 len;
        } data;
    } u;
} BtCbEvtInfo;

typedef void (*BtCbEvtProc)( PalmAppGlobals* globals, BtCbEvtInfo* evt );
XP_Bool palm_bt_doWork( PalmAppGlobals* globals, BtCbEvtProc proc, BtUIState* btState );

void palm_bt_addrString( PalmAppGlobals* globals, const XP_BtAddr* btAddr, 
                         XP_BtAddrStr* str );

XP_S16 palm_bt_send( const XP_U8* buf, XP_U16 len, const CommsAddrRec* addr,
                     PalmAppGlobals* globals, XP_Bool* userCancelled );

XP_Bool palm_bt_browse_device( PalmAppGlobals* globals, XP_BtAddr* btAddr,
                               XP_UCHAR* out, XP_U16 len );

#ifdef DEBUG
void palm_bt_getStats( PalmAppGlobals* globals, XWStreamCtxt* stream );
#endif
#endif /* XWFEATURE_BLUETOOTH */
#endif
