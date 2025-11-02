/* -*-mode: C; fill-column: 78; c-basic-offset: 4; -*- */
/* 
 * Copyright 2020 - 2024 by Eric House (xwords@eehouse.org).  All rights
 * reserved.
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
#include "gameref.h"

// void device_load( XW_DUtilCtxt dctxt );
# ifdef XWFEATURE_DEVICE
void dvc_store( XW_DUtilCtxt* dctxt, XWEnv xwe );
# else
#  define dvc_store(dctxt, xwe)
# endif

XP_S16 dvc_sendInvite( XW_DUtilCtxt* duc, XWEnv xwe, const NetLaunchInfo* nli,
                       XP_U32 createdStamp, const CommsAddrRec* addr,
                       CommsConnType typ );
XP_S16 dvc_sendMsgs( XW_DUtilCtxt* duc, XWEnv xwe,
                     const SendMsgsPacket* const packets,
                     XP_U16 streamVersion,
                     const CommsAddrRec* addr, CommsConnType typ,
                     GameRef gr );

typedef void (*MsgAndTopicProc)( void* closure, const XP_UCHAR* topic,
                                 const XP_U8* msgBuf, XP_U16 msgLen,
                                 XP_U8 qos );

void dvc_getMQTTDevID( XW_DUtilCtxt* dutil, XWEnv xwe, MQTTDevID* devID );
void dvc_setMQTTDevID( XW_DUtilCtxt* dutil, XWEnv xwe, const MQTTDevID* devID );
void dvc_resetMQTTDevID( XW_DUtilCtxt* dutil, XWEnv xwe );
void dvc_makeMQTTNukeInvite( XW_DUtilCtxt* dutil, XWEnv xwe,
                             MsgAndTopicProc proc, void* closure,
                             const NetLaunchInfo* nli );
void dvc_makeMQTTNoSuchGames( XW_DUtilCtxt* dutil, XWEnv xwe,
                              MsgAndTopicProc proc, void* closure,
                              const MQTTDevID* addressee,
                              XP_U32 gameID );

void dvc_parseMQTTPacket( XW_DUtilCtxt* dutil, XWEnv xwe, const XP_UCHAR* topic,
                          const XP_U8* buf, XP_U16 len );
void dvc_parseSMSPacket( XW_DUtilCtxt* dutil, XWEnv xwe,
                         const CommsAddrRec* fromAddr,
                         const XP_U8* buf, XP_U16 len );
void dvc_parseBTPacket( XW_DUtilCtxt* dutil, XWEnv xwe,
                        const XP_U8* buf, XP_U16 len,
                        const XP_UCHAR* fromName, const XP_UCHAR* fromAddr );
XP_Bool dvc_parseUrl( XW_DUtilCtxt* duc, XWEnv xwe, const XP_UCHAR* buf,
                      XP_U16 len, const XP_UCHAR* host, const XP_UCHAR* prefix );

void dvc_onBLEMtuChangedFor( XW_DUtilCtxt* dutil, XWEnv xwe,
                             const XP_UCHAR* phone, XP_U16 mtu );
void dvc_onWebSendResult( XW_DUtilCtxt* dutil, XWEnv xwe, XP_U32 resultKey,
                          XP_Bool succeeded, const XP_UCHAR* result );

void dvc_addLegalPhony( XW_DUtilCtxt* dutil, XWEnv xwe,
                        const XP_UCHAR* isoCode, const XP_UCHAR* phony );
XP_Bool dvc_isLegalPhony( XW_DUtilCtxt* dutil, XWEnv xwe,
                          const XP_UCHAR* isoCode, const XP_UCHAR* phony );
XP_Bool dvc_haveLegalPhonies( XW_DUtilCtxt* dutil, XWEnv xwe );
void dvc_clearLegalPhony( XW_DUtilCtxt* dutil, XWEnv xwe,
                          const XP_UCHAR* isoCode, const XP_UCHAR* phony );

typedef void (*WordCollector)(const XP_UCHAR* str, void* closure);
void dvc_getIsoCodes( XW_DUtilCtxt* dutil, XWEnv env, WordCollector proc,
                      void* closure );
void dvc_getPhoniesFor( XW_DUtilCtxt* dutil, XWEnv env, const XP_UCHAR* code,
                        WordCollector proc, void* closure );

void dvc_onTimerFired( XW_DUtilCtxt* dutil, XWEnv env, TimerKey key );

XP_U8 dvc_getQOS( XW_DUtilCtxt* dutil, XWEnv env );

/* All platforms need to call this shortly after setting up their XW_DUtilCtxt */
void dvc_init( XW_DUtilCtxt* dutil, XWEnv xwe );
void dvc_cleanup( XW_DUtilCtxt* dutil, XWEnv xwe );

/* I want stream creation out of the public API eventually... */
XWStreamCtxt* dvc_makeStream( XW_DUtilCtxt* dutil );

GameRef dvc_makeFromStream( XW_DUtilCtxt* dutil, XWEnv xwe,
                            XWStreamCtxt* stream, const CurGameInfo* gi,
                            XW_UtilCtxt* util, DrawCtx* draw, CommonPrefs* cp);

void dvc_storeStream( XW_DUtilCtxt* dutil, XWEnv xwe,
                      const XP_UCHAR* keys[], XWStreamCtxt* stream );
XWStreamCtxt* dvc_loadStream( XW_DUtilCtxt* duc, XWEnv xwe, const XP_UCHAR* keys[]);
void dvc_removeStream( XW_DUtilCtxt* duc, XWEnv xwe, const XP_UCHAR* keys[] );
void dvc_getKeysLike( XW_DUtilCtxt* duc, XWEnv xwe, const XP_UCHAR* keys[],
                      OnGotKey proc, void* closure );

void dvc_parseKey( XP_UCHAR* buf, XP_UCHAR* parts[], XP_U16* nParts );

void dvc_onGameGoneReceived( XW_DUtilCtxt* duc, XWEnv xwe, XP_U32 gameID,
                             const CommsAddrRec* from );
void dvc_onDictAdded( XW_DUtilCtxt* duc, XWEnv xwe, const XP_UCHAR* dictName );
void dvc_onDictRemoved( XW_DUtilCtxt* duc, XWEnv xwe, const XP_UCHAR* dictName );

/* To avoid making mempool "public" */
XWStreamCtxt* dvc_makeStream( XW_DUtilCtxt* dutil );

#endif
