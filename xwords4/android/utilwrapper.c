/* -*-mode: C; compile-command: "cd XWords4; ../scripts/ndkbuild.sh"; -*- */
/* 
 * Copyright 2001-2009 by Eric House (xwords@eehouse.org).  All rights
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
#include <jni.h>
#include "utilwrapper.h"
#include "andutils.h"

typedef struct _TimerStorage {
    XWTimerProc proc;
    void* closure;
} TimerStorage;

typedef struct _AndUtil {
    XW_UtilCtxt util;
    JNIEnv** env;
    jobject j_util;  /* global ref to object implementing XW_UtilCtxt */
    TimerStorage timerStorage[NUM_TIMERS_PLUS_ONE];
} AndUtil;


static VTableMgr*
and_util_getVTManager( XW_UtilCtxt* uc )
{
    AndGlobals* globals = (AndGlobals*)uc->closure;
    return globals->vtMgr;
}

#ifndef XWFEATURE_STANDALONE_ONLY
static XWStreamCtxt*
and_util_makeStreamFromAddr( XW_UtilCtxt* uc, XP_PlayerAddr channelNo )
{
}
#endif
    
static XWBonusType and_util_getSquareBonus( XW_UtilCtxt* uc, 
                                            const ModelCtxt* XP_UNUSED(model),
                                            XP_U16 col, XP_U16 row )
{
    AndUtil* util = (AndUtil*)uc;
    JNIEnv* env = *util->env;
    const char* sig = "(II)I";
    jmethodID mid = getMethodID( env, util->j_util, "getSquareBonus", sig );
    return (*env)->CallIntMethod( env, util->j_util, mid, 
                                  col, row );
}

static void
and_util_userError( XW_UtilCtxt* uc, UtilErrID id )
{
    XP_LOGF( "%s(id=%d)", __func__, id );
}


static XP_Bool
and_util_userQuery( XW_UtilCtxt* uc, UtilQueryID id, XWStreamCtxt* stream )
{
    XP_LOGF( "%s(id=%d)", __func__, id );
}

static XP_S16
and_util_userPickTile( XW_UtilCtxt* uc, const PickInfo* pi, 
                       XP_U16 playerNum, const XP_UCHAR** texts, XP_U16 nTiles )
{
    XP_S16 result = -1;
    AndUtil* util = (AndUtil*)uc;
    JNIEnv* env = *util->env;
    const char* sig = "(I[Ljava/lang/String;)I";
    jmethodID mid = getMethodID( env, util->j_util, "userPickTile", sig );

#ifdef FEATURE_TRAY_EDIT
    ++error;                       /* need to pass pi if this is on */
#endif

    jobject jtexts = makeStringArray( env, nTiles, texts );

    result = (*env)->CallIntMethod( env, util->j_util, mid, 
                                    playerNum, jtexts );

    (*env)->DeleteLocalRef( env, jtexts );

    return result;
} /* and_util_userPickTile */


static XP_Bool
and_util_askPassword( XW_UtilCtxt* uc, const XP_UCHAR* name, 
                      XP_UCHAR* buf, XP_U16* len )
{
    LOG_FUNC();
}


static void
and_util_trayHiddenChange(XW_UtilCtxt* uc, XW_TrayVisState newState,
                          XP_U16 nVisibleRows )
{
    LOG_FUNC();
}

static void
and_util_yOffsetChange(XW_UtilCtxt* uc, XP_U16 oldOffset, XP_U16 newOffset )
{
#if 0
    AndUtil* util = (AndUtil*)uc;
    JNIEnv* env = *util->env;
    const char* sig = "(II)V";
    jmethodID mid = getMethodID( env, util->j_util, "yOffsetChange", sig );

    (*env)->CallVoidMethod( env, util->j_util, mid, 
                            oldOffset, newOffset );
#endif
}

#ifdef XWFEATURE_TURNCHANGENOTIFY
static void
and_util_turnChanged(XW_UtilCtxt* uc)
{
    /* don't log; this is getting called a lot */
}

#endif
static void
and_util_notifyGameOver( XW_UtilCtxt* uc )
{
    LOG_FUNC();
}


static XP_Bool
and_util_hiliteCell( XW_UtilCtxt* uc, XP_U16 col, XP_U16 row )
{
    /* don't log; this is getting called a lot */
}


static XP_Bool
and_util_engineProgressCallback( XW_UtilCtxt* uc )
{
    /* don't log; this is getting called a lot */
    return XP_TRUE;
}

/* This is added for java, not part of the util api */
bool
utilTimerFired( XW_UtilCtxt* uc, XWTimerReason why, int handle )
{
    AndUtil* util = (AndUtil*)uc;
    TimerStorage* timerStorage = &util->timerStorage[why];
    XP_ASSERT( handle == (int)timerStorage );
    return (*timerStorage->proc)( timerStorage->closure, why );
}

static void
and_util_setTimer( XW_UtilCtxt* uc, XWTimerReason why, XP_U16 when,
                   XWTimerProc proc, void* closure )
{
    AndUtil* util = (AndUtil*)uc;
    JNIEnv* env = *util->env;
    const char* sig = "(III)V";
    jmethodID mid = getMethodID( env, util->j_util, "setTimer", sig );

    XP_ASSERT( why < VSIZE(util->timerStorage) );
    TimerStorage* storage = &util->timerStorage[why];
    storage->proc = proc;
    storage->closure = closure;
    (*env)->CallVoidMethod( env, util->j_util, mid,
                            why, when, (int)storage );
}

