/* -*-mode: C; fill-column: 77; c-basic-offset: 4; -*- */
/* 
 * Copyright 2009 by Eric House (xwords@eehouse.org).  All rights reserved.
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

#ifdef XWFEATURE_SMS

#include "cesms.h"

XP_S16
ce_sms_send( CEAppGlobals* XP_UNUSED(globals), const XP_U8* XP_UNUSED(buf), 
             XP_U16 len, const CommsAddrRec* addrp )
{
    XP_LOGF( "%s: got %d bytes to send to port %d at %s but don't know how.",
             __func__, len, addrp->u.sms.port, addrp->u.sms.phone );

    /* Will use SmsOpen, SmsSend, etc, and WaitForSingleObject.  But it
       appears that SMS APIs are only usable by signed apps and that users
       can't override that restriction in some cases, so SMS is not a
       priority until I can confirm otherwise or find a way for open-source
       apps to be signed.
    */

    return -1;
}

#endif
