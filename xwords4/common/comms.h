/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 2001 by Eric House (fixin@peak.org).  All rights reserved.
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

#ifndef _COMMS_H_
#define _COMMS_H_

#include "comtypes.h"
#include "mempool.h"

EXTERN_C_START

#define CHANNEL_NONE ((XP_PlayerAddr)0)
#define CONN_ID_NONE 0L

typedef XP_U32 MsgID;

typedef enum {
    COMMS_CONN_UNUSED,          /* I want errors on uninited case */
    COMMS_CONN_IP,
    COMMS_CONN_BT,
    COMMS_CONN_IR,

    LAST_____FOO
} CommsConnType;

#define MAX_HOSTNAME_LEN 63
typedef struct CommsAddrRec {
    CommsConnType conType;

    union {
        struct {
            XP_UCHAR hostName[MAX_HOSTNAME_LEN + 1];
            XP_U32 ipAddr;      /* looked up from above */
            XP_U16 port;
        } ip;
        struct {
            /* nothing? */
            XP_UCHAR foo;       /* wince doesn't like nothing here */
        } ir;
    } u;
} CommsAddrRec;

typedef XP_S16 (*TransportSend)( XP_U8* buf, XP_U16 len, 
                                 CommsAddrRec* addr,
                                 void* closure );

CommsCtxt* comms_make( MPFORMAL XW_UtilCtxt* util,
                       XP_Bool isServer, TransportSend sendproc, 
                       void* closure );

void comms_reset( CommsCtxt* comms, XP_Bool isServer );
void comms_destroy( CommsCtxt* comms );

void comms_setConnID( CommsCtxt* comms, XP_U32 connID );

void comms_getAddr( CommsCtxt* comms, CommsAddrRec* addr, XP_U16* listenPort );
void comms_setAddr( CommsCtxt* comms, CommsAddrRec* addr, XP_U16 listenPort );
CommsConnType comms_getConType( CommsCtxt* comms );

CommsCtxt* comms_makeFromStream( MPFORMAL XWStreamCtxt* stream, 
                                 XW_UtilCtxt* util, TransportSend sendproc, 
                                 void* closure );
void comms_writeToStream( CommsCtxt* comms, XWStreamCtxt* stream );

/* void comms_setDefaultTarget( CommsCtxt* comms, char* hostName,  */
/* 			     short hostPort ); */

XP_S16 comms_send( CommsCtxt* comms, CommsConnType conType,
                   XWStreamCtxt* stream );
XP_S16 comms_resendAll( CommsCtxt* comms );


XP_Bool comms_checkIncommingStream( CommsCtxt* comms, XWStreamCtxt* stream, 
                                    CommsAddrRec* addr );

# ifdef DEBUG
void comms_getStats( CommsCtxt* comms, XWStreamCtxt* stream );
# endif

EXTERN_C_END

#endif /* _COMMS_H_ */