static void
and_util_clearTimer( XW_UtilCtxt* uc, XWTimerReason why )
{
    AndUtil* util = (AndUtil*)uc;
    JNIEnv* env = *util->env;
    const char* sig = "(I)V";
    jmethodID mid = getMethodID( env, util->j_util, "clearTimer", sig );
    (*env)->CallVoidMethod( env, util->j_util, mid, why );
}


static void
and_util_requestTime( XW_UtilCtxt* uc )
{
    AndUtil* util = (AndUtil*)uc;
    JNIEnv* env = *util->env;
    const char* sig = "()V";
    jmethodID mid = getMethodID( env, util->j_util, "requestTime", sig );
    (*env)->CallVoidMethod( env, util->j_util, mid );
}

static XP_Bool
and_util_altKeyDown( XW_UtilCtxt* uc )
{
    LOG_FUNC();
    return XP_FALSE;
}


static XP_U32
and_util_getCurSeconds( XW_UtilCtxt* uc )
{
    LOG_FUNC();
}


static DictionaryCtxt* 
and_util_makeEmptyDict( XW_UtilCtxt* uc )
{
    LOG_FUNC();
}


static const XP_UCHAR*
and_util_getUserString( XW_UtilCtxt* uc, XP_U16 stringCode )
{
    LOG_FUNC();
    return "";
}


static XP_Bool
and_util_warnIllegalWord( XW_UtilCtxt* uc, BadWordInfo* bwi, 
                          XP_U16 turn, XP_Bool turnLost )
{
    LOG_FUNC();
}


static void
and_util_remSelected(XW_UtilCtxt* uc)
{
    LOG_FUNC();
}


#ifndef XWFEATURE_STANDALONE_ONLY
static void
and_util_addrChange( XW_UtilCtxt* uc, const CommsAddrRec* oldAddr,
                     const CommsAddrRec* newAddr )
{
    LOG_FUNC();
}

#endif

#ifdef XWFEATURE_SEARCHLIMIT
static XP_Bool
and_util_getTraySearchLimits(XW_UtilCtxt* uc, XP_U16* min, XP_U16* max )
{
    LOG_FUNC();
}

#endif

#ifdef SHOW_PROGRESS
static void
and_util_engineStarting( XW_UtilCtxt* uc, XP_U16 nBlanks )
{
    LOG_FUNC();
}

static void
and_util_engineStopping( XW_UtilCtxt* uc )
{
    LOG_FUNC();
}
#endif


XW_UtilCtxt*
makeUtil( MPFORMAL JNIEnv** envp, jobject j_util, CurGameInfo* gi, 
          AndGlobals* closure )
{
    AndUtil* util = (AndUtil*)XP_CALLOC( mpool, sizeof(*util) );
    UtilVtable* vtable = (UtilVtable*)XP_CALLOC( mpool, sizeof(*vtable) );
    util->env = envp;
    JNIEnv* env = *envp;
    util->j_util = (*env)->NewGlobalRef( env, j_util );
    util->util.vtable = vtable;
    MPASSIGN( util->util.mpool, mpool );
    util->util.closure = closure;
    util->util.gameInfo = gi;

#define SET_PROC(nam) vtable->m_util_##nam = and_util_##nam
    SET_PROC(getVTManager);
#ifndef XWFEATURE_STANDALONE_ONLY
    SET_PROC(makeStreamFromAddr);
#endif
    SET_PROC(getSquareBonus);
    SET_PROC(userError);
    SET_PROC(userQuery);
    SET_PROC(userPickTile);
    SET_PROC(askPassword);
    SET_PROC(trayHiddenChange);
    SET_PROC(yOffsetChange);
#ifdef XWFEATURE_TURNCHANGENOTIFY
SET_PROC(    turnChanged);
#endif
    SET_PROC(notifyGameOver);
    SET_PROC(hiliteCell);
    SET_PROC(engineProgressCallback);
    SET_PROC(setTimer);
    SET_PROC(clearTimer);
    SET_PROC(requestTime);
    SET_PROC(altKeyDown);
    SET_PROC(getCurSeconds);
    SET_PROC(makeEmptyDict);
    SET_PROC(getUserString);
    SET_PROC(warnIllegalWord);
    SET_PROC(remSelected);

#ifndef XWFEATURE_STANDALONE_ONLY
    SET_PROC(addrChange);
#endif
#ifdef XWFEATURE_SEARCHLIMIT
    SET_PROC(getTraySearchLimits);
#endif
#ifdef SHOW_PROGRESS
    SET_PROC(engineStarting);
    SET_PROC(engineStopping);
#endif
#undef SET_PROC

    return (XW_UtilCtxt*)util;
} /* makeUtil */

void
destroyUtil( XW_UtilCtxt* utilc )
{
    AndUtil* util = (AndUtil*)utilc;
    JNIEnv *env = *util->env;
    (*env)->DeleteGlobalRef( env, util->j_util );
    XP_FREE( util->util.mpool, util->util.vtable );
    XP_FREE( util->util.mpool, util );
}
