/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 2020 by Eric House (xwords@eehouse.org).  All rights reserved.
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


#ifndef _DEVICE_H_
#define _DEVICE_H_

#include "dutil.h"

// void device_load( XW_DUtilCtxt dctxt );
# ifdef XWFEATURE_DEVICE
void dvc_store( XW_DUtilCtxt* dctxt, XWEnv xwe );
# else
#  define dvc_store(dctxt, xwe)
# endif

typedef void (*MsgAndTopicProc)( void* closure, const XP_UCHAR* topic,
                                 const XP_U8* msgBuf, XP_U16 msgLen );

void dvc_getMQTTDevID( XW_DUtilCtxt* dutil, XWEnv xwe, MQTTDevID* devID );
void dvc_setMQTTDevID( XW_DUtilCtxt* dutil, XWEnv xwe, const MQTTDevID* devID );
void dvc_resetMQTTDevID( XW_DUtilCtxt* dutil, XWEnv xwe );
void dvc_getMQTTSubTopics( XW_DUtilCtxt* dutil, XWEnv xwe,
                           XP_UCHAR* storage, XP_U16 storageLen,
                           XP_U16* nTopics, XP_UCHAR* topics[] );
void dvc_makeMQTTInvites( XW_DUtilCtxt* dutil, XWEnv xwe,
                          MsgAndTopicProc proc, void* closure,
                          const MQTTDevID* addressee,
                          const NetLaunchInfo* nli );
void dvc_makeMQTTNukeInvite( XW_DUtilCtxt* dutil, XWEnv xwe,
                             MsgAndTopicProc proc, void* closure,
                             const NetLaunchInfo* nli );

XP_S16 dvc_makeMQTTMessages( XW_DUtilCtxt* dutil, XWEnv xwe,
                             MsgAndTopicProc proc, void* closure,
                             const SendMsgsPacket* const msgs,
                             const MQTTDevID* addressee, XP_U32 gameID,
                             XP_U16 streamVersion );

void dvc_makeMQTTNoSuchGames( XW_DUtilCtxt* dutil, XWEnv xwe,
                              MsgAndTopicProc proc, void* closure,
                              const MQTTDevID* addressee,
                              XP_U32 gameID );
void dvc_parseMQTTPacket( XW_DUtilCtxt* dutil, XWEnv xwe, const XP_UCHAR* topic,
                          const XP_U8* buf, XP_U16 len );

void dvc_onWebSendResult( XW_DUtilCtxt* dutil, XWEnv xwe, XP_U32 resultKey,
                          XP_Bool succeeded, const XP_UCHAR* result );

/* All platforms need to call this shortly after setting up their XW_DUtilCtxt */
void dvc_init( XW_DUtilCtxt* dutil, XWEnv xwe );

#endif
