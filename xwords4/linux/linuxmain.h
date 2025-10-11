/* 
 * Copyright 1997-2011 by Eric House (xwords@eehouse.org).  All rights
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
#ifndef _LINUXMAIN_H_
#define _LINUXMAIN_H_

#include <stdbool.h>

#include "main.h"
#include "dictnry.h"
#include "mempool.h"
#include "comms.h"
#include "memstream.h"
/* #include "compipe.h" */

extern int errno;

typedef struct LinuxBMStruct {
    XP_U8 nCols;
    XP_U8 nRows;
    XP_U8 nBytes;
} LinuxBMStruct;

void cg_init(CommonGlobals* cGlobals, cg_destructor proc);
CommonGlobals* _cg_ref(CommonGlobals* cGlobals, const char* proc, int line);
void _cg_unref(CommonGlobals* cGlobals, const char* proc, int line);
#define cg_ref(cg) _cg_ref(cg, __func__, __LINE__)
#define cg_unref(cg) _cg_unref(cg, __func__, __LINE__)

#ifdef COMMS_HEARTBEAT
void linux_reset( void* closure );
#endif
int linux_relay_receive( CommonGlobals* cGlobals, int sock,
                         unsigned char* buf, int bufSize );

XP_Bool linuxFireTimer( CommonGlobals* cGlobals, XWTimerReason why );


XWStreamCtxt* stream_from_msgbuf( CommonGlobals* cGlobals, 
                                  const unsigned char* bufPtr, XP_U16 nBytes );
XP_UCHAR* strFromStream( XWStreamCtxt* stream );

void catGameHistory( LaunchParams* params, GameRef gr );
void catAndClose( XWStreamCtxt* stream );
void sendOnClose( XWStreamCtxt* stream, XWEnv xwe, void* closure );

void catFinalScores( LaunchParams* params, GameRef gr, XP_S16 quitter );
XP_Bool file_exists( const char* fileName );
XWStreamCtxt* streamFromFile( CommonGlobals* cGlobals, char* name );
XWStreamCtxt* streamFromDB( CommonGlobals* cGlobals );
void writeToFile( XWStreamCtxt* stream, XWEnv xwe, void* closure );
XP_Bool getDictPath( const LaunchParams *params, const char* name, 
                     char* result, int resultLen );
GSList* listDicts( const LaunchParams *params );
#ifdef XWFEATURE_DEVICE_STORES
# define linuxSaveGame( cGlobals )
#else
void linuxSaveGame( CommonGlobals* cGlobals );
#endif

void linux_close_socket( CommonGlobals* cGlobals );

XP_Bool linux_makeMoveIf( CommonGlobals* cGlobals, XP_Bool tryTrade );
void linux_addInvites( CommonGlobals* cGlobals, XP_U16 nRemotes,
                       const CommsAddrRec destAddrs[] );

#ifdef KEYBOARD_NAV
XP_Bool linShiftFocus( CommonGlobals* cGlobals, XP_Key key,
                       const BoardObjectType* order,
                       BoardObjectType* nxtP );
#endif

void setupLinuxUtilCallbacks( XW_UtilCtxt* util, XP_Bool useCurses );
void assertUtilCallbacksSet( XW_UtilCtxt* util );
void assertDrawCallbacksSet( const DrawCtxVTable* vtable );

void sendRelayReg( LaunchParams* params, sqlite3* pDb );
void gameGotBuf( CommonGlobals* globals, XP_Bool haveDraw, 
                 const XP_U8* buf, XP_U16 len, const CommsAddrRec* from );
gboolean app_socket_proc( GIOChannel* source, GIOCondition condition, 
                          gpointer data );
const XP_U32 linux_getDevIDRelay( LaunchParams* params );
const XP_UCHAR* linux_getDevID( LaunchParams* params, DevIDType* typ );
void linux_doInitialReg( LaunchParams* params, XP_Bool idIsNew );
XP_Bool linux_setupDevidParams( LaunchParams* params );
XP_Bool parseSMSParams( LaunchParams* params, gchar** myPhone, XP_U16* myPort );

void makeSelfAddress( CommsAddrRec* selfAddr, LaunchParams* params );

unsigned int makeRandomInt();
bool linuxOpenGame( CommonGlobals* cGlobals );
void cancelTimers( CommonGlobals* cGlobals );
CommonAppGlobals* getCag( const XW_UtilCtxt* util );
CommonGlobals* globalsForGameRef( CommonAppGlobals* cag, GameRef gr,
                                  XP_Bool allocMissing );
CommonGlobals* globalsForUtil( const XW_UtilCtxt* uc,
                               XP_Bool allocMissing );
void forgetGameGlobals( CommonAppGlobals* cag, CommonGlobals* cGlobals );

void cpFromLP( CommonPrefs* cp, const LaunchParams* params );

RematchOrder roFromStr(const char* rematchOrder );

/* void initParams( LaunchParams* params ); */
/* void freeParams( LaunchParams* params ); */

void assertMainThread(const CommonGlobals* cGlobals );

XP_Bool getAndClear( GameChangeEvent evt, GameChangeEvents* gces );

#endif
